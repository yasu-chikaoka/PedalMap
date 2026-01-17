#pragma once

#include <string>
#include <vector>

#include "../services/Coordinate.h"

namespace utils {

class PolylineDecoder {
   public:
    // エンコードされたポリライン文字列を座標のベクターにデコードします
    // 精度はデフォルトで1e5（小数点以下5桁）で、OSRMおよびGoogleマップと一致しています
    static std::vector<services::Coordinate> decode(const std::string& encodedPolyline,
                                                    double precision = 1e5);

    // 座標のベクターをポリライン文字列にエンコードします
    static std::string encode(const std::vector<services::Coordinate>& points,
                              double precision = 1e5);
};

}  // namespace utils
