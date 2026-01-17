#include "SpotService.h"

#include <drogon/HttpClient.h>
#include <trantor/utils/Logger.h>

#include <algorithm>
#include <future>
#include <memory>
#include <set>
#include <thread>
#include <vector>

#include "../utils/PolylineDecoder.h"

namespace services {

SpotService::SpotService(const ConfigService& configService) : configService_(configService) {}

std::vector<Spot> SpotService::searchSpotsAlongRoute(const std::string& polylineGeometry,
                                                     double bufferMeters) {
    if (polylineGeometry.empty()) return {};

    std::string apiKey = configService_.getGoogleApiKey();
    if (apiKey.empty()) {
        LOG_WARN << "Google API Key is not set. Skipping spot search.";
        return {};
    }

    // Decode polyline
    auto path = utils::PolylineDecoder::decode(polylineGeometry);
    if (path.empty()) return {};

    std::vector<Spot> allSpots;
    std::set<std::string> seenNames;

    // Sample points along the route (e.g., every 25% or max 5 points)
    size_t step = std::max(static_cast<size_t>(1), path.size() / 4);
    std::vector<Coordinate> searchPoints;
    for (size_t i = 0; i < path.size(); i += step) {
        searchPoints.push_back(path[i]);
    }
    // Ensure end point is included
    if (searchPoints.back().lat != path.back().lat || searchPoints.back().lon != path.back().lon) {
        searchPoints.push_back(path.back());
    }

    double radius = configService_.getSpotSearchRadius();
    if (radius <= 0) radius = 1000.0;

    int timeoutSec = configService_.getApiTimeoutSeconds();
    if (timeoutSec <= 0) timeoutSec = 5;

    int maxRetries = configService_.getApiRetryCount();
    if (maxRetries < 0) maxRetries = 0;

    std::string baseUrl = configService_.getGoogleMapsApiBaseUrl();
    std::string apiPath = configService_.getGoogleMapsNearbySearchPath();

    // Reuse HttpClient
    auto client = drogon::HttpClient::newHttpClient(baseUrl);

    for (const auto& point : searchPoints) {
        LOG_INFO << "Searching spots around: " << point.lat << ", " << point.lon;

        for (int attempt = 0; attempt <= maxRetries; ++attempt) {
            if (attempt > 0) {
                LOG_INFO << "Retrying spot search (attempt " << attempt << "/" << maxRetries << ")...";
                std::this_thread::sleep_for(std::chrono::milliseconds(500 * attempt));
            }

            // Use shared_ptr for promise to avoid use-after-free on timeout
            auto promise = std::make_shared<std::promise<std::pair<bool, std::vector<Spot>>>>();
            auto future = promise->get_future();

            auto req = drogon::HttpRequest::newHttpRequest();
            req->setMethod(drogon::Get);
            req->setPath(apiPath);
            req->setParameter("location",
                              std::to_string(point.lat) + "," + std::to_string(point.lon));
            req->setParameter("radius", std::to_string(radius));
            req->setParameter("type", "restaurant|cafe|convenience_store|point_of_interest");
            req->setParameter("key", apiKey);
            req->setParameter("language", "ja");

            // Capture client to keep it alive if needed, though here we wait for it.
            // Capture promise by value (shared_ptr copy)
            client->sendRequest(req, [promise](drogon::ReqResult result,
                                                const drogon::HttpResponsePtr& response) {
                std::vector<Spot> spots;
                bool success = false;

                if (result == drogon::ReqResult::Ok && response->getStatusCode() == 200) {
                    auto jsonPtr = response->getJsonObject();
                    if (jsonPtr && jsonPtr->isMember("results") &&
                        (*jsonPtr)["results"].isArray()) {
                        success = true;
                        const auto& results = (*jsonPtr)["results"];
                        for (const auto& item : results) {
                            Spot spot;
                            spot.name = item.get("name", "").asString();
                            if (item.isMember("geometry") &&
                                item["geometry"].isMember("location")) {
                                spot.lat = item["geometry"]["location"].get("lat", 0.0).asDouble();
                                spot.lon = item["geometry"]["location"].get("lng", 0.0).asDouble();
                            }
                            spot.rating = item.get("rating", 0.0).asDouble();

                            if (item.isMember("types") && item["types"].isArray() &&
                                !item["types"].empty()) {
                                spot.type = item["types"][0].asString();
                            } else {
                                spot.type = "unknown";
                            }

                            spots.push_back(spot);
                        }
                    } else if (jsonPtr && jsonPtr->isMember("status") &&
                               (*jsonPtr)["status"].asString() == "ZERO_RESULTS") {
                        // Success but no results
                        success = true;
                    } else {
                        LOG_ERROR << "Invalid response or API error.";
                        if (jsonPtr)
                            LOG_DEBUG << "JSON: " << jsonPtr->toStyledString();
                    }
                } else {
                    LOG_ERROR << "Request failed. Result: " << (int)result
                              << ", Status: " << (response ? response->getStatusCode() : 0);
                }
                promise->set_value({success, spots});
            });

            if (future.wait_for(std::chrono::seconds(timeoutSec)) == std::future_status::ready) {
                auto result = future.get();
                if (result.first) {
                    for (const auto& spot : result.second) {
                        if (seenNames.find(spot.name) == seenNames.end()) {
                            allSpots.push_back(spot);
                            seenNames.insert(spot.name);
                        }
                    }
                    break;  // Success, exit retry loop
                }
            } else {
                LOG_WARN << "Spot search timed out for point.";
            }
        }
    }

    LOG_INFO << "Found total " << allSpots.size() << " unique spots.";
    return allSpots;
}

}  // namespace services
