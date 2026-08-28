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
    return kv_store_.erase(key) > 0;
}

bool Database::exists(const std::string& key) const {
    return kv_store_.find(key) != kv_store_.end();
}

} // namespace elphin::store