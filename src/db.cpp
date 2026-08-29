#include "elphin/db.hpp"
#include <algorithm>

namespace elphin::store {

int64_t Database::current_time_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

bool Database::is_expired(const std::string& key) {
    auto it = expires_.find(key);
    if (it == expires_.end()) {
        return false;
    }
    return current_time_ms() >= it->second;
}

void Database::check_and_evict_if_expired(const std::string& key) {
    if (is_expired(key)) {
        del(key);
    }
}

void Database::set(const std::string& key, const std::string& value) {
    expires_.erase(key); // Clear existing TTL on overwrite
    kv_store_[key] = value;
}

std::optional<std::string> Database::get(const std::string& key) {
    check_and_evict_if_expired(key); // Lazy eviction
    auto it = kv_store_.find(key);
    if (it != kv_store_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool Database::del(const std::string& key) {
    expires_.erase(key);
    bool removed = kv_store_.erase(key) > 0;
    if (zset_store_.erase(key) > 0) {
        removed = true;
    }
    return removed;
}

bool Database::exists(const std::string& key) {
    check_and_evict_if_expired(key); // Lazy eviction
    return kv_store_.find(key) != kv_store_.end() || zset_store_.find(key) != zset_store_.end();
}

bool Database::expire(const std::string& key, int64_t seconds) {
    if (!exists(key)) {
        return false;
    }
    expires_[key] = current_time_ms() + (seconds * 1000);
    return true;
}

int64_t Database::ttl(const std::string& key) {
    if (!exists(key)) {
        return -2; // Key does not exist
    }
    auto it = expires_.find(key);
    if (it == expires_.end()) {
        return -1; // Key exists but has no associated expire
    }
    int64_t remain_ms = it->second - current_time_ms();
    return remain_ms > 0 ? (remain_ms / 1000) : -2;
}

// Active eviction loop: Sample random keys from expires_ table and clean expired ones
int Database::active_expire_cycle(size_t sample_size) {
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

// ZSet operations with lazy eviction
bool Database::zadd(const std::string& key, double score, const std::string& member) {
    check_and_evict_if_expired(key);
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
    check_and_evict_if_expired(key);
    auto it = zset_store_.find(key);
    if (it == zset_store_.end()) {
        return {};
    }
    return it->second->skiplist.get_range_by_score(min_score, max_score);
}

} // namespace elphin::store