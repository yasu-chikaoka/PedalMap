#pragma once

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point.hpp>
#include <boost/geometry/index/rtree.hpp>
#include <string>
#include <vector>

#include "RouteService.h"  // Coordinate構造体用
#include "ConfigService.h"

namespace services {

namespace bg = boost::geometry;
namespace bgi = boost::geometry::index;

// 地理座標系（緯度、経度は度数法）を使用してポイントを定義
using Point = bg::model::point<double, 2, bg::cs::geographic<bg::degree>>;
using Value = std::pair<Point, size_t>;  // ポイントとspots_ベクトルのインデックス

struct Spot {
    std::string name;
    std::string type;
    double lat;
    double lon;
    double rating;
};

class SpotService {
   public:
    explicit SpotService(const ConfigService& configService);

    // ルート（ポリライン）沿いのスポットを検索
    // polylineGeometry: エンコードされたポリライン文字列（未実装、現在は座標のパスを使用）
    // 現時点では、ポイント周辺のバッファリングや単純なパスロジックで「ルート沿い」をシミュレートしています。
    // 「ルート沿い」を適切にサポートするには、ポリラインをLineStringに変換し、buffer/distanceを使用する必要があります。
    std::vector<Spot> searchSpotsAlongPath(const std::vector<Coordinate>& path,
                                           double bufferMeters);
    
    // ルートジオメトリ（ポリライン）沿いのスポットを検索
    std::vector<Spot> searchSpotsAlongRoute(const std::string& polylineGeometry, double bufferMeters);

   private:
    std::vector<Spot> spots_;
    bgi::rtree<Value, bgi::quadratic<16>> rtree_;

    void loadSpotsFromCsv(const std::string& filePath);
};

}  // namespace services