#pragma once

#include <drogon/HttpController.h>

#include <memory>
#include <osrm/engine_config.hpp>
#include <osrm/json_container.hpp>
#include <osrm/osrm.hpp>

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
    // / で始まる絶対パスは、名前空間/クラスのプレフィックスを無視します
    ADD_METHOD_TO(Route::generate, "/api/v1/route/generate", Post);
    METHOD_LIST_END

    /**
     * @brief ルート生成リクエストを処理するハンドラ
     *
     * @param req HTTPリクエスト
     * @param callback レスポンスを返すためのコールバック関数
     */
    void generate(const HttpRequestPtr &req,
                  std::function<void(const HttpResponsePtr &)> &&callback);

    // コンストラクタで依存サービスを初期化
    Route();

    // 依存性注入のためのセッター
    void setConfigService(std::shared_ptr<services::ConfigService> config) {
        configService_ = config;
    }
    void setOSRMClient(std::shared_ptr<services::OSRMClient> client) { osrmClient_ = client; }
    void setSpotService(std::shared_ptr<services::SpotService> service) { spotService_ = service; }
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
