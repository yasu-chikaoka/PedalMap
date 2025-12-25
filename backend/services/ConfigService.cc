#include "ConfigService.h"

#include <limits.h>
#include <unistd.h>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <vector>

namespace services {

ConfigService::ConfigService() {
    // 実行ファイルのディレクトリを取得
    char result[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
    if (count != -1) {
        std::string exePath(result, count);
        size_t lastSlash = exePath.find_last_of("/");
        if (lastSlash != std::string::npos) {
            exeDir_ = exePath.substr(0, lastSlash);
        }
    }

    // 環境変数からロードするか、デフォルト値を使用する
    const char* osrmPathEnv = std::getenv("OSRM_DATA_PATH");
    std::string osrmTarget = osrmPathEnv ? osrmPathEnv : "kanto-latest.osrm";
    osrmPath_ = findPath(osrmTarget, osrmTarget);
    if (osrmPath_.empty() || osrmPath_ == osrmTarget) {
        // 見つからなかった場合、またはデフォルトのままの場合で、元のデフォルト値を保持したい場合
        if (!osrmPathEnv) osrmPath_ = "/data/kanto-latest.osrm";
    }

    const char* radiusEnv = std::getenv("SPOT_SEARCH_RADIUS");
    spotSearchRadius_ = radiusEnv ? std::stod(radiusEnv) : 500.0;

    const char* csvPathEnv = std::getenv("SPOTS_CSV_PATH");
    std::string csvTarget = csvPathEnv ? csvPathEnv : "spots.csv";
    spotsCsvPath_ = findPath(csvTarget, csvTarget);

    // フォールバック: 見つからず、環境変数も指定されていない場合は従来のデフォルトを使用
    if (!csvPathEnv && (spotsCsvPath_ == csvTarget) && std::ifstream(spotsCsvPath_).fail()) {
        spotsCsvPath_ = "/data/spots.csv";
    }

    const char* apiKeyEnv = std::getenv("GOOGLE_PLACES_API_KEY");
    googleApiKey_ = apiKeyEnv ? apiKeyEnv : "";
}

std::string ConfigService::findPath(const std::string& target, const std::string& fallback) {
    auto exists = [](const std::string& p) {
        std::ifstream f(p);
        return f.good();
    };

    if (exists(target)) return target;

    std::string filename = target;
    size_t lastSlash = target.find_last_of("/");
    if (lastSlash != std::string::npos) {
        filename = target.substr(lastSlash + 1);
    }

    // 探索するベースディレクトリのリスト
    std::vector<std::string> baseDirs = {exeDir_, exeDir_ + "/..", exeDir_ + "/../..",
                                         exeDir_ + "/../../..",
                                         // ビルドディレクトリからの相対位置を想定
                                         exeDir_ + "/backend", exeDir_ + "/../backend"};

    // 特定のサブディレクトリも探索対象に含める
    std::vector<std::string> subDirs = {"", "data/", "tests/data/", "backend/tests/data/"};

    for (const auto& base : baseDirs) {
        if (base.empty()) continue;

        // 1. ベースディレクトリ + 元のパス (targetが相対パスの場合)
        std::string p1 = base + "/" + target;
        if (exists(p1)) return p1;

        // 2. ベースディレクトリ + サブディレクトリ + ファイル名
        for (const auto& sub : subDirs) {
            std::string p2 = base + "/" + sub + filename;
            if (exists(p2)) return p2;
        }
    }

    return fallback;
}

std::string ConfigService::getOsrmPath() const { return osrmPath_; }

std::string ConfigService::getSpotsCsvPath() const { return spotsCsvPath_; }

double ConfigService::getSpotSearchRadius() const { return spotSearchRadius_; }

std::string ConfigService::getGoogleApiKey() const { return googleApiKey_; }

}  // namespace services