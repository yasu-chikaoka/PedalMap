#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "IElevationCacheRepository.h"
#include "IElevationProvider.h"

namespace services::elevation {

class ElevationCacheManager;

/**
 * @brief Manages smart refreshing of elevation cache.
 * Handles access statistics, decay algorithms, and background fetching.
 */
class SmartRefreshService {
   public:
    SmartRefreshService(
        std::shared_ptr<IElevationCacheRepository> repository,
        std::shared_ptr<IElevationProvider> provider);
    virtual ~SmartRefreshService();

    // Lifecycle
    void startWorker();
    void stopWorker();

    // Stats & Queue
    void recordAccess(int z, int x, int y);
    void checkAndQueueRefresh(int z, int x, int y, uint64_t lastUpdated);

    // Configuration
    void setRefreshThreshold(double threshold);
    void setDecayFactor(double factor);

   private:
    std::shared_ptr<IElevationCacheRepository> repository_;
    std::shared_ptr<IElevationProvider> provider_;

    // Configuration
    double refreshThreshold_ = 10.0;
    double decayFactor_ = 0.95;

    // Background Worker
    std::atomic<bool> running_{false};
    std::thread workerThread_;
    void workerLoop();
    void processRefreshQueue();
    void performDecay();

    // Helper to generate cache key
    std::string makeKey(int z, int x, int y) const;
};

}  // namespace services::elevation
