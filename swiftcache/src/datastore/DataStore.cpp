#include "DataStore.h"

#include <algorithm>
#include <chrono>
#include <exception>
#include <iterator>
#include <limits>
#include <random>

namespace swiftcache {

long long DataStore::nowMillis() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

bool DataStore::isExpired(const ValueObject& object, long long now) const {
    return object.expiresAt != -1 && object.expiresAt <= now;
}

std::size_t DataStore::estimateObjectBytes(const std::string& key, const ValueObject& object) {
    std::size_t total = key.size() + object.type.size() + object.value.size() + sizeof(ValueObject);
    for (const auto& value : object.list) {
        total += value.size() + sizeof(std::string);
    }
    for (const auto& entry : object.hash) {
        total += entry.first.size() + entry.second.size() + (sizeof(std::string) * 2);
    }
    for (const auto& member : object.set) {
        total += member.size() + sizeof(std::string);
    }
    return total;
}

bool DataStore::overEvictionLimitLocked() const {
    if (evictionConfig_.maxKeys > 0 && values_.size() > evictionConfig_.maxKeys) {
        return true;
    }

    if (evictionConfig_.maxMemoryBytes == 0) {
        return false;
    }

    std::size_t estimated = 0;
    for (const auto& entry : values_) {
        estimated += estimateObjectBytes(entry.first, entry.second);
        if (estimated > evictionConfig_.maxMemoryBytes) {
            return true;
        }
    }
    return false;
}

bool DataStore::evictOneLocked() {
    if (values_.empty() || evictionConfig_.policy == EvictionPolicy::None) {
        return false;
    }

    auto victim = values_.end();

    if (evictionConfig_.policy == EvictionPolicy::Random) {
        static thread_local std::mt19937 generator{std::random_device{}()};
        std::uniform_int_distribution<std::size_t> distribution(0, values_.size() - 1);
        victim = values_.begin();
        std::advance(victim, static_cast<long>(distribution(generator)));
    } else if (evictionConfig_.policy == EvictionPolicy::TtlPriority) {
        long long earliestExpiry = std::numeric_limits<long long>::max();
        for (auto it = values_.begin(); it != values_.end(); ++it) {
            if (it->second.expiresAt != -1 && it->second.expiresAt < earliestExpiry) {
                earliestExpiry = it->second.expiresAt;
                victim = it;
            }
        }
    } else {
        long long oldestAccess = std::numeric_limits<long long>::max();
        for (auto it = values_.begin(); it != values_.end(); ++it) {
            if (evictionConfig_.policy == EvictionPolicy::VolatileLru && it->second.expiresAt == -1) {
                continue;
            }
            const long long lastAccessed = it->second.lastAccessedAt == -1
                ? it->second.createdAt
                : it->second.lastAccessedAt;
            if (lastAccessed < oldestAccess) {
                oldestAccess = lastAccessed;
                victim = it;
            }
        }
    }

    if (victim == values_.end()) {
        return false;
    }

    values_.erase(victim);
    ++evictedKeys_;
    return true;
}

void DataStore::evictIfNeededLocked() {
    while (overEvictionLimitLocked()) {
        if (!evictOneLocked()) {
            break;
        }
    }
}

void DataStore::configureEviction(const EvictionConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    evictionConfig_ = config;
    evictIfNeededLocked();
}

bool DataStore::parseInteger(const std::string& value, long long& parsed) {
    try {
        std::size_t consumed = 0;
        parsed = std::stoll(value, &consumed);
        return consumed == value.size();
    } catch (const std::exception&) {
        return false;
    }
}

bool DataStore::matchesPattern(const std::string& value, const std::string& pattern) {
    std::size_t valueIndex = 0;
    std::size_t patternIndex = 0;
    std::size_t starIndex = std::string::npos;
    std::size_t matchIndex = 0;

    while (valueIndex < value.size()) {
        if (patternIndex < pattern.size() &&
            (pattern[patternIndex] == '?' || pattern[patternIndex] == value[valueIndex])) {
            ++valueIndex;
            ++patternIndex;
        } else if (patternIndex < pattern.size() && pattern[patternIndex] == '*') {
            starIndex = patternIndex++;
            matchIndex = valueIndex;
        } else if (starIndex != std::string::npos) {
            patternIndex = starIndex + 1;
            valueIndex = ++matchIndex;
        } else {
            return false;
        }
    }

    while (patternIndex < pattern.size() && pattern[patternIndex] == '*') {
        ++patternIndex;
    }

    return patternIndex == pattern.size();
}

std::pair<std::size_t, std::size_t> DataStore::normalizeRange(long long start, long long stop,
                                                              std::size_t size) {
    if (size == 0) {
        return {1, 0};
    }

    const long long length = static_cast<long long>(size);
    if (start < 0) {
        start = length + start;
    }
    if (stop < 0) {
        stop = length + stop;
    }

    start = std::max<long long>(start, 0);
    stop = std::min<long long>(stop, length - 1);

    if (start > stop || start >= length) {
        return {1, 0};
    }

    return {static_cast<std::size_t>(start), static_cast<std::size_t>(stop)};
}

void DataStore::set(const std::string& key, const std::string& value, long long ttlSeconds) {
    std::lock_guard<std::mutex> lock(mutex_);
    const long long createdAt = nowMillis();
    const long long expiresAt = ttlSeconds >= 0 ? createdAt + (ttlSeconds * 1000) : -1;
    values_[key] = ValueObject{value, {}, {}, {}, data_type::String, createdAt, expiresAt, createdAt};
    evictIfNeededLocked();
}

std::optional<std::string> DataStore::get(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = values_.find(key);
    if (it == values_.end()) {
        return std::nullopt;
    }

    if (isExpired(it->second, nowMillis())) {
        values_.erase(it);
        return std::nullopt;
    }

    if (it->second.type != data_type::String) {
        return std::nullopt;
    }

    it->second.lastAccessedAt = nowMillis();
    return it->second.value;
}

bool DataStore::del(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    return values_.erase(key) > 0;
}

bool DataStore::exists(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = values_.find(key);
    if (it == values_.end()) {
        return false;
    }

    if (isExpired(it->second, nowMillis())) {
        values_.erase(it);
        return false;
    }

    it->second.lastAccessedAt = nowMillis();
    return true;
}

bool DataStore::expire(const std::string& key, long long ttlSeconds) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = values_.find(key);
    if (it == values_.end()) {
        return false;
    }

