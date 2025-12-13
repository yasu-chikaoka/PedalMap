#include <gtest/gtest.h>

#include "../services/RouteService.h"

using namespace services;

TEST(RouteServiceTest, CalculateDetourPoint_NoDetourNeeded) {
    // 直線距離に近い場合、経由地は生成されないはず
    // NOLINTNEXTLINE(readability-magic-numbers)
    Coordinate start{35.681236, 139.767125};  // 東京駅
    // NOLINTNEXTLINE(readability-magic-numbers)
    Coordinate end{35.685175, 139.7528};  // 皇居

    // 直線距離は約1.5km程度
    // 目標距離1km（直線より短い）
    // NOLINTNEXTLINE(readability-magic-numbers)
    auto result = RouteService::calculateDetourPoint(start, end, 1.0);
    EXPECT_FALSE(result.has_value());

    // 目標距離1.6km（直線よりわずかに長い程度）
    // NOLINTNEXTLINE(readability-magic-numbers)
    result = RouteService::calculateDetourPoint(start, end, 1.6);
    EXPECT_FALSE(result.has_value());
}

TEST(RouteServiceTest, CalculateDetourPoint_DetourNeeded) {
    // NOLINTNEXTLINE(readability-magic-numbers)
    Coordinate start{35.0, 139.0};
    // NOLINTNEXTLINE(readability-magic-numbers)
    Coordinate end{35.0, 139.1};
    // 緯度35度での経度0.1度差は約9km (111 * 0.8 * 0.1)

    // 目標距離20km（直線の2倍以上）
    // NOLINTNEXTLINE(readability-magic-numbers)
    auto result = RouteService::calculateDetourPoint(start, end, 20.0);

    ASSERT_TRUE(result.has_value());

    // 経由地は中間地点から離れているはず
    // start, endの緯度が同じなので、経由地の緯度は35.0から大きく離れるはず
    // NOLINTNEXTLINE(readability-magic-numbers,bugprone-unchecked-optional-access)
    EXPECT_NE(result->lat, 35.0);

    // start, endの中点 (139.05) に近いはず（垂直方向への移動なので経度は中点と同じになるはずだが、
    // 簡易計算ロジックでは経度方向のベクトル成分が0になるため）
    // NOLINTNEXTLINE(readability-magic-numbers,bugprone-unchecked-optional-access)
    EXPECT_NEAR(result->lon, 139.05, 0.001);
}

TEST(RouteServiceTest, CalculateDetourPoint_HighLatitude) {
    // 高緯度地域 (北緯60度)
    // 経度1度あたりの距離は赤道の半分程度になるはず (cos(60) = 0.5)
    // NOLINTNEXTLINE(readability-magic-numbers)
    Coordinate start{60.0, 10.0};
    // NOLINTNEXTLINE(readability-magic-numbers)
    Coordinate end{60.0, 11.0};

    // 直線距離計算
    // 緯度変化なし、経度1度差
    // 距離 ≒ 111.32 * cos(60) * 1 ≒ 55.66km

    // 目標距離 100km
    // NOLINTNEXTLINE(readability-magic-numbers)
    auto result = RouteService::calculateDetourPoint(start, end, 100.0);
    ASSERT_TRUE(result.has_value());

    // 垂直方向に移動するはずなので、経度は中点(10.5)付近
    // NOLINTNEXTLINE(readability-magic-numbers,bugprone-unchecked-optional-access)
    EXPECT_NEAR(result->lon, 10.5, 0.01);

    // 緯度は60.0から離れる
    // NOLINTNEXTLINE(readability-magic-numbers,bugprone-unchecked-optional-access)
    EXPECT_NE(result->lat, 60.0);
}