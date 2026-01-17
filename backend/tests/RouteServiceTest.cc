#include <gtest/gtest.h>
#include <json/json.h>

#include <osrm/json_container.hpp>

#include "../services/RouteService.h"
#include "../services/elevation/IElevationProvider.h"

using namespace services;
using namespace services::elevation;

class MockElevationProvider : public IElevationProvider {
   public:
    void getElevation(const Coordinate& coord, ElevationCallback&& callback) override {
        callback(100.0);
    }
    void getElevations(const std::vector<Coordinate>& coords,
                       ElevationsCallback&& callback) override {
        callback(std::vector<double>(coords.size(), 100.0));
    }
    std::optional<double> getElevationSync(const Coordinate& coord) override {
        // Mock elevation: 100 * lat
        return coord.lat * 100.0;
    }
};

class RouteServiceTest : public ::testing::Test {
   protected:
    void SetUp() override {
        mockElevation_ = std::make_shared<MockElevationProvider>();
        service_ = std::make_unique<RouteService>(mockElevation_);
    }

    std::shared_ptr<MockElevationProvider> mockElevation_;
    std::unique_ptr<RouteService> service_;
};

TEST_F(RouteServiceTest, ParseWaypoints_Valid) {
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

TEST_F(RouteServiceTest, CalculateDetourPoint_DetourNeeded) {
    Coordinate start{35.0, 139.0};
    Coordinate end{35.0, 139.1};
    auto result = service_->calculateDetourPoint(start, end, 20.0);
    ASSERT_TRUE(result.has_value());
    // Should be different from start/end
    EXPECT_NE(result->lat, 35.0);
}

TEST_F(RouteServiceTest, CalculateElevationGain) {
    std::vector<Coordinate> path = {{35.0, 139.0}, {35.1, 139.0}, {35.2, 139.0}, {35.1, 139.0}};
    // Elevations: 3500, 3510, 3520, 3510
    // Gain: (3510-3500) + (3520-3510) = 10 + 10 = 20
    double gain = service_->calculateElevationGain(path);
    EXPECT_NEAR(gain, 20.0, 0.001);
}

TEST_F(RouteServiceTest, FindBestRoute_ElevationConsidered) {
    Coordinate start{35.0, 139.0};
    Coordinate end{35.0, 139.1};
    double targetDist = 20.0;
    double targetElev = 100.0;

    auto evaluator = [](const std::vector<Coordinate>& wps) -> std::optional<RouteResult> {
        RouteResult res;
        res.distance_m = 20000.0;
        res.duration_s = 1000.0;
        res.geometry = "poly";
        res.path = {{35.0, 139.0}, {35.1, 139.0}, {35.0, 139.1}};
        // Mock gain calculation inside evaluator or processRoute
        // Here we just return a fixed value to simulate OSRM result processing
        res.elevation_gain_m = 10.0;
        return res;
    };

    auto result = service_->findBestRoute(start, end, {}, targetDist, targetElev, evaluator);
    ASSERT_TRUE(result.has_value());
    // The logic selects the best route. Since we only have one candidate in this mock evaluator
    // (conceptually), it returns that.
    EXPECT_DOUBLE_EQ(result->elevation_gain_m, 10.0);
}

TEST_F(RouteServiceTest, ProcessRoute_WithElevation) {
    osrm::json::Object osrmResult;
    osrm::json::Array routes;
    osrm::json::Object route;
    route.values["distance"] = osrm::json::Number(1000.0);
    route.values["duration"] = osrm::json::Number(100.0);
    route.values["geometry"] = osrm::json::String("geom");

    osrm::json::Array legs;
    osrm::json::Object leg;
    osrm::json::Array steps;
    osrm::json::Object step;
    osrm::json::Array intersections;
    osrm::json::Object intersection1, intersection2;
    osrm::json::Array loc1, loc2;

    // Intersection 1: 35.0, 139.0
    loc1.values.push_back(osrm::json::Number(139.0));
    loc1.values.push_back(osrm::json::Number(35.0));
    intersection1.values["location"] = loc1;

    // Intersection 2: 35.1, 139.0
    loc2.values.push_back(osrm::json::Number(139.0));
    loc2.values.push_back(osrm::json::Number(35.1));
    intersection2.values["location"] = loc2;

    intersections.values.push_back(intersection1);
    intersections.values.push_back(intersection2);
    step.values["intersections"] = intersections;
    steps.values.push_back(step);
    leg.values["steps"] = steps;
    legs.values.push_back(leg);
    route.values["legs"] = legs;
    routes.values.push_back(route);
    osrmResult.values["routes"] = routes;

    auto result = service_->processRoute(osrmResult);
    ASSERT_TRUE(result.has_value());
    // Elevation diff: 35.1*100 - 35.0*100 = 10.0
    EXPECT_NEAR(result->elevation_gain_m, 10.0, 0.001);
}