    const long long now = nowMillis();
    if (isExpired(it->second, now)) {
        values_.erase(it);
        return false;
    }

    it->second.expiresAt = now + (ttlSeconds * 1000);
    return true;
}

long long DataStore::ttl(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = values_.find(key);
    if (it == values_.end()) {
        return -2;
    }

    const long long now = nowMillis();
    if (isExpired(it->second, now)) {
        values_.erase(it);
        return -2;
    }

    if (it->second.expiresAt == -1) {
        return -1;
    }

    it->second.lastAccessedAt = now;
    const long long remainingMillis = it->second.expiresAt - now;
    return (remainingMillis + 999) / 1000;
}

bool DataStore::persist(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = values_.find(key);
    if (it == values_.end()) {
        return false;
    }

    if (isExpired(it->second, nowMillis())) {
        values_.erase(it);
        return false;
    }

    if (it->second.expiresAt == -1) {
        return false;
    }

    it->second.expiresAt = -1;
    it->second.lastAccessedAt = nowMillis();
    evictIfNeededLocked();
    return true;
}

std::optional<long long> DataStore::incrBy(const std::string& key, long long delta) {
    std::lock_guard<std::mutex> lock(mutex_);
    const long long now = nowMillis();
    auto it = values_.find(key);

    if (it != values_.end() && isExpired(it->second, now)) {
        values_.erase(it);
        it = values_.end();
    }

    long long current = 0;
    long long expiresAt = -1;
    if (it != values_.end()) {
        if (it->second.type != data_type::String) {
            return std::nullopt;
        }
        if (!parseInteger(it->second.value, current)) {
            return std::nullopt;
        }
        expiresAt = it->second.expiresAt;
    }

    const long long updated = current + delta;
    const long long createdAt = it == values_.end() ? now : it->second.createdAt;
    values_[key] = ValueObject{std::to_string(updated), {}, {}, {}, data_type::String, createdAt, expiresAt, now};
    evictIfNeededLocked();
    return updated;
}

std::optional<std::size_t> DataStore::append(const std::string& key, const std::string& suffix) {
    std::lock_guard<std::mutex> lock(mutex_);
    const long long now = nowMillis();
    auto it = values_.find(key);

    if (it != values_.end() && isExpired(it->second, now)) {
        values_.erase(it);
        it = values_.end();
    }

    if (it == values_.end()) {
        values_[key] = ValueObject{suffix, {}, {}, {}, data_type::String, now, -1, now};
        evictIfNeededLocked();
        return suffix.size();
    }

    if (it->second.type != data_type::String) {
        return std::nullopt;
    }

    it->second.value += suffix;
    it->second.lastAccessedAt = now;
    evictIfNeededLocked();
    return it->second.value.size();
}

