#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace swiftcache {

class ServerMetrics {
public:
    void clientConnected();
    void clientDisconnected();
    void commandExecuted(const std::string& commandName);
    void commandRejected();
    void snapshotSaved();

    std::uint64_t connectedClients() const;
    std::uint64_t peakConnectedClients() const;
    std::uint64_t totalCommands() const;
    std::uint64_t rejectedCommands() const;
    std::uint64_t lastSnapshotUnixSeconds() const;
    std::vector<std::pair<std::string, std::uint64_t>> commandCounts() const;

private:
    std::atomic<std::uint64_t> connectedClients_{0};
    std::atomic<std::uint64_t> peakConnectedClients_{0};
    std::atomic<std::uint64_t> totalCommands_{0};
    std::atomic<std::uint64_t> rejectedCommands_{0};
    std::atomic<std::uint64_t> lastSnapshotUnixSeconds_{0};
    mutable std::mutex commandCountsMutex_;
    std::unordered_map<std::string, std::uint64_t> commandCounts_;
};

} // namespace swiftcache
