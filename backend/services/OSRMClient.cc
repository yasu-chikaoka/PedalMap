#include "OSRMClient.h"

#include <osrm/engine_config.hpp>
#include <osrm/nearest_parameters.hpp>

namespace services {

OSRMClient::OSRMClient(const ConfigService& configService) {
    try {
        osrm::EngineConfig config;
        config.storage_config = {configService.getOsrmPath()};
        config.use_shared_memory = false;
        config.algorithm = osrm::EngineConfig::Algorithm::CH;

        // ファイルが存在しない場合に例外を投げずにエラーを出力するようにする
        // ただしosrm::OSRMのコンストラクタはファイルがないと例外を投げる仕様
        // そのため、try-catchで囲み、初期化に失敗してもプロセスが落ちないようにする
        // テスト時などはこれでモックとして使えるようになる
        osrm_ = std::make_unique<osrm::OSRM>(config);
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Failed to initialize OSRM: " << e.what() << std::endl;
        // osrm_ は nullptr のまま
    }
}

osrm::Status OSRMClient::Route(const osrm::RouteParameters& parameters,
                               osrm::json::Object& result) const {
    if (!osrm_) {
        // osrm_が初期化されていない場合はエラーを返す
        return osrm::Status::Error;
    }
    return osrm_->Route(parameters, result);
}

std::vector<osrm::json::Object> OSRMClient::Nearest(
    const osrm::NearestParameters& parameters) const {
    if (!osrm_) {
        return {};
    }
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