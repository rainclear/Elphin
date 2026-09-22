#include "storage/db.hpp"
#include <mutex>

namespace elphin::storage {

int64_t Database::current_time_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

// Unsafe helper: Must be called under at least a shared or unique lock
bool Database::is_expired_unsafe(const std::string& key) const {
    auto it = expires_.find(key);
    if (it == expires_.end()) {
        return false;
    }
    return current_time_ms() >= it->second;
}

// Unsafe helper: Must be called under a unique (exclusive) lock
bool Database::check_and_evict_if_expired_unsafe(const std::string& key) {
    if (is_expired_unsafe(key)) {
        expires_.erase(key);
        kv_store_.erase(key);
        zset_store_.erase(key);
        return true;
    }
    return false;
}

void Database::set(const std::string& key, const std::string& value) {
    std::unique_lock lock(db_mutex_);
    expires_.erase(key);
    kv_store_[key] = value;
}

std::optional<std::string> Database::get(const std::string& key) {
    std::unique_lock lock(db_mutex_); // Unique lock required due to lazy eviction write
    check_and_evict_if_expired_unsafe(key);

    auto it = kv_store_.find(key);
    if (it != kv_store_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool Database::del(const std::string& key) {
    std::unique_lock lock(db_mutex_);
    expires_.erase(key);
    bool removed = kv_store_.erase(key) > 0;
    if (zset_store_.erase(key) > 0) {
        removed = true;
    }
    return removed;
}

bool Database::exists(const std::string& key) {
    std::unique_lock lock(db_mutex_); // Unique lock required due to lazy eviction write
    check_and_evict_if_expired_unsafe(key);
    return kv_store_.find(key) != kv_store_.end() || zset_store_.find(key) != zset_store_.end();
}

bool Database::expire(const std::string& key, int64_t seconds) {
    std::unique_lock lock(db_mutex_);
    check_and_evict_if_expired_unsafe(key);

    bool key_exists = (kv_store_.find(key) != kv_store_.end()) || (zset_store_.find(key) != zset_store_.end());
    if (!key_exists) {
        return false;
    }

    expires_[key] = current_time_ms() + (seconds * 1000);
    return true;
}

int64_t Database::ttl(const std::string& key) {
    std::unique_lock lock(db_mutex_);
    check_and_evict_if_expired_unsafe(key);

    bool key_exists = (kv_store_.find(key) != kv_store_.end()) || (zset_store_.find(key) != zset_store_.end());
    if (!key_exists) {
        return -2; // Key does not exist
    }

    auto it = expires_.find(key);
    if (it == expires_.end()) {
        return -1; // Key exists but has no associated TTL
    }

    int64_t remain_ms = it->second - current_time_ms();
    return remain_ms > 0 ? (remain_ms / 1000) : -2;
}

int Database::active_expire_cycle(size_t sample_size) {
    std::unique_lock lock(db_mutex_);
    if (expires_.empty()) return 0;

    int evicted = 0;
    size_t count = 0;
    auto it = expires_.begin();

    while (it != expires_.end() && count < sample_size) {
        if (current_time_ms() >= it->second) {
            std::string expired_key = it->first;
            it = expires_.erase(it);
            kv_store_.erase(expired_key);
            zset_store_.erase(expired_key);
            evicted++;
        } else {
            ++it;
        }
        count++;
    }
    return evicted;
}

bool Database::zadd(const std::string& key, double score, const std::string& member) {
    std::unique_lock lock(db_mutex_);
    check_and_evict_if_expired_unsafe(key);
    expires_.erase(key);

    auto it = zset_store_.find(key);
    if (it == zset_store_.end()) {
        auto zset = std::make_shared<SortedSet>();
        zset_store_[key] = zset;
        it = zset_store_.find(key);
    }

    auto& zset = it->second;
    auto dict_it = zset->dict.find(member);

    if (dict_it != zset->dict.end()) {
        double old_score = dict_it->second;
        if (old_score == score) return false;

        zset->skiplist.erase(member, old_score);
        zset->dict[member] = score;
        zset->skiplist.insert(member, score);
        return false;
    }

    zset->dict[member] = score;
    zset->skiplist.insert(member, score);
    return true;
}

std::vector<std::pair<std::string, double>> Database::zrangebyscore(const std::string& key, double min_score, double max_score) {
    std::unique_lock lock(db_mutex_); // Lock required for safe lookup and lazy eviction
    check_and_evict_if_expired_unsafe(key);

    auto it = zset_store_.find(key);
    if (it == zset_store_.end()) {
        return {};
    }
    return it->second->skiplist.get_range_by_score(min_score, max_score);
}

} // namespace elphin::storage