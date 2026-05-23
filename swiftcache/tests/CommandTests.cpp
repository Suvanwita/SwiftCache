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

    assert(run(registry, store, {"INCR", "counter"}).response == "1\n");
    assert(run(registry, store, {"INCR", "counter"}).response == "2\n");
    assert(run(registry, store, {"DECR", "counter"}).response == "1\n");
    assert(run(registry, store, {"SET", "word", "swift"}).response == "OK\n");
    assert(run(registry, store, {"APPEND", "word", "cache"}).response == "10\n");
    assert(run(registry, store, {"GET", "word"}).response == "swiftcache\n");
    assert(run(registry, store, {"STRLEN", "word"}).response == "10\n");
    assert(run(registry, store, {"STRLEN", "missing"}).response == "0\n");
    assert(run(registry, store, {"MSET", "a", "1", "b", "2", "c", "3"}).response == "OK\n");
    assert(run(registry, store, {"MGET", "a", "missing", "c"}).response == "1\n(nil)\n3\n");
    assert(run(registry, store, {"SET", "not-int", "abc"}).response == "OK\n");
    assert(run(registry, store, {"INCR", "not-int"}).response == "ERR value is not an integer\n");

    assert(run(registry, store, {"LPUSH", "queue", "b", "a"}).response == "2\n");
    assert(run(registry, store, {"RPUSH", "queue", "c", "d"}).response == "4\n");
    assert(run(registry, store, {"LRANGE", "queue", "0", "-1"}).response == "a\nb\nc\nd\n");
    assert(run(registry, store, {"LPOP", "queue"}).response == "a\n");
    assert(run(registry, store, {"RPOP", "queue"}).response == "d\n");
    assert(run(registry, store, {"LRANGE", "queue", "0", "-1"}).response == "b\nc\n");
    assert(run(registry, store, {"LRANGE", "queue", "-2", "-1"}).response == "b\nc\n");
    assert(run(registry, store, {"LPOP", "empty-list"}).response == "(nil)\n");
    assert(run(registry, store, {"LPUSH", "word", "nope"}).response == "ERR wrong type for LPUSH\n");

    assert(run(registry, store, {"HSET", "user:1", "name", "ada"}).response == "1\n");
    assert(run(registry, store, {"HSET", "user:1", "role", "engineer"}).response == "1\n");
    assert(run(registry, store, {"HSET", "user:1", "role", "architect"}).response == "0\n");
    assert(run(registry, store, {"HGET", "user:1", "name"}).response == "ada\n");
    assert(run(registry, store, {"HGET", "user:1", "missing"}).response == "(nil)\n");
    assert(run(registry, store, {"HEXISTS", "user:1", "role"}).response == "1\n");
    assert(run(registry, store, {"HEXISTS", "user:1", "missing"}).response == "0\n");
    assert(run(registry, store, {"HGETALL", "user:1"}).response == "name\nada\nrole\narchitect\n");
    assert(run(registry, store, {"HDEL", "user:1", "role"}).response == "1\n");
    assert(run(registry, store, {"HDEL", "user:1", "role"}).response == "0\n");
    assert(run(registry, store, {"HSET", "word", "field", "value"}).response == "ERR wrong type for HSET\n");

    assert(run(registry, store, {"SADD", "tags", "fast", "cache", "fast"}).response == "2\n");
    assert(run(registry, store, {"SADD", "tags", "systems"}).response == "1\n");
    assert(run(registry, store, {"SCARD", "tags"}).response == "3\n");
    assert(run(registry, store, {"SISMEMBER", "tags", "cache"}).response == "1\n");
    assert(run(registry, store, {"SISMEMBER", "tags", "missing"}).response == "0\n");
    assert(run(registry, store, {"SMEMBERS", "tags"}).response == "cache\nfast\nsystems\n");
    assert(run(registry, store, {"SREM", "tags", "fast", "missing"}).response == "1\n");
    assert(run(registry, store, {"SMEMBERS", "tags"}).response == "cache\nsystems\n");
    assert(run(registry, store, {"SADD", "word", "nope"}).response == "ERR wrong type for SADD\n");

    return 0;
}
