#include "TcpServer.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cctype>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <sys/select.h>
#include <thread>
#include <unistd.h>

#include "../parser/CommandParser.h"

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

std::string formatRespResponse(const std::string& response) {
    const std::string trimmed = trimTrailingNewline(response);

    if (trimmed.rfind("ERR ", 0) == 0) {
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

} // namespace

TcpServer::TcpServer(std::uint16_t port, DataStore& store, const CommandRegistry& registry, ServerMetrics& metrics)
    : port_(port), store_(store), registry_(registry), metrics_(metrics) {}

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
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
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
    std::cout << "SwiftCache listening on localhost:" << port_ << "\n";
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
                    closeIfOpen(clientFd);
                    metrics_.clientDisconnected();
                    return;
                }
                greetingSent = true;
            }

            if (!command.tokens.empty()) {
                metrics_.commandExecuted();
                const auto result = registry_.execute(command.tokens, store_);
                const auto response = command.protocol == RequestProtocol::Resp
                    ? formatRespResponse(result.response)
                    : result.response;
                if (!sendResponse(clientFd, response) || result.closeConnection) {
                    closeIfOpen(clientFd);
                    metrics_.clientDisconnected();
                    return;
                }
            }
        }
    }

    closeIfOpen(clientFd);
    metrics_.clientDisconnected();
}

bool TcpServer::sendResponse(int clientFd, const std::string& response) const {
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

} // namespace swiftcache
