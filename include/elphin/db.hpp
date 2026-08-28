#ifndef ELPHIN_DB_HPP
#define ELPHIN_DB_HPP

#include <string>
#include <unordered_map>
#include <optional>
#include <vector>

namespace elphin::store {

class Database {
public:
    Database() = default;

    void set(const std::string& key, const std::string& value);
    std::optional<std::string> get(const std::string& key) const;
    bool del(const std::string& key);
    bool exists(const std::string& key) const;

private:
    std::unordered_map<std::string, std::string> kv_store_;
};

} // namespace elphin::store

#endif // ELPHIN_DB_HPP