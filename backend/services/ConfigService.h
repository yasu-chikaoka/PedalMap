#pragma once

#include <string>

namespace services {

class ConfigService {
   public:
    ConfigService();
    virtual ~ConfigService() = default;

    [[nodiscard]] std::string getOsrmPath() const;
    [[nodiscard]] std::string getSpotsCsvPath() const;
    [[nodiscard]] double getSpotSearchRadius() const;
    [[nodiscard]] std::string getGoogleApiKey() const;
    [[nodiscard]] int getApiTimeoutSeconds() const;
    [[nodiscard]] int getApiRetryCount() const;

    // Additional configuration items
    [[nodiscard]] int getServerPort() const;
    [[nodiscard]] std::string getGoogleMapsApiBaseUrl() const;
    [[nodiscard]] std::string getGoogleMapsNearbySearchPath() const;
    [[nodiscard]] std::string getAllowOrigin() const;

    // Redis and Cache configuration
    [[nodiscard]] std::string getRedisHost() const;
    [[nodiscard]] int getRedisPort() const;
    [[nodiscard]] std::string getRedisPassword() const;
    [[nodiscard]] int getElevationCacheTtlDays() const;
    [[nodiscard]] int getElevationRefreshThresholdScore() const;
    [[nodiscard]] int getElevationLruCacheCapacity() const;

   private:
    std::string findPath(const std::string& filename, const std::string& defaultPath);
    std::string exeDir_;
    std::string osrmPath_;
    std::string spotsCsvPath_;
    double spotSearchRadius_;
    std::string googleApiKey_;
    int apiTimeoutSeconds_;
    int apiRetryCount_;

    // Internal cache
    int serverPort_;
    std::string googleMapsApiBaseUrl_;
    std::string googleMapsNearbySearchPath_;
    std::string allowOrigin_;

    // Redis and Cache
    std::string redisHost_;
    int redisPort_;
    std::string redisPassword_;
    int elevationCacheTtlDays_;
    int elevationRefreshThresholdScore_;
    int elevationLruCacheCapacity_;
};

}  // namespace services