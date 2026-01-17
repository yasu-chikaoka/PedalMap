#pragma once

#include <string>
#include <vector>

#include "ConfigService.h"
#include "Coordinate.h"

namespace services {

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
