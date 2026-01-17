#include <drogon/drogon.h>

#include <iostream>
#include <memory>

#include "controllers/RouteController.h"
#include "services/ConfigService.h"
#include "services/OSRMClient.h"
#include "services/RouteService.h"
#include "services/SpotService.h"
#include "services/elevation/ElevationCacheManager.h"
#include "services/elevation/GSIElevationProvider.h"
#include "services/elevation/RedisElevationAdapter.h"
#include "services/elevation/SmartRefreshService.h"

int main() {
    std::cout << "Starting Cycling Backend Server..." << std::endl;

    // 1. Initialize Config Service
    auto configService = std::make_shared<services::ConfigService>();

    // 2. Configure Drogon
    const int kPort = configService->getServerPort();
    drogon::app().addListener("0.0.0.0", kPort);
    drogon::app().setLogLevel(trantor::Logger::kDebug);

    // Redis Client
    drogon::app().createRedisClient(configService->getRedisHost(), configService->getRedisPort(),
                                    configService->getRedisPassword());

    // CORS
    std::string allowOrigin = configService->getAllowOrigin();
    drogon::app().registerPostHandlingAdvice(
        [allowOrigin](const drogon::HttpRequestPtr &req, const drogon::HttpResponsePtr &resp) {
            resp->addHeader("Access-Control-Allow-Origin", allowOrigin);
            resp->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
            resp->addHeader("Access-Control-Allow-Headers", "Content-Type");
        });

    // OPTIONS Handler
    drogon::app().registerHandler(
        "/api/v1/route/generate",
        [](const drogon::HttpRequestPtr &req,
           std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k200OK);
            callback(resp);
        },
        {drogon::Options});

    // 3. Initialize Services (Dependency Injection Wiring)

    // OSRM & Spot Services
    auto osrmClient = std::make_shared<services::OSRMClient>(*configService);
    auto spotService = std::make_shared<services::SpotService>(*configService);

    // Elevation Stack
    auto backendProvider = std::make_shared<services::elevation::GSIElevationProvider>();

    // We need to get the Redis client from Drogon.
    // Note: createRedisClient is async/lazy, but getRedisClient returns the pointer.
    auto redisClient = drogon::app().getRedisClient();

    std::shared_ptr<services::RouteService> routeService;

    if (redisClient) {
        LOG_INFO << "Redis client initialized. Setting up Elevation Cache Layer.";
        auto repository = std::make_shared<services::elevation::RedisElevationAdapter>(redisClient);

        auto refreshService =
            std::make_shared<services::elevation::SmartRefreshService>(repository, backendProvider);
        refreshService->setRefreshThreshold(configService->getElevationRefreshThresholdScore());
        refreshService->startWorker();

        auto elevationManager = std::make_shared<services::elevation::ElevationCacheManager>(
            repository, backendProvider, refreshService,
            configService->getElevationLruCacheCapacity());

        routeService = std::make_shared<services::RouteService>(elevationManager);
    } else {
        LOG_WARN << "Redis client not available. Using direct GSI Elevation Provider.";
        routeService = std::make_shared<services::RouteService>(backendProvider);
    }

    // 4. Inject Dependencies into Controller
    // Drogon creates the controller instance automatically. We use static setters to inject
    // dependencies.
    api::v1::Route::setConfigService(configService);
    api::v1::Route::setOSRMClient(osrmClient);
    api::v1::Route::setSpotService(spotService);
    api::v1::Route::setRouteService(routeService);

    // 5. Run Server
    drogon::app().run();

    return 0;
}
