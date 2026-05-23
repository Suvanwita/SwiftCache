#include "DataStore.h"

#include <chrono>

namespace swiftcache {

void DataStore::set(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const auto createdAt = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    values_[key] = ValueObject{value, "string", createdAt};
}

std::optional<std::string> DataStore::get(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = values_.find(key);
    if (it == values_.end()) {
        return std::nullopt;
    }
    return it->second.value;
}

bool DataStore::del(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    return values_.erase(key) > 0;
}

bool DataStore::exists(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return values_.find(key) != values_.end();
}

StoreStats DataStore::stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return StoreStats{values_.size()};
}

} // namespace swiftcache
