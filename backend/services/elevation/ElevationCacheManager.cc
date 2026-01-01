#include "ElevationCacheManager.h"
#include "SmartRefreshService.h"
#include "GSIElevationProvider.h" // Include for dynamic_pointer_cast
#include <sstream>
#include <chrono>
#include <cmath>
#include <numbers>
#include <future>
#include <drogon/drogon.h>

namespace services::elevation {

ElevationCacheManager::ElevationCacheManager(
    std::shared_ptr<IElevationCacheRepository> repository,
    std::shared_ptr<IElevationProvider> backendProvider,
    std::shared_ptr<SmartRefreshService> refreshService,
    size_t lruCapacity)
    : repository_(std::move(repository)),
      backendProvider_(std::move(backendProvider)),
      refreshService_(std::move(refreshService)),
      l1Cache_(lruCapacity) {}

void ElevationCacheManager::getElevation(const Coordinate& coord, ElevationCallback&& callback) {
    auto tc = calculateTileCoord(coord);
    auto tileData = getTile(tc.z, tc.x, tc.y);
    
    if (tileData && !tileData->empty()) {
        double elevation = (*tileData)[tc.pixel_y * 256 + tc.pixel_x];
        callback(elevation);
    } else {
        // Fallback or Error
        callback(std::nullopt);
    }
}

void ElevationCacheManager::getElevations(const std::vector<Coordinate>& coords,
                                          ElevationsCallback&& callback) {
    if (coords.empty()) {
        callback({});
        return;
    }

    auto results = std::make_shared<std::vector<double>>(coords.size(), 0.0);
    // Note: This is synchronous/blocking in getTile, but we are inside drogon loop usually.
    // Ideally getTile should be async. But for now we proceed with synchronous cache/async fetch approach
    // wrapped in synchronous looking method for getTile, but wait...
    
    // If getTile fetches from API using GSI provider async fetch, we need to wait.
    // But getTile implementation below blocks?
    // We need to implement getTile carefully.
    
    for (size_t i = 0; i < coords.size(); ++i) {
        auto tc = calculateTileCoord(coords[i]);
        auto tileData = getTile(tc.z, tc.x, tc.y);
        if (tileData && !tileData->empty()) {
            (*results)[i] = (*tileData)[tc.pixel_y * 256 + tc.pixel_x];
        }
    }
    
    callback(*results);
}

std::optional<double> ElevationCacheManager::getElevationSync(const Coordinate& coord) {
    auto tc = calculateTileCoord(coord);
    auto tileData = getTile(tc.z, tc.x, tc.y);
    
    if (tileData && !tileData->empty()) {
        return (*tileData)[tc.pixel_y * 256 + tc.pixel_x];
    }
    return std::nullopt;
}

std::shared_ptr<std::vector<double>> ElevationCacheManager::getTile(int z, int x, int y) {
    std::string key = makeKey(z, x, y);

    // 1. L1 Cache (Memory)
    auto l1Result = l1Cache_.get(key);
    if (l1Result.has_value()) {
        if (refreshService_) refreshService_->recordAccess(z, x, y);
        return *l1Result;
    }

    // 2. L2 Cache (Redis)
    auto l2Result = repository_->getTile(z, x, y);
    if (l2Result.has_value()) {
        auto elevations = parseContent(l2Result->content);
        if (elevations) {
            // Populate L1
            l1Cache_.put(key, elevations);
            
            // Async Refresh Check
            if (refreshService_) {
                refreshService_->recordAccess(z, x, y);
                refreshService_->checkAndQueueRefresh(z, x, y, l2Result->updated_at);
            }
            
            return elevations;
        }
    }

    // 3. API Fetch (Fallback)
    LOG_DEBUG << "Cache Miss: " << key << " -> Fetching from API";
    
    auto gsiProvider = std::dynamic_pointer_cast<GSIElevationProvider>(backendProvider_);
    if (!gsiProvider) {
        LOG_ERROR << "Backend provider is not GSIElevationProvider";
        return nullptr;
    }

    // Use promise/future to wait for async callback
    std::promise<std::shared_ptr<std::vector<double>>> promise;
    auto future = promise.get_future();

    gsiProvider->fetchTile(z, x, y, 
        [this, z, x, y, &promise](std::shared_ptr<GSIElevationProvider::TileData> data) {
            if (data) {
                auto elevations = std::make_shared<std::vector<double>>(data->elevations);
                promise.set_value(elevations);
                
                // Save to L1
                std::string key = makeKey(z, x, y);
                l1Cache_.put(key, elevations);

                // Save to L2 (Convert back to CSV string for storage? Or raw text?)
                // Since we don't have the original raw text here easily unless we change fetchTile to return it.
                // Re-serializing is costly.
                // For now, let's skip saving to L2 here OR update fetchTile to return raw text too.
                // To minimize changes, let's assume we skip L2 save on Miss for a moment or implement serialization.
                
                // Proper way: fetchTile should return raw text or we reconstruct it.
                // Reconstructing 256x256 CSV is heavy.
                // Better approach: GSIElevationProvider should cache raw text in L2 if configured?
                // Or ElevationCacheManager should handle fetching directly.
                
                // Let's implement simple serialization for now to satisfy requirements.
                std::stringstream ss;
                for (size_t i = 0; i < elevations->size(); ++i) {
                   ss << (*elevations)[i];
                   if ((i + 1) % 256 == 0) ss << "\n";
                   else ss << ",";
                }
                repository_->saveTile(z, x, y, ss.str());
                
            } else {
                promise.set_value(nullptr);
            }
        });

    // Wait for the result (Blocking!)
    // Warning: Blocking the event loop is bad in Drogon.
    // However, this getTile is called from getElevationSync or internal logic.
    // If called from async getElevation, we are blocking the thread.
    // Since we are in the middle of refactoring, this is acceptable for Phase 2 strict adherence to plan?
    // The plan says "GSIElevationProvider is responsible for data fetch".
    // Ideally, getTile should be async. But we defined it as returning shared_ptr directly.
    
    // For now, wait with timeout.
    if (future.wait_for(std::chrono::seconds(5)) == std::future_status::ready) {
        return future.get();
    }

    return nullptr;
}

std::shared_ptr<std::vector<double>> ElevationCacheManager::parseContent(const std::string& content) {
    auto elevations = std::make_shared<std::vector<double>>();
    elevations->reserve(256 * 256);

    std::stringstream ss(content);
    std::string line;
    while (std::getline(ss, line)) {
        if (line.empty()) continue;
        std::stringstream ls(line);
        std::string val;
        while (std::getline(ls, val, ',')) {
            try {
                if (val == "e") {
                    elevations->push_back(0.0);
                } else {
                    elevations->push_back(std::stod(val));
                }
            } catch (...) {
                elevations->push_back(0.0);
            }
        }
    }

    if (elevations->size() != 256 * 256) {
        LOG_ERROR << "Parsed elevation data size mismatch: " << elevations->size();
        return nullptr;
    }
    return elevations;
}

std::string ElevationCacheManager::makeKey(int z, int x, int y) const {
    return std::to_string(z) + ":" + std::to_string(x) + ":" + std::to_string(y);
}

ElevationCacheManager::TileCoord ElevationCacheManager::calculateTileCoord(const Coordinate& coord, int zoom) {
    // Re-use logic from GSIElevationProvider or implement here.
    // Since we made it static in GSIElevationProvider header (but need to link), 
    // we can call GSIElevationProvider::calculateTileCoord if we include the header.
    // But TileCoord struct needs to be compatible.
    
    auto tc = GSIElevationProvider::calculateTileCoord(coord, zoom);
    TileCoord myTc;
    myTc.z = tc.z;
    myTc.x = tc.x;
    myTc.y = tc.y;
    myTc.pixel_x = tc.pixel_x;
    myTc.pixel_y = tc.pixel_y;
    return myTc;
}

}  // namespace services::elevation
