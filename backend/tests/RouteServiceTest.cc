#include <gtest/gtest.h>
#include <json/json.h>

#include <osrm/json_container.hpp>

#include "../services/RouteService.h"

using namespace services;

TEST(RouteServiceTest, ParseWaypoints_Valid) {
    Json::Value json;
    Json::Value waypoint1;
    waypoint1["lat"] = 35.0;
    waypoint1["lon"] = 139.0;
    Json::Value waypoint2;
    waypoint2["lat"] = 36.0;
    waypoint2["lon"] = 140.0;
    json["waypoints"].append(waypoint1);
    json["waypoints"].append(waypoint2);

    auto waypoints = RouteService::parseWaypoints(json);
    ASSERT_EQ(waypoints.size(), 2);
    EXPECT_DOUBLE_EQ(waypoints[0].lat, 35.0);
    EXPECT_DOUBLE_EQ(waypoints[1].lon, 140.0);
}

TEST(RouteServiceTest, ParseWaypoints_Invalid) {
    Json::Value json;
    Json::Value waypoint1;
    waypoint1["latitude"] = 35.0;  // 間違ったキー
    waypoint1["lon"] = 139.0;
    json["waypoints"].append(waypoint1);

    auto waypoints = RouteService::parseWaypoints(json);
    EXPECT_TRUE(waypoints.empty());
}

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

    // start, endの中点 (139.05) に近いはず
    // NOLINTNEXTLINE(readability-magic-numbers,bugprone-unchecked-optional-access)
    EXPECT_NEAR(result->lon, 139.05, 0.001);
}

TEST(RouteServiceTest, CalculateDetourPoints_DetourNeeded) {
    Coordinate start{35.0, 139.0};
    Coordinate end{35.0, 139.1};
    double targetKm = 20.0;

    auto candidates = RouteService::calculateDetourPoints(start, end, targetKm);
    EXPECT_FALSE(candidates.empty());
    // 3 heightFactors * 2 sideFactors = 6 candidates expected
    EXPECT_EQ(candidates.size(), 6);

    for (const auto& c : candidates) {
        EXPECT_GT(c.lat, 34.0);
        EXPECT_LT(c.lat, 36.0);
        EXPECT_GT(c.lon, 138.0);
        EXPECT_LT(c.lon, 140.0);
    }
}

TEST(RouteServiceTest, CalculatePolygonDetourPoints_DetourNeeded) {
    Coordinate start{35.0, 139.0};
    Coordinate end{35.0, 139.1};
    double targetKm = 20.0;

    auto waypoints = RouteService::calculatePolygonDetourPoints(start, end, targetKm);
    EXPECT_EQ(waypoints.size(), 2);

    for (const auto& c : waypoints) {
        EXPECT_GT(c.lat, 34.0);
        EXPECT_LT(c.lat, 36.0);
        EXPECT_GT(c.lon, 138.0);
        EXPECT_LT(c.lon, 140.0);
    }
}

TEST(RouteServiceTest, CalculateDetourPoint_HighLatitude) {
    // 高緯度地域 (北緯60度)
    // NOLINTNEXTLINE(readability-magic-numbers)
    Coordinate start{60.0, 10.0};
    // NOLINTNEXTLINE(readability-magic-numbers)
    Coordinate end{60.0, 11.0};

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

TEST(RouteServiceTest, ProcessRoute_Valid) {
    osrm::json::Object osrmResult;
    osrm::json::Array routes;
    osrm::json::Object route;
    route.values["distance"] = osrm::json::Number(1234.5);
    route.values["duration"] = osrm::json::Number(123.4);
    route.values["geometry"] = osrm::json::String("some_polyline");

    osrm::json::Array legs;
    osrm::json::Object leg;
    osrm::json::Array steps;
    osrm::json::Object step;
    osrm::json::Array intersections;
    osrm::json::Object intersection;
    osrm::json::Array location;
    location.values.push_back(osrm::json::Number(139.0));
    location.values.push_back(osrm::json::Number(35.0));
    intersection.values["location"] = location;
    intersections.values.push_back(intersection);
    step.values["intersections"] = intersections;
    steps.values.push_back(step);
    leg.values["steps"] = steps;
    legs.values.push_back(leg);
    route.values["legs"] = legs;

    routes.values.push_back(route);
    osrmResult.values["routes"] = routes;

    auto result = RouteService::processRoute(osrmResult);
    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(result->distance_m, 1234.5);
    EXPECT_DOUBLE_EQ(result->duration_s, 123.4);
    EXPECT_EQ(result->geometry, "some_polyline");
    ASSERT_EQ(result->path.size(), 1);
    EXPECT_DOUBLE_EQ(result->path[0].lat, 35.0);
}

TEST(RouteServiceTest, ProcessRoute_NoRoutes) {
    osrm::json::Object osrmResult;
    osrmResult.values["routes"] = osrm::json::Array();
    auto result = RouteService::processRoute(osrmResult);
    EXPECT_FALSE(result.has_value());
}

TEST(RouteServiceTest, CalculateDetourPoint_SameStartEnd) {
    Coordinate start{35.0, 139.0};
    auto result = RouteService::calculateDetourPoint(start, start, 10.0);
    EXPECT_FALSE(result.has_value());
}

TEST(RouteServiceTest, CalculateDetourPoint_SlightlyLongerDistance) {
    Coordinate start{35.0, 139.0};
    Coordinate end{35.1, 139.1};
    auto result = RouteService::calculateDetourPoint(start, end, 18.8);

    osrm::json::Object osrmResult;
    osrm::json::Array routes;
    osrm::json::Object route;
    route.values["distance"] = osrm::json::Number(1.0);
    route.values["duration"] = osrm::json::Number(1.0);
    route.values["geometry"] = osrm::json::String("g");
    routes.values.push_back(route);
    osrmResult.values["routes"] = routes;

    auto procResult = RouteService::processRoute(osrmResult);
    ASSERT_TRUE(procResult.has_value());
    EXPECT_TRUE(procResult->path.empty());
}
