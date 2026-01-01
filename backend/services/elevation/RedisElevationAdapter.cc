#include "RedisElevationAdapter.h"
#include <chrono>
#include <drogon/utils/Utilities.h>

namespace services::elevation {

RedisElevationAdapter::RedisElevationAdapter(drogon::nosql::RedisClientPtr redisClient)
    : redisClient_(std::move(redisClient)) {}

std::optional<ElevationCacheEntry> RedisElevationAdapter::getTile(int z, int x, int y) {
    std::string key = makeDataKey(z, x, y);
    
    try {
        auto result = redisClient_->execCommandSync(
            [](const drogon::nosql::RedisResult& r) { return r; },
            "HGETALL %s", key.c_str());
            
        if (result.type() == drogon::nosql::RedisResultType::kArray && result.size() > 0) {
            ElevationCacheEntry entry;
            for (size_t i = 0; i < result.size(); i += 2) {
                std::string field = result[i].asString();
                if (field == "content") {
                    entry.content = result[i+1].asString();
                } else if (field == "updated_at") {
                    entry.updated_at = std::stoull(result[i+1].asString());
                }
            }
            if (!entry.content.empty()) return entry;
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
        redisClient_->execCommandSync(
            [](const drogon::nosql::RedisResult& r) { return r; },
            "HSET %s content %s updated_at %llu", 
            key.c_str(), content.c_str(), (unsigned long long)now);
            
        // EXPIRE command (365 days)
        redisClient_->execCommandSync(
            [](const drogon::nosql::RedisResult& r) { return r; },
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
        auto result = redisClient_->execCommandSync(
            [](const drogon::nosql::RedisResult& r) { return r; },
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
        auto result = redisClient_->execCommandSync(
            [](const drogon::nosql::RedisResult& r) { return r; },
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
    return "cycling:elevation:v1:data:" + std::to_string(z) + ":" + std::to_string(x) + ":" + std::to_string(y);
}

std::string RedisElevationAdapter::makeTileId(int z, int x, int y) const {
    return std::to_string(z) + ":" + std::to_string(x) + ":" + std::to_string(y);
}

}  // namespace services::elevation
