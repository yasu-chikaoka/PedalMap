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
        // OSRMClientのモックを作成する際に、実際のOSRM初期化を回避する必要があるかもしれません。
        // しかし、MockOSRMClientのコンストラクタはOSRMClientのコンストラクタを呼び出します。
        // OSRMClientのコンストラクタがConfigServiceを使用してOSRMエンジンを初期化しようとする場合、
        // 必要なファイルがないと失敗する可能性があります。
        // ここでは、ConfigServiceが有効な（しかしダミーの）パスを返すようにするか、
        // OSRMClientのコンストラクタが例外をスローしないようにする必要があります。
        // 実際のOSRMClientの実装を見ると、ConfigServiceからパスを取得してOSRMを初期化しています。
        // テスト環境ではこれらのファイルが存在しないため、例外が発生します。
        // これを回避するために、MockOSRMClientでOSRMClientのコンストラクタの動作をバイパスすることはC++では難しいです。
        // したがって、ConfigServiceをモックして、OSRMClientが初期化をスキップするか、
        // 存在しないファイルを読み込もうとして失敗するのを防ぐ必要があります。
        
        // しかし、OSRMClientのコンストラクタはConfigServiceへの参照を受け取るだけです。
        // OSRMエンジンの初期化はコンストラクタ内で行われていると思われます。
        
        // とりあえず、例外をキャッチして続行するようにしてみますが、
        // RouteControllerが依存するOSRMClientが不完全な状態になる可能性があります。
        // RouteControllerTestでは、Routeメソッドがオーバーライドされているため、
        // OSRMエンジン自体は使用されません。
        
        try {
             mockOSRMClient = std::make_shared<MockOSRMClient>(*configService);
        } catch (...) {
            // OSRM初期化エラーを無視（モックメソッドを使用するため）
            // 注意: これはハックです。本来はOSRMClientの設計を改善して、エンジン初期化を分離すべきです。
            // しかし、MockOSRMClientのインスタンス自体は作成される必要があります。
            // コンストラクタで例外が投げられるとインスタンス化できません。
            
            // 仕方がないので、ここではOSRMClientのコンストラクタが例外を投げないように
            // 修正するか、ConfigServiceで空のパスを返してOSRMが初期化されないようにする必要がありますが、
            // OSRMClientの実装次第です。
            
            // 暫定対応: OSRMClient.cc を修正して、ファイルが見つからない場合に例外ではなく
            // エラーログを出して初期化をスキップするように変更済みであればOKですが、
            // エラーログを見る限り "thrown in SetUp()" とあるので例外が出ています。
        }
        
        // 例外でmockOSRMClientがnullのままだとテストになりません。
        // OSRMClient.cc を修正する必要があります。
        // しかし、今はテストコードの修正で対応したいです。
        
        // ConfigServiceをモックして、OSRM関連の設定を空にすることで
        // OSRMClientが初期化を試みないようにできるかもしれません。
        // しかしConfigServiceは非仮想メソッドが多いです。
        
        // 最も確実なのは、OSRMClientのコンストラクタでの例外送出を抑制する変更を
        // OSRMClient.cc に加えることですが、それはプロダクションコードへの変更になります。
        // テストのためにプロダクションコードを堅牢にする（ファイルがない場合でもクラッシュしない）のは
        // 良いことなので、OSRMClient.cc を修正する方針が良いかもしれません。
        
        // ここでは、MockOSRMClientを正しくインスタンス化するために、
        // OSRMClient.cc の修正を前提として、通常の初期化を行います。
        // もしOSRMClient.ccが未修正なら、ここでのtry-catchは意味がありません
        // （make_shared内で例外が出るとポインタはnullのまま）。
        
        // 既存のコードはそのままにしておき、OSRMClient.ccを修正します。
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
