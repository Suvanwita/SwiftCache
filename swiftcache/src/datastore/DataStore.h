#pragma once

#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace swiftcache {

struct ValueObject {
    std::string value;
    std::string type;
    long long createdAt;
};

struct StoreStats {
    std::size_t keys{0};
};

class DataStore {
public:
    void set(const std::string& key, const std::string& value);
    std::optional<std::string> get(const std::string& key) const;
    bool del(const std::string& key);
    bool exists(const std::string& key) const;
    StoreStats stats() const;

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, ValueObject> values_;
};

} // namespace swiftcache
