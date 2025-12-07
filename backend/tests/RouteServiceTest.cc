#include <gtest/gtest.h>
#include "../services/RouteService.h"

using namespace services;

TEST(RouteServiceTest, CalculateDetourPoint_NoDetourNeeded) {
    // 直線距離に近い場合、経由地は生成されないはず
    Coordinate start{35.681236, 139.767125}; // 東京駅
    Coordinate end{35.685175, 139.7528};     // 皇居
    
    // 直線距離は約1.5km程度
    // 目標距離1km（直線より短い）
    auto result = RouteService::calculateDetourPoint(start, end, 1.0);
    EXPECT_FALSE(result.has_value());

    // 目標距離1.6km（直線よりわずかに長い程度）
    result = RouteService::calculateDetourPoint(start, end, 1.6);
    EXPECT_FALSE(result.has_value());
}

TEST(RouteServiceTest, CalculateDetourPoint_DetourNeeded) {
    Coordinate start{35.0, 139.0};
    Coordinate end{35.0, 139.1}; 
    // 緯度35度での経度0.1度差は約9km (111 * 0.8 * 0.1)

    // 目標距離20km（直線の2倍以上）
    auto result = RouteService::calculateDetourPoint(start, end, 20.0);
    
    ASSERT_TRUE(result.has_value());
    
    // 経由地は中間地点から離れているはず
    // start, endの緯度が同じなので、経由地の緯度は35.0から大きく離れるはず
    EXPECT_NE(result->lat, 35.0);
    
    // start, endの中点 (139.05) に近いはず（垂直方向への移動なので経度は中点と同じになるはずだが、
    // 簡易計算ロジックでは経度方向のベクトル成分が0になるため）
    EXPECT_NEAR(result->lon, 139.05, 0.001);
}