#pragma once

#include <atomic>
#include <cstdint>

namespace swiftcache {

class ServerMetrics {
public:
    void clientConnected();
    void clientDisconnected();
    void commandExecuted();

    std::uint64_t connectedClients() const;
    std::uint64_t totalCommands() const;

private:
    std::atomic<std::uint64_t> connectedClients_{0};
    std::atomic<std::uint64_t> totalCommands_{0};
};

} // namespace swiftcache
