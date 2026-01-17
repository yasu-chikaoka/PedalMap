#pragma once

#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "../../utils/LruCache.h"
#include "IElevationCacheRepository.h"
#include "IElevationProvider.h"

namespace services::elevation {

class SmartRefreshService;

/**
 * @brief Manages multi-level caching (L1: Memory, L2: Redis) and coordinates data fetching.
 */
class ElevationCacheManager : public IElevationProvider {
   public:
    ElevationCacheManager(std::shared_ptr<IElevationCacheRepository> repository,
                          std::shared_ptr<IElevationProvider> backendProvider,
                          std::shared_ptr<SmartRefreshService> refreshService,
                          size_t lruCapacity = 1000);
    ~ElevationCacheManager() override = default;

    // IElevationProvider implementation
    void getElevation(const Coordinate& coord, ElevationCallback&& callback) override;
    void getElevations(const std::vector<Coordinate>& coords,
                       ElevationsCallback&& callback) override;
    std::optional<double> getElevationSync(const Coordinate& coord) override;

    /**
     * @brief Get elevation data for a specific tile.
     *
     * Flow:
     * 1. Check L1 (Memory)
     * 2. Check L2 (Redis) -> If hit, populate L1 and trigger async refresh check
     * 3. Fetch from API -> Populate L1 & L2
     *
     * @param z Zoom level
     * @param x Tile X
     * @param y Tile Y
     * @return std::shared_ptr<std::vector<double>> Tile elevation data (256x256)
     */
    std::shared_ptr<std::vector<double>> getTile(int z, int x, int y);

   private:
    std::shared_ptr<IElevationCacheRepository> repository_;
    std::shared_ptr<IElevationProvider> backendProvider_;
    std::shared_ptr<SmartRefreshService> refreshService_;

    // L1 Cache: Stores parsed elevation data (vector<double>)
    // Key: "z:x:y"
    ::cycling::utils::LruCache<std::string, std::shared_ptr<std::vector<double>>> l1Cache_;

    // Helper to parse CSV content
    std::shared_ptr<std::vector<double>> parseContent(const std::string& content);

    // Helper to generate cache key
    std::string makeKey(int z, int x, int y) const;

    // Cache Stampede protection (Thundering Herd)
    std::mutex inFlightMutex_;
    std::unordered_map<std::string, std::shared_future<std::shared_ptr<std::vector<double>>>>
        inFlightRequests_;

    // Helper to calculate tile coord (Shared logic)
    struct TileCoord {
        int z;
        int x;
        int y;
        int pixel_x;
        int pixel_y;
    };
    TileCoord calculateTileCoord(const Coordinate& coord, int zoom = 15);
};

}  // namespace services::elevation
