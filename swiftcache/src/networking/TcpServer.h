#pragma once

#include <atomic>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../core/AofPersistence.h"
#include "../core/CommandRegistry.h"
#include "../core/ServerMetrics.h"
#include "../datastore/DataStore.h"
#include "../parser/CommandParser.h"

namespace swiftcache {

class TcpServer {
public:
    TcpServer(std::string host, std::uint16_t port, DataStore& store,
              const CommandRegistry& registry, ServerMetrics& metrics,
              AofPersistence* aof = nullptr);
    TcpServer(std::string host, std::uint16_t port, std::deque<DataStore>& stores,
              const CommandRegistry& registry, ServerMetrics& metrics,
              AofPersistence* aof = nullptr);

    bool start();
    void stop();

private:
    void acceptLoop();
    void handleClient(int clientFd) const;
    bool sendResponse(int clientFd, const std::string& response) const;
    bool handleSelectCommand(int clientFd, const ParsedCommand& command,
                             const std::vector<std::string>& tokens,
                             std::size_t& databaseIndex) const;
    bool handlePubSubCommand(int clientFd, const ParsedCommand& command,
                             const std::vector<std::string>& tokens) const;
    std::string subscribe(int clientFd, RequestProtocol protocol,
                          const std::vector<std::string>& channels) const;
    std::string unsubscribe(int clientFd, RequestProtocol protocol,
                            const std::vector<std::string>& channels) const;
    std::string publish(RequestProtocol protocol, const std::string& channel,
                        const std::string& message) const;
    void removeClientSubscriptions(int clientFd) const;

    std::string host_;
    std::uint16_t port_;
    DataStore* store_;
    std::deque<DataStore>* stores_;
    const CommandRegistry& registry_;
    ServerMetrics& metrics_;
    AofPersistence* aof_;
    CommandParser parser_;
    int serverFd_{-1};
    std::atomic<bool> running_{false};
    mutable std::mutex sendMutex_;
    mutable std::mutex pubsubMutex_;
    mutable std::unordered_map<std::string, std::unordered_map<int, RequestProtocol>> channelSubscribers_;
    mutable std::unordered_map<int, std::unordered_set<std::string>> clientSubscriptions_;
};

} // namespace swiftcache
