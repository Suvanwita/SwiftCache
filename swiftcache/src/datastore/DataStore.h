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
    long long expiresAt;
};

struct StoreStats {
    std::size_t keys{0};
};

class DataStore {
public:
    void set(const std::string& key, const std::string& value, long long ttlSeconds = -1);
    std::optional<std::string> get(const std::string& key);
    bool del(const std::string& key);
    bool exists(const std::string& key);
    bool expire(const std::string& key, long long ttlSeconds);
    long long ttl(const std::string& key);
    bool persist(const std::string& key);
    std::size_t removeExpired();
    StoreStats stats() const;

private:
    static long long nowMillis();
    bool isExpired(const ValueObject& object, long long now) const;

    mutable std::mutex mutex_;
    std::unordered_map<std::string, ValueObject> values_;
};

} // namespace swiftcache