std::optional<std::size_t> DataStore::strlen(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = values_.find(key);
    if (it == values_.end()) {
        return std::nullopt;
    }

    if (isExpired(it->second, nowMillis())) {
        values_.erase(it);
        return std::nullopt;
    }

    if (it->second.type != data_type::String) {
        return std::nullopt;
    }

    it->second.lastAccessedAt = nowMillis();
    return it->second.value.size();
}

std::vector<std::optional<std::string>> DataStore::mget(const std::vector<std::string>& keys) {
    std::lock_guard<std::mutex> lock(mutex_);
    const long long now = nowMillis();
    std::vector<std::optional<std::string>> values;
    values.reserve(keys.size());

    for (const auto& key : keys) {
        const auto it = values_.find(key);
        if (it == values_.end()) {
            values.push_back(std::nullopt);
            continue;
        }

        if (isExpired(it->second, now)) {
            values_.erase(it);
            values.push_back(std::nullopt);
            continue;
        }

        if (it->second.type == data_type::String) {
            it->second.lastAccessedAt = now;
            values.push_back(it->second.value);
        } else {
            values.push_back(std::nullopt);
        }
    }

    return values;
}

void DataStore::mset(const std::vector<std::pair<std::string, std::string>>& entries) {
    std::lock_guard<std::mutex> lock(mutex_);
    const long long now = nowMillis();

    for (const auto& entry : entries) {
        values_[entry.first] = ValueObject{entry.second, {}, {}, {}, data_type::String, now, -1, now};
    }
    evictIfNeededLocked();
}

std::optional<std::size_t> DataStore::lpush(const std::string& key,
                                            const std::vector<std::string>& values) {
    std::lock_guard<std::mutex> lock(mutex_);
    const long long now = nowMillis();
    auto it = values_.find(key);

    if (it != values_.end() && isExpired(it->second, now)) {
        values_.erase(it);
        it = values_.end();
    }

    if (it == values_.end()) {
        ValueObject object{"", {}, {}, {}, data_type::List, now, -1};
        for (const auto& value : values) {
            object.list.push_front(value);
        }
        const std::size_t size = object.list.size();
        object.lastAccessedAt = now;
        values_[key] = std::move(object);
        evictIfNeededLocked();
        return size;
    }

    if (it->second.type != data_type::List) {
        return std::nullopt;
    }

    for (const auto& value : values) {
        it->second.list.push_front(value);
    }
    it->second.lastAccessedAt = now;
    evictIfNeededLocked();
    return it->second.list.size();
}

std::optional<std::size_t> DataStore::rpush(const std::string& key,
                                            const std::vector<std::string>& values) {
    std::lock_guard<std::mutex> lock(mutex_);
    const long long now = nowMillis();
    auto it = values_.find(key);

    if (it != values_.end() && isExpired(it->second, now)) {
        values_.erase(it);
        it = values_.end();
    }

    if (it == values_.end()) {
        ValueObject object{"", {}, {}, {}, data_type::List, now, -1};
        for (const auto& value : values) {
            object.list.push_back(value);
        }
        const std::size_t size = object.list.size();
        object.lastAccessedAt = now;
        values_[key] = std::move(object);
        evictIfNeededLocked();
        return size;
    }

    if (it->second.type != data_type::List) {
        return std::nullopt;
    }

    for (const auto& value : values) {
        it->second.list.push_back(value);
    }
    it->second.lastAccessedAt = now;
    evictIfNeededLocked();
    return it->second.list.size();
}

ListPopResult DataStore::lpop(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    const long long now = nowMillis();
    const auto it = values_.find(key);
    if (it == values_.end()) {
        return {DataStoreStatus::Missing, ""};
    }

    if (isExpired(it->second, now)) {
        values_.erase(it);
        return {DataStoreStatus::Missing, ""};
    }

    if (it->second.type != data_type::List) {
        return {DataStoreStatus::WrongType, ""};
    }

    if (it->second.list.empty()) {
        return {DataStoreStatus::Missing, ""};
    }

    const std::string value = it->second.list.front();
    it->second.list.pop_front();
    if (it->second.list.empty()) {
        values_.erase(it);
    } else {
        it->second.lastAccessedAt = now;
    }
    return {DataStoreStatus::Ok, value};
}

