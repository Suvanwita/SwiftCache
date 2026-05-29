#include "TcpServer.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cctype>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <sys/select.h>
#include <thread>
#include <unordered_set>
#include <unistd.h>
#include <utility>

#include "../parser/CommandParser.h"
#include "../utils/StringUtils.h"

namespace swiftcache {
namespace {

constexpr int kBacklog = 128;
constexpr std::size_t kBufferSize = 4096;

void closeIfOpen(int fd) {
    if (fd >= 0) {
        close(fd);
    }
}

std::string trimTrailingNewline(std::string value) {
    while (!value.empty() && (value.back() == '\n' || value.back() == '\r')) {
        value.pop_back();
    }
    return value;
}

bool isIntegerLine(const std::string& value) {
    if (value.empty()) {
        return false;
    }
    std::size_t index = value[0] == '-' ? 1 : 0;
    if (index == value.size()) {
        return false;
    }
    for (; index < value.size(); ++index) {
        if (!std::isdigit(static_cast<unsigned char>(value[index]))) {
            return false;
        }
    }
    return true;
}

std::string bulkString(const std::string& value) {
    return "$" + std::to_string(value.size()) + "\r\n" + value + "\r\n";
}

std::string formatRespArray(const std::vector<std::string>& values) {
    std::ostringstream out;
    out << "*" << values.size() << "\r\n";
    for (const auto& value : values) {
        out << bulkString(value);
    }
    return out.str();
}

std::string formatRespResponse(const std::string& response) {
    const std::string trimmed = trimTrailingNewline(response);

    if (trimmed.rfind("ERR ", 0) == 0 || trimmed.rfind("NOAUTH ", 0) == 0) {
        return "-" + trimmed + "\r\n";
    }
    if (trimmed == "OK" || trimmed == "PONG") {
        return "+" + trimmed + "\r\n";
    }
    if (trimmed == "(nil)") {
        return "$-1\r\n";
    }
    if (isIntegerLine(trimmed)) {
        return ":" + trimmed + "\r\n";
    }
    if (!trimmed.empty() && trimmed.front() == '{') {
        return bulkString(trimmed);
    }

    std::vector<std::string> lines;
    std::istringstream input(trimmed);
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(line);
    }

    if (lines.size() <= 1) {
        return bulkString(trimmed);
    }

