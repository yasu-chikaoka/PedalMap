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

    /**
     * @brief 候補プローブポイントを生成
     * @param start 開始地点
     * @param end 終了地点
     * @param straightDist 直線距離
     * @param targetDistanceKm 目標距離
     * @param numDirections 方向数（8または16）
     * @param distanceFactors 距離係数リスト
     * @return 候補ポイントのリスト
     */
    std::vector<Coordinate> generateProbePoints(const Coordinate& start, const Coordinate& end,
                                                double straightDist, double targetDistanceKm,
                                                int numDirections = 8,
                                                const std::vector<double>& distanceFactors = {
                                                    0.7, 1.0, 1.3});

    /**
     * @brief プローブポイントから最良候補を選択
     * @param probePoints 候補ポイント
     * @param elevations 各ポイントの標高
     * @param currentElevation 現在のルートの獲得標高
     * @param targetElevation 目標獲得標高
     * @param topN 上位N個を返す
     * @return 選択されたインデックスのリスト
     */
    std::vector<int> selectBestProbes(const std::vector<Coordinate>& probePoints,
                                      const std::vector<double>& elevations,
                                      double currentElevation, double targetElevation,
                                      int topN = 2);
};

}  // namespace services
