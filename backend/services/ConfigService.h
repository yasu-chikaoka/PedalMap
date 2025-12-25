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

   private:
    std::string findPath(const std::string& filename, const std::string& defaultPath);
    std::string exeDir_;
    std::string osrmPath_;
    std::string spotsCsvPath_;
    double spotSearchRadius_;
    std::string googleApiKey_;
};

}  // namespace services