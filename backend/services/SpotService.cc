#include "SpotService.h"

#include <drogon/HttpClient.h>
#include <future>
#include <iostream>

#include "../utils/PolylineDecoder.h"

namespace services {

SpotService::SpotService(const ConfigService& configService) : configService_(configService) {}

std::vector<Spot> SpotService::searchSpotsAlongRoute(const std::string& polylineGeometry,
                                                     double bufferMeters) {
    if (polylineGeometry.empty()) return {};

    std::string apiKey = configService_.getGoogleApiKey();
    if (apiKey.empty()) {
        // APIキーがない場合は空リストを返す（ログ出力しても良い）
        std::cerr << "[WARN] Google API Key is not set. Skipping spot search." << std::endl;
        return {};
    }

    auto path = utils::PolylineDecoder::decode(polylineGeometry);
    if (path.empty()) return {};

    // 簡易実装：ルートの中間地点を1箇所だけ検索対象とする
    auto midPoint = path[path.size() / 2];

    std::cout << "[INFO] Searching spots around: " << midPoint.lat << ", " << midPoint.lon << std::endl;

    // 非同期リクエストを同期的に待機するためのPromise
    std::promise<std::vector<Spot>> promise;
    auto future = promise.get_future();

    auto client = drogon::HttpClient::newHttpClient("https://maps.googleapis.com");
    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/maps/api/place/nearbysearch/json");
    req->setParameter("location", std::to_string(midPoint.lat) + "," + std::to_string(midPoint.lon));
    
    // ConfigServiceから半径を取得（なければデフォルト）
    double radius = configService_.getSpotSearchRadius();
    if (radius <= 0) radius = 1000.0;
    std::cout << "[INFO] Search radius: " << radius << " meters" << std::endl;
    req->setParameter("radius", std::to_string(radius));
    
    req->setParameter("type", "restaurant|cafe|convenience_store|point_of_interest");
    req->setParameter("key", apiKey);
    req->setParameter("language", "ja");

    client->sendRequest(req, [&promise](drogon::ReqResult result, const drogon::HttpResponsePtr& response) {
        std::vector<Spot> spots;
        if (result == drogon::ReqResult::Ok && response->getStatusCode() == 200) {
            auto jsonPtr = response->getJsonObject();
            if (jsonPtr && jsonPtr->isMember("results") && (*jsonPtr)["results"].isArray()) {
                const auto& results = (*jsonPtr)["results"];
                std::cout << "[INFO] Found " << results.size() << " spots." << std::endl;
                for (const auto& item : results) {
                    Spot spot;
                    spot.name = item.get("name", "").asString();
                    if (item.isMember("geometry") && item["geometry"].isMember("location")) {
                        spot.lat = item["geometry"]["location"].get("lat", 0.0).asDouble();
                        spot.lon = item["geometry"]["location"].get("lng", 0.0).asDouble();
                    }
                    spot.rating = item.get("rating", 0.0).asDouble();
                    
                    if (item.isMember("types") && item["types"].isArray() && !item["types"].empty()) {
                        spot.type = item["types"][0].asString();
                    } else {
                        spot.type = "unknown";
                    }
                    
                    spots.push_back(spot);
                }
            } else {
                 std::cerr << "[ERROR] Invalid response from Google Places API or no results found." << std::endl;
                 if (jsonPtr) {
                     std::cout << "[DEBUG] Response JSON: " << jsonPtr->toStyledString() << std::endl;
                 }
            }
        } else {
            std::cerr << "[ERROR] Google Places API request failed. Result: " << (int)result 
                      << ", Status: " << (response ? response->getStatusCode() : 0) << std::endl;
            if (response) {
                 std::cerr << "[ERROR] Response body: " << response->getBody() << std::endl;
            }
        }
        promise.set_value(spots);
    });

    // タイムアウト設定 (5秒)
    if (future.wait_for(std::chrono::seconds(5)) == std::future_status::ready) {
        return future.get();
    } else {
        std::cerr << "[WARN] Spot search timed out" << std::endl;
        return {};
    }
}

}  // namespace services
