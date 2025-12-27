#include <gtest/gtest.h>

#include "../services/ConfigService.h"
#include "../services/SpotService.h"
#include "../utils/PolylineDecoder.h"

using namespace services;
using namespace utils;

class SpotServiceTest : public ::testing::Test {
   protected:
    void SetUp() override {
        // テストデータファイル名を設定するだけです。
        // ConfigServiceが実行ファイルの場所を基準にファイルを探します。
        setenv("SPOTS_CSV_PATH", "spots_test.csv", 1);

        // Re-initialize services after setting env var
        configService = std::make_unique<ConfigService>();
        spotService = std::make_unique<SpotService>(*configService);
    }

    void TearDown() override { unsetenv("SPOTS_CSV_PATH"); }

    std::unique_ptr<ConfigService> configService;
    std::unique_ptr<SpotService> spotService;
};

// Google Maps
// APIへのモック実装が難しいため、実際のネットワーク呼び出しを行うテストはスキップします。
// 将来的には、HttpClientをモックできるようにリファクタリングしてテストを有効化すべきです。
TEST_F(SpotServiceTest, DISABLED_SearchSpotsAlongPath_Simple) {
    // ...
}

TEST_F(SpotServiceTest, DISABLED_SearchSpotsAlongPath_ComplexPath) {
    // ...
}

TEST_F(SpotServiceTest, DISABLED_SearchSpotsAlongRoute_EncodedPolyline) {
    // ...
}
