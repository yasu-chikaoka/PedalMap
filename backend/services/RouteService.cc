#include "RouteService.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <numbers>
#include <osrm/route_parameters.hpp>

#include "elevation/IElevationProvider.h"

// Logger
#include <trantor/utils/Logger.h>

namespace services {

namespace {

constexpr double kEarthRadiusKm = 6371.0;
constexpr double kDetourThresholdFactor = 1.2;

double toRadians(double degrees) { return degrees * std::numbers::pi / 180.0; }

// Haversine formula
double calculateDistanceKm(const Coordinate& p1, const Coordinate& p2) {
    double dLat = toRadians(p2.lat - p1.lat);
    double dLon = toRadians(p2.lon - p1.lon);
    double lat1 = toRadians(p1.lat);
    double lat2 = toRadians(p2.lat);

    double a = std::sin(dLat / 2) * std::sin(dLat / 2) +
               std::sin(dLon / 2) * std::sin(dLon / 2) * std::cos(lat1) * std::cos(lat2);
    double c = 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a));
    return kEarthRadiusKm * c;
}

std::vector<Coordinate> decodePolyline(const std::string& encoded) {
    std::vector<Coordinate> points;
    int index = 0, len = encoded.length();
    int lat = 0, lng = 0;

    while (index < len) {
        int b, shift = 0, result = 0;
        do {
            if (index >= len) break;
            b = encoded[index++] - 63;
            result |= (b & 0x1f) << shift;
            shift += 5;
        } while (b >= 0x20);
        int dlat = ((result & 1) ? ~(result >> 1) : (result >> 1));
        lat += dlat;

        shift = 0;
        result = 0;
        do {
            if (index >= len) break;
            b = encoded[index++] - 63;
            result |= (b & 0x1f) << shift;
            shift += 5;
        } while (b >= 0x20);
        int dlng = ((result & 1) ? ~(result >> 1) : (result >> 1));
        lng += dlng;

        points.push_back({static_cast<double>(lat) / 1e5, static_cast<double>(lng) / 1e5});
    }
    return points;
}

}  // namespace

RouteService::RouteService(std::shared_ptr<elevation::IElevationProvider> elevationProvider)
    : elevationProvider_(std::move(elevationProvider)) {}

std::optional<Coordinate> RouteService::calculateDetourPoint(const Coordinate& start,
                                                             const Coordinate& end,
                                                             double targetDistanceKm) {
    if (targetDistanceKm <= 0) {
        return std::nullopt;
    }

    double straightDist = calculateDistanceKm(start, end);

    // If target distance is less than 1.2x straight distance, don't detour
    if (straightDist == 0 || targetDistanceKm <= straightDist * kDetourThresholdFactor) {
        return std::nullopt;
    }

    double midLat = (start.lat + end.lat) / 2.0;
    double midLon = (start.lon + end.lon) / 2.0;

    double halfTarget = targetDistanceKm / 2.0;
    double halfStraight = straightDist / 2.0;
    double detourHeight = std::sqrt(halfTarget * halfTarget - halfStraight * halfStraight);

    if (detourHeight <= 0) {
        return std::nullopt;
    }

    const double kLatDegToKm = 2 * std::numbers::pi * kEarthRadiusKm / 360.0;
    double kLonDegToKm = kLatDegToKm * std::cos(toRadians(midLat));

    double vecX = (end.lon - start.lon) * kLonDegToKm;
    double vecY = (end.lat - start.lat) * kLatDegToKm;
    double vecLen = std::sqrt(vecX * vecX + vecY * vecY);

    if (vecLen == 0) {
        return std::nullopt;
    }

    double perpX = -vecY / vecLen;
    double perpY = vecX / vecLen;

    double viaLat = midLat + (perpY * detourHeight) / kLatDegToKm;
    double viaLon = midLon + (perpX * detourHeight) / kLonDegToKm;

    return Coordinate{viaLat, viaLon};
}

