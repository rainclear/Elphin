#ifndef ELPHIN_DB_HPP
#define ELPHIN_DB_HPP

#include <string>
#include <unordered_map>
#include <optional>
#include <vector>
#include <memory>
#include "elphin/skiplist.hpp"

namespace elphin::store {

struct SortedSet {
    std::unordered_map<std::string, double> dict;
    SkipList skiplist;
};

class Database {
public:
    Database() = default;

    // String commands
    void set(const std::string& key, const std::string& value);
    std::optional<std::string> get(const std::string& key) const;
    bool del(const std::string& key);
    bool exists(const std::string& key) const;

    // ZSet commands
    bool zadd(const std::string& key, double score, const std::string& member);
    std::vector<std::pair<std::string, double>> zrangebyscore(const std::string& key, double min_score, double max_score) const;

private:
    std::unordered_map<std::string, std::string> kv_store_;
    std::unordered_map<std::string, std::shared_ptr<SortedSet>> zset_store_;
};

} // namespace elphin::store

#endif // ELPHIN_DB_HPP