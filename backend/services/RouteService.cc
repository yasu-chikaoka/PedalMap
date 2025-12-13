#include "RouteService.h"

#include <cmath>
#include <numbers>

namespace services {

namespace {

// NOLINTNEXTLINE(readability-magic-numbers)
constexpr double kEarthRadiusKm = 6371.0;

double toRadians(double degrees) {
    return degrees * std::numbers::pi / 180.0;
}

double toDegrees(double radians) {
    return radians * 180.0 / std::numbers::pi;
}

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

}  // namespace

std::optional<Coordinate> RouteService::calculateDetourPoint(const Coordinate& start,
                                                             const Coordinate& end,
                                                             double targetDistanceKm) {
    if (targetDistanceKm <= 0) {
        return std::nullopt;
    }

    double straightDist = calculateDistanceKm(start, end);

    // NOLINTNEXTLINE(readability-magic-numbers)
    const double kDetourThresholdFactor = 1.2;

    // 目標距離が直線距離の1.2倍未満なら迂回しない
    if (straightDist == 0 || targetDistanceKm <= straightDist * kDetourThresholdFactor) {
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
    double vecLen = std::sqrt(vecX * vecX + vecY * vecY); // calculateDistanceKmとほぼ同じはずだが平面近似

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

    return Coordinate{viaLat, viaLon};
}

}  // namespace services