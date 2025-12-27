#include "RouteService.h"

#include <cmath>
#include <iostream>
#include <numbers>
#include <osrm/nearest_parameters.hpp>

namespace services {

namespace {

constexpr double kEarthRadiusKm = 6371.0;
constexpr double kDetourThresholdFactor = 1.2;
constexpr double kMicroDegreeFactor = 1000000.0;

double toRadians(double degrees) { return degrees * std::numbers::pi / 180.0; }

double toDegrees(double radians) { return radians * 180.0 / std::numbers::pi; }

// ハーバーサイン公式
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

}  // namespace

std::optional<Coordinate> RouteService::calculateDetourPoint(const Coordinate& start,
                                                             const Coordinate& end,
                                                             double targetDistanceKm) {
    if (targetDistanceKm <= 0) {
        return std::nullopt;
    }

    double straightDist = calculateDistanceKm(start, end);

    // 目標距離が直線距離の1.2倍未満なら迂回しない
    if (straightDist == 0 || targetDistanceKm <= straightDist * kDetourThresholdFactor) {
        return std::nullopt;
    }

    // NOLINTNEXTLINE(readability-magic-numbers)
    double midLat = (start.lat + end.lat) / 2.0;
    // NOLINTNEXTLINE(readability-magic-numbers)
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
    double p1Lon = start.lon + (end.lon - start.lon) / 3.0 + (perpX * offsetHeight) / kLonDegToKm;
    result.push_back({p1Lat, p1Lon});

    // P2: 2/3 point + offset
    double p2Lat =
        start.lat + 2.0 * (end.lat - start.lat) / 3.0 + (perpY * offsetHeight) / kLatDegToKm;
    double p2Lon =
        start.lon + 2.0 * (end.lon - start.lon) / 3.0 + (perpX * offsetHeight) / kLonDegToKm;
    result.push_back({p2Lat, p2Lon});

    return result;
}

std::optional<RouteResult> RouteService::findBestRoute(const Coordinate& start,
                                                       const Coordinate& end,
                                                       double targetDistanceKm,
                                                       double targetElevationM,
                                                       const RouteEvaluator& evaluator) {
    if (targetDistanceKm <= 0) return std::nullopt;

    double straightDist = calculateDistanceKm(start, end);

    // 候補生成のためのパラメータ
    // 直線距離が短い場合は、より大きな係数で探索範囲を広げる
    double baseExpansion = (straightDist > 0) ? (targetDistanceKm / straightDist) : 1.5;

    // 複数の展開係数で候補を生成 (MCSSアプローチ)
    std::vector<double> expansionFactors;
    if (straightDist == 0) {  // 完全な周回
        expansionFactors = {0.2, 0.3, 0.4, 0.5, 0.6};
    } else {
        // 現在の距離に対する目標距離の比率に基づく
        double ratio = targetDistanceKm / straightDist;
        if (ratio < 1.2) {
            expansionFactors = {0.3, 0.5};  // わずかな迂回
        } else {
            // 目標距離に応じた「膨らみ」のバリエーション
            expansionFactors = {0.8, 1.0, 1.2, 1.5};
        }
    }

    // 左右(-1, 1)および拡張係数の組み合わせで候補点を生成
    struct Candidate {
        std::vector<Coordinate> waypoints;
        std::string type;
    };
    std::vector<Candidate> candidates;

    double midLat = (start.lat + end.lat) / 2.0;
    double midLon = (start.lon + end.lon) / 2.0;
    const double kLatDegToKm = 2 * std::numbers::pi * kEarthRadiusKm / 360.0;
    double kLonDegToKm = kLatDegToKm * std::cos(toRadians(midLat));

    double vecX = (end.lon - start.lon) * kLonDegToKm;
    double vecY = (end.lat - start.lat) * kLatDegToKm;
    double vecLen = std::sqrt(vecX * vecX + vecY * vecY);

    // ベクトルが0（完全な周回）の場合、適当な方向（北向きなど）を基準にする
    double perpX, perpY;
    if (vecLen == 0) {
        perpX = 1.0;
        perpY = 0.0;
    } else {
        perpX = -vecY / vecLen;
        perpY = vecX / vecLen;
    }

    // 周回の場合、目標距離の円周を想定した半径
    double loopRadiusKm = targetDistanceKm / (2 * std::numbers::pi);

    for (double factor : expansionFactors) {
        double currentHeight = (vecLen == 0) ? (loopRadiusKm * factor * 5.0)  // 周回時の調整
                                             : (targetDistanceKm - straightDist) * 0.5 * factor;

        // 幾何学的計算による迂回点 (Single Point)
        for (double side : {-1.0, 1.0}) {
            double viaLat = midLat + (side * perpY * currentHeight) / kLatDegToKm;
            double viaLon = midLon + (side * perpX * currentHeight) / kLonDegToKm;
            candidates.push_back({{{viaLat, viaLon}}, "Single"});
        }

        // Polygon (2 points)
        // targetDistanceKmに基づいて分割点を計算
        if (vecLen > 0) {
            double offsetHeight = currentHeight * 0.8;
            double p1Lat =
                start.lat + (end.lat - start.lat) / 3.0 + (perpY * offsetHeight) / kLatDegToKm;
            double p1Lon =
                start.lon + (end.lon - start.lon) / 3.0 + (perpX * offsetHeight) / kLonDegToKm;
            double p2Lat = start.lat + 2.0 * (end.lat - start.lat) / 3.0 +
                           (perpY * offsetHeight) / kLatDegToKm;
            double p2Lon = start.lon + 2.0 * (end.lon - start.lon) / 3.0 +
                           (perpX * offsetHeight) / kLonDegToKm;
            candidates.push_back({{{p1Lat, p1Lon}, {p2Lat, p2Lon}}, "Polygon"});

            // 反対側
            p1Lat = start.lat + (end.lat - start.lat) / 3.0 - (perpY * offsetHeight) / kLatDegToKm;
            p1Lon = start.lon + (end.lon - start.lon) / 3.0 - (perpX * offsetHeight) / kLonDegToKm;
            p2Lat = start.lat + 2.0 * (end.lat - start.lat) / 3.0 -
                    (perpY * offsetHeight) / kLatDegToKm;
            p2Lon = start.lon + 2.0 * (end.lon - start.lon) / 3.0 -
                    (perpX * offsetHeight) / kLonDegToKm;
            candidates.push_back({{{p1Lat, p1Lon}, {p2Lat, p2Lon}}, "Polygon"});
        }
    }

    std::optional<RouteResult> bestRoute = std::nullopt;
    double minCost = std::numeric_limits<double>::max();

    // 重み設定
    const double kW_Distance = 1.0;
    const double kW_Elevation = 2.0;  // 標高の優先度を高く設定

    for (const auto& cand : candidates) {
        auto result = evaluator(cand.waypoints);
        if (result) {
            double distDiff = std::abs(result->distance_m / 1000.0 - targetDistanceKm);
            double elevDiff = 0.0;
            if (targetElevationM > 0) {
                // TODO: 標高データが取得できている場合のみ計算
                // 現状は0が入っているため、機能しないが枠組みとして用意
                elevDiff = std::abs(result->elevation_gain_m - targetElevationM);
            }

            // コスト関数: 距離誤差と標高誤差の加重和
            // 正規化: 距離はkm単位、標高はm単位だが、1kmの重みと100mの重みをどう考えるか
            // ここでは単純に値を重み付けする
            double cost = kW_Distance * distDiff + kW_Elevation * (elevDiff / 100.0);

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
    res.elevation_gain_m = 0.0;
    res.geometry = route.values.at("geometry").get<osrm::json::String>().value;

    if (route.values.contains("legs")) {
        const auto& legs = route.values.at("legs").get<osrm::json::Array>();
        for (const auto& legValue : legs.values) {
            const auto& leg = legValue.get<osrm::json::Object>();
            if (leg.values.contains("steps")) {
                const auto& steps = leg.values.at("steps").get<osrm::json::Array>();
                for (const auto& stepValue : steps.values) {
                    const auto& step = stepValue.get<osrm::json::Object>();
                    if (step.values.contains("intersections")) {
                        const auto& intersections =
                            step.values.at("intersections").get<osrm::json::Array>();
                        for (const auto& intersectionValue : intersections.values) {
                            const auto& intersection = intersectionValue.get<osrm::json::Object>();
                            if (intersection.values.contains("location")) {
                                const auto& loc =
                                    intersection.values.at("location").get<osrm::json::Array>();
                                res.path.push_back({loc.values[1].get<osrm::json::Number>().value,
                                                    loc.values[0].get<osrm::json::Number>().value});
                            }
                        }
                    }
                }
            }
            // Annotation processing for elevation gain
            if (leg.values.contains("annotation")) {
                const auto& annotation = leg.values.at("annotation").get<osrm::json::Object>();
                if (annotation.values.contains("datasources")) {
                    const auto& datasources =
                        annotation.values.at("datasources").get<osrm::json::Array>();
                    // TODO: Implement elevation lookup from datasources or metadata if available
                    // For now, we rely on external or simulation data as OSRM doesn't output
                    // elevation by default without specific profile adjustments.
                }
            }
        }
    }
    return res;
}

}  // namespace services
