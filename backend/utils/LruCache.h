#pragma once

#include <list>
#include <mutex>
#include <optional>
#include <unordered_map>

namespace cycling::utils {

/**
 * @brief Thread-safe Least Recently Used (LRU) Cache
 *
 * @tparam K Key type
 * @tparam V Value type
 */
template <typename K, typename V>
class LruCache {
   public:
    explicit LruCache(size_t capacity) : capacity_(capacity) {}

    /**
     * @brief Get an item from the cache
     *
     * @param key
     * @return std::optional<V>
     */
    std::optional<V> get(const K& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = map_.find(key);
        if (it == map_.end()) {
            return std::nullopt;
        }

        // Move the accessed item to the front of the list (most recently used)
        list_.splice(list_.begin(), list_, it->second);
        return it->second->second;
    }

    /**
     * @brief Insert or update an item in the cache
     *
     * @param key
     * @param value
     */
    void put(const K& key, const V& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = map_.find(key);

        if (it != map_.end()) {
            // Update existing value and move to front
            it->second->second = value;
            list_.splice(list_.begin(), list_, it->second);
            return;
        }

        // Check capacity
        if (list_.size() >= capacity_) {
            // Remove least recently used item (from the back)
            auto last = list_.back();
            map_.erase(last.first);
            list_.pop_back();
        }

        // Insert new item at the front
        list_.emplace_front(key, value);
        map_[key] = list_.begin();
    }

    /**
     * @brief Remove an item from the cache
     *
     * @param key
     */
    void remove(const K& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = map_.find(key);
        if (it != map_.end()) {
            list_.erase(it->second);
            map_.erase(it);
        }
    }

    /**
     * @brief Clear all items from the cache
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        list_.clear();
        map_.clear();
    }

    /**
     * @brief Get current number of items in the cache
     *
     * @return size_t
     */
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return list_.size();
    }

   private:
    size_t capacity_;
    mutable std::mutex mutex_;
    std::list<std::pair<K, V>> list_;
    std::unordered_map<K, typename std::list<std::pair<K, V>>::iterator> map_;
};

}  // namespace cycling::utils
