#include <drogon/drogon.h>
#include <drogon/nosql/RedisClient.h>
#include <gtest/gtest.h>

#include <chrono>
#include <thread>

#include "../../services/elevation/RedisElevationAdapter.h"

using namespace services::elevation;
using namespace std::string_literals;

class RedisIntegrationTest : public ::testing::Test {
   protected:
    static void SetUpTestSuite() {
        // Initialize Drogon app and Redis client for integration testing
        // Note: In real CI, we need to ensure Redis is running at this host/port
        const std::string host =
            std::getenv("REDIS_HOST") ? std::getenv("REDIS_HOST") : "127.0.0.1";
        const int port =
            std::getenv("REDIS_PORT") ? std::stoi(std::getenv("REDIS_PORT")) : 6379;

        // Check if client is already created to avoid multiple creation attempts
        try {
            drogon::app().getRedisClient();
        } catch (...) {
            drogon::app().createRedisClient(host, port);
        }
    }

    void SetUp() override {
        redisClient_ = drogon::app().getRedisClient();
        if (!redisClient_) {
            // Try to create it if it doesn't exist (fallback)
            const std::string host =
                std::getenv("REDIS_HOST") ? std::getenv("REDIS_HOST") : "127.0.0.1";
            const int port =
                std::getenv("REDIS_PORT") ? std::stoi(std::getenv("REDIS_PORT")) : 6379;
            drogon::app().createRedisClient(host, port);
            redisClient_ = drogon::app().getRedisClient();
        }

        if (!redisClient_) {
            // ここで ASSERT ではなく GTEST_SKIP しても、後続のコードが実行される可能性があるため即座に戻る
            // ただし、戻ったとしてもテスト本体は実行されてしまうので、
            // 各テストケース側で redisClient_ のチェックが必要
            return; 
        }
        
        adapter_ = std::make_unique<RedisElevationAdapter>(redisClient_);

        // Wait for Redis connection (simple retry)
        int retries = 0;
        bool connected = false;
        while (retries < 5 && !connected) {
            try {
                auto result = redisClient_->execCommandSync(
                    [](const drogon::nosql::RedisResult& r) { return r; }, "PING");
                if (result.asString() == "PONG") {
                    connected = true;
                }
            } catch (...) {
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                retries++;
            }
        }
        // ここでの判定はログ出力に留め、実際のスキップはテストケース内で行うのが安全
        if (!connected) {
            std::cout << "[WARN] Redis server not available at "
                         << (std::getenv("REDIS_HOST") ? std::getenv("REDIS_HOST") : "127.0.0.1") << std::endl;
        }
    }

    drogon::nosql::RedisClientPtr redisClient_;
    std::unique_ptr<RedisElevationAdapter> adapter_;
};

TEST_F(RedisIntegrationTest, ConnectionAndPing) {
    if (!redisClient_ || !adapter_) GTEST_SKIP() << "Redis client or adapter is null";
    try {
        auto result = redisClient_->execCommandSync(
            [](const drogon::nosql::RedisResult& r) { return r; }, "PING");
        EXPECT_EQ(result.asString(), "PONG");
    } catch (const std::exception& e) {
        // DrogonがRedisサポートなしでビルドされている場合、ここで例外が投げられる可能性がある
        // その場合は失敗ではなくスキップ扱いにするのが適切かもしれないが、現状はFAILさせる
        FAIL() << "Redis connection failed: " << e.what();
    } catch (...) {
        FAIL() << "Unknown error during Redis ping";
    }
}

TEST_F(RedisIntegrationTest, SaveAndGetTile) {
    if (!redisClient_ || !adapter_) GTEST_SKIP() << "Redis client or adapter is null";
    int z = 15, x = 123, y = 456;
    std::string content = "1.0,2.0,3.0";

    // Save
    bool saved = adapter_->saveTile(z, x, y, content);
    EXPECT_TRUE(saved);

    // Get
    auto entry = adapter_->getTile(z, x, y);
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(entry->content, content);
    EXPECT_GT(entry->updated_at, 0);
}

TEST_F(RedisIntegrationTest, BinarySafety) {
    if (!redisClient_ || !adapter_) GTEST_SKIP() << "Redis client or adapter is null";
    int z = 15, x = 999, y = 999;
    // Mocking binary data with null bytes
    std::string binaryContent = "start\0middle\0end"s;

    // Save
    bool saved = adapter_->saveTile(z, x, y, binaryContent);
    EXPECT_TRUE(saved);

    // Get
    auto entry = adapter_->getTile(z, x, y);
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(entry->content, binaryContent);
}

TEST_F(RedisIntegrationTest, RefreshQueue) {
    if (!redisClient_ || !adapter_) GTEST_SKIP() << "Redis client or adapter is null";
    int z = 10, x = 1, y = 2;
    adapter_->addToRefreshQueue(z, x, y);

    // Wait for async SADD
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto popped = adapter_->popRefreshQueue();
    ASSERT_TRUE(popped.has_value());
    EXPECT_EQ(*popped, "10:1:2");
}

TEST_F(RedisIntegrationTest, ScoreAndDecay) {
    if (!redisClient_ || !adapter_) GTEST_SKIP() << "Redis client or adapter is null";
    int z = 15, x = 0, y = 0;

    // Initial score increment
    adapter_->incrementAccessScore(z, x, y);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    double score = adapter_->getAccessScore(z, x, y);
    EXPECT_GE(score, 1.0);

    // Decay
    adapter_->decayScores(0.5);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    double decayedScore = adapter_->getAccessScore(z, x, y);
    EXPECT_NEAR(decayedScore, score * 0.5, 0.1);
}
