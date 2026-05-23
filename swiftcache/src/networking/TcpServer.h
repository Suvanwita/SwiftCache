#pragma once

#include <atomic>
#include <cstdint>

#include "../core/CommandRegistry.h"
#include "../core/ServerMetrics.h"
#include "../datastore/DataStore.h"
#include "../parser/CommandParser.h"

namespace swiftcache {

class TcpServer {
public:
    TcpServer(std::uint16_t port, DataStore& store, const CommandRegistry& registry, ServerMetrics& metrics);

    bool start();
    void stop();

private:
    void acceptLoop();
    void handleClient(int clientFd) const;
    bool sendResponse(int clientFd, const std::string& response) const;

    std::uint16_t port_;
    DataStore& store_;
    const CommandRegistry& registry_;
    ServerMetrics& metrics_;
    CommandParser parser_;
    int serverFd_{-1};
    std::atomic<bool> running_{false};
};

} // namespace swiftcache
