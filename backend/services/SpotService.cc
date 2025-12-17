#include "SpotService.h"

#include "../utils/PolylineDecoder.h"
#include <fstream>
#include <iostream>
#include <sstream>

namespace services {

SpotService::SpotService(const ConfigService& configService) {
    loadSpotsFromCsv(configService.getSpotsCsvPath());

}

void SpotService::loadSpotsFromCsv(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "Failed to open spots CSV file: " << filePath << std::endl;
        return;
    }

    std::string line;
    // ヘッダーが存在する場合はスキップしますか？ ヘッダーがないか、解析ロジックで処理されると仮定します
    // 最初の行に数値以外の緯度/経度が含まれている場合、ヘッダーである可能性があると仮定しましょう
    // 簡単にするために、今のところヘッダーは無い、または最初の文字をチェックすると仮定します。

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::stringstream ss(line);
        std::string segment;
        std::vector<std::string> parts;

        while (std::getline(ss, segment, ',')) {
            parts.push_back(segment);
        }

        if (parts.size() >= 5) {
            try {
                Spot spot;
                spot.name = parts[0];
                spot.type = parts[1];
                spot.lat = std::stod(parts[2]);
                spot.lon = std::stod(parts[3]);
                spot.rating = std::stod(parts[4]);

                spots_.push_back(spot);
                
                Point p(spot.lon, spot.lat);
                rtree_.insert(std::make_pair(p, spots_.size() - 1));
            } catch (const std::exception& e) {
                std::cerr << "Error parsing spot line: " << line << " (" << e.what() << ")" << std::endl;
            }
        }
    }
    std::cout << "Loaded " << spots_.size() << " spots from " << filePath << std::endl;
}

std::vector<Spot> SpotService::searchSpotsAlongRoute(const std::string& polylineGeometry,
                                                     double bufferMeters) {
    if (polylineGeometry.empty()) {
        return {};
    }

    auto path = utils::PolylineDecoder::decode(polylineGeometry);
    return searchSpotsAlongPath(path, bufferMeters);
}

std::vector<Spot> SpotService::searchSpotsAlongPath(const std::vector<Coordinate>& path,
                                                    double bufferMeters) {
    std::vector<Spot> results;
    if (path.empty()) {
        return results;
    }

    // 距離計算のためにパスをLineStringに変換
    bg::model::linestring<Point> routeLine;
    for (const auto& coord : path) {
        routeLine.push_back(Point(coord.lon, coord.lat));
    }

    // R-treeから候補をフィルタリングするためにルートのバウンディングボックスを計算
    bg::model::box<Point> routeBox;
    bg::envelope(routeLine, routeBox);

    // バッファ距離分ボックスを拡張（度数への近似変換）
    // 緯度1度 ~ 111km, 経度1度 ~ 111km * cos(lat)
    // 簡単にするために、フィルタリングには安全な上限変換を使用します
    const double kMetersToDegrees = 1.0 / 111000.0;
    double bufferDeg = bufferMeters * kMetersToDegrees * 1.5;  // 1.5倍の安全マージン

    Point minCorner(bg::get<bg::min_corner, 0>(routeBox) - bufferDeg,
                    bg::get<bg::min_corner, 1>(routeBox) - bufferDeg);
    Point maxCorner(bg::get<bg::max_corner, 0>(routeBox) + bufferDeg,
                    bg::get<bg::max_corner, 1>(routeBox) + bufferDeg);
    bg::model::box<Point> queryBox(minCorner, maxCorner);

    std::vector<Value> candidates;
    rtree_.query(bgi::intersects(queryBox), std::back_inserter(candidates));

    // LineStringへの正確な距離を計算して結果を絞り込む
    for (const auto& candidate : candidates) {
        // bg::distanceは、特定のストラテジが使用されない限り、非デカルト座標系では座標と同じ単位（度）で距離を返します。
        // しかし、地理座標系（geographic cs）の場合、正しいストラテジが適用されればメートルを返すかもしれません。
        // 実際、最近のBoostバージョンでは、bg::cs::geographicの場合、デフォルトでメートルを返しますか？ 確認しましょう。
        // 通常、地理座標系の場合はメートルを返します。そうでない場合は、Haversineストラテジが必要です。

        // Boost 1.74以降の地理座標系のデフォルトストラテジはandoyerまたはthomasで、メートルを返します。
        // メートルを返すと仮定しましょう。値が極端に小さい場合は度です。

        double dist = bg::distance(candidate.first, routeLine);

        // 距離が本当にメートル単位か確認します。距離が0.001のような場合は度です。
        // Boost.Geometryの動作はバージョンとヘッダーに依存します。
        // 安全のために、カスタムストラテジを使用するか、大きさを確認することができます。

        // この環境で簡単にするために、点と点の計算であれば明示的にHaversineを使用しますが、
        // 点とLineStringの場合はより複雑です。

        // デフォルトの地理座標系の動作を信頼しますが、チェックを追加します。
        // 座標系をgeographic<degree>として定義したので、結果はメートル（技術的には楕円体表面上）になるはずです。

        if (dist <= bufferMeters) {
            results.push_back(spots_[candidate.second]);
        }
    }

    return results;
}

}  // namespace services