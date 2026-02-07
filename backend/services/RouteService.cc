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

    // Calculate straight distance for reference
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

    // === Phase 1: Baseline Evaluation ===
    LOG_DEBUG << "Phase 1: Baseline evaluation";
    auto baseRoute = evaluator(fixedWaypoints);
    if (!baseRoute) {
        LOG_WARN << "Baseline route calculation failed";
        return std::nullopt;
    }

    LOG_DEBUG << "Baseline: dist=" << baseRoute->distance_m / 1000.0 << "km, elev="
              << baseRoute->elevation_gain_m << "m";

    // If no elevation target specified, or already close enough, return baseline
    const double kElevationToleranceM = 50.0;  // 50m tolerance
    if (targetElevationM <= 0 ||
        std::abs(baseRoute->elevation_gain_m - targetElevationM) < kElevationToleranceM) {
        LOG_DEBUG << "Baseline route acceptable (target elev: " << targetElevationM << "m)";
        return baseRoute;
    }

    // === Phase 2: Smart Probing ===
    LOG_DEBUG << "Phase 2: Smart probing. Current elev: " << baseRoute->elevation_gain_m
              << "m, Target: " << targetElevationM << "m";

    // Adaptive direction count based on elevation difference
    int numDirections = (std::abs(baseRoute->elevation_gain_m - targetElevationM) > 200) ? 16 : 8;
    std::vector<double> distFactors = {0.6, 0.8, 1.0, 1.2};

    auto probePoints =
        generateProbePoints(start, end, straightDist, targetDistanceKm, numDirections, distFactors);

    if (probePoints.empty()) {
        LOG_WARN << "No probe points generated, returning baseline";
        return baseRoute;
    }

    LOG_DEBUG << "Generated " << probePoints.size() << " probe points";

    // Batch elevation retrieval (leveraging existing cache infrastructure)
    std::vector<double> probeElevations;
    if (elevationProvider_) {
        probeElevations.reserve(probePoints.size());
        for (const auto& pt : probePoints) {
            auto elev = elevationProvider_->getElevationSync(pt);
            probeElevations.push_back(elev.value_or(0.0));
        }
        LOG_DEBUG << "Retrieved elevations for " << probeElevations.size() << " probe points";
    } else {
        LOG_WARN << "No elevation provider available, cannot perform smart probing";
        return baseRoute;
    }

    // Select best candidate probes
    auto selectedIndices =
        selectBestProbes(probePoints, probeElevations, baseRoute->elevation_gain_m,
                         targetElevationM, 2  // Top 2 candidates
        );

    if (selectedIndices.empty()) {
        LOG_WARN << "No candidates selected from probes, returning baseline";
        return baseRoute;
    }

    // === Phase 3: Detailed Evaluation ===
    LOG_DEBUG << "Phase 3: Detailed evaluation of " << selectedIndices.size() << " candidates";

    std::optional<RouteResult> bestRoute = baseRoute;
    double bestCost = std::abs(baseRoute->elevation_gain_m - targetElevationM) / 100.0 +
                      std::abs(baseRoute->distance_m / 1000.0 - targetDistanceKm);

    LOG_DEBUG << "Baseline cost: " << bestCost;

    for (int idx : selectedIndices) {
        std::vector<Coordinate> candidateWps;
        candidateWps.push_back(probePoints[idx]);
        candidateWps.insert(candidateWps.end(), fixedWaypoints.begin(), fixedWaypoints.end());

        auto result = evaluator(candidateWps);
        if (result) {
            double distDiff = std::abs(result->distance_m / 1000.0 - targetDistanceKm);
            double elevDiff = std::abs(result->elevation_gain_m - targetElevationM);
            double cost = distDiff + elevDiff / 100.0;

            LOG_DEBUG << "Candidate[" << idx << "]: probe_elev=" << probeElevations[idx]
                      << "m, route_dist=" << result->distance_m / 1000.0
                      << "km, route_elev_gain=" << result->elevation_gain_m << "m, cost=" << cost;

            if (cost < bestCost) {
                bestCost = cost;
                bestRoute = result;
                LOG_DEBUG << "New best route found!";
            }
        } else {
            LOG_WARN << "Candidate[" << idx << "] route calculation failed";
        }
    }

    LOG_DEBUG << "Final best route: dist=" << bestRoute->distance_m / 1000.0
              << "km, elev=" << bestRoute->elevation_gain_m << "m";

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

std::vector<Coordinate> RouteService::generateProbePoints(
    const Coordinate& start, const Coordinate& end, double straightDist, double targetDistanceKm,
    int numDirections, const std::vector<double>& distanceFactors) {
    std::vector<Coordinate> probes;

    double midLat = (start.lat + end.lat) / 2.0;
    double midLon = (start.lon + end.lon) / 2.0;

    const double kLatDegToKm = 2 * std::numbers::pi * kEarthRadiusKm / 360.0;
    double kLonDegToKm = kLatDegToKm * std::cos(toRadians(midLat));

    double vecX = (end.lon - start.lon) * kLonDegToKm;
    double vecY = (end.lat - start.lat) * kLatDegToKm;
    double vecLen = std::sqrt(vecX * vecX + vecY * vecY);

    double perpX = (vecLen > 0) ? -vecY / vecLen : 1.0;
    double perpY = (vecLen > 0) ? vecX / vecLen : 0.0;

    for (double factor : distanceFactors) {
        double detourHeight = (targetDistanceKm - straightDist) * 0.5 * factor;
        if (detourHeight <= 0) continue;

        for (int i = 0; i < numDirections; ++i) {
            double angle = 2.0 * std::numbers::pi * i / numDirections;
            double perpX_rot = perpX * std::cos(angle) - perpY * std::sin(angle);
            double perpY_rot = perpX * std::sin(angle) + perpY * std::cos(angle);

            double lat = midLat + (perpY_rot * detourHeight) / kLatDegToKm;
            double lon = midLon + (perpX_rot * detourHeight) / kLonDegToKm;

            probes.push_back({lat, lon});
        }
    }

    return probes;
}

std::vector<int> RouteService::selectBestProbes(const std::vector<Coordinate>& probePoints,
                                                 const std::vector<double>& elevations,
                                                 double currentElevation, double targetElevation,
                                                 int topN) {
    if (probePoints.size() != elevations.size()) {
        LOG_WARN << "Probe points and elevations size mismatch";
        return {};
    }

    bool needMore = targetElevation > currentElevation;

    // Scoring: favor directions that move closer to target elevation
    std::vector<std::pair<int, double>> scored;
    for (size_t i = 0; i < elevations.size(); ++i) {
        // Higher elevation points score higher when we need more elevation
        // Lower elevation points score higher when we need less elevation
        double score = needMore ? elevations[i] : -elevations[i];
        scored.push_back({i, score});
    }

    // Sort by score descending
    std::sort(scored.begin(), scored.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    std::vector<int> selected;
    for (int i = 0; i < std::min(topN, static_cast<int>(scored.size())); ++i) {
        selected.push_back(scored[i].first);
        LOG_DEBUG << "Selected probe #" << scored[i].first << " with elevation "
                  << elevations[scored[i].first] << "m (score: " << scored[i].second << ")";
    }

    return selected;
}

}  // namespace services
