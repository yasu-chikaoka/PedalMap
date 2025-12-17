#pragma once

#include <drogon/HttpController.h>

#include <memory>
#include <osrm/engine_config.hpp>
#include <osrm/json_container.hpp>
#include <osrm/osrm.hpp>

#include "services/SpotService.h"
#include "services/ConfigService.h"
#include "services/OSRMClient.h"

using namespace drogon;

namespace api::v1 {
class Route : public drogon::HttpController<Route> {
   public:
    METHOD_LIST_BEGIN
    // POST /api/v1/route/generate
    // Absolute path starting with / ignores namespace/class prefix
    ADD_METHOD_TO(Route::generate, "/api/v1/route/generate", Post);
    METHOD_LIST_END

    void generate(const HttpRequestPtr &req,
                  std::function<void(const HttpResponsePtr &)> &&callback);

    // コンストラクタで依存サービスを初期化
    Route();

   private:
    services::ConfigService configService_;
    services::OSRMClient osrmClient_;
    services::SpotService spotService_;
};
}  // namespace api::v1