#include "DataStore.h"

#include <algorithm>
#include <chrono>
#include <exception>

namespace swiftcache {

long long DataStore::nowMillis() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

bool DataStore::isExpired(const ValueObject& object, long long now) const {
    return object.expiresAt != -1 && object.expiresAt <= now;
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
    values_[key] = ValueObject{value, {}, data_type::String, createdAt, expiresAt};
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
    values_[key] = ValueObject{std::to_string(updated), {}, data_type::String, createdAt, expiresAt};
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
        values_[key] = ValueObject{suffix, {}, data_type::String, now, -1};
        return suffix.size();
    }

    if (it->second.type != data_type::String) {
        return std::nullopt;
    }

    it->second.value += suffix;
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

        values.push_back(it->second.type == data_type::String ? std::optional<std::string>(it->second.value)
                                                              : std::nullopt);
    }

    return values;
}

void DataStore::mset(const std::vector<std::pair<std::string, std::string>>& entries) {
    std::lock_guard<std::mutex> lock(mutex_);
    const long long now = nowMillis();

    for (const auto& entry : entries) {
        values_[entry.first] = ValueObject{entry.second, {}, data_type::String, now, -1};
    }
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
        ValueObject object{"", {}, data_type::List, now, -1};
        for (const auto& value : values) {
            object.list.push_front(value);
        }
        const std::size_t size = object.list.size();
        values_[key] = std::move(object);
        return size;
    }

    if (it->second.type != data_type::List) {
        return std::nullopt;
    }

    for (const auto& value : values) {
        it->second.list.push_front(value);
    }
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
        ValueObject object{"", {}, data_type::List, now, -1};
        for (const auto& value : values) {
            object.list.push_back(value);
        }
        const std::size_t size = object.list.size();
        values_[key] = std::move(object);
        return size;
    }

    if (it->second.type != data_type::List) {
        return std::nullopt;
    }

    for (const auto& value : values) {
        it->second.list.push_back(value);
    }
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

    return {DataStoreStatus::Ok, result};
}

std::size_t DataStore::removeExpired() {
    std::lock_guard<std::mutex> lock(mutex_);
    const long long now = nowMillis();
    std::size_t removed = 0;

    for (auto it = values_.begin(); it != values_.end();) {
        if (isExpired(it->second, now)) {
            it = values_.erase(it);
            ++removed;
        } else {
            ++it;
        }
    }

    return removed;
}

StoreStats DataStore::stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return StoreStats{values_.size()};
}

} // namespace swiftcache
