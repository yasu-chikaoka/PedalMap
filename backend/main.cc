#include <drogon/drogon.h>

#include <iostream>

#include "services/ConfigService.h"

int main() {
    services::ConfigService config;
    
    std::cout << "Starting Cycling Backend Server..." << std::endl;

    const int kPort = config.getServerPort();
    // HTTPリスナーのアドレスとポートを設定
    drogon::app().addListener("0.0.0.0", kPort);

    // ログ出力を有効化
    drogon::app().setLogLevel(trantor::Logger::kDebug);

    // CORSフィルタ
    std::string allowOrigin = config.getAllowOrigin();
    drogon::app().registerPostHandlingAdvice(
        [allowOrigin](const drogon::HttpRequestPtr &req, const drogon::HttpResponsePtr &resp) {
            resp->addHeader("Access-Control-Allow-Origin", allowOrigin);
            resp->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
            resp->addHeader("Access-Control-Allow-Headers", "Content-Type");
        });

    // CORSプリフライト用のOPTIONSメソッドハンドラ
    drogon::app().registerHandler(
        "/api/v1/route/generate",
        [](const drogon::HttpRequestPtr &req,
           std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k200OK);
            callback(resp);
        },
        {drogon::Options});

    // APIサーバーを実行
    drogon::app().run();

    return 0;
}