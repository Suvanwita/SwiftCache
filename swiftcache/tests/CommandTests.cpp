#include <cassert>
#include <chrono>
#include <string>
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

    return 0;
}
