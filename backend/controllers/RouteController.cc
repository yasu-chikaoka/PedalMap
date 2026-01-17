#include "RouteController.h"

#include <iostream>
#include <osrm/coordinate.hpp>
#include <osrm/json_container.hpp>
#include <osrm/match_parameters.hpp>
#include <osrm/nearest_parameters.hpp>
#include <osrm/route_parameters.hpp>
#include <osrm/status.hpp>
#include <osrm/table_parameters.hpp>

namespace api::v1 {

void Route::generate(const HttpRequestPtr &req,
                     std::function<void(const HttpResponsePtr &)> &&callback) {
    
    // Ensure dependencies are injected
    if (!configService_ || !osrmClient_ || !spotService_ || !routeService_) {
        LOG_ERROR << "RouteController dependencies not initialized";
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k500InternalServerError);
        resp->setBody("Internal Server Error: Service dependencies missing");
        callback(resp);
        return;
    }

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

    std::optional<services::RouteResult> bestRoute;

    double targetElevationM = 0.0;
    if (jsonPtr->isMember("preferences") &&
        (*jsonPtr)["preferences"].isMember("target_elevation_gain_m")) {
        targetElevationM = (*jsonPtr)["preferences"]["target_elevation_gain_m"].asDouble();
    }

    if (targetDistanceKm > 0) {
        LOG_DEBUG << "Target Distance: " << targetDistanceKm << " km, Elevation: " << targetElevationM << " m";
        
        auto evaluator = [&](const std::vector<services::Coordinate> &candidateWaypoints)
            -> std::optional<services::RouteResult> {
            osrm::RouteParameters params =
                services::RouteService::buildRouteParameters(start, end, candidateWaypoints);
            osrm::json::Object osrmResult;
            if (osrmClient_->Route(params, osrmResult) == osrm::Status::Ok) {
                return routeService_->processRoute(osrmResult);
            }
            return std::nullopt;
        };

        bestRoute = routeService_->findBestRoute(start, end, waypoints, targetDistanceKm,
                                                 targetElevationM, evaluator);
    } else {
        // Simple route calculation
        osrm::RouteParameters params =
            services::RouteService::buildRouteParameters(start, end, waypoints);
        osrm::json::Object osrmResult;
        if (osrmClient_->Route(params, osrmResult) == osrm::Status::Ok) {
            bestRoute = routeService_->processRoute(osrmResult);
        }
    }

    if (!bestRoute) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k400BadRequest);
        resp->setBody("Route calculation failed");
        callback(resp);
        return;
    }

    LOG_DEBUG << "Route geometry found. Distance: " << bestRoute->distance_m << "m";

    Json::Value respJson;
    respJson["summary"]["total_distance_m"] = bestRoute->distance_m;
    respJson["summary"]["estimated_moving_time_s"] = bestRoute->duration_s;
    respJson["summary"]["total_elevation_gain_m"] = bestRoute->elevation_gain_m;
    respJson["geometry"] = bestRoute->geometry;

    // Search spots along the route
    double searchRadius = configService_->getSpotSearchRadius();
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
