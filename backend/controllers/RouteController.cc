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

namespace {
const double kLocationOffset = 0.001;
const double kRatingCafe = 4.5;
const double kRatingRestaurant = 4.2;
const double kAverageDivisor = 2.0;

std::vector<osrm::util::Coordinate> parseWaypoints(const std::shared_ptr<Json::Value> &jsonPtr) {
    std::vector<osrm::util::Coordinate> waypoints;
    if (jsonPtr->isMember("waypoints")) {
        const auto &wpArray = (*jsonPtr)["waypoints"];
        if (wpArray.isArray()) {
            for (const auto &waypoint : wpArray) {
                if (waypoint.isMember("lat") && waypoint.isMember("lon")) {
                    double lat = waypoint["lat"].asDouble();
                    double lon = waypoint["lon"].asDouble();
                    waypoints.emplace_back(
                        osrm::util::FloatLongitude{lon}, osrm::util::FloatLatitude{lat});
                }
            }
        }
    }
    return waypoints;
}

osrm::RouteParameters buildRouteParameters(double startLat, double startLon, double endLat,
                                           double endLon, double targetDistanceKm,
                                           const std::vector<osrm::util::Coordinate> &waypoints) {
    osrm::RouteParameters params;
    params.coordinates.emplace_back(osrm::util::FloatLongitude{startLon},
                                    osrm::util::FloatLatitude{startLat});

    if (!waypoints.empty()) {
        for (const auto &waypoint : waypoints) {
            params.coordinates.emplace_back(waypoint);
        }
    } else {
        if (targetDistanceKm > 0) {
            auto viaPoint = RouteService::calculateDetourPoint({startLat, startLon},
                                                               {endLat, endLon}, targetDistanceKm);

            if (viaPoint) {
                LOG_DEBUG << "Detour Via Point: (" << viaPoint->lat << ", " << viaPoint->lon << ")";
                params.coordinates.emplace_back(osrm::util::FloatLongitude{viaPoint->lon},
                                                osrm::util::FloatLatitude{viaPoint->lat});
            }
        }
    }

    params.coordinates.emplace_back(osrm::util::FloatLongitude{endLon},
                                    osrm::util::FloatLatitude{endLat});

    params.geometries = osrm::RouteParameters::GeometriesType::Polyline;
    params.overview = osrm::RouteParameters::OverviewType::Full;
    params.steps = true;
    return params;
}

struct Location {
    double lat;
    double lon;
};

Json::Value createResponseJson(const osrm::json::Object &result, Location start, Location end) {
    Json::Value respJson;
    if (!result.values.contains("routes")) {
        return respJson;
    }
    const auto &routes = result.values.at("routes").get<osrm::json::Array>();

    if (!routes.values.empty()) {
        const auto &route = routes.values[0].get<osrm::json::Object>();
        auto distance = route.values.at("distance").get<osrm::json::Number>().value;
        auto duration = route.values.at("duration").get<osrm::json::Number>().value;
        auto geometry = route.values.at("geometry").get<osrm::json::String>().value;

        respJson["summary"]["total_distance_m"] = distance;
        respJson["summary"]["estimated_moving_time_s"] = duration;
        respJson["geometry"] = geometry;

        // Spot search logic (dummy data generation)
        if (routes.values[0].get<osrm::json::Object>().values.contains("legs")) {
            double midLat = (start.lat + end.lat) / kAverageDivisor;
            double midLon = (start.lon + end.lon) / kAverageDivisor;

            Json::Value stop1;
            stop1["name"] = "Cycling Cafe Base";
            stop1["type"] = "cafe";
            stop1["location"]["lat"] = midLat + kLocationOffset;
            stop1["location"]["lon"] = midLon + kLocationOffset;
            stop1["rating"] = kRatingCafe;

            Json::Value stop2;
            stop2["name"] = "Ramen Energy";
            stop2["type"] = "restaurant";
            stop2["location"]["lat"] = midLat - kLocationOffset;
            stop2["location"]["lon"] = midLon - kLocationOffset;
            stop2["rating"] = kRatingRestaurant;

            respJson["stops"].append(stop1);
            respJson["stops"].append(stop2);
        }
    }
    return respJson;
}

}  // namespace

Route::Route() {
    // OSRM エンジンの初期化
    osrm::EngineConfig config;
    config.storage_config = {"/data/kanto-latest.osrm"};
    config.use_shared_memory = false;
    // アルゴリズムは CH (Contraction Hierarchies) または MLD (Multi-Level Dijkstra)
    // ここでは CH を使用（前処理で osrm-contract を実行したため）
    config.algorithm = osrm::EngineConfig::Algorithm::CH;

    osrm_ = std::make_unique<osrm::OSRM>(config);
}

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

    double startLat = (*jsonPtr)["start_point"]["lat"].asDouble();
    double startLon = (*jsonPtr)["start_point"]["lon"].asDouble();
    double endLat = (*jsonPtr)["end_point"]["lat"].asDouble();
    double endLon = (*jsonPtr)["end_point"]["lon"].asDouble();

    LOG_DEBUG << "Request: Start(" << startLat << ", " << startLon << ") End(" << endLat << ", "
              << endLon << ")";

    double targetDistanceKm = 0.0;
    if (jsonPtr->isMember("preferences") &&
        (*jsonPtr)["preferences"].isMember("target_distance_km")) {
        targetDistanceKm = (*jsonPtr)["preferences"]["target_distance_km"].asDouble();
    }

    std::vector<osrm::util::Coordinate> waypoints = parseWaypoints(jsonPtr);

    osrm::RouteParameters params = buildRouteParameters(startLat, startLon, endLat, endLon,
                                                        targetDistanceKm, waypoints);

    osrm::json::Object result;
    const auto status = osrm_->Route(params, result);

    if (status != osrm::Status::Ok) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k400BadRequest);
        resp->setBody("Route calculation failed");
        callback(resp);
        return;
    }

    Json::Value respJson = createResponseJson(result, {startLat, startLon}, {endLat, endLon});

    auto resp = HttpResponse::newHttpJsonResponse(respJson);
    callback(resp);
}