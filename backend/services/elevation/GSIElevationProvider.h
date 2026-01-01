#pragma once

#include <drogon/CacheMap.h>
#include <drogon/HttpAppFramework.h>
#include <drogon/HttpClient.h>
#include <drogon/utils/Utilities.h>

#include <memory>
#include <string>

#include "../RouteService.h"
#include "IElevationProvider.h"

namespace services::elevation {

class GSIElevationProvider : public IElevationProvider {
   public:
    GSIElevationProvider();
    ~GSIElevationProvider() override = default;

    void getElevation(const Coordinate& coord, ElevationCallback&& callback) override;
    void getElevations(const std::vector<Coordinate>& coords,
                       ElevationsCallback&& callback) override;
    std::optional<double> getElevationSync(const Coordinate& coord) override;

    struct TileData {
        std::vector<double> elevations;  // 256x256
    };

    // 公開: タイルデータの取得とパース
    void fetchTile(int z, int x, int y, std::function<void(std::shared_ptr<TileData>)>&& callback);

    // 公開: タイル座標の計算
    struct TileCoord {
        int z;
        int x;
        int y;
        int pixel_x;
        int pixel_y;
    };
    static TileCoord calculateTileCoord(const Coordinate& coord, int zoom = 15);

   protected:
    std::shared_ptr<TileData> parseTileText(const std::string& text);

   private:
    drogon::HttpClientPtr httpClient_;
    // タイルデータのキャッシュ (キー: "z/x/y", 有効期限: 1時間)
    drogon::CacheMap<std::string, std::shared_ptr<TileData>> tileCache_;
};

}  // namespace services::elevation
