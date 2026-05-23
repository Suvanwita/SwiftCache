#include <cassert>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include "../src/commands/CommandFactory.h"
#include "../src/datastore/DataStore.h"

namespace {

swiftcache::CommandResult run(const swiftcache::CommandRegistry& registry,
                              swiftcache::DataStore& store,
                              std::initializer_list<std::string> tokens) {
    return registry.execute(std::vector<std::string>(tokens), store);
}

} // namespace

int main() {
    swiftcache::DataStore store;
    swiftcache::ServerMetrics metrics;
    auto registry = swiftcache::buildCommandRegistry(std::chrono::steady_clock::now(), metrics);

    assert(run(registry, store, {"SET", "session:1", "active"}).response == "OK\n");
    assert(run(registry, store, {"GET", "session:1"}).response == "active\n");
    assert(run(registry, store, {"EXISTS", "session:1"}).response == "1\n");
    assert(run(registry, store, {"DEL", "session:1"}).response == "1\n");
    assert(run(registry, store, {"GET", "session:1"}).response == "(nil)\n");
    assert(run(registry, store, {"EXISTS", "session:1"}).response == "0\n");
    assert(run(registry, store, {"DEL", "session:1"}).response == "0\n");

    assert(run(registry, store, {"SET", "token", "abc", "EX", "2"}).response == "OK\n");
    assert(run(registry, store, {"GET", "token"}).response == "abc\n");
    const auto tokenTtl = std::stoll(run(registry, store, {"TTL", "token"}).response);
    assert(tokenTtl > 0 && tokenTtl <= 2);

    assert(run(registry, store, {"EXPIRE", "token", "120"}).response == "1\n");
    const auto refreshedTtl = std::stoll(run(registry, store, {"TTL", "token"}).response);
    assert(refreshedTtl > 0 && refreshedTtl <= 120);

    assert(run(registry, store, {"PERSIST", "token"}).response == "1\n");
    assert(run(registry, store, {"TTL", "token"}).response == "-1\n");
    assert(run(registry, store, {"TTL", "missing"}).response == "-2\n");

    assert(run(registry, store, {"SET", "short", "lived", "EX", "1"}).response == "OK\n");
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    assert(run(registry, store, {"GET", "short"}).response == "(nil)\n");
    assert(run(registry, store, {"TTL", "short"}).response == "-2\n");

    return 0;
}
