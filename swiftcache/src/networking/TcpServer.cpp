#include "TcpServer.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

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
    sendResponse(clientFd, "Connected to SwiftCache\n");

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
        std::size_t newline = pending.find('\n');
        while (newline != std::string::npos) {
            const std::string line = trimLineEnding(pending.substr(0, newline + 1));
            pending.erase(0, newline + 1);

            if (!line.empty()) {
                const auto tokens = parser_.parse(line);
                metrics_.commandExecuted();
                const auto result = registry_.execute(tokens, store_);
                if (!sendResponse(clientFd, result.response) || result.closeConnection) {
                    closeIfOpen(clientFd);
                    metrics_.clientDisconnected();
                    return;
                }
            }

            newline = pending.find('\n');
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
