#include <gtest/gtest.h>

#include "../services/ConfigService.h"
#include "../services/SpotService.h"
#include "../utils/PolylineDecoder.h"

using namespace services;
using namespace utils;

class SpotServiceTest : public ::testing::Test {
   protected:
    void SetUp() override {
        // Set environment variable for test CSV
        // Assume the test is run from the build directory, so we need to point to the source dir
        // Or better, use an absolute path or relative to where we expect to be.
        // In CMake/CTest environment, usually CWD is build dir.
        // Let's assume the data file is copied or we use relative path from source root.

        // Use absolute path in the container
        setenv("SPOTS_CSV_PATH", "/app/tests/data/spots_test.csv", 1);

        // Re-initialize services after setting env var
        configService = std::make_unique<ConfigService>();
        spotService = std::make_unique<SpotService>(*configService);
    }

    void TearDown() override { unsetenv("SPOTS_CSV_PATH"); }

    std::unique_ptr<ConfigService> configService;
    std::unique_ptr<SpotService> spotService;
};

TEST_F(SpotServiceTest, SearchSpotsAlongPath_Simple) {
    // 東京駅から秋葉原へのパス
    std::vector<Coordinate> path = {
        {35.681236, 139.767125},  // 東京駅
        {35.698383, 139.773072}   // 秋葉原
    };

    // パスをエンコード
    std::string polyline = PolylineDecoder::encode(path);

    // 半径1km以内で検索
    auto spots = spotService->searchSpotsAlongRoute(polyline, 1000.0);

    // "Cycling Cafe Base"（東京駅）と "Ramen Energy"（秋葉原）が見つかることを期待
    ASSERT_GE(spots.size(), 2);

    bool foundCafe = false;
    bool foundRamen = false;
    for (const auto& spot : spots) {
        if (spot.name == "Cycling Cafe Base") foundCafe = true;
        if (spot.name == "Ramen Energy") foundRamen = true;
    }

    EXPECT_TRUE(foundCafe);
    EXPECT_TRUE(foundRamen);
}

TEST_F(SpotServiceTest, SearchSpotsAlongPath_NoSpotsNearby) {
    // 登録されたスポットから遠く離れたパス
    std::vector<Coordinate> path = {{40.0, 135.0}, {40.1, 135.1}};

    // パスをエンコード
    std::string polyline = PolylineDecoder::encode(path);

    auto spots = spotService->searchSpotsAlongRoute(polyline, 1000.0);

    EXPECT_TRUE(spots.empty());
}

TEST_F(SpotServiceTest, SearchSpotsAlongPath_ComplexPath) {
    // 東京 -> 秋葉原 -> スカイツリー -> 上野
    std::vector<Coordinate> path = {
        {35.681236, 139.767125},  // 東京駅
        {35.698383, 139.773072},  // 秋葉原
        {35.710063, 139.810700},  // スカイツリー
        {35.714074, 139.774109}   // 上野
    };

    // パスをエンコード
    std::string polyline = PolylineDecoder::encode(path);

    // 半径1.5km以内で検索
    auto spots = spotService->searchSpotsAlongRoute(polyline, 1500.0);

    // 東京、秋葉原、スカイツリー、上野のスポットが見つかることを期待
    ASSERT_GE(spots.size(), 4);

    std::set<std::string> foundNames;
    for (const auto& spot : spots) {
        foundNames.insert(spot.name);
    }

    EXPECT_TRUE(foundNames.count("Cycling Cafe Base"));
    EXPECT_TRUE(foundNames.count("Ramen Energy"));
    EXPECT_TRUE(foundNames.count("Skytree View"));
    EXPECT_TRUE(foundNames.count("Ueno Park Cafe"));
}

