#pragma once

#include <drogon/nosql/RedisClient.h>
#include "IElevationCacheRepository.h"

namespace services::elevation {

class RedisElevationAdapter : public IElevationCacheRepository, public std::enable_shared_from_this<RedisElevationAdapter> {
   public:
    explicit RedisElevationAdapter(drogon::nosql::RedisClientPtr redisClient);
    ~RedisElevationAdapter() override = default;

    std::optional<ElevationCacheEntry> getTile(int z, int x, int y) override;
    bool saveTile(int z, int x, int y, const std::string& content) override;
    void incrementAccessScore(int z, int x, int y) override;
    void addToRefreshQueue(int z, int x, int y) override;
    std::optional<std::string> popRefreshQueue() override;
    void decayScores(double factor) override;
    double getAccessScore(int z, int x, int y) override;

   private:
    std::string makeDataKey(int z, int x, int y) const;
    std::string makeTileId(int z, int x, int y) const;
    
    drogon::nosql::RedisClientPtr redisClient_;
    const std::string rankKey_ = "cycling:elevation:v1:stats:rank";
    const std::string refreshQueueKey_ = "cycling:elevation:v1:queue:refresh";

    void scanStep(const std::string& cursor, double factor);
};

}  // namespace services::elevation
