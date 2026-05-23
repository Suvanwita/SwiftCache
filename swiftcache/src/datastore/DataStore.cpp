#include "DataStore.h"

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

void DataStore::set(const std::string& key, const std::string& value, long long ttlSeconds) {
    std::lock_guard<std::mutex> lock(mutex_);
    const long long createdAt = nowMillis();
    const long long expiresAt = ttlSeconds >= 0 ? createdAt + (ttlSeconds * 1000) : -1;
    values_[key] = ValueObject{value, "string", createdAt, expiresAt};
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
        if (!parseInteger(it->second.value, current)) {
            return std::nullopt;
        }
        expiresAt = it->second.expiresAt;
    }

    const long long updated = current + delta;
    const long long createdAt = it == values_.end() ? now : it->second.createdAt;
    values_[key] = ValueObject{std::to_string(updated), "string", createdAt, expiresAt};
    return updated;
}

std::size_t DataStore::append(const std::string& key, const std::string& suffix) {
    std::lock_guard<std::mutex> lock(mutex_);
    const long long now = nowMillis();
    auto it = values_.find(key);

    if (it != values_.end() && isExpired(it->second, now)) {
        values_.erase(it);
        it = values_.end();
    }

    if (it == values_.end()) {
        values_[key] = ValueObject{suffix, "string", now, -1};
        return suffix.size();
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

        values.push_back(it->second.value);
    }

    return values;
}

void DataStore::mset(const std::vector<std::pair<std::string, std::string>>& entries) {
    std::lock_guard<std::mutex> lock(mutex_);
    const long long now = nowMillis();

    for (const auto& entry : entries) {
        values_[entry.first] = ValueObject{entry.second, "string", now, -1};
    }
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