std::vector<Coordinate> RouteService::calculateDetourPoints(const Coordinate& start,
                                                            const Coordinate& end,
                                                            double targetDistanceKm) {
    if (targetDistanceKm <= 0) {
        return {};
    }

    double straightDist = calculateDistanceKm(start, end);

    if (straightDist == 0 || targetDistanceKm <= straightDist * kDetourThresholdFactor) {
        return {};
    }

    double midLat = (start.lat + end.lat) / 2.0;
    double midLon = (start.lon + end.lon) / 2.0;
    const double kLatDegToKm = 2 * std::numbers::pi * kEarthRadiusKm / 360.0;
    double kLonDegToKm = kLatDegToKm * std::cos(toRadians(midLat));

    double vecX = (end.lon - start.lon) * kLonDegToKm;
    double vecY = (end.lat - start.lat) * kLatDegToKm;
    double vecLen = std::sqrt(vecX * vecX + vecY * vecY);

    if (vecLen == 0) {
        return {};
    }

    double perpX = -vecY / vecLen;
    double perpY = vecX / vecLen;

    std::vector<Coordinate> candidates;
    std::vector<double> heightFactors = {0.8, 1.0, 1.2};
    std::vector<double> sideFactors = {-1.0, 1.0};

    for (double hf : heightFactors) {
        double currentTarget = straightDist + (targetDistanceKm - straightDist) * hf;
        double halfTarget = currentTarget / 2.0;
        double halfStraight = straightDist / 2.0;
        if (halfTarget <= halfStraight) continue;

        double detourHeight = std::sqrt(halfTarget * halfTarget - halfStraight * halfStraight);

        for (double sf : sideFactors) {
            double viaLat = midLat + (sf * perpY * detourHeight) / kLatDegToKm;
            double viaLon = midLon + (sf * perpX * detourHeight) / kLonDegToKm;
            candidates.push_back(Coordinate{viaLat, viaLon});
        }
    }

    return candidates;
}

std::vector<Coordinate> RouteService::calculatePolygonDetourPoints(const Coordinate& start,
                                                                   const Coordinate& end,
                                                                   double targetDistanceKm) {
    if (targetDistanceKm <= 0) return {};

    double straightDist = calculateDistanceKm(start, end);
    if (straightDist == 0 || targetDistanceKm <= straightDist * kDetourThresholdFactor) {
        return {};
    }

    double midLat = (start.lat + end.lat) / 2.0;
    const double kLatDegToKm = 2 * std::numbers::pi * kEarthRadiusKm / 360.0;
    double kLonDegToKm = kLatDegToKm * std::cos(toRadians(midLat));

    double vecX = (end.lon - start.lon) * kLonDegToKm;
    double vecY = (end.lat - start.lat) * kLatDegToKm;
    double vecLen = std::sqrt(vecX * vecX + vecY * vecY);

    if (vecLen == 0) return {};

    double perpX = -vecY / vecLen;
    double perpY = vecX / vecLen;

    double surplus = targetDistanceKm - straightDist;
    double offsetHeight = surplus * 0.4;

    std::vector<Coordinate> result;
    // P1: 1/3 point + offset
    double p1Lat = start.lat + (end.lat - start.lat) / 3.0 + (perpY * offsetHeight) / kLatDegToKm;
    double p1Lon = start.lon + (end.lon - start.lon) / 3.0 + (perpX * offsetHeight) / kLatDegToKm;
    result.push_back({p1Lat, p1Lon});

    // P2: 2/3 point + offset
    double p2Lat =
        start.lat + 2.0 * (end.lat - start.lat) / 3.0 + (perpY * offsetHeight) / kLatDegToKm;
    double p2Lon =
        start.lon + 2.0 * (end.lon - start.lon) / 3.0 + (perpX * offsetHeight) / kLatDegToKm;
    result.push_back({p2Lat, p2Lon});

    return result;
}

