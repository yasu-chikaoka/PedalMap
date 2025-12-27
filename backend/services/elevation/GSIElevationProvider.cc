#include "GSIElevationProvider.h"

#include <cmath>
#include <iostream>
#include <numbers>
#include <sstream>

namespace services::elevation {

GSIElevationProvider::GSIElevationProvider()
    : httpClient_(drogon::HttpClient::newHttpClient("https://cyberjapandata.gsi.go.jp")),
      tileCache_(drogon::app().getLoop()) {}

void GSIElevationProvider::getElevation(const Coordinate& coord, ElevationCallback&& callback) {
    auto tileCoord = calculateTileCoord(coord);
    std::string cacheKey = std::to_string(tileCoord.z) + "/" + std::to_string(tileCoord.x) + "/" +
                           std::to_string(tileCoord.y);

    std::shared_ptr<TileData> tileData;
    if (tileCache_.findAndFetch(cacheKey, tileData)) {
        double elevation = tileData->elevations[tileCoord.pixel_y * 256 + tileCoord.pixel_x];
        callback(elevation);
        return;
    }

    fetchTile(tileCoord.z, tileCoord.x, tileCoord.y,
              [tileCoord, callback = std::move(callback)](std::shared_ptr<TileData> tileData) {
                  if (!tileData) {
                      callback(std::nullopt);
                      return;
                  }
                  double elevation =
                      tileData->elevations[tileCoord.pixel_y * 256 + tileCoord.pixel_x];
                  callback(elevation);
              });
}

void GSIElevationProvider::getElevations(const std::vector<Coordinate>& coords,
                                         ElevationsCallback&& callback) {
    if (coords.empty()) {
        callback({});
        return;
    }

    auto results = std::make_shared<std::vector<double>>(coords.size(), 0.0);
    auto remaining = std::make_shared<size_t>(coords.size());

    for (size_t i = 0; i < coords.size(); ++i) {
        getElevation(coords[i], [results, i, remaining, callback](std::optional<double> elev) {
            if (elev) {
                (*results)[i] = *elev;
            }
            (*remaining)--;
            if (*remaining == 0) {
                callback(*results);
            }
        });
    }
}

std::optional<double> GSIElevationProvider::getElevationSync(const Coordinate& coord) {
    auto tileCoord = calculateTileCoord(coord);
    std::string cacheKey = std::to_string(tileCoord.z) + "/" + std::to_string(tileCoord.x) + "/" +
                           std::to_string(tileCoord.y);

    std::shared_ptr<TileData> tileData;
    if (tileCache_.findAndFetch(cacheKey, tileData)) {
        return tileData->elevations[tileCoord.pixel_y * 256 + tileCoord.pixel_x];
    }

    // 同期リクエスト
    auto req = drogon::HttpRequest::newHttpRequest();
    // タイルサーバーのURLを確認
    // DEM5A (5mメッシュ) を優先的に試用
    req->setPath("/xyz/dem5a/" + cacheKey + ".txt");
    LOG_DEBUG << "Sync fetching tile (dem5a): " << cacheKey;
    auto respResult = httpClient_->sendRequest(req, 5.0);

    if (respResult.first != drogon::ReqResult::Ok || respResult.second->statusCode() != 200) {
        // DEM5Aがなければ通常のDEMを試行
        req->setPath("/xyz/dem/" + cacheKey + ".txt");
        LOG_DEBUG << "Sync fetching tile (dem): " << cacheKey;
        respResult = httpClient_->sendRequest(req, 5.0);
    }

    if (respResult.first == drogon::ReqResult::Ok && respResult.second->statusCode() == 200) {
        LOG_DEBUG << "Sync fetch success, parsing tile: " << cacheKey;
        auto tileData = parseTileText(std::string(respResult.second->body()));
        if (tileData) {
            tileCache_.insert(cacheKey, tileData, 3600);  // 1時間キャッシュ
            return tileData->elevations[tileCoord.pixel_y * 256 + tileCoord.pixel_x];
        } else {
            LOG_DEBUG << "Sync parse failed for tile: " << cacheKey;
        }
    } else {
        LOG_DEBUG << "Sync fetch failed for tile: " << cacheKey
                  << " Result: " << (int)respResult.first
                  << " Status: " << (respResult.second ? respResult.second->statusCode() : 0);
        // エラー内容を確認するためにbodyの先頭を出力
        if (respResult.second) {
            LOG_DEBUG << "Response body head: " << respResult.second->body().substr(0, 100);
        }
    }

    return std::nullopt;
}

GSIElevationProvider::TileCoord GSIElevationProvider::calculateTileCoord(const Coordinate& coord,
                                                                         int zoom) {
    double lat_rad = coord.lat * std::numbers::pi / 180.0;
    double n = std::pow(2, zoom);
    double x = (coord.lon + 180.0) / 360.0 * n;
    double y = (1.0 - std::asinh(std::tan(lat_rad)) / std::numbers::pi) / 2.0 * n;

    TileCoord tc;
    tc.z = zoom;
    tc.x = static_cast<int>(x);
    tc.y = static_cast<int>(y);
    tc.pixel_x = static_cast<int>((x - tc.x) * 256);
    tc.pixel_y = static_cast<int>((y - tc.y) * 256);

    // 15/29105/12903.txt などは存在する
    // Zoom 15 で OK

    // 範囲外チェック
    tc.pixel_x = std::clamp(tc.pixel_x, 0, 255);
    tc.pixel_y = std::clamp(tc.pixel_y, 0, 255);

    LOG_DEBUG << "Coord: (" << coord.lat << ", " << coord.lon << ") -> Tile: " << tc.z << "/"
              << tc.x << "/" << tc.y << " Pixel: " << tc.pixel_x << "," << tc.pixel_y;

    return tc;
}

void GSIElevationProvider::fetchTile(int z, int x, int y,
                                     std::function<void(std::shared_ptr<TileData>)>&& callback) {
    std::string cacheKey = std::to_string(z) + "/" + std::to_string(x) + "/" + std::to_string(y);
    std::string path = "/xyz/dem/" + cacheKey + ".txt";

    auto req = drogon::HttpRequest::newHttpRequest();
    req->setPath(path);

    httpClient_->sendRequest(
        req, [this, cacheKey, callback = std::move(callback)](drogon::ReqResult result,
                                                              const drogon::HttpResponsePtr& resp) {
            if (result == drogon::ReqResult::Ok && resp->statusCode() == 200) {
                auto tileData = parseTileText(std::string(resp->body()));
                if (tileData) {
                    tileCache_.insert(cacheKey, tileData, 3600);
                    callback(tileData);
                    return;
                }
            }
            callback(nullptr);
        });
}

std::shared_ptr<GSIElevationProvider::TileData> GSIElevationProvider::parseTileText(
    const std::string& text) {
    auto data = std::make_shared<TileData>();
    data->elevations.reserve(256 * 256);

    std::stringstream ss(text);
    std::string line;
    while (std::getline(ss, line)) {
        if (line.empty()) continue;
        std::stringstream ls(line);
        std::string val;
        while (std::getline(ls, val, ',')) {
            try {
                if (val == "e") {
                    data->elevations.push_back(0.0);
                } else {
                    data->elevations.push_back(std::stod(val));
                }
            } catch (...) {
                data->elevations.push_back(0.0);
            }
        }
    }

    if (data->elevations.size() != 256 * 256) {
        return nullptr;
    }
    return data;
}

}  // namespace services::elevation
