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
        const int port = std::getenv("REDIS_PORT") ? std::stoi(std::getenv("REDIS_PORT")) : 6379;

        drogon::app().createRedisClient(host, port);
    }

    void SetUp() override {
        redisClient_ = drogon::app().getRedisClient();
        adapter_ = std::make_unique<RedisElevationAdapter>(redisClient_);

        // Wait for Redis connection (simple retry)
        int retries = 0;
        bool connected = false;
        while (retries < 5 && !connected) {
            try {
                redisClient_->execCommandSync([](const drogon::nosql::RedisResult& r) { return r; },
                                              "PING");
                connected = true;
            } catch (...) {
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                retries++;
            }
        }
    }

    drogon::nosql::RedisClientPtr redisClient_;
    std::unique_ptr<RedisElevationAdapter> adapter_;
};

TEST_F(RedisIntegrationTest, ConnectionAndPing) {
    try {
        auto result = redisClient_->execCommandSync(
            [](const drogon::nosql::RedisResult& r) { return r; }, "PING");
        EXPECT_EQ(result.asString(), "PONG");
    } catch (const std::exception& e) {
        FAIL() << "Redis connection failed: " << e.what();
    }
}

TEST_F(RedisIntegrationTest, SaveAndGetTile) {
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
    int z = 15, x = 999, y = 999;
    // Mocking binary data with null bytes
    std::string binaryContent = "start\0middle\0end"s;

    bool saved = adapter_->saveTile(z, x, y, binaryContent);
    EXPECT_TRUE(saved);

    auto entry = adapter_->getTile(z, x, y);
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(entry->content, binaryContent);
}

TEST_F(RedisIntegrationTest, RefreshQueue) {
    int z = 10, x = 1, y = 2;
    adapter_->addToRefreshQueue(z, x, y);

    // Wait for async SADD
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto popped = adapter_->popRefreshQueue();
    ASSERT_TRUE(popped.has_value());
    EXPECT_EQ(*popped, "10:1:2");
}

TEST_F(RedisIntegrationTest, ScoreAndDecay) {
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
