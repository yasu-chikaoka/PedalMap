#include <drogon/drogon.h>
#include <gtest/gtest.h>

#include "../controllers/RouteController.h"
#include "../services/OSRMClient.h"
#include "../services/RouteService.h"
#include "../services/SpotService.h"

using namespace api::v1;
using namespace services;
using namespace drogon;

// Mock SpotService
class MockSpotService : public SpotService {
   public:
    MockSpotService(const ConfigService& config) : SpotService(config) {}

    std::vector<Spot> searchSpotsAlongRoute(const std::string& polyline, double radius) override {
        return mockSpots;
    }
    std::vector<Spot> mockSpots;
};

// Mock OSRMClient
class MockOSRMClient : public OSRMClient {
   public:
    MockOSRMClient(const ConfigService& config) : OSRMClient(config) {}

    osrm::Status Route(const osrm::RouteParameters& params,
                       osrm::json::Object& result) const override {
        if (shouldFail) return osrm::Status::Error;

        osrm::json::Array routes;
        osrm::json::Object route;
        route.values["distance"] = osrm::json::Number(1000.0);
        route.values["duration"] = osrm::json::Number(600.0);
        route.values["geometry"] = osrm::json::String("dummy_polyline");

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
        result.values["routes"] = routes;
        return osrm::Status::Ok;
    }
    bool shouldFail = false;
};

// Mock RouteService
class MockRouteService : public RouteService {
   public:
    MockRouteService() : RouteService(nullptr) {}
    std::optional<RouteResult> processRoute(const osrm::json::Object& osrmResult) override {
        auto res = RouteService::processRoute(osrmResult);
        if (res) {
            res->elevation_gain_m = 50.0;  // 固定値
        }
        return res;
    }
    std::optional<RouteResult> findBestRoute(const Coordinate& start, const Coordinate& end,
                                             double targetDistanceKm, double targetElevationM,
                                             const RouteEvaluator& evaluator) override {
        return evaluator({});  // 簡単のため空のwaypointsで評価
    }
};

class RouteControllerTest : public ::testing::Test {
   protected:
    void SetUp() override {
        configService = std::make_shared<ConfigService>();
        mockSpotService = std::make_shared<MockSpotService>(*configService);
        mockOSRMClient = std::make_shared<MockOSRMClient>(*configService);
        mockRouteService = std::make_shared<MockRouteService>();

        controller = std::make_shared<Route>();
        controller->setConfigService(configService);
        controller->setSpotService(mockSpotService);
        controller->setOSRMClient(mockOSRMClient);
        controller->setRouteService(mockRouteService);
    }

    std::shared_ptr<ConfigService> configService;
    std::shared_ptr<MockSpotService> mockSpotService;
    std::shared_ptr<MockOSRMClient> mockOSRMClient;
    std::shared_ptr<MockRouteService> mockRouteService;
    std::shared_ptr<Route> controller;
};

TEST_F(RouteControllerTest, GenerateRoute_Success) {
    Spot spot;
    spot.name = "Test Spot";
    spot.lat = 35.0;
    spot.lon = 139.0;
    mockSpotService->mockSpots.push_back(spot);

    auto req = HttpRequest::newHttpRequest();
    Json::Value json;
    json["start_point"]["lat"] = 35.0;
    json["start_point"]["lon"] = 139.0;
    json["end_point"]["lat"] = 35.1;
    json["end_point"]["lon"] = 139.1;

    Json::StreamWriterBuilder builder;
    req->setBody(Json::writeString(builder, json));
    req->setContentTypeCode(CT_APPLICATION_JSON);

    bool callbackCalled = false;
    controller->generate(req, [&](const HttpResponsePtr& resp) {
        callbackCalled = true;
        EXPECT_EQ(resp->getStatusCode(), k200OK);
        auto jsonBody = resp->getJsonObject();
        ASSERT_NE(jsonBody, nullptr);
        EXPECT_EQ((*jsonBody)["summary"]["total_elevation_gain_m"].asDouble(), 50.0);
    });
    EXPECT_TRUE(callbackCalled);
}
