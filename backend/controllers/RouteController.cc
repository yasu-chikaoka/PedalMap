#include "RouteController.h"

#include <iostream>
#include <osrm/coordinate.hpp>
#include <osrm/json_container.hpp>
#include <osrm/match_parameters.hpp>
#include <osrm/nearest_parameters.hpp>
#include <osrm/route_parameters.hpp>
#include <osrm/status.hpp>
#include <osrm/table_parameters.hpp>

#include "services/RouteService.h"

namespace api::v1 {

Route::Route()
    : configService_(std::make_shared<services::ConfigService>()),
      osrmClient_(std::make_shared<services::OSRMClient>(*configService_)),
      spotService_(std::make_shared<services::SpotService>(*configService_)) {}

void Route::generate(const HttpRequestPtr &req,
                     std::function<void(const HttpResponsePtr &)> &&callback) {
    auto jsonPtr = req->getJsonObject();
    if (!jsonPtr) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k400BadRequest);
        resp->setBody("Invalid JSON format");
        callback(resp);
        return;
    }

    if (!jsonPtr->isMember("start_point") || !jsonPtr->isMember("end_point")) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k400BadRequest);
        resp->setBody("Missing start_point or end_point");
        callback(resp);
        return;
    }

    services::Coordinate start{(*jsonPtr)["start_point"]["lat"].asDouble(),
                               (*jsonPtr)["start_point"]["lon"].asDouble()};
    services::Coordinate end{(*jsonPtr)["end_point"]["lat"].asDouble(),
                             (*jsonPtr)["end_point"]["lon"].asDouble()};

    LOG_DEBUG << "Request: Start(" << start.lat << ", " << start.lon << ") End(" << end.lat << ", "
              << end.lon << ")";

    std::vector<services::Coordinate> waypoints = services::RouteService::parseWaypoints(*jsonPtr);

    double targetDistanceKm = 0.0;
    if (jsonPtr->isMember("preferences") &&
        (*jsonPtr)["preferences"].isMember("target_distance_km")) {
        targetDistanceKm = (*jsonPtr)["preferences"]["target_distance_km"].asDouble();
    }

    if (targetDistanceKm > 0) {
        LOG_DEBUG << "Target Distance: " << targetDistanceKm << " km";
    }

    std::optional<services::RouteResult> bestRoute;
    double minDistanceDiff = std::numeric_limits<double>::max();

    if (waypoints.empty() && targetDistanceKm > 0) {
        auto detourCandidates =
            services::RouteService::calculateDetourPoints(start, end, targetDistanceKm);

        if (detourCandidates.empty()) {
            // 迂回不要または計算不可の場合、直通ルートを試行
            osrm::RouteParameters params =
                services::RouteService::buildRouteParameters(start, end, {});
            osrm::json::Object osrmResult;
            if (osrmClient_->Route(params, osrmResult) == osrm::Status::Ok) {
                bestRoute = services::RouteService::processRoute(osrmResult);
            }
        } else {
            for (const auto& candidate : detourCandidates) {
                std::vector<services::Coordinate> currentWaypoints = {candidate};
                osrm::RouteParameters params =
                    services::RouteService::buildRouteParameters(start, end, currentWaypoints);
                osrm::json::Object osrmResult;

                if (osrmClient_->Route(params, osrmResult) == osrm::Status::Ok) {
                    auto route = services::RouteService::processRoute(osrmResult);
                    if (route) {
                        double diff = std::abs(route->distance_m / 1000.0 - targetDistanceKm);
                        if (diff < minDistanceDiff) {
                            minDistanceDiff = diff;
                            bestRoute = route;
                        }
                    }
                }
            }
        }
    } else {
        // Waypointsが指定されている、またはターゲット距離がない場合
        osrm::RouteParameters params =
            services::RouteService::buildRouteParameters(start, end, waypoints);
        osrm::json::Object osrmResult;
        if (osrmClient_->Route(params, osrmResult) == osrm::Status::Ok) {
            bestRoute = services::RouteService::processRoute(osrmResult);
        }
    }

    if (!bestRoute) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k400BadRequest);
        resp->setBody("Route calculation failed");
        callback(resp);
        return;
    }

    LOG_DEBUG << "Route geometry: " << bestRoute->geometry;

    Json::Value respJson;
    respJson["summary"]["total_distance_m"] = bestRoute->distance_m;
    respJson["summary"]["estimated_moving_time_s"] = bestRoute->duration_s;
    respJson["geometry"] = bestRoute->geometry;

    // ルート沿いのスポットを検索
    double searchRadius = configService_->getSpotSearchRadius();
    // ポリラインジオメトリ検索を使用
    auto spots = spotService_->searchSpotsAlongRoute(bestRoute->geometry, searchRadius);
    for (const auto &spot : spots) {
        Json::Value stop;
        stop["name"] = spot.name;
        stop["type"] = spot.type;
        stop["location"]["lat"] = spot.lat;
        stop["location"]["lon"] = spot.lon;
        stop["rating"] = spot.rating;
        respJson["stops"].append(stop);
    }

    auto resp = HttpResponse::newHttpJsonResponse(respJson);
    callback(resp);
}

}  // namespace api::v1
