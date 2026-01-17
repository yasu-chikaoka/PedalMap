#pragma once

#include <json/json.h>

#include <functional>
#include <memory>
#include <optional>
#include <osrm/osrm.hpp>
#include <osrm/route_parameters.hpp>
#include <string>
#include <vector>

#include "Coordinate.h"

namespace services {

namespace elevation {
class IElevationProvider;
}

struct RouteResult {
    double distance_m;
    double duration_s;
    double elevation_gain_m;
    std::string geometry;
    std::vector<Coordinate> path;
};

class RouteService {
   public:
    explicit RouteService(
        std::shared_ptr<elevation::IElevationProvider> elevationProvider = nullptr);
    virtual ~RouteService() = default;

    virtual std::optional<Coordinate> calculateDetourPoint(const Coordinate& start,
                                                           const Coordinate& end,
                                                           double targetDistanceKm);

    virtual std::vector<Coordinate> calculateDetourPoints(const Coordinate& start,
                                                          const Coordinate& end,
                                                          double targetDistanceKm);

    virtual std::vector<Coordinate> calculatePolygonDetourPoints(const Coordinate& start,
                                                                 const Coordinate& end,
                                                                 double targetDistanceKm);

    static std::vector<Coordinate> parseWaypoints(const Json::Value& json);

    using RouteEvaluator =
        std::function<std::optional<RouteResult>(const std::vector<Coordinate>&)>;

    virtual std::optional<RouteResult> findBestRoute(const Coordinate& start, const Coordinate& end,
                                                     const std::vector<Coordinate>& fixedWaypoints,
                                                     double targetDistanceKm,
                                                     double targetElevationM,
                                                     const RouteEvaluator& evaluator);

    static osrm::RouteParameters buildRouteParameters(const Coordinate& start,
                                                      const Coordinate& end,
                                                      const std::vector<Coordinate>& waypoints);

    virtual std::optional<RouteResult> processRoute(const osrm::json::Object& osrmResult);

    /**
     * @brief ルート全体の獲得標高を計算する
     */
    virtual double calculateElevationGain(const std::vector<Coordinate>& path);

   private:
    std::shared_ptr<elevation::IElevationProvider> elevationProvider_;
};

}  // namespace services
