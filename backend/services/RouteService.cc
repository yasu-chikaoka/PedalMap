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
    const double DEG_TO_KM = 111.0;
    const double LON_FACTOR = 0.8;

    double dx = (end.lon - start.lon) * DEG_TO_KM * LON_FACTOR;
    double dy = (end.lat - start.lat) * DEG_TO_KM;
    double straightDist = std::sqrt(dx * dx + dy * dy);

    // 目標距離が直線距離の1.2倍未満なら迂回しない
    if (straightDist == 0 || targetDistanceKm <= straightDist * 1.2) {
        return std::nullopt;
    }

    double midLat = (start.lat + end.lat) / 2.0;
    double midLon = (start.lon + end.lon) / 2.0;

    // 直交ベクトル (dy, -dx) を正規化して、迂回距離分だけ伸ばす
    // 不足分の半分を高さとする三角形をイメージ
    // detourHeight = sqrt((target/2)^2 - (straight/2)^2)
    double halfTarget = targetDistanceKm / 2.0;
    double halfStraight = straightDist / 2.0;
    double detourHeight = std::sqrt(halfTarget * halfTarget - halfStraight * halfStraight);

    if (detourHeight <= 0) {
        return std::nullopt;
    }

    // 単位ベクトル化
    double perpX = -dy / straightDist;
    double perpY = dx / straightDist;

    // 緯度経度に戻す
    double viaLat = midLat + (perpY * detourHeight) / DEG_TO_KM;
    double viaLon = midLon + (perpX * detourHeight) / (DEG_TO_KM * LON_FACTOR);

    return Coordinate{viaLat, viaLon};
}

}  // namespace services