#include "ServerMetrics.h"

namespace swiftcache {

void ServerMetrics::clientConnected() {
    connectedClients_.fetch_add(1, std::memory_order_relaxed);
}

void ServerMetrics::clientDisconnected() {
    connectedClients_.fetch_sub(1, std::memory_order_relaxed);
}

void ServerMetrics::commandExecuted() {
    totalCommands_.fetch_add(1, std::memory_order_relaxed);
}

std::uint64_t ServerMetrics::connectedClients() const {
    return connectedClients_.load(std::memory_order_relaxed);
}

std::uint64_t ServerMetrics::totalCommands() const {
    return totalCommands_.load(std::memory_order_relaxed);
}

} // namespace swiftcache
