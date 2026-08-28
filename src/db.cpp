#include "elphin/db.hpp"

namespace elphin::store {

void Database::set(const std::string& key, const std::string& value) {
    kv_store_[key] = value;
}

std::optional<std::string> Database::get(const std::string& key) const {
    auto it = kv_store_.find(key);
    if (it != kv_store_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool Database::del(const std::string& key) {
    bool removed = kv_store_.erase(key) > 0;
    if (zset_store_.erase(key) > 0) {
        removed = true;
    }
    return removed;
}

bool Database::exists(const std::string& key) const {
    return kv_store_.find(key) != kv_store_.end() || zset_store_.find(key) != zset_store_.end();
}

bool Database::zadd(const std::string& key, double score, const std::string& member) {
    auto it = zset_store_.find(key);
    if (it == zset_store_.end()) {
        auto zset = std::make_shared<SortedSet>();
        zset_store_[key] = zset;
        it = zset_store_.find(key);
    }

    auto& zset = it->second;
    auto dict_it = zset->dict.find(member);

    if (dict_it != zset->dict.end()) {
        // Update existing member: remove old score from SkipList first
        double old_score = dict_it->second;
        if (old_score == score) return false;

        zset->skiplist.erase(member, old_score);
        zset->dict[member] = score;
        zset->skiplist.insert(member, score);
        return false; // Return false indicating update
    }

    // New member addition
    zset->dict[member] = score;
    zset->skiplist.insert(member, score);
    return true;
}

std::vector<std::pair<std::string, double>> Database::zrangebyscore(const std::string& key, double min_score, double max_score) const {
    auto it = zset_store_.find(key);
    if (it == zset_store_.end()) {
        return {};
    }
    return it->second->skiplist.get_range_by_score(min_score, max_score);
}

} // namespace elphin::store