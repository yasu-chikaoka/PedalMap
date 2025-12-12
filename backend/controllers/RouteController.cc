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

    // リクエストパラメータの解析 (簡易版: start, end のみ)
    // 本来は preferences (標高, 距離) を考慮した独自ロジックが必要だが、
    // まずは単純な A -> B ルート検索を実装して疎通確認を行う。

    // JSON解析のエラーハンドリングが必要だが、まずは動くコードを書く
    if (!(*jsonPtr).isMember("start_point") || !(*jsonPtr).isMember("end_point")) {
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

    // 追加パラメータの取得
    double targetDistanceKm = 0.0;
    if ((*jsonPtr).isMember("preferences") &&
        (*jsonPtr)["preferences"].isMember("target_distance_km")) {
        targetDistanceKm = (*jsonPtr)["preferences"]["target_distance_km"].asDouble();
    }

    // 経由地の取得
    std::vector<osrm::util::Coordinate> waypoints;
    if ((*jsonPtr).isMember("waypoints")) {
        auto &wpArray = (*jsonPtr)["waypoints"];
        if (wpArray.isArray()) {
            for (const auto &wp : wpArray) {
                if (wp.isMember("lat") && wp.isMember("lon")) {
                    double lat = wp["lat"].asDouble();
                    double lon = wp["lon"].asDouble();
                    waypoints.push_back(
                        {osrm::util::FloatLongitude{lon}, osrm::util::FloatLatitude{lat}});
                }
            }
        }
    }

    // OSRM ルートパラメータの設定
    osrm::RouteParameters params;

    // 座標は (Longitude, Latitude) の順序 (GeoJSON準拠)
    params.coordinates.push_back(
        {osrm::util::FloatLongitude{startLon}, osrm::util::FloatLatitude{startLat}});

    if (!waypoints.empty()) {
        // 経由地が指定されている場合は、それらを追加（距離調整ロジックはスキップ）
        for (const auto &wp : waypoints) {
            params.coordinates.push_back(wp);
        }
    } else {
        // 距離調整ロジック: RouteService に委譲
        if (targetDistanceKm > 0) {
            auto viaPoint = RouteService::calculateDetourPoint({startLat, startLon},
                                                               {endLat, endLon}, targetDistanceKm);

            if (viaPoint) {
                LOG_DEBUG << "Detour Via Point: (" << viaPoint->lat << ", " << viaPoint->lon << ")";
                params.coordinates.push_back({osrm::util::FloatLongitude{viaPoint->lon},
                                              osrm::util::FloatLatitude{viaPoint->lat}});
            }
        }
    }

    params.coordinates.push_back(
        {osrm::util::FloatLongitude{endLon}, osrm::util::FloatLatitude{endLat}});

    // ジオメトリ（形状）を取得
    params.geometries = osrm::RouteParameters::GeometriesType::Polyline;
    params.overview = osrm::RouteParameters::OverviewType::Full;
    params.steps = true;

    // ルート計算実行
    osrm::json::Object result;
    const auto status = osrm_->Route(params, result);

    Json::Value respJson;

    if (status == osrm::Status::Ok) {
        auto &routes = result.values["routes"].get<osrm::json::Array>();
        if (!routes.values.empty()) {
            auto &route = routes.values[0].get<osrm::json::Object>();
            auto distance = route.values["distance"].get<osrm::json::Number>().value;
            auto duration = route.values["duration"].get<osrm::json::Number>().value;
            auto geometry = route.values["geometry"].get<osrm::json::String>().value;

            respJson["summary"]["total_distance_m"] = distance;
            respJson["summary"]["estimated_moving_time_s"] = duration;
            respJson["geometry"] = geometry;

            // TODO: ここで獲得標高などの詳細情報を付加する

            // 周辺店舗検索ロジック (ダミーデータ生成)
            // 中間地点付近にスポットを追加
            if (routes.values[0].get<osrm::json::Object>().values.count("legs")) {
                // 実際はステップから座標を取得して検索するが、ここでは簡易的にstart/endの中点を基準にする
                double midLat = (startLat + endLat) / 2.0;
                double midLon = (startLon + endLon) / 2.0;

                Json::Value stop1;
                stop1["name"] = "Cycling Cafe Base";
                stop1["type"] = "cafe";
                stop1["location"]["lat"] = midLat + 0.001;
                stop1["location"]["lon"] = midLon + 0.001;
                stop1["rating"] = 4.5;

                Json::Value stop2;
                stop2["name"] = "Ramen Energy";
                stop2["type"] = "restaurant";
                stop2["location"]["lat"] = midLat - 0.001;
                stop2["location"]["lon"] = midLon - 0.001;
                stop2["rating"] = 4.2;

                respJson["stops"].append(stop1);
                respJson["stops"].append(stop2);
            }
        }
    } else {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k400BadRequest);
        resp->setBody("Route calculation failed");
        callback(resp);
        return;
    }

    auto resp = HttpResponse::newHttpJsonResponse(respJson);
    callback(resp);
}