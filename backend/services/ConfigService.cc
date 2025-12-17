#include "ConfigService.h"

#include <cstdlib>

namespace services {

ConfigService::ConfigService() {
    // 環境変数からロードするか、デフォルト値を使用する
    const char* osrmPathEnv = std::getenv("OSRM_PATH");
    osrmPath_ = osrmPathEnv ? osrmPathEnv : "/data/kanto-latest.osrm";

    const char* radiusEnv = std::getenv("SPOT_SEARCH_RADIUS");
    spotSearchRadius_ = radiusEnv ? std::stod(radiusEnv) : 500.0;

    const char* csvPathEnv = std::getenv("SPOTS_CSV_PATH");
    spotsCsvPath_ = csvPathEnv ? csvPathEnv : "/data/spots.csv";
}

std::string ConfigService::getOsrmPath() const { return osrmPath_; }

std::string ConfigService::getSpotsCsvPath() const { return spotsCsvPath_; }

double ConfigService::getSpotSearchRadius() const { return spotSearchRadius_; }

}  // namespace services