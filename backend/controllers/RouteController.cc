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

using namespace api::v1;
using namespace services;

Route::Route() : configService_(), osrmClient_(configService_), spotService_(configService_) {}

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

    Coordinate start{(*jsonPtr)["start_point"]["lat"].asDouble(),
                     (*jsonPtr)["start_point"]["lon"].asDouble()};
    Coordinate end{(*jsonPtr)["end_point"]["lat"].asDouble(),
                   (*jsonPtr)["end_point"]["lon"].asDouble()};

    LOG_DEBUG << "Request: Start(" << start.lat << ", " << start.lon << ") End(" << end.lat << ", "
              << end.lon << ")";

    std::vector<Coordinate> waypoints = RouteService::parseWaypoints(*jsonPtr);

    double targetDistanceKm = 0.0;
    if (jsonPtr->isMember("preferences") &&
        (*jsonPtr)["preferences"].isMember("target_distance_km")) {
        targetDistanceKm = (*jsonPtr)["preferences"]["target_distance_km"].asDouble();
    }
    
    std::cout << "[DEBUG] Target Distance: " << targetDistanceKm << " km" << std::endl;

    if (waypoints.empty() && targetDistanceKm > 0) {
        if (auto viaPoint = RouteService::calculateDetourPoint(start, end, targetDistanceKm)) {
            LOG_DEBUG << "Calculated Detour Point: (" << viaPoint->lat << ", " << viaPoint->lon
                      << ")";
            // OSRMClientを使用してポイントを道路上にスナップする
            /*
            osrm::NearestParameters params;
            params.coordinates.push_back({osrm::util::FloatLongitude{viaPoint->lon},
                                          osrm::util::FloatLatitude{viaPoint->lat}});
            params.number_of_results = 1;

            auto nearestPoints = osrmClient_.Nearest(params);
            if (!nearestPoints.empty()) {
                auto location = nearestPoints[0].values.at("location").get<osrm::json::Array>();
                Coordinate snapped{location.values[1].get<osrm::json::Number>().value,
                                   location.values[0].get<osrm::json::Number>().value};
                waypoints.push_back(snapped);
            } else {
                LOG_WARN << "Failed to snap detour point to road, using original coordinate.";
                waypoints.push_back(*viaPoint);
            }
            */
           // Skip manual snapping for now to debug
           waypoints.push_back(*viaPoint);
        }
    }

    osrm::RouteParameters params = RouteService::buildRouteParameters(start, end, waypoints);
    osrm::json::Object osrmResult;
    if (osrmClient_.Route(params, osrmResult) != osrm::Status::Ok) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k400BadRequest);
        resp->setBody("Route calculation failed");
        callback(resp);
        return;
    }

    auto routeResult = RouteService::processRoute(osrmResult);
    if (!routeResult) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k500InternalServerError);
        resp->setBody("Failed to process OSRM result");
        callback(resp);
        return;
    }

    LOG_DEBUG << "Route geometry: " << routeResult->geometry;

    Json::Value respJson;
    respJson["summary"]["total_distance_m"] = routeResult->distance_m;
    respJson["summary"]["estimated_moving_time_s"] = routeResult->duration_s;
    respJson["geometry"] = routeResult->geometry;

    // ルート沿いのスポットを検索
    double searchRadius = configService_.getSpotSearchRadius();
    // ポリラインジオメトリ検索を使用
    auto spots = spotService_.searchSpotsAlongRoute(routeResult->geometry, searchRadius);
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