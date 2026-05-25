#pragma once

#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace swiftcache {

namespace data_type {
constexpr const char* String = "string";
constexpr const char* List = "list";
constexpr const char* Hash = "hash";
constexpr const char* Set = "set";
} // namespace data_type

struct ValueObject {
    std::string value;
    std::deque<std::string> list;
    std::unordered_map<std::string, std::string> hash;
    std::unordered_set<std::string> set;
    std::string type;
    long long createdAt;
    long long expiresAt;
    long long lastAccessedAt{-1};
};

struct StoreStats {
    std::size_t keys{0};
    std::size_t estimatedBytes{0};
    std::size_t evictedKeys{0};
    std::size_t expiredKeys{0};
};

enum class EvictionPolicy {
    None,
    AllKeysLru,
    VolatileLru,
    TtlPriority,
    Random
};

struct EvictionConfig {
    std::size_t maxKeys{0};
    std::size_t maxMemoryBytes{0};
    EvictionPolicy policy{EvictionPolicy::None};
};

enum class DataStoreStatus {
    Ok,
    Missing,
    WrongType
};

struct ListPopResult {
    DataStoreStatus status{DataStoreStatus::Missing};
    std::string value;
};

struct ListRangeResult {
    DataStoreStatus status{DataStoreStatus::Missing};
    std::vector<std::string> values;
};

struct HashGetResult {
    DataStoreStatus status{DataStoreStatus::Missing};
    std::optional<std::string> value;
};

struct HashGetAllResult {
    DataStoreStatus status{DataStoreStatus::Missing};
    std::vector<std::pair<std::string, std::string>> fields;
};

struct SetMembersResult {
    DataStoreStatus status{DataStoreStatus::Missing};
    std::vector<std::string> members;
};

struct SnapshotEntry {
    std::string key;
    ValueObject value;
};

class DataStore {
public:
    void configureEviction(const EvictionConfig& config);
    void set(const std::string& key, const std::string& value, long long ttlSeconds = -1);
    std::optional<std::string> get(const std::string& key);
    bool del(const std::string& key);
    bool exists(const std::string& key);
    bool expire(const std::string& key, long long ttlSeconds);
    long long ttl(const std::string& key);
    bool persist(const std::string& key);
    std::optional<long long> incrBy(const std::string& key, long long delta);
    std::optional<std::size_t> append(const std::string& key, const std::string& suffix);
    std::optional<std::size_t> strlen(const std::string& key);
    std::vector<std::optional<std::string>> mget(const std::vector<std::string>& keys);
    void mset(const std::vector<std::pair<std::string, std::string>>& entries);
    std::optional<std::size_t> lpush(const std::string& key, const std::vector<std::string>& values);
    std::optional<std::size_t> rpush(const std::string& key, const std::vector<std::string>& values);
    ListPopResult lpop(const std::string& key);
    ListPopResult rpop(const std::string& key);
    ListRangeResult lrange(const std::string& key, long long start, long long stop);
    std::optional<std::size_t> hset(const std::string& key, const std::string& field,
                                    const std::string& value);
    HashGetResult hget(const std::string& key, const std::string& field);
    std::optional<std::size_t> hdel(const std::string& key, const std::string& field);
    std::optional<bool> hexists(const std::string& key, const std::string& field);
    HashGetAllResult hgetall(const std::string& key);
    std::optional<std::size_t> sadd(const std::string& key, const std::vector<std::string>& members);
    std::optional<std::size_t> srem(const std::string& key, const std::vector<std::string>& members);
    std::optional<bool> sismember(const std::string& key, const std::string& member);
    SetMembersResult smembers(const std::string& key);
    std::optional<std::size_t> scard(const std::string& key);
    std::vector<std::string> keys(const std::string& pattern = "*");
    std::string type(const std::string& key);
    bool rename(const std::string& source, const std::string& destination);
    void flushdb();
    std::vector<SnapshotEntry> snapshot();
    void loadSnapshot(const std::vector<SnapshotEntry>& entries);
    std::size_t removeExpired();
    StoreStats stats() const;

private:
    static long long nowMillis();
    static bool parseInteger(const std::string& value, long long& parsed);
    static bool matchesPattern(const std::string& value, const std::string& pattern);
    static std::pair<std::size_t, std::size_t> normalizeRange(long long start, long long stop,
                                                              std::size_t size);
    static std::size_t estimateObjectBytes(const std::string& key, const ValueObject& object);
    bool isExpired(const ValueObject& object, long long now) const;
    bool overEvictionLimitLocked() const;
    bool evictOneLocked();
    void evictIfNeededLocked();

    mutable std::mutex mutex_;
    std::unordered_map<std::string, ValueObject> values_;
    EvictionConfig evictionConfig_;
    std::size_t evictedKeys_{0};
    std::size_t expiredKeys_{0};
};

} // namespace swiftcache
