#pragma once

#include <drogon/HttpController.h>

#include <functional>
#include <memory>
#include <osrm/json_container.hpp>

#include "services/ConfigService.h"
#include "services/OSRMClient.h"
#include "services/RouteService.h"
#include "services/SpotService.h"

namespace api::v1 {

class Route : public drogon::HttpController<Route> {
   public:
    METHOD_LIST_BEGIN
    // POST /api/v1/route/generate
    ADD_METHOD_TO(Route::generate, "/api/v1/route/generate", drogon::Post);
    METHOD_LIST_END

    Route() = default;
    virtual ~Route() = default;

    /**
     * @brief Handle route generation request
     */
    void generate(const drogon::HttpRequestPtr &req,
                  std::function<void(const drogon::HttpResponsePtr &)> &&callback);

    // Dependency Injection Setters
    static void setConfigService(std::shared_ptr<services::ConfigService> config) {
        configService_ = config;
    }
    static void setOSRMClient(std::shared_ptr<services::OSRMClient> client) {
        osrmClient_ = client;
    }
    static void setSpotService(std::shared_ptr<services::SpotService> service) {
        spotService_ = service;
    }
    static void setRouteService(std::shared_ptr<services::RouteService> service) {
        routeService_ = service;
    }

   private:
    static std::shared_ptr<services::ConfigService> configService_;
    static std::shared_ptr<services::OSRMClient> osrmClient_;
    static std::shared_ptr<services::SpotService> spotService_;
    static std::shared_ptr<services::RouteService> routeService_;
};

}  // namespace api::v1