ListPopResult DataStore::rpop(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    const long long now = nowMillis();
    const auto it = values_.find(key);
    if (it == values_.end()) {
        return {DataStoreStatus::Missing, ""};
    }

    if (isExpired(it->second, now)) {
        values_.erase(it);
        return {DataStoreStatus::Missing, ""};
    }

    if (it->second.type != data_type::List) {
        return {DataStoreStatus::WrongType, ""};
    }

    if (it->second.list.empty()) {
        return {DataStoreStatus::Missing, ""};
    }

    const std::string value = it->second.list.back();
    it->second.list.pop_back();
    if (it->second.list.empty()) {
        values_.erase(it);
    } else {
        it->second.lastAccessedAt = now;
    }
    return {DataStoreStatus::Ok, value};
}

ListRangeResult DataStore::lrange(const std::string& key, long long start, long long stop) {
    std::lock_guard<std::mutex> lock(mutex_);
    const long long now = nowMillis();
    const auto it = values_.find(key);
    if (it == values_.end()) {
        return {DataStoreStatus::Missing, {}};
    }

    if (isExpired(it->second, now)) {
        values_.erase(it);
        return {DataStoreStatus::Missing, {}};
    }

    if (it->second.type != data_type::List) {
        return {DataStoreStatus::WrongType, {}};
    }

    const auto range = normalizeRange(start, stop, it->second.list.size());
    std::vector<std::string> result;
    if (range.first > range.second) {
        return {DataStoreStatus::Ok, result};
    }

    result.reserve(range.second - range.first + 1);
    for (std::size_t i = range.first; i <= range.second; ++i) {
        result.push_back(it->second.list[i]);
    }

    it->second.lastAccessedAt = now;
    return {DataStoreStatus::Ok, result};
}

std::optional<std::size_t> DataStore::hset(const std::string& key, const std::string& field,
                                           const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    const long long now = nowMillis();
    auto it = values_.find(key);

    if (it != values_.end() && isExpired(it->second, now)) {
        values_.erase(it);
        it = values_.end();
    }

    if (it == values_.end()) {
        ValueObject object{"", {}, {}, {}, data_type::Hash, now, -1};
        object.hash[field] = value;
        object.lastAccessedAt = now;
        values_[key] = std::move(object);
        evictIfNeededLocked();
        return 1;
    }

    if (it->second.type != data_type::Hash) {
        return std::nullopt;
    }

    const bool isNewField = it->second.hash.find(field) == it->second.hash.end();
    it->second.hash[field] = value;
    it->second.lastAccessedAt = now;
    evictIfNeededLocked();
    return isNewField ? 1 : 0;
}

HashGetResult DataStore::hget(const std::string& key, const std::string& field) {
    std::lock_guard<std::mutex> lock(mutex_);
    const long long now = nowMillis();
    const auto it = values_.find(key);
    if (it == values_.end()) {
        return {DataStoreStatus::Missing, std::nullopt};
    }

    if (isExpired(it->second, now)) {
        values_.erase(it);
        return {DataStoreStatus::Missing, std::nullopt};
    }

    if (it->second.type != data_type::Hash) {
        return {DataStoreStatus::WrongType, std::nullopt};
    }

    const auto fieldIt = it->second.hash.find(field);
    if (fieldIt == it->second.hash.end()) {
        return {DataStoreStatus::Missing, std::nullopt};
    }

    it->second.lastAccessedAt = now;
    return {DataStoreStatus::Ok, fieldIt->second};
}

std::optional<std::size_t> DataStore::hdel(const std::string& key, const std::string& field) {
    std::lock_guard<std::mutex> lock(mutex_);
    const long long now = nowMillis();
    const auto it = values_.find(key);
    if (it == values_.end()) {
        return 0;
    }

    if (isExpired(it->second, now)) {
        values_.erase(it);
        return 0;
    }

    if (it->second.type != data_type::Hash) {
        return std::nullopt;
    }

    const std::size_t removed = it->second.hash.erase(field);
    if (it->second.hash.empty()) {
        values_.erase(it);
    } else {
        it->second.lastAccessedAt = now;
    }

    return removed;
}

