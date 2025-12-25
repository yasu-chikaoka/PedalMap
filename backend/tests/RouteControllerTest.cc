#include <gtest/gtest.h>
#include <drogon/drogon.h>
#include "../controllers/RouteController.h"
#include "../services/SpotService.h"
#include "../services/OSRMClient.h"

using namespace api::v1;
using namespace services;
using namespace drogon;

// Mock SpotService
class MockSpotService : public SpotService {
public:
    MockSpotService(const ConfigService& config) : SpotService(config) {}
    
    // Override searchSpotsAlongRoute
    std::vector<Spot> searchSpotsAlongRoute(const std::string& polyline, double radius) override {
        if (shouldFail) {
            return {};
        }
        return mockSpots;
    }

    bool shouldFail = false;
    std::vector<Spot> mockSpots;
};

// Mock OSRMClient
class MockOSRMClient : public OSRMClient {
public:
    MockOSRMClient(const ConfigService& config) : OSRMClient(config) {}

    osrm::Status Route(const osrm::RouteParameters& params, osrm::json::Object& result) const override {
        if (shouldFail) {
            return osrm::Status::Error;
        }
        
        // Populate result with dummy data to mimic a successful route response
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
        result.values["code"] = osrm::json::String("Ok");

        return osrm::Status::Ok;
    }

    bool shouldFail = false;
};

class RouteControllerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize services
        configService = std::make_shared<ConfigService>();
        
        mockSpotService = std::make_shared<MockSpotService>(*configService);
        mockOSRMClient = std::make_shared<MockOSRMClient>(*configService);
        
        controller = std::make_shared<Route>();
        controller->setConfigService(configService);
        controller->setSpotService(mockSpotService);
        controller->setOSRMClient(mockOSRMClient);
    }

    std::shared_ptr<ConfigService> configService;
    std::shared_ptr<MockSpotService> mockSpotService;
    std::shared_ptr<MockOSRMClient> mockOSRMClient;
    std::shared_ptr<Route> controller;
};

TEST_F(RouteControllerTest, GenerateRoute_Success) {
    // Setup Mock Data
    Spot spot;
    spot.name = "Test Spot";
    spot.lat = 35.0;
    spot.lon = 139.0;
    mockSpotService->mockSpots.push_back(spot);

    // Create Request
    auto req = HttpRequest::newHttpRequest();
    Json::Value json;
    json["start_point"]["lat"] = 35.0;
    json["start_point"]["lon"] = 139.0;
    json["end_point"]["lat"] = 35.1;
    json["end_point"]["lon"] = 139.1;
    
    Json::StreamWriterBuilder builder;
    std::string body = Json::writeString(builder, json);
    req->setBody(body);
    req->setContentTypeCode(CT_APPLICATION_JSON);

    // Run Controller
    bool callbackCalled = false;
    controller->generate(req, [&](const HttpResponsePtr& resp) {
        callbackCalled = true;
        EXPECT_EQ(resp->getStatusCode(), k200OK);
        auto jsonBody = resp->getJsonObject();
        ASSERT_TRUE(jsonBody != nullptr);
        EXPECT_EQ((*jsonBody)["stops"].size(), 1);
        EXPECT_EQ((*jsonBody)["stops"][0]["name"].asString(), "Test Spot");
    });

    EXPECT_TRUE(callbackCalled);
}

TEST_F(RouteControllerTest, GenerateRoute_OSRMFail) {
    mockOSRMClient->shouldFail = true;

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
        EXPECT_EQ(resp->getStatusCode(), k400BadRequest);
    });
    EXPECT_TRUE(callbackCalled);
}

TEST_F(RouteControllerTest, GenerateRoute_MissingFields) {
    auto req = HttpRequest::newHttpRequest();
    Json::Value json;
    // Missing end_point
    json["start_point"]["lat"] = 35.0;
    json["start_point"]["lon"] = 139.0;
    
    Json::StreamWriterBuilder builder;
    req->setBody(Json::writeString(builder, json));
    req->setContentTypeCode(CT_APPLICATION_JSON);

    bool callbackCalled = false;
    controller->generate(req, [&](const HttpResponsePtr& resp) {
        callbackCalled = true;
        EXPECT_EQ(resp->getStatusCode(), k400BadRequest);
    });
    EXPECT_TRUE(callbackCalled);
}
