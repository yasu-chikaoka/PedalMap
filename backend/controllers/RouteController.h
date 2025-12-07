#pragma once

#include <drogon/HttpController.h>
#include <osrm/engine_config.hpp>
#include <osrm/osrm.hpp>
#include <osrm/json_container.hpp>
#include <memory>

using namespace drogon;

namespace api
{
    namespace v1
    {
        class Route : public drogon::HttpController<Route>
        {
        public:
            METHOD_LIST_BEGIN
            // POST /api/v1/route/generate
            // Absolute path starting with / ignores namespace/class prefix
            ADD_METHOD_TO(Route::generate, "/api/v1/route/generate", Post);
            METHOD_LIST_END

            void generate(const HttpRequestPtr &req,
                          std::function<void(const HttpResponsePtr &)> &&callback);

            // コンストラクタでOSRMエンジンを初期化
            Route();

        private:
            std::unique_ptr<osrm::OSRM> osrm_;
        };
    }
}