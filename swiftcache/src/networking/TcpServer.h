#pragma once

#include <atomic>
#include <chrono>
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
#include "../core/SnapshotPersistence.h"
#include "../datastore/DataStore.h"
#include "../parser/CommandParser.h"

namespace swiftcache {

struct ClientConnectionInfo {
    std::uint64_t id{0};
    int fd{-1};
    std::size_t databaseIndex{0};
    RequestProtocol protocol{RequestProtocol::Inline};
    bool authenticated{false};
    std::string name;
    std::chrono::steady_clock::time_point connectedAt;
};

class TcpServer {
public:
    TcpServer(std::string host, std::uint16_t port, DataStore& store,
              const CommandRegistry& registry, ServerMetrics& metrics,
              AofPersistence* aof = nullptr, std::string authPassword = "",
              bool readOnly = false, SnapshotPersistence* snapshot = nullptr,
              EvictionConfig evictionConfig = {});
    TcpServer(std::string host, std::uint16_t port, std::deque<DataStore>& stores,
              const CommandRegistry& registry, ServerMetrics& metrics,
              AofPersistence* aof = nullptr, std::string authPassword = "",
              bool readOnly = false, SnapshotPersistence* snapshot = nullptr,
              EvictionConfig evictionConfig = {});

    bool start();
    void stop();

private:
    void acceptLoop();
    void handleClient(int clientFd) const;
    bool sendResponse(int clientFd, const std::string& response) const;
    bool handleAuthCommand(int clientFd, const ParsedCommand& command,
                           const std::vector<std::string>& tokens,
                           bool& authenticated) const;
    bool requireAuthenticatedClient(int clientFd, RequestProtocol protocol,
                                    bool authenticated) const;
    bool handleReadOnlyCommand(int clientFd, const ParsedCommand& command,
                               const std::vector<std::string>& tokens) const;
    bool handleConfigCommand(int clientFd, const ParsedCommand& command,
                             const std::vector<std::string>& tokens) const;
    bool handleCommandCommand(int clientFd, const ParsedCommand& command,
                              const std::vector<std::string>& tokens) const;
    bool handleClientCommand(int clientFd, const ParsedCommand& command,
                             const std::vector<std::string>& tokens) const;
    bool handlePersistenceCommand(int clientFd, const ParsedCommand& command,
                                  const std::vector<std::string>& tokens) const;
    bool rejectReadOnlyWrite(int clientFd, const ParsedCommand& command,
                             const std::vector<std::string>& tokens) const;
    bool handleSelectCommand(int clientFd, const ParsedCommand& command,
                             const std::vector<std::string>& tokens,
                             std::size_t& databaseIndex) const;
    bool handleMoveCommand(int clientFd, const ParsedCommand& command,
                           const std::vector<std::string>& tokens,
                           std::size_t databaseIndex) const;
    bool handleFlushAllCommand(int clientFd, const ParsedCommand& command,
                               const std::vector<std::string>& tokens,
                               std::size_t databaseIndex) const;
    bool handlePubSubCommand(int clientFd, const ParsedCommand& command,
                             const std::vector<std::string>& tokens) const;
    std::string subscribe(int clientFd, RequestProtocol protocol,
                          const std::vector<std::string>& channels) const;
    std::string unsubscribe(int clientFd, RequestProtocol protocol,
                            const std::vector<std::string>& channels) const;
    std::string publish(RequestProtocol protocol, const std::string& channel,
                        const std::string& message) const;
    std::uint64_t registerClient(int clientFd, bool authenticated) const;
    void unregisterClient(int clientFd) const;
    void updateClientState(int clientFd, RequestProtocol protocol, std::size_t databaseIndex,
                           bool authenticated) const;
    std::size_t clientSubscriptionCount(int clientFd) const;
    std::string formatClientInfo(const ClientConnectionInfo& client) const;
    void removeClientSubscriptions(int clientFd) const;

    std::string host_;
    std::uint16_t port_;
    DataStore* store_;
    std::deque<DataStore>* stores_;
    const CommandRegistry& registry_;
    ServerMetrics& metrics_;
    AofPersistence* aof_;
    SnapshotPersistence* snapshot_;
    EvictionConfig evictionConfig_;
    std::string authPassword_;
    CommandParser parser_;
    int serverFd_{-1};
    std::atomic<bool> running_{false};
    mutable std::atomic<bool> readOnly_{false};
    mutable std::atomic<std::uint64_t> nextClientId_{1};
    mutable std::mutex sendMutex_;
    mutable std::mutex pubsubMutex_;
    mutable std::mutex clientsMutex_;
    mutable std::unordered_map<std::string, std::unordered_map<int, RequestProtocol>> channelSubscribers_;
    mutable std::unordered_map<int, std::unordered_set<std::string>> clientSubscriptions_;
    mutable std::unordered_map<int, ClientConnectionInfo> clients_;
};

} // namespace swiftcache
