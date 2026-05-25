#include "ServerMetrics.h"

#include <algorithm>
#include <chrono>

namespace swiftcache {

void ServerMetrics::clientConnected() {
    const auto connected = connectedClients_.fetch_add(1, std::memory_order_relaxed) + 1;
    auto peak = peakConnectedClients_.load(std::memory_order_relaxed);
    while (connected > peak &&
           !peakConnectedClients_.compare_exchange_weak(peak, connected, std::memory_order_relaxed)) {
    }
}

void ServerMetrics::clientDisconnected() {
    connectedClients_.fetch_sub(1, std::memory_order_relaxed);
}

void ServerMetrics::commandExecuted(const std::string& commandName) {
    totalCommands_.fetch_add(1, std::memory_order_relaxed);
    std::lock_guard<std::mutex> lock(commandCountsMutex_);
    ++commandCounts_[commandName];
}

void ServerMetrics::commandRejected() {
    rejectedCommands_.fetch_add(1, std::memory_order_relaxed);
}

void ServerMetrics::snapshotSaved() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(now).count();
    lastSnapshotUnixSeconds_.store(static_cast<std::uint64_t>(seconds), std::memory_order_relaxed);
}

std::uint64_t ServerMetrics::connectedClients() const {
    return connectedClients_.load(std::memory_order_relaxed);
}

std::uint64_t ServerMetrics::peakConnectedClients() const {
    return peakConnectedClients_.load(std::memory_order_relaxed);
}

std::uint64_t ServerMetrics::totalCommands() const {
    return totalCommands_.load(std::memory_order_relaxed);
}

std::uint64_t ServerMetrics::rejectedCommands() const {
    return rejectedCommands_.load(std::memory_order_relaxed);
}

std::uint64_t ServerMetrics::lastSnapshotUnixSeconds() const {
    return lastSnapshotUnixSeconds_.load(std::memory_order_relaxed);
}

std::vector<std::pair<std::string, std::uint64_t>> ServerMetrics::commandCounts() const {
    std::lock_guard<std::mutex> lock(commandCountsMutex_);
    std::vector<std::pair<std::string, std::uint64_t>> counts(commandCounts_.begin(),
                                                             commandCounts_.end());
    std::sort(counts.begin(), counts.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.first < rhs.first;
    });
    return counts;
}

} // namespace swiftcache