    std::ostringstream out;
    out << "*" << lines.size() << "\r\n";
    for (const auto& item : lines) {
        if (item == "(nil)") {
            out << "$-1\r\n";
        } else if (isIntegerLine(item)) {
            out << ":" << item << "\r\n";
        } else {
            out << bulkString(item);
        }
    }
    return out.str();
}

bool isSuccessfulResponse(const std::string& response) {
    return response.rfind("ERR ", 0) != 0;
}

bool isRejectedResponse(const std::string& response) {
    return response.rfind("ERR ", 0) == 0;
}

std::string formatPubSubTuple(RequestProtocol protocol, const std::string& kind,
                              const std::string& channel, std::size_t count) {
    if (protocol == RequestProtocol::Resp) {
        std::ostringstream out;
        out << "*3\r\n" << bulkString(kind) << bulkString(channel) << ":" << count << "\r\n";
        return out.str();
    }

    return kind + "\n" + channel + "\n" + std::to_string(count) + "\n";
}

std::string formatPubSubMessage(RequestProtocol protocol, const std::string& channel,
                                const std::string& message) {
    if (protocol == RequestProtocol::Resp) {
        return formatRespArray({"message", channel, message});
    }

    return "message\n" + channel + "\n" + message + "\n";
}

bool resolveHost(const std::string& host, in_addr& address) {
    if (host == "localhost") {
        address.s_addr = htonl(INADDR_LOOPBACK);
        return true;
    }
    if (host == "*") {
        address.s_addr = htonl(INADDR_ANY);
        return true;
    }
    return inet_pton(AF_INET, host.c_str(), &address) == 1;
}

bool parseDatabaseIndex(const std::string& raw, std::size_t databaseCount,
                        std::size_t& databaseIndex) {
    if (raw.empty() || raw.front() == '-') {
        return false;
    }

    try {
        std::size_t consumed = 0;
        const auto parsed = std::stoull(raw, &consumed);
        if (consumed != raw.size() || parsed >= databaseCount) {
            return false;
        }
        databaseIndex = static_cast<std::size_t>(parsed);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool isWriteCommand(const std::vector<std::string>& tokens) {
    static const std::unordered_set<std::string> kWriteCommands{
        "SET", "MSET", "DEL", "EXPIRE", "PERSIST",
        "INCR", "DECR", "APPEND",
        "LPUSH", "RPUSH", "LPOP", "RPOP",
        "HSET", "HDEL",
        "SADD", "SREM",
        "RENAME", "FLUSHDB",
        "PUBLISH"
    };

    if (tokens.empty()) {
        return false;
    }
    return kWriteCommands.find(toUpper(tokens.front())) != kWriteCommands.end();
}

} // namespace

TcpServer::TcpServer(std::string host, std::uint16_t port, DataStore& store,
                     const CommandRegistry& registry, ServerMetrics& metrics,
                     AofPersistence* aof, std::string authPassword, bool readOnly)
    : host_(std::move(host)), port_(port), store_(&store), stores_(nullptr), registry_(registry),
      metrics_(metrics), aof_(aof), authPassword_(std::move(authPassword)), readOnly_(readOnly) {}

TcpServer::TcpServer(std::string host, std::uint16_t port, std::deque<DataStore>& stores,
                     const CommandRegistry& registry, ServerMetrics& metrics,
                     AofPersistence* aof, std::string authPassword, bool readOnly)
    : host_(std::move(host)), port_(port), store_(nullptr), stores_(&stores), registry_(registry),
      metrics_(metrics), aof_(aof), authPassword_(std::move(authPassword)), readOnly_(readOnly) {}

bool TcpServer::start() {
    serverFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (serverFd_ < 0) {
        std::cerr << "socket failed: " << std::strerror(errno) << "\n";
        return false;
    }

    int enabled = 1;
    if (setsockopt(serverFd_, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled)) < 0) {
        std::cerr << "setsockopt failed: " << std::strerror(errno) << "\n";
        closeIfOpen(serverFd_);
        serverFd_ = -1;
        return false;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    if (!resolveHost(host_, address.sin_addr)) {
        std::cerr << "invalid host: " << host_ << "\n";
        closeIfOpen(serverFd_);
        serverFd_ = -1;
        return false;
    }
    address.sin_port = htons(port_);

    if (bind(serverFd_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        std::cerr << "bind failed: " << std::strerror(errno) << "\n";
        closeIfOpen(serverFd_);
        serverFd_ = -1;
        return false;
    }

    if (listen(serverFd_, kBacklog) < 0) {
        std::cerr << "listen failed: " << std::strerror(errno) << "\n";
        closeIfOpen(serverFd_);
        serverFd_ = -1;
        return false;
    }

    running_ = true;
    std::cout << "SwiftCache listening on " << host_ << ":" << port_ << "\n";
    acceptLoop();
    return true;
}

void TcpServer::stop() {
    running_ = false;
    closeIfOpen(serverFd_);
    serverFd_ = -1;
}

void TcpServer::acceptLoop() {
    while (running_) {
        sockaddr_in clientAddress{};
        socklen_t clientLength = sizeof(clientAddress);
        const int clientFd = accept(serverFd_, reinterpret_cast<sockaddr*>(&clientAddress), &clientLength);
        if (clientFd < 0) {
            if (running_) {
                std::cerr << "accept failed: " << std::strerror(errno) << "\n";
            }
            continue;
        }

        std::thread(&TcpServer::handleClient, this, clientFd).detach();
    }
}

void TcpServer::handleClient(int clientFd) const {
    metrics_.clientConnected();
    bool greetingSent = false;
    bool authenticated = authPassword_.empty();
    std::size_t databaseIndex = 0;

    std::string pending;
    char buffer[kBufferSize];

    while (true) {
        const ssize_t bytesRead = recv(clientFd, buffer, sizeof(buffer), 0);
        if (bytesRead == 0) {
            break;
        }
        if (bytesRead < 0) {
            if (errno != EINTR) {
                break;
            }
            continue;
        }

        pending.append(buffer, static_cast<std::size_t>(bytesRead));
        const auto commands = parser_.parseAvailable(pending);
        for (const auto& command : commands) {
            if (!greetingSent && command.protocol == RequestProtocol::Inline) {
                if (!sendResponse(clientFd, "Connected to SwiftCache\n")) {
                    removeClientSubscriptions(clientFd);
                    closeIfOpen(clientFd);
                    metrics_.clientDisconnected();
                    return;
                }
                greetingSent = true;
            }

            if (!command.tokens.empty()) {
                metrics_.commandExecuted(toUpper(command.tokens.front()));
                if (handleAuthCommand(clientFd, command, command.tokens, authenticated)) {
                    continue;
                }
                if (!requireAuthenticatedClient(clientFd, command.protocol, authenticated)) {
                    metrics_.commandRejected();
                    continue;
                }
                if (handleReadOnlyCommand(clientFd, command, command.tokens)) {
                    continue;
                }
                if (handleSelectCommand(clientFd, command, command.tokens, databaseIndex)) {
                    continue;
                }
                if (rejectReadOnlyWrite(clientFd, command, command.tokens)) {
                    metrics_.commandRejected();
                    continue;
                }
                if (handlePubSubCommand(clientFd, command, command.tokens)) {
                    continue;
                }

                bool appendSucceeded = true;
                DataStore& selectedStore = stores_ == nullptr ? *store_ : (*stores_)[databaseIndex];
                const auto result = aof_ == nullptr
                    ? registry_.execute(command.tokens, selectedStore)
                    : aof_->executeAndAppend(command.tokens, databaseIndex, [this, &command, &selectedStore]() {
                        return registry_.execute(command.tokens, selectedStore);
                    }, appendSucceeded);
                if (!appendSucceeded && isSuccessfulResponse(result.response)) {
                    std::cerr << "failed to append command to AOF\n";
                }
                if (isRejectedResponse(result.response)) {
                    metrics_.commandRejected();
                }
                const auto response = command.protocol == RequestProtocol::Resp
                    ? formatRespResponse(result.response)
                    : result.response;
                if (!sendResponse(clientFd, response) || result.closeConnection) {
                    removeClientSubscriptions(clientFd);
                    closeIfOpen(clientFd);
                    metrics_.clientDisconnected();
                    return;
                }
            }
        }
    }

    removeClientSubscriptions(clientFd);
    closeIfOpen(clientFd);
    metrics_.clientDisconnected();
}

bool TcpServer::handleAuthCommand(int clientFd, const ParsedCommand& command,
                                  const std::vector<std::string>& tokens,
                                  bool& authenticated) const {
    if (toUpper(tokens.front()) != "AUTH") {
        return false;
    }

    std::string response;
    if (authPassword_.empty()) {
        response = "ERR AUTH called without any password configured\n";
    } else if (tokens.size() != 2) {
        response = "ERR wrong number of arguments for AUTH\n";
    } else if (tokens[1] != authPassword_) {
        response = "ERR invalid password\n";
    } else {
        authenticated = true;
        response = "OK\n";
    }

    if (isRejectedResponse(response)) {
        metrics_.commandRejected();
    }
    if (command.protocol == RequestProtocol::Resp) {
        response = formatRespResponse(response);
    }
    sendResponse(clientFd, response);
    return true;
}

bool TcpServer::requireAuthenticatedClient(int clientFd, RequestProtocol protocol,
                                           bool authenticated) const {
    if (authenticated) {
        return true;
    }

    std::string response = "NOAUTH Authentication required\n";
    if (protocol == RequestProtocol::Resp) {
        response = formatRespResponse(response);
    }
    sendResponse(clientFd, response);
    return false;
}

bool TcpServer::handleReadOnlyCommand(int clientFd, const ParsedCommand& command,
                                      const std::vector<std::string>& tokens) const {
    if (toUpper(tokens.front()) != "READONLY") {
        return false;
    }

    std::string response;
    if (tokens.size() == 1) {
        response = readOnly_.load() ? "1\n" : "0\n";
    } else if (tokens.size() == 2 && toUpper(tokens[1]) == "ON") {
        readOnly_.store(true);
        response = "OK\n";
    } else if (tokens.size() == 2 && toUpper(tokens[1]) == "OFF") {
        readOnly_.store(false);
        response = "OK\n";
    } else {
        response = "ERR usage: READONLY [ON|OFF]\n";
    }

    if (isRejectedResponse(response)) {
        metrics_.commandRejected();
    }
    if (command.protocol == RequestProtocol::Resp) {
        response = formatRespResponse(response);
    }
    sendResponse(clientFd, response);
    return true;
}

bool TcpServer::rejectReadOnlyWrite(int clientFd, const ParsedCommand& command,
                                    const std::vector<std::string>& tokens) const {
    if (!readOnly_.load() || !isWriteCommand(tokens)) {
        return false;
    }

    std::string response = "ERR server is read-only\n";
    if (command.protocol == RequestProtocol::Resp) {
        response = formatRespResponse(response);
    }
    sendResponse(clientFd, response);
    return true;
}

bool TcpServer::handleSelectCommand(int clientFd, const ParsedCommand& command,
                                    const std::vector<std::string>& tokens,
                                    std::size_t& databaseIndex) const {
    if (toUpper(tokens.front()) != "SELECT") {
        return false;
    }

    std::string response;
    if (tokens.size() != 2) {
        response = "ERR wrong number of arguments for SELECT\n";
    } else {
        const std::size_t databaseCount = stores_ == nullptr ? 1 : stores_->size();
        std::size_t selected = 0;
        if (!parseDatabaseIndex(tokens[1], databaseCount, selected)) {
            response = "ERR DB index is out of range\n";
        } else {
            databaseIndex = selected;
            response = "OK\n";
        }
    }

    if (isRejectedResponse(response)) {
        metrics_.commandRejected();
    }
    if (command.protocol == RequestProtocol::Resp) {
        response = formatRespResponse(response);
    }
    sendResponse(clientFd, response);
    return true;
}

bool TcpServer::sendResponse(int clientFd, const std::string& response) const {
    std::lock_guard<std::mutex> lock(sendMutex_);
    const char* data = response.data();
    std::size_t remaining = response.size();

    while (remaining > 0) {
        const ssize_t sent = send(clientFd, data, remaining, 0);
        if (sent < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        data += sent;
        remaining -= static_cast<std::size_t>(sent);
    }

    return true;
}

bool TcpServer::handlePubSubCommand(int clientFd, const ParsedCommand& command,
                                    const std::vector<std::string>& tokens) const {
    const auto commandName = toUpper(tokens.front());
    std::string response;

    if (commandName == "SUBSCRIBE") {
        if (tokens.size() < 2) {
            response = "ERR wrong number of arguments for SUBSCRIBE\n";
        } else {
            response = subscribe(clientFd, command.protocol,
                                 std::vector<std::string>(tokens.begin() + 1, tokens.end()));
        }
    } else if (commandName == "UNSUBSCRIBE") {
        response = unsubscribe(clientFd, command.protocol,
                               std::vector<std::string>(tokens.begin() + 1, tokens.end()));
    } else if (commandName == "PUBLISH") {
        if (tokens.size() != 3) {
            response = "ERR wrong number of arguments for PUBLISH\n";
        } else {
            response = publish(command.protocol, tokens[1], tokens[2]);
        }
    } else {
        return false;
    }

    const bool rejected = isRejectedResponse(response);
    if (command.protocol == RequestProtocol::Resp && rejected) {
        response = formatRespResponse(response);
    }
    if (rejected) {
        metrics_.commandRejected();
    }
    sendResponse(clientFd, response);
    return true;
}

std::string TcpServer::subscribe(int clientFd, RequestProtocol protocol,
                                 const std::vector<std::string>& channels) const {
    std::ostringstream response;

    std::lock_guard<std::mutex> lock(pubsubMutex_);
    auto& subscriptions = clientSubscriptions_[clientFd];
    for (const auto& channel : channels) {
        subscriptions.insert(channel);
        channelSubscribers_[channel][clientFd] = protocol;

        const auto item = formatPubSubTuple(protocol, "subscribe", channel, subscriptions.size());
        response << item;
    }

    return response.str();
}

std::string TcpServer::unsubscribe(int clientFd, RequestProtocol protocol,
                                   const std::vector<std::string>& channels) const {
    std::ostringstream response;
    std::vector<std::string> targets = channels;

    std::lock_guard<std::mutex> lock(pubsubMutex_);
    auto subscriptionsIt = clientSubscriptions_.find(clientFd);
    if (targets.empty() && subscriptionsIt != clientSubscriptions_.end()) {
        targets.assign(subscriptionsIt->second.begin(), subscriptionsIt->second.end());
    }

    for (const auto& channel : targets) {
        if (subscriptionsIt != clientSubscriptions_.end()) {
            subscriptionsIt->second.erase(channel);
            if (subscriptionsIt->second.empty()) {
                clientSubscriptions_.erase(subscriptionsIt);
                subscriptionsIt = clientSubscriptions_.end();
            }
        }

        auto subscribersIt = channelSubscribers_.find(channel);
        if (subscribersIt != channelSubscribers_.end()) {
            subscribersIt->second.erase(clientFd);
            if (subscribersIt->second.empty()) {
                channelSubscribers_.erase(subscribersIt);
            }
        }

        const auto count = subscriptionsIt == clientSubscriptions_.end()
            ? 0
            : subscriptionsIt->second.size();
        response << formatPubSubTuple(protocol, "unsubscribe", channel, count);
    }

    return response.str();
}

std::string TcpServer::publish(RequestProtocol protocol, const std::string& channel,
                               const std::string& message) const {
    std::vector<std::pair<int, RequestProtocol>> subscribers;

    {
        std::lock_guard<std::mutex> lock(pubsubMutex_);
        const auto subscribersIt = channelSubscribers_.find(channel);
        if (subscribersIt != channelSubscribers_.end()) {
            subscribers.reserve(subscribersIt->second.size());
            for (const auto& [clientFd, subscriberProtocol] : subscribersIt->second) {
                subscribers.emplace_back(clientFd, subscriberProtocol);
            }
        }
    }

    std::vector<int> failedClients;
    for (const auto& [clientFd, subscriberProtocol] : subscribers) {
        if (!sendResponse(clientFd, formatPubSubMessage(subscriberProtocol, channel, message))) {
            failedClients.push_back(clientFd);
        }
    }

    for (const auto clientFd : failedClients) {
        removeClientSubscriptions(clientFd);
    }

    const auto delivered = subscribers.size() - failedClients.size();
    if (protocol == RequestProtocol::Resp) {
        return ":" + std::to_string(delivered) + "\r\n";
    }
    return std::to_string(delivered) + "\n";
}

void TcpServer::removeClientSubscriptions(int clientFd) const {
    std::lock_guard<std::mutex> lock(pubsubMutex_);
    const auto subscriptionsIt = clientSubscriptions_.find(clientFd);
    if (subscriptionsIt == clientSubscriptions_.end()) {
        return;
    }

    for (const auto& channel : subscriptionsIt->second) {
        auto subscribersIt = channelSubscribers_.find(channel);
        if (subscribersIt == channelSubscribers_.end()) {
            continue;
        }

        subscribersIt->second.erase(clientFd);
        if (subscribersIt->second.empty()) {
            channelSubscribers_.erase(subscribersIt);
        }
    }

    clientSubscriptions_.erase(subscriptionsIt);
}

} // namespace swiftcache
