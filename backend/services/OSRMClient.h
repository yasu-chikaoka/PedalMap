#pragma once

#include <memory>
#include <osrm/json_container.hpp>
#include <osrm/osrm.hpp>
#include <osrm/route_parameters.hpp>
#include <osrm/status.hpp>

#include "ConfigService.h"

namespace services {

class OSRMClient {
   public:
    explicit OSRMClient(const ConfigService& configService);
    virtual ~OSRMClient() = default;

    // モック作成のためにRouteメソッドを仮想関数にする
    virtual osrm::Status Route(const osrm::RouteParameters& parameters,
                               osrm::json::Object& result) const;

    // ポイントをスナップするためのヘルパー（RouteControllerで使用）
    virtual std::vector<osrm::json::Object> Nearest(
        const osrm::NearestParameters& parameters) const;

    const osrm::OSRM& getOSRM() const { return *osrm_; }

   private:
    std::unique_ptr<osrm::OSRM> osrm_;
};

}  // namespace services