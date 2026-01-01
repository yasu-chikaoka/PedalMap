#include "SmartRefreshService.h"
#include "GSIElevationProvider.h" // For dynamic_pointer_cast
#include <drogon/drogon.h>
#include <chrono>

namespace services::elevation {

SmartRefreshService::SmartRefreshService(
    std::shared_ptr<IElevationCacheRepository> repository,
    std::shared_ptr<IElevationProvider> provider)
    : repository_(std::move(repository)),
      provider_(std::move(provider)) {}

SmartRefreshService::~SmartRefreshService() {
    stopWorker();
}

void SmartRefreshService::startWorker() {
    if (running_.exchange(true)) {
        return; // Already running
    }
    workerThread_ = std::thread(&SmartRefreshService::workerLoop, this);
}

void SmartRefreshService::stopWorker() {
    if (running_.exchange(false)) {
        if (workerThread_.joinable()) {
            workerThread_.join();
        }
    }
}

void SmartRefreshService::recordAccess(int z, int x, int y) {
    // Fire and forget (Async handled by Redis adapter)
    repository_->incrementAccessScore(z, x, y);
}

void SmartRefreshService::checkAndQueueRefresh(int z, int x, int y, uint64_t lastUpdated) {
    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    // 3 months = 90 days
    const uint64_t threeMonthsSeconds = 90 * 24 * 60 * 60;
    
    if (now - lastUpdated > threeMonthsSeconds) {
        // Check Access Score (Synchronously here, or move logic to Redis side via Lua/job)
        // Ideally we shouldn't block.
        // Let's assume we do a quick check or just queue it and let worker decide.
        // But plan says "Lazy Check: updated_at > 3 months & score high -> queue".
        
        // Blocking here for ZSCORE might be slow.
        // Instead, just queue it if old? Or fire async check?
        // Let's fire async task to check score and queue.
        
        // Using drogon's thread pool for this small task
        drogon::app().getLoop()->runInLoop([this, z, x, y]() {
             double score = repository_->getAccessScore(z, x, y);
             if (score >= refreshThreshold_) {
                 repository_->addToRefreshQueue(z, x, y);
             }
        });
    }
}

void SmartRefreshService::setRefreshThreshold(double threshold) {
    refreshThreshold_ = threshold;
}

void SmartRefreshService::setDecayFactor(double factor) {
    decayFactor_ = factor;
}

void SmartRefreshService::workerLoop() {
    LOG_INFO << "SmartRefreshService worker started.";
    
    int loopCount = 0;
    while (running_) {
        try {
            processRefreshQueue();
            
            // Perform decay once a day (approx)
            // Assuming 1s loop interval, 86400 loops = 1 day
            if (++loopCount >= 86400) {
                performDecay();
                loopCount = 0;
            }
        } catch (const std::exception& e) {
            LOG_ERROR << "Exception in SmartRefreshService worker: " << e.what();
        }
        
        // Sleep 1s (Rate limit for API check loop)
        // Also serves as minimal interval between tasks if queue is empty
        // If queue is full, we process one by one with 1s interval (1 QPS)
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    
    LOG_INFO << "SmartRefreshService worker stopped.";
}

void SmartRefreshService::processRefreshQueue() {
    auto tileKeyOpt = repository_->popRefreshQueue();
    if (!tileKeyOpt) {
        return; // Empty queue
    }
    
    std::string tileKey = *tileKeyOpt;
    // Key format: "z:x:y"
    int z, x, y;
    if (sscanf(tileKey.c_str(), "%d:%d:%d", &z, &x, &y) != 3) {
        LOG_ERROR << "Invalid tile key in refresh queue: " << tileKey;
        return;
    }
    
    LOG_INFO << "Refeshing tile: " << tileKey;
    
    // Fetch from API
    auto gsiProvider = std::dynamic_pointer_cast<GSIElevationProvider>(provider_);
    if (!gsiProvider) return;
    
    // We need to wait for the fetch to complete to control rate limit properly in this thread.
    std::promise<void> promise;
    auto future = promise.get_future();
    
    gsiProvider->fetchTile(z, x, y, 
        [this, z, x, y, &promise](std::shared_ptr<GSIElevationProvider::TileData> data) {
            if (data) {
                // Save to Redis (Refresh TTL)
                std::stringstream ss;
                for (size_t i = 0; i < data->elevations.size(); ++i) {
                   ss << data->elevations[i];
                   if ((i + 1) % 256 == 0) ss << "\n";
                   else ss << ",";
                }
                repository_->saveTile(z, x, y, ss.str());
                LOG_DEBUG << "Tile refreshed: " << z << "/" << x << "/" << y;
            } else {
                LOG_WARN << "Failed to refresh tile: " << z << "/" << x << "/" << y;
            }
            promise.set_value();
        });
        
    // Wait for API call to finish
    if (future.wait_for(std::chrono::seconds(10)) != std::future_status::ready) {
        LOG_ERROR << "Timeout refreshing tile: " << tileKey;
    }
}

void SmartRefreshService::performDecay() {
    LOG_INFO << "Performing score decay with factor: " << decayFactor_;
    repository_->decayScores(decayFactor_);
}

std::string SmartRefreshService::makeKey(int z, int x, int y) const {
    return std::to_string(z) + ":" + std::to_string(x) + ":" + std::to_string(y);
}

}  // namespace services::elevation
