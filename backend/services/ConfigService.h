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

   private:
    std::string osrmPath_;
    std::string spotsCsvPath_;
    double spotSearchRadius_;
};

}  // namespace services