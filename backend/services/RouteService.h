#pragma once

#include <json/json.h>

#include <functional>
#include <optional>
#include <osrm/osrm.hpp>
#include <osrm/route_parameters.hpp>
#include <string>
#include <vector>

namespace services {

struct Coordinate {
    double lat;
    double lon;
};

struct RouteResult {
    double distance_m;
    double duration_s;
    double elevation_gain_m;
    std::string geometry;
    std::vector<Coordinate> path;
};

class RouteService {
   public:
    static std::optional<Coordinate> calculateDetourPoint(const Coordinate& start,
                                                          const Coordinate& end,
                                                          double targetDistanceKm);

    static std::vector<Coordinate> calculateDetourPoints(const Coordinate& start,
                                                         const Coordinate& end,
                                                         double targetDistanceKm);

    static std::vector<Coordinate> calculatePolygonDetourPoints(const Coordinate& start,
                                                                const Coordinate& end,
                                                                double targetDistanceKm);

    static std::vector<Coordinate> parseWaypoints(const Json::Value& json);

    using RouteEvaluator =
        std::function<std::optional<RouteResult>(const std::vector<Coordinate>&)>;

    static std::optional<RouteResult> findBestRoute(const Coordinate& start, const Coordinate& end,
                                                    double targetDistanceKm,
                                                    double targetElevationM,
                                                    const RouteEvaluator& evaluator);

    static osrm::RouteParameters buildRouteParameters(const Coordinate& start,
                                                      const Coordinate& end,
                                                      const std::vector<Coordinate>& waypoints);

    static std::optional<RouteResult> processRoute(const osrm::json::Object& osrmResult);
};

}  // namespace services
