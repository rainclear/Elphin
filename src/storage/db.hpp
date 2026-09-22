#ifndef ELPHIN_STORAGE_DB_HPP
#define ELPHIN_STORAGE_DB_HPP

#include <string>
#include <unordered_map>
#include <optional>
#include <vector>
#include <memory>
#include <chrono>
#include <shared_mutex>
#include "storage/skip_list.hpp"

namespace elphin::storage {

struct SortedSet {
    std::unordered_map<std::string, double> dict;
    SkipList skiplist;
};

class Database {
public:
    Database() = default;

    // String commands
    void set(const std::string& key, const std::string& value);
    std::optional<std::string> get(const std::string& key);
    bool del(const std::string& key);
    bool exists(const std::string& key);

    // ZSet commands
    bool zadd(const std::string& key, double score, const std::string& member);
    std::vector<std::pair<std::string, double>> zrangebyscore(const std::string& key, double min_score, double max_score);

    // TTL / Expiry commands
    bool expire(const std::string& key, int64_t seconds);
    int64_t ttl(const std::string& key);

    // Active eviction sampling task
    int active_expire_cycle(size_t sample_size = 20);

private:
    bool is_expired_unsafe(const std::string& key) const;
    bool check_and_evict_if_expired_unsafe(const std::string& key);
    static int64_t current_time_ms();

    std::unordered_map<std::string, std::string> kv_store_;
    std::unordered_map<std::string, std::shared_ptr<SortedSet>> zset_store_;
    std::unordered_map<std::string, int64_t> expires_; // key -> expire_at_ms

    // Reader-Writer Lock to protect all internal stores across threads
    mutable std::shared_mutex db_mutex_;
};

} // namespace elphin::storage

#endif // ELPHIN_STORAGE_DB_HPP