std::optional<RouteResult> RouteService::findBestRoute(
    const Coordinate& start, const Coordinate& end, const std::vector<Coordinate>& fixedWaypoints,
    double targetDistanceKm, double targetElevationM, const RouteEvaluator& evaluator) {
    if (targetDistanceKm <= 0) return std::nullopt;

    double straightDist;
    if (fixedWaypoints.empty()) {
        straightDist = calculateDistanceKm(start, end);
    } else {
        straightDist = calculateDistanceKm(start, fixedWaypoints[0]);
        for (size_t i = 0; i < fixedWaypoints.size() - 1; ++i) {
            straightDist += calculateDistanceKm(fixedWaypoints[i], fixedWaypoints[i + 1]);
        }
        straightDist += calculateDistanceKm(fixedWaypoints.back(), end);
    }

    // MCSS Algorithm: Multi-Candidate Sampling & Selection

    // 1. Determine expansion factors based on ratio
    std::vector<double> expansionFactors;
    if (straightDist == 0) {  // Loop
        expansionFactors = {0.2, 0.3, 0.4, 0.5, 0.6};
    } else {
        double ratio = targetDistanceKm / straightDist;
        if (ratio < 1.1) {
            expansionFactors = {0.1, 0.2};
        } else {
            expansionFactors = {0.5, 0.8, 1.0, 1.2, 1.5};
        }
    }

    struct Candidate {
        std::vector<Coordinate> waypoints;
        std::string type;
    };
    std::vector<Candidate> candidates;

    // Base candidate: Direct path (or just fixed waypoints)
    candidates.push_back({fixedWaypoints, "Direct"});

    // Determine segment to insert detour
    Coordinate segmentStart = start;
    Coordinate segmentEnd = fixedWaypoints.empty() ? end : fixedWaypoints[0];

    double midLat = (segmentStart.lat + segmentEnd.lat) / 2.0;
    double midLon = (segmentStart.lon + segmentEnd.lon) / 2.0;
    const double kLatDegToKm = 2 * std::numbers::pi * kEarthRadiusKm / 360.0;
    double kLonDegToKm = kLatDegToKm * std::cos(toRadians(midLat));

    double vecX = (segmentEnd.lon - segmentStart.lon) * kLonDegToKm;
    double vecY = (segmentEnd.lat - segmentStart.lat) * kLatDegToKm;
    double vecLen = std::sqrt(vecX * vecX + vecY * vecY);

    double perpX, perpY;
    if (vecLen == 0) {
        perpX = 1.0;
        perpY = 0.0;
    } else {
        perpX = -vecY / vecLen;
        perpY = vecX / vecLen;
    }

    double loopRadiusKm = targetDistanceKm / (2 * std::numbers::pi);

    for (double factor : expansionFactors) {
        double currentHeight = (vecLen == 0) ? (loopRadiusKm * factor * 5.0)
                                             : (targetDistanceKm - straightDist) * 0.5 * factor;

        if (currentHeight <= 0) continue;

        // Single Point Detour
        for (double side : {-1.0, 1.0}) {
            double viaLat = midLat + (side * perpY * currentHeight) / kLatDegToKm;
            double viaLon = midLon + (side * perpX * currentHeight) / kLonDegToKm;

            std::vector<Coordinate> candWps;
            candWps.push_back({viaLat, viaLon});
            candWps.insert(candWps.end(), fixedWaypoints.begin(), fixedWaypoints.end());
            candidates.push_back({candWps, "Single"});
        }

        // Polygon Detour (2 points)
        if (vecLen > 0) {
            double offsetHeight = currentHeight * 0.8;
            for (double side : {-1.0, 1.0}) {
                double p1Lat = segmentStart.lat + (segmentEnd.lat - segmentStart.lat) / 3.0 +
                               (side * perpY * offsetHeight) / kLatDegToKm;
                double p1Lon = segmentStart.lon + (segmentEnd.lon - segmentStart.lon) / 3.0 +
                               (side * perpX * offsetHeight) / kLonDegToKm;
                double p2Lat = segmentStart.lat + 2.0 * (segmentEnd.lat - segmentStart.lat) / 3.0 +
                               (side * perpY * offsetHeight) / kLatDegToKm;
                double p2Lon = segmentStart.lon + 2.0 * (segmentEnd.lon - segmentStart.lon) / 3.0 +
                               (side * perpX * offsetHeight) / kLonDegToKm;

                std::vector<Coordinate> candWps;
                candWps.push_back({p1Lat, p1Lon});
                candWps.push_back({p2Lat, p2Lon});
                candWps.insert(candWps.end(), fixedWaypoints.begin(), fixedWaypoints.end());
                candidates.push_back({candWps, "Polygon"});
            }
        }
    }

    std::optional<RouteResult> bestRoute = std::nullopt;
    double minCost = std::numeric_limits<double>::max();

    const double kW_Distance = 1.0;
    // Increased weight for elevation to ensure it's prioritized when requested
    const double kW_Elevation = 5.0;

    for (const auto& cand : candidates) {
        auto result = evaluator(cand.waypoints);
        if (result) {
            double distDiff = std::abs(result->distance_m / 1000.0 - targetDistanceKm);
            double elevDiff = 0.0;
            if (targetElevationM > 0) {
                elevDiff = std::abs(result->elevation_gain_m - targetElevationM);
            }

            // Cost function
            double cost = kW_Distance * distDiff + kW_Elevation * (elevDiff / 100.0);

            LOG_TRACE << "Candidate: dist=" << result->distance_m 
                      << "m, elev=" << result->elevation_gain_m 
                      << "m, cost=" << cost;

            if (cost < minCost) {
                minCost = cost;
                bestRoute = result;
            }
        }
    }

    return bestRoute;
}

