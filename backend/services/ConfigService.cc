#include "ConfigService.h"

#include <limits.h>
#include <unistd.h>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <vector>

namespace services {

ConfigService::ConfigService() {
    // 1. Determine executable directory
    char result[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
    if (count != -1) {
        std::string exePath(result, count);
        size_t lastSlash = exePath.find_last_of("/");
        if (lastSlash != std::string::npos) {
            exeDir_ = exePath.substr(0, lastSlash);
        }
    }

    // 2. Load configurations from Environment Variables or Defaults
    
    // Paths
    std::string osrmTarget = getEnvString("OSRM_DATA_PATH", "kanto-latest.osrm");
    osrmPath_ = findPath(osrmTarget, "/data/" + osrmTarget);

    std::string csvTarget = getEnvString("SPOTS_CSV_PATH", "spots.csv");
    spotsCsvPath_ = findPath(csvTarget, "/data/" + csvTarget);

    // API
    googleApiKey_ = getEnvString("GOOGLE_PLACES_API_KEY", "");
    googleMapsApiBaseUrl_ = getEnvString("GOOGLE_MAPS_API_BASE_URL", "https://maps.googleapis.com");
    googleMapsNearbySearchPath_ = getEnvString("GOOGLE_MAPS_NEARBY_SEARCH_PATH", "/maps/api/place/nearbysearch/json");
    apiTimeoutSeconds_ = getEnvInt("API_TIMEOUT_SECONDS", 5);
    apiRetryCount_ = getEnvInt("API_RETRY_COUNT", 3);

    // Server
    serverPort_ = getEnvInt("SERVER_PORT", 8080);
    allowOrigin_ = getEnvString("ALLOW_ORIGIN", "*");

    // Logic
    spotSearchRadius_ = getEnvDouble("SPOT_SEARCH_RADIUS", 500.0);

    // Redis & Cache
    redisHost_ = getEnvString("REDIS_HOST", "127.0.0.1");
    redisPort_ = getEnvInt("REDIS_PORT", 6379);
    redisPassword_ = getEnvString("REDIS_PASSWORD", "");
    elevationCacheTtlDays_ = getEnvInt("ELEVATION_CACHE_TTL_DAYS", 365);
    elevationRefreshThresholdScore_ = getEnvInt("ELEVATION_REFRESH_THRESHOLD_SCORE", 10);
    elevationLruCacheCapacity_ = getEnvInt("ELEVATION_LRU_CACHE_CAPACITY", 1000);
}

std::string ConfigService::getEnvString(const char* key, const std::string& defaultValue) {
    const char* val = std::getenv(key);
    return val ? std::string(val) : defaultValue;
}

int ConfigService::getEnvInt(const char* key, int defaultValue) {
    const char* val = std::getenv(key);
    return val ? std::stoi(val) : defaultValue;
}

double ConfigService::getEnvDouble(const char* key, double defaultValue) {
    const char* val = std::getenv(key);
    return val ? std::stod(val) : defaultValue;
}

std::string ConfigService::findPath(const std::string& target, const std::string& fallback) {
    auto exists = [](const std::string& p) {
        std::ifstream f(p);
        return f.good();
    };

    // If absolute path and exists
    if (target.front() == '/' && exists(target)) return target;
    
    // If relative path, check relative to CWD
    if (exists(target)) return target;

    std::string filename = target;
    size_t lastSlash = target.find_last_of("/");
    if (lastSlash != std::string::npos) {
        filename = target.substr(lastSlash + 1);
    }

    // Search base directories relative to executable
    std::vector<std::string> baseDirs = {
        exeDir_, 
        exeDir_ + "/..", 
        exeDir_ + "/../..",
        exeDir_ + "/../../..",
        exeDir_ + "/backend", 
        exeDir_ + "/../backend"
    };

    // Search subdirectories
    std::vector<std::string> subDirs = {"", "data/", "tests/data/", "backend/tests/data/"};

    for (const auto& base : baseDirs) {
        if (base.empty()) continue;
        for (const auto& sub : subDirs) {
            std::string p = base + "/" + sub + filename;
            if (exists(p)) return p;
        }
    }

    return fallback;
}

std::string ConfigService::getOsrmPath() const { return osrmPath_; }
std::string ConfigService::getSpotsCsvPath() const { return spotsCsvPath_; }
std::string ConfigService::getGoogleApiKey() const { return googleApiKey_; }
std::string ConfigService::getGoogleMapsApiBaseUrl() const { return googleMapsApiBaseUrl_; }
std::string ConfigService::getGoogleMapsNearbySearchPath() const { return googleMapsNearbySearchPath_; }
int ConfigService::getApiTimeoutSeconds() const { return apiTimeoutSeconds_; }
int ConfigService::getApiRetryCount() const { return apiRetryCount_; }
int ConfigService::getServerPort() const { return serverPort_; }
std::string ConfigService::getAllowOrigin() const { return allowOrigin_; }
double ConfigService::getSpotSearchRadius() const { return spotSearchRadius_; }
std::string ConfigService::getRedisHost() const { return redisHost_; }
int ConfigService::getRedisPort() const { return redisPort_; }
std::string ConfigService::getRedisPassword() const { return redisPassword_; }
int ConfigService::getElevationCacheTtlDays() const { return elevationCacheTtlDays_; }
int ConfigService::getElevationRefreshThresholdScore() const { return elevationRefreshThresholdScore_; }
int ConfigService::getElevationLruCacheCapacity() const { return elevationLruCacheCapacity_; }

}  // namespace services
