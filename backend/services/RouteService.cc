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
        std::cout << "[WARN] Target distance too short for detour. Straight: " << straightDist
                  << ", Target: " << targetDistanceKm
                  << ", Threshold: " << (straightDist * kDetourThresholdFactor) << std::endl;
        return std::nullopt;
    }

    // NOLINTNEXTLINE(readability-magic-numbers)
    double midLat = (start.lat + end.lat) / 2.0;
    // NOLINTNEXTLINE(readability-magic-numbers)
    double midLon = (start.lon + end.lon) / 2.0;

    // 直交ベクトル (dy, -dx) を正規化して、迂回距離分だけ伸ばす
    // 不足分の半分を高さとする三角形をイメージ
    // detourHeight = sqrt((target/2)^2 - (straight/2)^2)
    // NOLINTNEXTLINE(readability-magic-numbers)
    double halfTarget = targetDistanceKm / 2.0;
    // NOLINTNEXTLINE(readability-magic-numbers)
    double halfStraight = straightDist / 2.0;
    double detourHeight = std::sqrt(halfTarget * halfTarget - halfStraight * halfStraight);

    if (detourHeight <= 0) {
        return std::nullopt;
    }

    // 局所的な平面近似での移動量を計算するための係数
    // 緯度1度あたりの距離 (約111km)
    // NOLINTNEXTLINE(readability-magic-numbers)
    const double kLatDegToKm = 2 * std::numbers::pi * kEarthRadiusKm / 360.0;
    // 経度1度あたりの距離 (緯度によって変化)
    double kLonDegToKm = kLatDegToKm * std::cos(toRadians(midLat));

    // start -> end ベクトル (km単位)
    // 簡易的に中心緯度での変換係数を使用
    double vecX = (end.lon - start.lon) * kLonDegToKm;
    double vecY = (end.lat - start.lat) * kLatDegToKm;
    double vecLen =
        std::sqrt(vecX * vecX + vecY * vecY);  // calculateDistanceKmとほぼ同じはずだが平面近似

    if (vecLen == 0) {
        return std::nullopt;
    }

    // 単位法線ベクトル (-dy, dx)
    // 元のコードは (-distY, distX) だったので (-y, x) -> 左回転
    double perpX = -vecY / vecLen;
    double perpY = vecX / vecLen;

    // 緯度経度に戻す
    double viaLat = midLat + (perpY * detourHeight) / kLatDegToKm;
    double viaLon = midLon + (perpX * detourHeight) / kLonDegToKm;

    std::cout << "[DEBUG] Detour calculated. Mid(" << midLat << ", " << midLon << ") Height("
              << detourHeight << ") Via(" << viaLat << ", " << viaLon << ")" << std::endl;

    return Coordinate{viaLat, viaLon};
}

std::vector<Coordinate> RouteService::calculateDetourPoints(const Coordinate& start,
                                                            const Coordinate& end,
                                                            double targetDistanceKm) {
    if (targetDistanceKm <= 0) {
        return {};
    }

    double straightDist = calculateDistanceKm(start, end);

    // 目標距離が直線距離の1.2倍未満なら迂回しない
    if (straightDist == 0 || targetDistanceKm <= straightDist * kDetourThresholdFactor) {
        return {};
    }

    // 局所的な平面近似での移動量を計算するための係数
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

    // 単位法線ベクトル (-dy, dx)
    double perpX = -vecY / vecLen;
    double perpY = vecX / vecLen;

    std::vector<Coordinate> candidates;
    // 複数の高さを試す (目標距離の 80%, 100%, 120% に相当する高さを適当にサンプリング)
    // また、左右両方に振る
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
    // LOG_DEBUG << "Request: Start(" << start.lat << ", " << start.lon << ") End(" << end.lat << ",
    // "
    //           << end.lon << ")";

    osrm::RouteParameters params;
    params.coordinates.emplace_back(osrm::util::FloatLongitude{start.lon},
                                    osrm::util::FloatLatitude{start.lat});
    for (const auto& wp : waypoints) {
        params.coordinates.emplace_back(osrm::util::FloatLongitude{wp.lon},
                                        osrm::util::FloatLatitude{wp.lat});
    }
    params.coordinates.emplace_back(osrm::util::FloatLongitude{end.lon},
                                    osrm::util::FloatLatitude{end.lat});

    std::cout << "[DEBUG] Route params coordinates: " << params.coordinates.size() << std::endl;
    for (const auto& c : params.coordinates) {
        // osrm::util::Coordinate stores FixedLatitude (int), convert to double by dividing by 1e6
        double lat = static_cast<int>(c.lat) / kMicroDegreeFactor;
        double lon = static_cast<int>(c.lon) / kMicroDegreeFactor;
        std::cout << "  (" << lat << ", " << lon << ")" << std::endl;
    }

    params.geometries = osrm::RouteParameters::GeometriesType::Polyline;
    params.overview = osrm::RouteParameters::OverviewType::Full;
    params.steps = true;  // パス座標を取得するため
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

    std::cout << "[DEBUG] OSRM Result - Distance: " << res.distance_m
              << ", Duration: " << res.duration_s << ", Geometry: " << res.geometry << std::endl;

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
        }
    }
    return res;
}

}  // namespace services