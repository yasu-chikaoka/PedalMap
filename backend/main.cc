#include <drogon/drogon.h>

#include <iostream>

int main() {
    std::cout << "Starting Cycling Backend Server..." << std::endl;

    const int kPort = 8080;
    // Set HTTP listener address and port
    drogon::app().addListener("0.0.0.0", kPort);

    // Enable logging
    drogon::app().setLogLevel(trantor::Logger::kDebug);

    // CORS Filter
    drogon::app().registerPostHandlingAdvice(
        [](const drogon::HttpRequestPtr &req, const drogon::HttpResponsePtr &resp) {
            resp->addHeader("Access-Control-Allow-Origin", "*");
            resp->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
            resp->addHeader("Access-Control-Allow-Headers", "Content-Type");
        });

    // Options method handler for CORS preflight
    drogon::app().registerHandler(
        "/api/v1/route/generate",
        [](const drogon::HttpRequestPtr &req,
           std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k200OK);
            callback(resp);
        },
        {drogon::Options});

    // Run API server
    drogon::app().run();

    return 0;
}