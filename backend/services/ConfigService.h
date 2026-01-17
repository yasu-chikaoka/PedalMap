#pragma once

#include <optional>
#include <string>

namespace services {

class ConfigService {
   public:
    ConfigService();
    virtual ~ConfigService() = default;

    // Path configurations
    [[nodiscard]] virtual std::string getOsrmPath() const;
    [[nodiscard]] virtual std::string getSpotsCsvPath() const;
    
    // API configurations
    [[nodiscard]] virtual std::string getGoogleApiKey() const;
    [[nodiscard]] virtual std::string getGoogleMapsApiBaseUrl() const;
    [[nodiscard]] virtual std::string getGoogleMapsNearbySearchPath() const;
    [[nodiscard]] virtual int getApiTimeoutSeconds() const;
    [[nodiscard]] virtual int getApiRetryCount() const;

    // Server configurations
    [[nodiscard]] virtual int getServerPort() const;
    [[nodiscard]] virtual std::string getAllowOrigin() const;

    // Logic configurations
    [[nodiscard]] virtual double getSpotSearchRadius() const;

    // Redis and Cache configurations
    [[nodiscard]] virtual std::string getRedisHost() const;
    [[nodiscard]] virtual int getRedisPort() const;
    [[nodiscard]] virtual std::string getRedisPassword() const;
    [[nodiscard]] virtual int getElevationCacheTtlDays() const;
    [[nodiscard]] virtual int getElevationRefreshThresholdScore() const;
    [[nodiscard]] virtual int getElevationLruCacheCapacity() const;

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
