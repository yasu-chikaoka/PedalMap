#include "RedisElevationAdapter.h"
#include <chrono>
#include <drogon/utils/Utilities.h>

namespace services::elevation {

RedisElevationAdapter::RedisElevationAdapter(drogon::nosql::RedisClientPtr redisClient)
    : redisClient_(std::move(redisClient)) {}

std::optional<ElevationCacheEntry> RedisElevationAdapter::getTile(int z, int x, int y) {
    std::string key = makeDataKey(z, x, y);
    
    try {
        // HGETALL command
        auto result = redisClient_->execCommandSync("HGETALL %s", key.c_str());
        if (result.type() == drogon::nosql::RedisResultType::Map && !result.asMap().empty()) {
            auto map = result.asMap();
            ElevationCacheEntry entry;
            entry.content = map.at("content").asString();
            entry.updated_at = std::stoull(map.at("updated_at").asString());
            return entry;
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
        redisClient_->execCommandSync("HSET %s content %s updated_at %llu", 
                                      key.c_str(), content.c_str(), now);
        // EXPIRE command (365 days)
        redisClient_->execCommandSync("EXPIRE %s %d", key.c_str(), 365 * 24 * 60 * 60);
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
            if (r.type() == drogon::nosql::RedisResultType::Error) {
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
    redisClient_->execCommandAsync(
        [](const drogon::nosql::RedisResult& r) {},
        [](const std::exception& e) {
            LOG_ERROR << "Redis error in addToRefreshQueue: " << e.what();
        },
        "SADD %s %s", refreshQueueKey_.c_str(), tileId.c_str());
}

std::optional<std::string> RedisElevationAdapter::popRefreshQueue() {
    try {
        // SPOP
        auto result = redisClient_->execCommandSync("SPOP %s", refreshQueueKey_.c_str());
        if (result.type() == drogon::nosql::RedisResultType::String) {
            return result.asString();
        }
    } catch (const std::exception& e) {
        LOG_ERROR << "Redis error in popRefreshQueue: " << e.what();
    }
    return std::nullopt;
}

void RedisElevationAdapter::decayScores(double factor) {
    // Lua script implementation as planned
    std::string script = 
        "local key = KEYS[1] "
        "local factor = ARGV[1] "
        "local members = redis.call('ZRANGE', key, 0, -1, 'WITHSCORES') "
        "for i = 1, #members, 2 do "
        "    local member = members[i] "
        "    local score = tonumber(members[i+1]) "
        "    redis.call('ZADD', key, score * factor, member) "
        "end";
    
    redisClient_->execCommandAsync(
        [](const drogon::nosql::RedisResult& r) {
            LOG_DEBUG << "Decay scores completed";
        },
        [](const std::exception& e) {
            LOG_ERROR << "Redis error in decayScores: " << e.what();
        },
        "EVAL %s 1 %s %f", script.c_str(), rankKey_.c_str(), factor);
}

double RedisElevationAdapter::getAccessScore(int z, int x, int y) {
    std::string tileId = makeTileId(z, x, y);
    try {
        // ZSCORE
        auto result = redisClient_->execCommandSync("ZSCORE %s %s", rankKey_.c_str(), tileId.c_str());
        if (result.type() == drogon::nosql::RedisResultType::String) {
            return std::stod(result.asString());
        }
    } catch (const std::exception& e) {
        LOG_ERROR << "Redis error in getAccessScore: " << e.what();
    }
    return 0.0;
}

std::string RedisElevationAdapter::makeDataKey(int z, int x, int y) const {
    return "cycling:elevation:v1:data:" + std::to_string(z) + ":" + std::to_string(x) + ":" + std::to_string(y);
}

std::string RedisElevationAdapter::makeTileId(int z, int x, int y) const {
    return std::to_string(z) + ":" + std::to_string(x) + ":" + std::to_string(y);
}

}  // namespace services::elevation