std::optional<bool> DataStore::hexists(const std::string& key, const std::string& field) {
    std::lock_guard<std::mutex> lock(mutex_);
    const long long now = nowMillis();
    const auto it = values_.find(key);
    if (it == values_.end()) {
        return false;
    }

    if (isExpired(it->second, now)) {
        values_.erase(it);
        return false;
    }

    if (it->second.type != data_type::Hash) {
        return std::nullopt;
    }

    it->second.lastAccessedAt = now;
    return it->second.hash.find(field) != it->second.hash.end();
}

HashGetAllResult DataStore::hgetall(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    const long long now = nowMillis();
    const auto it = values_.find(key);
    if (it == values_.end()) {
        return {DataStoreStatus::Missing, {}};
    }

    if (isExpired(it->second, now)) {
        values_.erase(it);
        return {DataStoreStatus::Missing, {}};
    }

    if (it->second.type != data_type::Hash) {
        return {DataStoreStatus::WrongType, {}};
    }

    std::vector<std::pair<std::string, std::string>> fields;
    fields.reserve(it->second.hash.size());
    for (const auto& entry : it->second.hash) {
        fields.push_back(entry);
    }

    std::sort(fields.begin(), fields.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.first < rhs.first;
    });

    it->second.lastAccessedAt = now;
    return {DataStoreStatus::Ok, fields};
}

std::optional<std::size_t> DataStore::sadd(const std::string& key,
                                           const std::vector<std::string>& members) {
    std::lock_guard<std::mutex> lock(mutex_);
    const long long now = nowMillis();
    auto it = values_.find(key);

    if (it != values_.end() && isExpired(it->second, now)) {
        values_.erase(it);
        it = values_.end();
    }

    if (it == values_.end()) {
        ValueObject object{"", {}, {}, {}, data_type::Set, now, -1};
        std::size_t added = 0;
        for (const auto& member : members) {
            added += object.set.insert(member).second ? 1 : 0;
        }
        object.lastAccessedAt = now;
        values_[key] = std::move(object);
        evictIfNeededLocked();
        return added;
    }

    if (it->second.type != data_type::Set) {
        return std::nullopt;
    }

    std::size_t added = 0;
    for (const auto& member : members) {
        added += it->second.set.insert(member).second ? 1 : 0;
    }
    it->second.lastAccessedAt = now;
    evictIfNeededLocked();
    return added;
}

std::optional<std::size_t> DataStore::srem(const std::string& key,
                                           const std::vector<std::string>& members) {
    std::lock_guard<std::mutex> lock(mutex_);
    const long long now = nowMillis();
    const auto it = values_.find(key);
    if (it == values_.end()) {
        return 0;
    }

    if (isExpired(it->second, now)) {
        values_.erase(it);
        return 0;
    }

    if (it->second.type != data_type::Set) {
        return std::nullopt;
    }

    std::size_t removed = 0;
    for (const auto& member : members) {
        removed += it->second.set.erase(member);
    }
    if (it->second.set.empty()) {
        values_.erase(it);
    } else {
        it->second.lastAccessedAt = now;
    }

    return removed;
}

std::optional<bool> DataStore::sismember(const std::string& key, const std::string& member) {
    std::lock_guard<std::mutex> lock(mutex_);
    const long long now = nowMillis();
    const auto it = values_.find(key);
    if (it == values_.end()) {
        return false;
    }

    if (isExpired(it->second, now)) {
        values_.erase(it);
        return false;
    }

    if (it->second.type != data_type::Set) {
        return std::nullopt;
    }

    it->second.lastAccessedAt = now;
    return it->second.set.find(member) != it->second.set.end();
}

SetMembersResult DataStore::smembers(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    const long long now = nowMillis();
    const auto it = values_.find(key);
    if (it == values_.end()) {
        return {DataStoreStatus::Missing, {}};
    }

    if (isExpired(it->second, now)) {
        values_.erase(it);
        return {DataStoreStatus::Missing, {}};
    }

    if (it->second.type != data_type::Set) {
        return {DataStoreStatus::WrongType, {}};
    }

    std::vector<std::string> members(it->second.set.begin(), it->second.set.end());
    std::sort(members.begin(), members.end());
    it->second.lastAccessedAt = now;
    return {DataStoreStatus::Ok, members};
}

std::optional<std::size_t> DataStore::scard(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    const long long now = nowMillis();
    const auto it = values_.find(key);
    if (it == values_.end()) {
        return 0;
    }

    if (isExpired(it->second, now)) {
        values_.erase(it);
        return 0;
    }

    if (it->second.type != data_type::Set) {
        return std::nullopt;
    }

    it->second.lastAccessedAt = now;
    return it->second.set.size();
}