TEST_F(SpotServiceTest, SearchSpotsAlongRoute_EncodedPolyline) {
    // 東京駅を通過するパス
    // 東京駅周辺のセグメントのエンコードされたポリライン
    // ポイント: (35.6800, 139.7600) -> (35.6850, 139.7700)
    // "Cycling Cafe Base" は 35.681236, 139.767125 にあり、この線に近いはずです。

    // エンコード (35.6800, 139.7600), (35.6850, 139.7700)
    // 35.6800, 139.7600 -> _ibE_seK
    // デルタ: +0.0050, +0.0100 -> +500, +1000
    // 500 << 1 = 1000 = 1111101000_2
    // 11101000 -> 101000(40) -> 101000 + 63 = 103('g')
    // 111 -> 000111(7) -> 7+63=70('F')
    // -> _ibE_seK_gF_qJ (近似的な手動チェック、信頼性のために生成された文字列を使用しましょう)

    // 安全のために、より単純な既知の文字列または構築された座標を使用しましょう。
    // または、PolylineDecoderを別途テストしたという事実に頼り、
    // ここではSpotServiceが正しく呼び出してフィルタリングすることだけをテストすることもできます。

    // Simpleテストのパスにデコードされることがわかっている文字列でモックしましょう：
    // (35.681236, 139.767125) -> (35.698383, 139.773072)
    // これを手動で完全にエンコードするのは困難です。

    // 代替案：ロジックのテストに単純な整数座標パスを使用しますか？
    // いいえ、SpotServiceは実際の緯度/経度を使用します。

    // デコーダーを使用して最初に文字列を生成しますか？ いいえ、デコーダーはデコードのみです。
    // (35.68, 139.76) -> (35.70, 139.78) 用に事前に計算された文字列を使用します
    // これは東京駅 (35.681236, 139.767125) と秋葉原 (35.698383, 139.773072) の近くを通ります
    // ポリライン: "_ibE_seK_}hI_}hR" (近似)

    // (35.68, 139.76) から (35.70, 139.78) への有効な文字列:
    // 35.68000, 139.76000
    // 35.70000, 139.78000
    // 文字列: "_ibE_seK_qiF_qoJ"
    // 35.68000 -> 3568000
    // 139.76000 -> 13976000
    // 35.70000 - 35.68000 = 2000
    // 139.78000 - 139.76000 = 2000

    // この文字列を手動で確認するか、信頼してみましょう。
    // デコードに失敗/ずれている場合、テストは適切に失敗する可能性があります（スポットが見つからない）。
    // 簡単にエンコードできない場合は、スポットの正確な座標を使用して一致することを確認しましょう。
    // 実際、テスト用にEncode関数をユーティリティに追加できますか？
    // または、手動でエンコードするだけです：
    // ポイント 1: 35.68124, 139.76713 (東京駅 近似)
    // ポイント 2: 35.69838, 139.77307 (秋葉原 近似)

    // 35.68124 -> 3568124. 139.76713 -> 13976713
    // _kbE_~eK  <-- 手動では間違っている可能性が高いです。

    // PolylineDecoderTestの文字列を使用して (38.5, -120.2) にデコードされることがわかっているので、
    // このテストケースのためにそこにダミースポットを追加しますか？
    // しかし、SpotServiceは東京周辺のハードコードされたスポットをロードします。

    // OK、東京駅を通る非常に単純な垂直線を使用しましょう。
    // スポット: 35.681236, 139.767125
    // 線: (35.68000, 139.76713) -> (35.69000, 139.76713)
    // 緯度変化: +0.01000 (+1000). 経度変化: 0.
    // 開始: 35.68000, 139.76713
    // 35.68000 -> 3568000 -> 1101100110111000000000 (2進数) ... 難しいです。

    // より簡単なアプローチ: オンラインツールを使用して「東京駅から秋葉原」のポリラインを取得し、
    // その文字列を使用します。
    // 東京駅: 35.681236, 139.767125
    // 秋葉原: 35.698383, 139.773072
    // OSRM/Google エンコード済みポリライン (近似):
    // "{kbEua}K_@s@u@e@k@g@i@e@c@a@_@??" (単なる推測ですか？ いいえ。)

    // 有効であることが「わかっている」文字列を試して、何かが見つかるか確認してみましょう。
    // そうでない場合は、`searchSpotsAlongPath` テストとデコーダーの単体テストに頼ることができます。
    // しかし、統合テストの方が優れています。

    // この単純な文字列を使用します: "_ibE_seK_qiF_qoJ"
    // (38.5, -120.2) にデコードされるPolylineDecoderTestの既知のポリライン文字列を使用し、
    // spots_test.csvのこの場所に "Polyline Test Spot" を追加しました。
    std::string polyline = "_p~iF~ps|U";
    // (38.5, -120.2) にデコードされます

    auto spots = spotService->searchSpotsAlongRoute(polyline, 100.0);

    bool foundTestSpot = false;
    for (const auto& spot : spots) {
        if (spot.name == "Polyline Test Spot") foundTestSpot = true;
    }

    EXPECT_TRUE(foundTestSpot) << "Should find Polyline Test Spot";
}