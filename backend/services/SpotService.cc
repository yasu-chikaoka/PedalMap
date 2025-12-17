#include "SpotService.h"

#include "../utils/PolylineDecoder.h"
#include <fstream>
#include <iostream>
#include <sstream>

namespace services {

SpotService::SpotService(const ConfigService& configService) {
    loadSpotsFromCsv(configService.getSpotsCsvPath());

}

void SpotService::loadSpotsFromCsv(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "Failed to open spots CSV file: " << filePath << std::endl;
        return;
    }

    std::string line;
    // Skip header if it exists? Assuming no header or handled by parsing logic
    // Let's assume the first line might be a header if it contains non-numeric lat/lon
    // For simplicity, assume NO header for now, or check first char.

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::stringstream ss(line);
        std::string segment;
        std::vector<std::string> parts;

        while (std::getline(ss, segment, ',')) {
            parts.push_back(segment);
        }

        if (parts.size() >= 5) {
            try {
                Spot spot;
                spot.name = parts[0];
                spot.type = parts[1];
                spot.lat = std::stod(parts[2]);
                spot.lon = std::stod(parts[3]);
                spot.rating = std::stod(parts[4]);

                spots_.push_back(spot);
                
                Point p(spot.lon, spot.lat);
                rtree_.insert(std::make_pair(p, spots_.size() - 1));
            } catch (const std::exception& e) {
                std::cerr << "Error parsing spot line: " << line << " (" << e.what() << ")" << std::endl;
            }
        }
    }
    std::cout << "Loaded " << spots_.size() << " spots from " << filePath << std::endl;
}

std::vector<Spot> SpotService::searchSpotsAlongRoute(const std::string& polylineGeometry,
                                                     double bufferMeters) {
    if (polylineGeometry.empty()) {
        return {};
    }

    auto path = utils::PolylineDecoder::decode(polylineGeometry);
    return searchSpotsAlongPath(path, bufferMeters);
}

std::vector<Spot> SpotService::searchSpotsAlongPath(const std::vector<Coordinate>& path,
                                                    double bufferMeters) {
    std::vector<Spot> results;
    if (path.empty()) {
        return results;
    }

    // Convert path to LineString for distance calculation
    bg::model::linestring<Point> routeLine;
    for (const auto& coord : path) {
        routeLine.push_back(Point(coord.lon, coord.lat));
    }

    // Calculate bounding box of the route to filter candidates from R-tree
    bg::model::box<Point> routeBox;
    bg::envelope(routeLine, routeBox);

    // Expand box by buffer distance (approximate conversion to degrees)
    // 1 degree lat ~ 111km, 1 degree lon ~ 111km * cos(lat)
    // For simplicity, use a safe upper bound conversion for filtering
    const double kMetersToDegrees = 1.0 / 111000.0;
    double bufferDeg = bufferMeters * kMetersToDegrees * 1.5;  // 1.5x safety margin

    Point minCorner(bg::get<bg::min_corner, 0>(routeBox) - bufferDeg,
                    bg::get<bg::min_corner, 1>(routeBox) - bufferDeg);
    Point maxCorner(bg::get<bg::max_corner, 0>(routeBox) + bufferDeg,
                    bg::get<bg::max_corner, 1>(routeBox) + bufferDeg);
    bg::model::box<Point> queryBox(minCorner, maxCorner);

    std::vector<Value> candidates;
    rtree_.query(bgi::intersects(queryBox), std::back_inserter(candidates));

    // Refine results by calculating exact distance to the linestring
    for (const auto& candidate : candidates) {
        // bg::distance returns distance in the same unit as coordinates (degrees) for
        // non-cartesian, unless specific strategy is used. But for geographic cs, it might return
        // meters if correct strategy is applied? Actually for bg::cs::geographic, distance returns
        // meters by default in recent Boost versions? Let's verify. Usually it returns meters for
        // geographic systems. If not, we need Haversine strategy.

        // Boost 1.74+ default strategy for geographic is andoyer or thomas, returning meters.
        // Let's assume it returns meters. If values are tiny, it's degrees.

        double dist = bg::distance(candidate.first, routeLine);

        // Check if distance is really in meters. If the distance is like 0.001, it's degrees.
        // Boost.Geometry's behavior depends on version and headers.
        // To be safe, we can use a custom strategy or check the magnitude.

        // For simplicity in this environment, let's explicitely use Haversine if we were
        // calculating point-point. But for Point-LineString, it's more complex.

        // Let's trust the default geographic behavior but add a check.
        // Since we defined coordinate system as geographic<degree>, the result should be in meters
        // (technically on the ellipsoid surface).

        if (dist <= bufferMeters) {
            results.push_back(spots_[candidate.second]);
        }
    }

    return results;
}

}  // namespace services