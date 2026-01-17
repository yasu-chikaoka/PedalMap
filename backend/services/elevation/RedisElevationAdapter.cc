#include "RedisElevationAdapter.h"

#include <drogon/utils/Utilities.h>

#include <chrono>

namespace services::elevation {

RedisElevationAdapter::RedisElevationAdapter(drogon::nosql::RedisClientPtr redisClient)
    : redisClient_(std::move(redisClient)) {
    if (!redisClient_) {
        throw std::runtime_error("Redis client is null");
    }
}

std::optional<ElevationCacheEntry> RedisElevationAdapter::getTile(int z, int x, int y) {
    std::string key = makeDataKey(z, x, y);

    try {
        // HGETALL command
        auto result = redisClient_->execCommandSync(
            [](const drogon::nosql::RedisResult& r) { return r; }, "HGETALL %s", key.c_str());

        if (result.type() == drogon::nosql::RedisResultType::kArray) {
            auto arr = result.asArray();
            if (!arr.empty()) {
                ElevationCacheEntry entry;
                for (size_t i = 0; i < arr.size(); i += 2) {
                    std::string field = arr[i].asString();
                    if (field == "content") {
                        entry.content = arr[i + 1].asString();
                    } else if (field == "updated_at") {
                        entry.updated_at = std::stoull(arr[i + 1].asString());
                    }
                }
                if (!entry.content.empty()) return entry;
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR << "Redis error in getTile: " << e.what();
    }

    return std::nullopt;
}

bool RedisElevationAdapter::saveTile(int z, int x, int y, const std::string& content) {
    std::string key = makeDataKey(z, x, y);
    uint64_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    try {
        // HSET command
        redisClient_->execCommandSync([](const drogon::nosql::RedisResult& r) { return r; },
                                      "HSET %s content %s updated_at %llu", key.c_str(),
                                      content.c_str(), (unsigned long long)now);

        // EXPIRE command (365 days)
        redisClient_->execCommandSync([](const drogon::nosql::RedisResult& r) { return r; },
                                      "EXPIRE %s %d", key.c_str(), 365 * 24 * 60 * 60);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR << "Redis error in saveTile: " << e.what();
    }

    return false;
}

void RedisElevationAdapter::incrementAccessScore(int z, int x, int y) {
    std::string tileId = makeTileId(z, x, y);

    // ZINCRBY (Async is better for fire-and-forget stats)
    redisClient_->execCommandAsync(
        [](const drogon::nosql::RedisResult& r) {
            if (r.type() == drogon::nosql::RedisResultType::kError) {
                LOG_ERROR << "Redis error in incrementAccessScore: " << r.asString();
            }
        },
        [](const std::exception& e) {
            LOG_ERROR << "Redis exception in incrementAccessScore: " << e.what();
        },
        "ZINCRBY %s 1 %s", rankKey_.c_str(), tileId.c_str());
}

void RedisElevationAdapter::addToRefreshQueue(int z, int x, int y) {
    std::string tileId = makeTileId(z, x, y);

    // SADD
    redisClient_->execCommandAsync([](const drogon::nosql::RedisResult& r) {},
                                   [](const std::exception& e) {
                                       LOG_ERROR << "Redis error in addToRefreshQueue: "
                                                 << e.what();
                                   },
                                   "SADD %s %s", refreshQueueKey_.c_str(), tileId.c_str());
}

std::optional<std::string> RedisElevationAdapter::popRefreshQueue() {
    try {
        // SPOP
        auto result =
            redisClient_->execCommandSync([](const drogon::nosql::RedisResult& r) { return r; },
                                          "SPOP %s", refreshQueueKey_.c_str());
        if (result.type() == drogon::nosql::RedisResultType::kString) {
            return result.asString();
        }
    } catch (const std::exception& e) {
        LOG_ERROR << "Redis error in popRefreshQueue: " << e.what();
    }
    return std::nullopt;
}

void RedisElevationAdapter::decayScores(double factor) {
    // 計画 3.3.3節に基づき、ZSCAN を使ったバッチ処理に分割して Redis ブロックを回避する
    scanStep("0", factor);
}

void RedisElevationAdapter::scanStep(const std::string& cursor, double factor) {
    auto self = shared_from_this();
    redisClient_->execCommandAsync(
        [self, factor](const drogon::nosql::RedisResult& r) {
            if (r.type() == drogon::nosql::RedisResultType::kArray) {
                auto arr = r.asArray();
                if (arr.size() >= 2) {
                    std::string nextCursor = arr[0].asString();
                    auto elements = arr[1];

                    if (elements.type() == drogon::nosql::RedisResultType::kArray) {
                        auto elArr = elements.asArray();
                        for (size_t i = 0; i < elArr.size(); i += 2) {
                            if (i + 1 >= elArr.size()) break;
                            std::string member = elArr[i].asString();
                            double score = std::stod(elArr[i + 1].asString());

                            self->redisClient_->execCommandAsync(
                                [](const auto&) {}, [](const auto&) {}, "ZADD %s %f %s",
                                self->rankKey_.c_str(), score * factor, member.c_str());
                        }
                    }

                    if (nextCursor != "0") {
                        self->scanStep(nextCursor, factor);
                    } else {
                        LOG_DEBUG << "Score decay completed (ZSCAN finished).";
                    }
                }
            }
        },
        [](const std::exception& e) {
            LOG_ERROR << "Redis error in decayScores (ZSCAN): " << e.what();
        },
        "ZSCAN %s %s COUNT 100", rankKey_.c_str(), cursor.c_str());
}

double RedisElevationAdapter::getAccessScore(int z, int x, int y) {
    std::string tileId = makeTileId(z, x, y);
    try {
        // ZSCORE
        auto result =
            redisClient_->execCommandSync([](const drogon::nosql::RedisResult& r) { return r; },
                                          "ZSCORE %s %s", rankKey_.c_str(), tileId.c_str());
        if (result.type() == drogon::nosql::RedisResultType::kString) {
            return std::stod(result.asString());
        }
    } catch (const std::exception& e) {
        LOG_ERROR << "Redis error in getAccessScore: " << e.what();
    }
    return 0.0;
}

std::string RedisElevationAdapter::makeDataKey(int z, int x, int y) const {
    return "cycling:elevation:v1:data:" + std::to_string(z) + ":" + std::to_string(x) + ":" +
           std::to_string(y);
}

std::string RedisElevationAdapter::makeTileId(int z, int x, int y) const {
    return std::to_string(z) + ":" + std::to_string(x) + ":" + std::to_string(y);
}

}  // namespace services::elevation
