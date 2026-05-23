#include "DataStore.h"

#include <chrono>

namespace swiftcache {

long long DataStore::nowMillis() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

bool DataStore::isExpired(const ValueObject& object, long long now) const {
    return object.expiresAt != -1 && object.expiresAt <= now;
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
