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
            l1Cache_.put(key, elevations);
            if (refreshService_) {
                refreshService_->recordAccess(z, x, y);
                refreshService_->checkAndQueueRefresh(z, x, y, l2Result->updated_at);
            }
            return elevations;
        }
    }

    // 3. API Fetch with Cache Stampede Protection
    std::shared_future<std::shared_ptr<std::vector<double>>> future;
    {
        std::lock_guard<std::mutex> lock(inFlightMutex_);
        auto it = inFlightRequests_.find(key);
        if (it != inFlightRequests_.end()) {
            future = it->second;
        } else {
            // Start new request
            std::promise<std::shared_ptr<std::vector<double>>> promise;
            future = promise.get_future().share();
            inFlightRequests_[key] = future;

            LOG_DEBUG << "Cache Miss: " << key << " -> Fetching from API";
            auto gsiProvider = std::dynamic_pointer_cast<GSIElevationProvider>(backendProvider_);
            if (!gsiProvider) {
                LOG_ERROR << "Backend provider is not GSIElevationProvider";
                promise.set_value(nullptr);
            } else {
                gsiProvider->fetchTile(z, x, y, 
                    [this, z, x, y, key, p = std::move(promise)](std::shared_ptr<GSIElevationProvider::TileData> data) mutable {
                        std::shared_ptr<std::vector<double>> elevations = nullptr;
                        if (data) {
                            elevations = std::make_shared<std::vector<double>>(data->elevations);
                            l1Cache_.put(key, elevations);
                            
                            // Save to L2
                            std::stringstream ss;
                            for (size_t i = 0; i < elevations->size(); ++i) {
                               ss << (*elevations)[i];
                               if ((i + 1) % 256 == 0) ss << "\n";
                               else ss << ",";
                            }
                            repository_->saveTile(z, x, y, ss.str());
                        }
                        p.set_value(elevations);
                        
                        // Cleanup in-flight
                        {
                            std::lock_guard<std::mutex> lock(inFlightMutex_);
                            inFlightRequests_.erase(key);
                        }
                    });
            }
        }
    }

    // Wait for the shared future
    if (future.wait_for(std::chrono::seconds(10)) == std::future_status::ready) {
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
