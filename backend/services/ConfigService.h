#pragma once

#include <string>
#include <optional>

namespace services {

class ConfigService {
   public:
    ConfigService();
    virtual ~ConfigService() = default;

    // Path configurations
    [[nodiscard]] std::string getOsrmPath() const;
    [[nodiscard]] std::string getSpotsCsvPath() const;
    
    // API configurations
    [[nodiscard]] std::string getGoogleApiKey() const;
    [[nodiscard]] std::string getGoogleMapsApiBaseUrl() const;
    [[nodiscard]] std::string getGoogleMapsNearbySearchPath() const;
    [[nodiscard]] int getApiTimeoutSeconds() const;
    [[nodiscard]] int getApiRetryCount() const;

    // Server configurations
    [[nodiscard]] int getServerPort() const;
    [[nodiscard]] std::string getAllowOrigin() const;

    // Logic configurations
    [[nodiscard]] double getSpotSearchRadius() const;

    // Redis and Cache configurations
    [[nodiscard]] std::string getRedisHost() const;
    [[nodiscard]] int getRedisPort() const;
    [[nodiscard]] std::string getRedisPassword() const;
    [[nodiscard]] int getElevationCacheTtlDays() const;
    [[nodiscard]] int getElevationRefreshThresholdScore() const;
    [[nodiscard]] int getElevationLruCacheCapacity() const;

   private:
    std::string findPath(const std::string& filename, const std::string& defaultPath);
    
    // Helper to get env vars with defaults
    std::string getEnvString(const char* key, const std::string& defaultValue);
    int getEnvInt(const char* key, int defaultValue);
    double getEnvDouble(const char* key, double defaultValue);

    std::string exeDir_;
    
    // Cached configuration values
    std::string osrmPath_;
    std::string spotsCsvPath_;
    std::string googleApiKey_;
    std::string googleMapsApiBaseUrl_;
    std::string googleMapsNearbySearchPath_;
    int apiTimeoutSeconds_;
    int apiRetryCount_;
    int serverPort_;
    std::string allowOrigin_;
    double spotSearchRadius_;
    std::string redisHost_;
    int redisPort_;
    std::string redisPassword_;
    int elevationCacheTtlDays_;
    int elevationRefreshThresholdScore_;
    int elevationLruCacheCapacity_;
};

}  // namespace services
