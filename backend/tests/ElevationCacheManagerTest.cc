#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "../services/elevation/ElevationCacheManager.h"
#include "../services/elevation/IElevationCacheRepository.h"
#include "../services/elevation/IElevationProvider.h"
#include "../services/elevation/SmartRefreshService.h"

using namespace services::elevation;
using namespace testing;

// Mocks
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
                (const services::Coordinate& coord, ElevationCallback&& callback), (override));
    MOCK_METHOD(void, getElevations,
                (const std::vector<services::Coordinate>& coords, ElevationsCallback&& callback),
                (override));
    MOCK_METHOD(std::optional<double>, getElevationSync, (const services::Coordinate& coord),
                (override));
};

TEST(ElevationCacheManagerTest, L1CacheHit) {
    auto mockRepo = std::make_shared<MockRepository>();
    auto mockProvider = std::make_shared<MockProvider>();
    auto refreshService = std::make_shared<SmartRefreshService>(mockRepo, mockProvider);

    ElevationCacheManager manager(mockRepo, mockProvider, refreshService);

    // Setup L2 return with full size
    std::string csvContent;
    for (int i = 0; i < 256 * 256; ++i) {
        if (i > 0) csvContent += ",";
        csvContent += "0.0";
    }

    // Expect L2 getTile to be called once
    EXPECT_CALL(*mockRepo, getTile(15, 0, 0))
        .WillOnce(Return(ElevationCacheEntry{csvContent, 123456789}));

    // First call: L2 Hit -> L1 Populated
    auto result1 = manager.getTile(15, 0, 0);
    ASSERT_NE(result1, nullptr);
    EXPECT_EQ(result1->size(), 256 * 256);

    // Second call: L1 Hit (Repository should NOT be called again)
    auto result2 = manager.getTile(15, 0, 0);
    ASSERT_NE(result2, nullptr);
    EXPECT_EQ((*result2)[0], 0.0);
}

TEST(ElevationCacheManagerTest, L2CacheHit) {
    auto mockRepo = std::make_shared<MockRepository>();
    auto mockProvider = std::make_shared<MockProvider>();
    auto refreshService = std::make_shared<SmartRefreshService>(mockRepo, mockProvider);

    ElevationCacheManager manager(mockRepo, mockProvider, refreshService);

    // Setup L2 return
    std::string csvContent;
    // 256*256 zeros
    for (int i = 0; i < 256 * 256; ++i) {
        if (i > 0) csvContent += ",";
        csvContent += "5.0";
    }

    EXPECT_CALL(*mockRepo, getTile(15, 100, 100))
        .Times(1)
        .WillOnce(Return(ElevationCacheEntry{csvContent, 123456789}));

    // Call
    auto result = manager.getTile(15, 100, 100);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ((*result)[0], 5.0);
}
