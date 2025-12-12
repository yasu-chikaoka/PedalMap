#include "RouteService.h"

#include <cmath>

namespace services {

std::optional<Coordinate> RouteService::calculateDetourPoint(const Coordinate& start,
                                                             const Coordinate& end,
                                                             double targetDistanceKm) {
    if (targetDistanceKm <= 0) {
        return std::nullopt;
    }

    // 簡易的な直線距離計算 (1度あたり約111kmと仮定)
    // 経度は緯度によって変わるが、日本付近(北緯35度)のcos(35)≒0.82 を掛ける
    // より正確には cos(midLat) を使うべきだが、簡易計算とする
    const double kDegToKm = 111.0;
    const double kLonFactor = 0.8;
    const double kDetourThresholdFactor = 1.2;

    double distX = (end.lon - start.lon) * kDegToKm * kLonFactor;
    double distY = (end.lat - start.lat) * kDegToKm;
    double straightDist = std::sqrt(distX * distX + distY * distY);

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

    // 単位ベクトル化
    double perpX = -distY / straightDist;
    double perpY = distX / straightDist;

    // 緯度経度に戻す
    double viaLat = midLat + (perpY * detourHeight) / kDegToKm;
    double viaLon = midLon + (perpX * detourHeight) / (kDegToKm * kLonFactor);

    return Coordinate{viaLat, viaLon};
}

}  // namespace services