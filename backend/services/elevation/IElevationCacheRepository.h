#pragma once

#include <optional>
#include <string>
#include <vector>

namespace services::elevation {

/**
 * @brief Elevation cache entry metadata
 */
struct ElevationCacheEntry {
    std::string content;
    uint64_t updated_at;
};

/**
 * @brief Interface for elevation data persistence (L2 Cache)
 */
class IElevationCacheRepository {
   public:
    virtual ~IElevationCacheRepository() = default;

    /**
     * @brief Get elevation data from cache
     * 
     * @param z zoom
     * @param x tile x
     * @param y tile y
     * @return std::optional<ElevationCacheEntry>
     */
    virtual std::optional<ElevationCacheEntry> getTile(int z, int x, int y) = 0;

    /**
     * @brief Save elevation data to cache
     * 
     * @param z zoom
     * @param x tile x
     * @param y tile y
     * @param content CSV string
     * @return true if success
     */
    virtual bool saveTile(int z, int x, int y, const std::string& content) = 0;

    /**
     * @brief Increment access score for a tile
     * 
     * @param z zoom
     * @param x tile x
     * @param y tile y
     */
    virtual void incrementAccessScore(int z, int x, int y) = 0;

    /**
     * @brief Add tile to refresh queue
     * 
     * @param z zoom
     * @param x tile x
     * @param y tile y
     */
    virtual void addToRefreshQueue(int z, int x, int y) = 0;

    /**
     * @brief Pop a tile from refresh queue
     * 
     * @return std::optional<std::string> tile key "z:x:y"
     */
    virtual std::optional<std::string> popRefreshQueue() = 0;

    /**
     * @brief Apply decay factor to all access scores
     * 
     * @param factor 0.0 to 1.0
     */
    virtual void decayScores(double factor) = 0;

    /**
     * @brief Get access score for a tile
     * 
     * @param z zoom
     * @param x tile x
     * @param y tile y
     * @return double score
     */
    virtual double getAccessScore(int z, int x, int y) = 0;
};

}  // namespace services::elevation
