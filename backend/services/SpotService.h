#pragma once

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point.hpp>
#include <boost/geometry/index/rtree.hpp>
#include <string>
#include <vector>

#include "ConfigService.h"
#include "RouteService.h"  // Coordinate構造体用

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
    virtual ~SpotService() = default;

    // ルートの中間地点周辺のスポットをGoogle Places APIで検索（簡易実装のため同期ブロック）
    virtual std::vector<Spot> searchSpotsAlongRoute(const std::string& polylineGeometry,
                                                    double bufferMeters);

   private:
    const ConfigService& configService_;
};

}  // namespace services