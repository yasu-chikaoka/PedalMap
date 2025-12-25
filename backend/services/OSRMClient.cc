#include "OSRMClient.h"

#include <osrm/engine_config.hpp>
#include <osrm/nearest_parameters.hpp>

namespace services {

OSRMClient::OSRMClient(const ConfigService& configService) {
    osrm::EngineConfig config;
    config.storage_config = {configService.getOsrmPath()};
    config.use_shared_memory = false;
    // Default algorithm is CH (Contraction Hierarchies), which requires .hsgr file
    // We will generate it using osrm-contract
    config.algorithm = osrm::EngineConfig::Algorithm::CH;

    osrm_ = std::make_unique<osrm::OSRM>(config);
}

osrm::Status OSRMClient::Route(const osrm::RouteParameters& parameters,
                               osrm::json::Object& result) const {
    return osrm_->Route(parameters, result);
}

std::vector<osrm::json::Object> OSRMClient::Nearest(
    const osrm::NearestParameters& parameters) const {
    osrm::json::Object result;
    auto status = osrm_->Nearest(parameters, result);

    std::vector<osrm::json::Object> waypoints;
    if (status == osrm::Status::Ok) {
        if (result.values.find("waypoints") != result.values.end()) {
            const auto& wps = result.values.at("waypoints").get<osrm::json::Array>();
            for (const auto& wp : wps.values) {
                waypoints.push_back(wp.get<osrm::json::Object>());
            }
        }
    }
    return waypoints;
}

}  // namespace services