std::vector<std::string> DataStore::keys(const std::string& pattern) {
    removeExpired();

    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> result;
    for (const auto& entry : values_) {
        if (matchesPattern(entry.first, pattern)) {
            result.push_back(entry.first);
        }
    }

    std::sort(result.begin(), result.end());
    return result;
}

std::size_t DataStore::dbsize() {
    removeExpired();

    std::lock_guard<std::mutex> lock(mutex_);
    return values_.size();
}

std::optional<std::size_t> DataStore::memoryUsage(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = values_.find(key);
    if (it == values_.end()) {
        return std::nullopt;
    }

    const long long now = nowMillis();
    if (isExpired(it->second, now)) {
        values_.erase(it);
        return std::nullopt;
    }

    it->second.lastAccessedAt = now;
    return estimateObjectBytes(it->first, it->second);
}

std::string DataStore::type(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = values_.find(key);
    if (it == values_.end()) {
        return "none";
    }

    if (isExpired(it->second, nowMillis())) {
        values_.erase(it);
        return "none";
    }

    it->second.lastAccessedAt = nowMillis();
    return it->second.type;
}

bool DataStore::rename(const std::string& source, const std::string& destination) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = values_.find(source);
    if (it == values_.end()) {
        return false;
    }

    if (isExpired(it->second, nowMillis())) {
        values_.erase(it);
        return false;
    }

    if (source == destination) {
        return true;
    }

    values_[destination] = std::move(it->second);
    values_[destination].lastAccessedAt = nowMillis();
    values_.erase(it);
    evictIfNeededLocked();
    return true;
}

bool DataStore::moveKeyTo(const std::string& key, DataStore& destination) {
    if (this == &destination) {
        return false;
    }

    std::scoped_lock lock(mutex_, destination.mutex_);
    const long long now = nowMillis();

    auto sourceIt = values_.find(key);
    if (sourceIt == values_.end()) {
        return false;
    }
    if (isExpired(sourceIt->second, now)) {
        values_.erase(sourceIt);
        return false;
    }

    auto destinationIt = destination.values_.find(key);
    if (destinationIt != destination.values_.end() &&
        destination.isExpired(destinationIt->second, now)) {
        destination.values_.erase(destinationIt);
        destinationIt = destination.values_.end();
    }
    if (destinationIt != destination.values_.end()) {
        return false;
    }

    sourceIt->second.lastAccessedAt = now;
    destination.values_[key] = std::move(sourceIt->second);
    values_.erase(sourceIt);
    destination.evictIfNeededLocked();
    return true;
}

void DataStore::flushdb() {
    std::lock_guard<std::mutex> lock(mutex_);
    values_.clear();
}

std::vector<SnapshotEntry> DataStore::snapshot() {
    std::lock_guard<std::mutex> lock(mutex_);
    const long long now = nowMillis();
    std::vector<SnapshotEntry> entries;

    for (auto it = values_.begin(); it != values_.end();) {
        if (isExpired(it->second, now)) {
            it = values_.erase(it);
            continue;
        }

        entries.push_back(SnapshotEntry{it->first, it->second});
        ++it;
    }

    std::sort(entries.begin(), entries.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.key < rhs.key;
    });

    return entries;
}

void DataStore::loadSnapshot(const std::vector<SnapshotEntry>& entries) {
    std::lock_guard<std::mutex> lock(mutex_);
    const long long now = nowMillis();
    values_.clear();

    for (const auto& entry : entries) {
        if (isExpired(entry.value, now)) {
            continue;
        }
        auto value = entry.value;
        if (value.lastAccessedAt == -1) {
            value.lastAccessedAt = now;
        }
        values_[entry.key] = std::move(value);
    }
    evictIfNeededLocked();
}

std::size_t DataStore::removeExpired() {
    std::lock_guard<std::mutex> lock(mutex_);
    const long long now = nowMillis();
    std::size_t removed = 0;

    for (auto it = values_.begin(); it != values_.end();) {
        if (isExpired(it->second, now)) {
            it = values_.erase(it);
            ++removed;
            ++expiredKeys_;
        } else {
            ++it;
        }
    }

    return removed;
}

StoreStats DataStore::stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::size_t estimatedBytes = 0;
    for (const auto& entry : values_) {
        estimatedBytes += estimateObjectBytes(entry.first, entry.second);
    }
    return StoreStats{values_.size(), estimatedBytes, evictedKeys_, expiredKeys_};
}

} // namespace swiftcache
