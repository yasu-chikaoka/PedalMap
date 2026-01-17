#include <cstdint>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "../services/Coordinate.h"
#include "../services/elevation/IElevationCacheRepository.h"
#include "../services/elevation/IElevationProvider.h"
#include "../services/elevation/SmartRefreshService.h"

using namespace services::elevation;
using namespace testing;

class MockRepository : public IElevationCacheRepository {
   public:
    MOCK_METHOD(std::optional<ElevationCacheEntry>, getTile, (int z, int x, int y), (override));
    MOCK_METHOD(bool, saveTile, (int z, int x, int y, const std::string& content), (override));
    MOCK_METHOD(void, incrementAccessScore, (int z, int x, int y), (override));
    MOCK_METHOD(void, addToRefreshQueue, (int z, int x, int y), (override));
    MOCK_METHOD(std::optional<std::string>, popRefreshQueue, (), (override));
    MOCK_METHOD(void, decayScores, (double factor), (override));
    MOCK_METHOD(double, getAccessScore, (int z, int x, int y), (override));
};

class MockProvider : public IElevationProvider {
   public:
    MOCK_METHOD(void, getElevation,
                (const services::Coordinate& coord, IElevationProvider::ElevationCallback&& callback), (override));
    MOCK_METHOD(void, getElevations,
                (const std::vector<services::Coordinate>& coords, IElevationProvider::ElevationsCallback&& callback),
                (override));
    MOCK_METHOD(std::optional<double>, getElevationSync, (const services::Coordinate& coord),
                (override));
};

TEST(SmartRefreshServiceTest, RecordAccess) {
    auto mockRepo = std::make_shared<MockRepository>();
    auto mockProvider = std::make_shared<MockProvider>();
    SmartRefreshService service(mockRepo, mockProvider);

    EXPECT_CALL(*mockRepo, incrementAccessScore(15, 10, 20)).Times(1);

    service.recordAccess(15, 10, 20);
}

// checkAndQueueRefresh involves async call on event loop, hard to test synchronously without full
// Drogon setup. We can test the logic flow if we could intercept the loop. For unit test, we might
// skip the async part or rely on Integration test.

TEST(SmartRefreshServiceTest, Lifecycle) {
    auto mockRepo = std::make_shared<MockRepository>();
    auto mockProvider = std::make_shared<MockProvider>();
    SmartRefreshService service(mockRepo, mockProvider);

    service.startWorker();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    service.stopWorker();
}
