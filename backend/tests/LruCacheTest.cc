#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include "../utils/LruCache.h"

namespace {

TEST(LruCacheTest, InsertAndGet) {
    cycling::utils::LruCache<std::string, int> cache(2);
    cache.put("a", 1);
    
    auto val = cache.get("a");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, 1);
}

TEST(LruCacheTest, UpdateExisting) {
    cycling::utils::LruCache<std::string, int> cache(2);
    cache.put("a", 1);
    cache.put("a", 2);
    
    auto val = cache.get("a");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, 2);
}

TEST(LruCacheTest, CapacityLimitEviction) {
    cycling::utils::LruCache<std::string, int> cache(2);
    cache.put("a", 1);
    cache.put("b", 2);
    cache.put("c", 3); // "a" should be evicted
    
    EXPECT_FALSE(cache.get("a").has_value());
    EXPECT_TRUE(cache.get("b").has_value());
    EXPECT_TRUE(cache.get("c").has_value());
}

TEST(LruCacheTest, AccessOrderUpdate) {
    cycling::utils::LruCache<std::string, int> cache(2);
    cache.put("a", 1);
    cache.put("b", 2);
    
    // Access "a" to make it MRU
    cache.get("a");
    
    cache.put("c", 3); // "b" should be evicted instead of "a"
    
    EXPECT_TRUE(cache.get("a").has_value());
    EXPECT_FALSE(cache.get("b").has_value());
    EXPECT_TRUE(cache.get("c").has_value());
}

TEST(LruCacheTest, EmptyGet) {
    cycling::utils::LruCache<std::string, int> cache(2);
    EXPECT_FALSE(cache.get("non-existent").has_value());
}

TEST(LruCacheTest, ThreadSafety) {
    cycling::utils::LruCache<int, int> cache(100);
    const int num_threads = 10;
    const int iterations = 1000;
    
    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&cache, i, iterations]() {
            for (int j = 0; j < iterations; ++j) {
                cache.put(i * iterations + j, j);
                cache.get(i * iterations + (j % 10));
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    // Check that it didn't crash and has items (may be less than total due to capacity)
    EXPECT_GT(cache.size(), 0);
}

} // namespace
