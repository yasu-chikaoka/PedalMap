#pragma once

#include <drogon/HttpController.h>
#include <memory>
#include <osrm/json_container.hpp>

#include "services/ConfigService.h"
#include "services/OSRMClient.h"
#include "services/RouteService.h"
#include "services/SpotService.h"

using namespace drogon;

namespace api::v1 {

class Route : public drogon::HttpController<Route> {
   public:
    METHOD_LIST_BEGIN
    // POST /api/v1/route/generate
    ADD_METHOD_TO(Route::generate, "/api/v1/route/generate", Post);
    METHOD_LIST_END

    Route() = default;
    virtual ~Route() = default;

    /**
     * @brief Handle route generation request
     */
    void generate(const HttpRequestPtr &req,
                  std::function<void(const HttpResponsePtr &)> &&callback);

    // Dependency Injection Setters
    void setConfigService(std::shared_ptr<services::ConfigService> config) {
        configService_ = config;
    }
    void setOSRMClient(std::shared_ptr<services::OSRMClient> client) { 
        osrmClient_ = client; 
    }
    void setSpotService(std::shared_ptr<services::SpotService> service) { 
        spotService_ = service; 
    }
    void setRouteService(std::shared_ptr<services::RouteService> service) {
        routeService_ = service;
    }

   private:
    std::shared_ptr<services::ConfigService> configService_;
    std::shared_ptr<services::OSRMClient> osrmClient_;
    std::shared_ptr<services::SpotService> spotService_;
    std::shared_ptr<services::RouteService> routeService_;
};

}  // namespace api::v1
