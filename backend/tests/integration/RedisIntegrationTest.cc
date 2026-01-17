#include <arpa/inet.h>
#include <drogon/drogon.h>
#include <drogon/nosql/RedisClient.h>
#include <gtest/gtest.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <iostream>
#include <thread>

#include "../../services/elevation/RedisElevationAdapter.h"

using namespace services::elevation;
using namespace std::string_literals;

class RedisIntegrationTest : public ::testing::Test {
   public:
    RedisIntegrationTest() { std::cout << "DEBUG: RedisIntegrationTest Constructor" << std::endl; }

   protected:
    static void SetUpTestSuite() {
        // We defer Drogon initialization to SetUp to check connectivity first
    }

    bool isRedisRunning(const std::string& host, int port) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) return false;

        struct sockaddr_in serv_addr;
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(port);

        if (inet_pton(AF_INET, host.c_str(), &serv_addr.sin_addr) <= 0) {
            close(sock);
            return false;
        }

        // Set a short timeout for connection
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));

        bool connected = (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == 0);
        close(sock);
        return connected;
    }

    void SetUp() override {
        std::cout << "DEBUG: Starting SetUp" << std::endl;

        const std::string host =
            std::getenv("REDIS_HOST") ? std::getenv("REDIS_HOST") : "127.0.0.1";
        const int port = std::getenv("REDIS_PORT") ? std::stoi(std::getenv("REDIS_PORT")) : 6379;

        if (!isRedisRunning(host, port)) {
            std::cout << "DEBUG: Redis is not running at " << host << ":" << port
                      << ", skipping test." << std::endl;
            GTEST_SKIP();
            return;
        }

        // Only initialize Drogon Redis client if Redis is actually running
        try {
            // Check if client is already created to avoid multiple creation attempts
            try {
                redisClient_ = drogon::app().getRedisClient();
            } catch (...) {
                // Not created yet
            }

            if (!redisClient_) {
                drogon::app().createRedisClient(host, port);
                redisClient_ = drogon::app().getRedisClient();
            }
        } catch (const std::exception& e) {
            std::cout << "DEBUG: Failed to create Redis client: " << e.what() << std::endl;
            GTEST_SKIP();
            return;
        }

        if (!redisClient_) {
            std::cout << "DEBUG: Redis client is null after creation attempt, skipping" << std::endl;
            GTEST_SKIP();
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
        if (!connected) {
            std::cout << "[WARN] Redis server not available via Drogon client" << std::endl;
            // Clear adapter if connection failed to ensure tests skip properly
            adapter_.reset();
            redisClient_.reset();
            GTEST_SKIP();
        }
    }

    void TearDown() override {
        std::cout << "DEBUG: Starting TearDown" << std::endl;
        if (adapter_) {
            std::cout << "DEBUG: Adapter is active" << std::endl;
        } else {
            std::cout << "DEBUG: Adapter is null" << std::endl;
        }

        if (redisClient_) {
            try {
                // Clean up Redis database to avoid interference between tests
                redisClient_->execCommandSync(
                    [](const drogon::nosql::RedisResult&) { return true; }, "FLUSHDB");
            } catch (...) {
                // Ignore any errors during cleanup (e.g. connection lost)
            }
        }

        // Attempt to stop Drogon's event loop to prevent background thread crashes
        // This is crucial if Drogon started background threads for Redis
        // Only quit if we actually started something
        if (redisClient_) {
            drogon::app().quit();
            // Give a short grace period for threads to exit
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    drogon::nosql::RedisClientPtr redisClient_ = nullptr;
    std::unique_ptr<RedisElevationAdapter> adapter_ = nullptr;
};

TEST_F(RedisIntegrationTest, ConnectionAndPing) {
    if (!redisClient_ || !adapter_) GTEST_SKIP() << "Redis client or adapter is null";
    try {
        auto result = redisClient_->execCommandSync(
            [](const drogon::nosql::RedisResult& r) { return r; }, "PING");
        EXPECT_EQ(result.asString(), "PONG");
    } catch (const std::exception& e) {
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
