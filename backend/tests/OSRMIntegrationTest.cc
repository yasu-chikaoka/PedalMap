#include <iostream>
#include <memory>

#include <gtest/gtest.h>
#include <osrm/engine_config.hpp>
#include <osrm/json_container.hpp>
#include <osrm/nearest_parameters.hpp>
#include <osrm/osrm.hpp>
#include <osrm/route_parameters.hpp>

#include "../services/RouteService.h"

// OSRM統合テスト
// 実際のデータファイル（/data/kanto-latest.osrm）を使用して、
// NearestおよびRoute機能が期待どおりに機能することを確認します。
class OSRMIntegrationTest : public ::testing::Test {
   protected:
    std::unique_ptr<osrm::OSRM> osrm_;

    void SetUp() override {
        osrm::EngineConfig config;
        config.storage_config = {"/data/kanto-latest.osrm"};
        config.use_shared_memory = false;
        config.algorithm = osrm::EngineConfig::Algorithm::CH;

        // データファイルが存在しない環境（CIなど）ではテストをスキップします。
        // コンテナ内で実行しているため、データが存在すると想定します。
        // ファイルの存在を確認することをお勧めします。
        // .osrmファイル自体が存在しない場合があるため、設定ファイルの1つ（.hsgrなど）を確認します
        const std::string osrm_path = "/data/kanto-latest.osrm";
        const std::string check_path = "/data/kanto-latest.osrm.hsgr";

        if (access(check_path.c_str(), F_OK) == -1) {
            // 別のファイル形式（CH以外）である可能性があるため、警告を出して続行するかスキップします。
            // ここでは.fileIndexの確認など代替案もありますが、単純にスキップします。
            // .osrmがある場合はそちらを確認すべきですが、リストになかったためです。
            GTEST_SKIP() << "OSRM data file not found at " << check_path;
        }

        try {
            osrm_ = std::make_unique<osrm::OSRM>(config);
        } catch (const std::exception &e) {
            GTEST_SKIP() << "Failed to initialize OSRM: " << e.what();
        }
    }
};

TEST_F(OSRMIntegrationTest, SnapToRoadTest) {
    if (!osrm_) return;

    // 道路から少し離れた座標（例：皇居内など）
    // 皇居のおおよその中心: 35.685175, 139.7528
    // 最寄りの道路（内堀通りなど）にスナップされるはずです
    double lat = 35.685175;
    double lon = 139.7528;

    osrm::NearestParameters params;
    params.coordinates.emplace_back(osrm::util::FloatLongitude{lon},
                                    osrm::util::FloatLatitude{lat});
    params.number_of_results = 1;

    osrm::json::Object result;
    const auto status = osrm_->Nearest(params, result);

    ASSERT_EQ(status, osrm::Status::Ok);

    // レスポンスの解析
    ASSERT_TRUE(result.values.contains("waypoints"));
    const auto &waypoints = result.values.at("waypoints").get<osrm::json::Array>();
    ASSERT_FALSE(waypoints.values.empty());

    const auto &waypoint = waypoints.values[0].get<osrm::json::Object>();
    ASSERT_TRUE(waypoint.values.contains("location"));

    const auto &location = waypoint.values.at("location").get<osrm::json::Array>();
    double snappedLon = location.values[0].get<osrm::json::Number>().value;
    double snappedLat = location.values[1].get<osrm::json::Number>().value;

    std::cout << "Original: (" << lat << ", " << lon << ")" << std::endl;
    std::cout << "Snapped : (" << snappedLat << ", " << snappedLon << ")" << std::endl;

    // 元の座標と異なる（補正された）ことを確認
    // ただし、偶然道路上だった場合は同じになるので、誤差判定
    EXPECT_TRUE(std::abs(lat - snappedLat) > 0.00001 || std::abs(lon - snappedLon) > 0.00001)
        << "Coordinate should be snapped to a road";
}

TEST_F(OSRMIntegrationTest, RouteCalculationTest) {
    if (!osrm_) return;

    // 東京駅 -> 秋葉原駅
    osrm::RouteParameters params;
    params.coordinates.emplace_back(osrm::util::FloatLongitude{139.767125},
                                    osrm::util::FloatLatitude{35.681236});
    params.coordinates.emplace_back(osrm::util::FloatLongitude{139.773072},
                                    osrm::util::FloatLatitude{35.698383});

    osrm::json::Object result;
    const auto status = osrm_->Route(params, result);

    ASSERT_EQ(status, osrm::Status::Ok);
    ASSERT_TRUE(result.values.contains("routes"));
    const auto &routes = result.values.at("routes").get<osrm::json::Array>();
    ASSERT_FALSE(routes.values.empty());

    const auto &route = routes.values[0].get<osrm::json::Object>();
    ASSERT_TRUE(route.values.contains("distance"));
    double distance = route.values.at("distance").get<osrm::json::Number>().value;

    std::cout << "Route Distance: " << distance << "m" << std::endl;
    EXPECT_GT(distance, 0.0);
}
