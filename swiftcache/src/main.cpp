#include <chrono>
#include <csignal>
#include <iostream>

#include "commands/CommandFactory.h"
#include "core/AofPersistence.h"
#include "core/ExpiryWorker.h"
#include "datastore/DataStore.h"
#include "networking/TcpServer.h"

namespace {

volatile std::sig_atomic_t g_shutdownRequested = 0;

#ifndef SWIFTCACHE_AOF_PATH
#define SWIFTCACHE_AOF_PATH "storage/swiftcache.aof"
#endif

void handleSignal(int) {
    g_shutdownRequested = 1;
}

} // namespace

int main() {
    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    swiftcache::DataStore store;
    swiftcache::ServerMetrics metrics;
    swiftcache::ExpiryWorker expiryWorker(store);
    auto registry = swiftcache::buildCommandRegistry(std::chrono::steady_clock::now(), metrics);
    swiftcache::AofPersistence aof(SWIFTCACHE_AOF_PATH);
    if (!aof.replay(registry, store)) {
        std::cerr << "SwiftCache failed to replay AOF\n";
        return 1;
    }

    swiftcache::TcpServer server(6379, store, registry, metrics, &aof);
    expiryWorker.start();

    if (g_shutdownRequested) {
        server.stop();
        expiryWorker.stop();
        return 0;
    }

    const bool started = server.start();
    expiryWorker.stop();
    if (!started) {
        std::cerr << "SwiftCache failed to start\n";
        return 1;
    }

    return 0;
}