std::vector<Coordinate> RouteService::parseWaypoints(const Json::Value& json) {
    std::vector<Coordinate> waypoints;
    if (json.isMember("waypoints")) {
        const auto& wpArray = json["waypoints"];
        if (wpArray.isArray()) {
            for (const auto& waypoint : wpArray) {
                if (waypoint.isMember("lat") && waypoint.isMember("lon")) {
                    double lat = waypoint["lat"].asDouble();
                    double lon = waypoint["lon"].asDouble();
                    waypoints.emplace_back(Coordinate{lat, lon});
                }
            }
        }
    }
    return waypoints;
}

osrm::RouteParameters RouteService::buildRouteParameters(const Coordinate& start,
                                                         const Coordinate& end,
                                                         const std::vector<Coordinate>& waypoints) {
    osrm::RouteParameters params;
    params.coordinates.emplace_back(osrm::util::FloatLongitude{start.lon},
                                    osrm::util::FloatLatitude{start.lat});
    for (const auto& wp : waypoints) {
        params.coordinates.emplace_back(osrm::util::FloatLongitude{wp.lon},
                                        osrm::util::FloatLatitude{wp.lat});
    }
    params.coordinates.emplace_back(osrm::util::FloatLongitude{end.lon},
                                    osrm::util::FloatLatitude{end.lat});

    params.geometries = osrm::RouteParameters::GeometriesType::Polyline;
    params.overview = osrm::RouteParameters::OverviewType::Full;
    params.steps = true;
    return params;
}

std::optional<RouteResult> RouteService::processRoute(const osrm::json::Object& osrmResult) {
    if (!osrmResult.values.contains("routes")) {
        return std::nullopt;
    }
    const auto& routes = osrmResult.values.at("routes").get<osrm::json::Array>();
    if (routes.values.empty()) {
        return std::nullopt;
    }

    const auto& route = routes.values[0].get<osrm::json::Object>();
    RouteResult res;
    res.distance_m = route.values.at("distance").get<osrm::json::Number>().value;
    res.duration_s = route.values.at("duration").get<osrm::json::Number>().value;
    res.geometry = route.values.at("geometry").get<osrm::json::String>().value;
    res.elevation_gain_m = 0.0;

    // Use decoded polyline for path to ensure accurate elevation calculation
    res.path = decodePolyline(res.geometry);

    // Calculate elevation gain
    if (elevationProvider_ && !res.path.empty()) {
        res.elevation_gain_m = calculateElevationGain(res.path);
        LOG_DEBUG << "Processed path size: " << res.path.size()
                  << ", calculated elevation gain: " << res.elevation_gain_m;
    } else {
        if (!elevationProvider_) LOG_WARN << "Elevation provider is null";
        if (res.path.empty()) LOG_WARN << "Path is empty after decoding";
    }

    return res;
}

double RouteService::calculateElevationGain(const std::vector<Coordinate>& path) {
    if (!elevationProvider_ || path.empty()) {
        return 0.0;
    }

    double totalGain = 0.0;
    std::optional<double> lastElevation = std::nullopt;

    // Sampling could be implemented here for performance
    for (const auto& coord : path) {
        auto currentElevation = elevationProvider_->getElevationSync(coord);
        if (currentElevation) {
            if (lastElevation) {
                if (*currentElevation > *lastElevation) {
                    totalGain += (*currentElevation - *lastElevation);
                }
            }
            lastElevation = currentElevation;
        }
    }

    return totalGain;
}

}  // namespace services
