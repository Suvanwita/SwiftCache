#include <cassert>
#include <chrono>
#include <deque>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

#include "../src/commands/CommandFactory.h"
#include "../src/core/AofPersistence.h"
#include "../src/core/SnapshotPersistence.h"
#include "../src/datastore/DataStore.h"
#include "../src/parser/CommandParser.h"

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
    swiftcache::CommandParser parser;

    std::string inlineBuffer = "SET protocol inline\nGET protocol\n";
    const auto inlineCommands = parser.parseAvailable(inlineBuffer);
    assert(inlineCommands.size() == 2);
    assert(inlineCommands[0].protocol == swiftcache::RequestProtocol::Inline);
    assert((inlineCommands[0].tokens == std::vector<std::string>{"SET", "protocol", "inline"}));
    assert(inlineBuffer.empty());

    std::string respBuffer = "*3\r\n$3\r\nSET\r\n$8\r\nprotocol\r\n$4\r\nresp\r\n";
    const auto respCommands = parser.parseAvailable(respBuffer);
    assert(respCommands.size() == 1);
    assert(respCommands[0].protocol == swiftcache::RequestProtocol::Resp);
    assert((respCommands[0].tokens == std::vector<std::string>{"SET", "protocol", "resp"}));
    assert(respBuffer.empty());

    std::string partialRespBuffer = "*2\r\n$4\r\nPING\r\n";
    const auto partialRespCommands = parser.parseAvailable(partialRespBuffer);
    assert(partialRespCommands.empty());
    assert(partialRespBuffer == "*2\r\n$4\r\nPING\r\n");

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

    assert(run(registry, store, {"SET", "key:string", "value"}).response == "OK\n");
    assert(run(registry, store, {"LPUSH", "key:list", "left"}).response == "1\n");
    assert(run(registry, store, {"HSET", "key:hash", "field", "value"}).response == "1\n");
    assert(run(registry, store, {"SADD", "key:set", "member"}).response == "1\n");
    assert(run(registry, store, {"TYPE", "key:string"}).response == "string\n");
    assert(run(registry, store, {"TYPE", "key:list"}).response == "list\n");
    assert(run(registry, store, {"TYPE", "key:hash"}).response == "hash\n");
    assert(run(registry, store, {"TYPE", "key:set"}).response == "set\n");
    assert(run(registry, store, {"TYPE", "key:missing"}).response == "none\n");
    assert(run(registry, store, {"KEYS", "key:*"}).response ==
           "key:hash\nkey:list\nkey:set\nkey:string\n");
    assert(run(registry, store, {"RENAME", "key:string", "key:string2"}).response == "OK\n");
    assert(run(registry, store, {"GET", "key:string2"}).response == "value\n");
    assert(run(registry, store, {"SCAN", "0", "MATCH", "key:*"}).response ==
           "0\nkey:hash\nkey:list\nkey:set\nkey:string2\n");
    assert(run(registry, store, {"FLUSHDB"}).response == "OK\n");
    assert(run(registry, store, {"DBSIZE"}).response == "0\n");
    assert(run(registry, store, {"KEYS"}).response == "");

    swiftcache::DataStore dbsizeStore;
    assert(run(registry, dbsizeStore, {"DBSIZE"}).response == "0\n");
    assert(run(registry, dbsizeStore, {"SET", "plain", "1"}).response == "OK\n");
    assert(run(registry, dbsizeStore, {"SET", "expires", "1", "EX", "1"}).response == "OK\n");
    assert(run(registry, dbsizeStore, {"DBSIZE"}).response == "2\n");
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    assert(run(registry, dbsizeStore, {"DBSIZE"}).response == "1\n");

    swiftcache::DataStore dbZero;
    swiftcache::DataStore dbOne;
    assert(run(registry, dbZero, {"SET", "shared", "zero"}).response == "OK\n");
    assert(run(registry, dbOne, {"SET", "shared", "one"}).response == "OK\n");
    assert(run(registry, dbOne, {"SET", "other", "one"}).response == "OK\n");
    assert(run(registry, dbZero, {"DBSIZE"}).response == "1\n");
    assert(run(registry, dbOne, {"DBSIZE"}).response == "2\n");

    swiftcache::DataStore lruStore;
    lruStore.configureEviction(swiftcache::EvictionConfig{2, 0, swiftcache::EvictionPolicy::AllKeysLru});
    assert(run(registry, lruStore, {"SET", "lru:a", "1"}).response == "OK\n");
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    assert(run(registry, lruStore, {"SET", "lru:b", "2"}).response == "OK\n");
    assert(run(registry, lruStore, {"GET", "lru:a"}).response == "1\n");
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    assert(run(registry, lruStore, {"SET", "lru:c", "3"}).response == "OK\n");
    assert(run(registry, lruStore, {"GET", "lru:a"}).response == "1\n");
    assert(run(registry, lruStore, {"GET", "lru:b"}).response == "(nil)\n");
    assert(run(registry, lruStore, {"GET", "lru:c"}).response == "3\n");
    assert(lruStore.stats().evictedKeys == 1);

    swiftcache::DataStore volatileStore;
    volatileStore.configureEviction(swiftcache::EvictionConfig{1, 0, swiftcache::EvictionPolicy::VolatileLru});
    assert(run(registry, volatileStore, {"SET", "permanent", "1"}).response == "OK\n");
    assert(run(registry, volatileStore, {"SET", "temporary", "2", "EX", "60"}).response == "OK\n");
    assert(run(registry, volatileStore, {"GET", "permanent"}).response == "1\n");
    assert(run(registry, volatileStore, {"GET", "temporary"}).response == "(nil)\n");

    swiftcache::DataStore ttlPriorityStore;
    ttlPriorityStore.configureEviction(swiftcache::EvictionConfig{2, 0, swiftcache::EvictionPolicy::TtlPriority});
    assert(run(registry, ttlPriorityStore, {"SET", "far", "1", "EX", "60"}).response == "OK\n");
    assert(run(registry, ttlPriorityStore, {"SET", "near", "2", "EX", "1"}).response == "OK\n");
    assert(run(registry, ttlPriorityStore, {"SET", "plain", "3"}).response == "OK\n");
    assert(run(registry, ttlPriorityStore, {"GET", "near"}).response == "(nil)\n");
    assert(run(registry, ttlPriorityStore, {"GET", "far"}).response == "1\n");
    assert(run(registry, ttlPriorityStore, {"GET", "plain"}).response == "3\n");

    const std::string aofPath = "/tmp/swiftcache-command-tests.aof";
    std::filesystem::remove(aofPath);
    swiftcache::AofPersistence aof(aofPath);
    assert(aof.append({"SET", "persisted:string", "value"}));
    assert(aof.append({"SADD", "persisted:set", "one", "two"}));
    assert(aof.append({"HSET", "persisted:hash", "field", "value"}));
    assert(aof.append({"EXPIRE", "persisted:string", "120"}));

    swiftcache::DataStore replayedStore;
    assert(aof.replay(registry, replayedStore));
    assert(run(registry, replayedStore, {"GET", "persisted:string"}).response == "value\n");
    assert(run(registry, replayedStore, {"SMEMBERS", "persisted:set"}).response == "one\ntwo\n");
    assert(run(registry, replayedStore, {"HGET", "persisted:hash", "field"}).response == "value\n");
    assert(std::stoll(run(registry, replayedStore, {"TTL", "persisted:string"}).response) > 0);
    std::filesystem::remove(aofPath);

    const std::string multiDbAofPath = "/tmp/swiftcache-command-tests-multidb.aof";
    std::filesystem::remove(multiDbAofPath);
    swiftcache::AofPersistence multiDbAof(multiDbAofPath);
    assert(multiDbAof.append({"SET", "shared", "db0"}, 0));
    assert(multiDbAof.append({"SET", "shared", "db1"}, 1));
    assert(multiDbAof.append({"SADD", "tags", "isolated"}, 1));
    assert(multiDbAof.append({"SET", "shared", "db0-again"}, 0));

    std::deque<swiftcache::DataStore> replayedStores(2);
    assert(multiDbAof.replay(registry, replayedStores));
    assert(run(registry, replayedStores[0], {"GET", "shared"}).response == "db0-again\n");
    assert(run(registry, replayedStores[0], {"SMEMBERS", "tags"}).response == "");
    assert(run(registry, replayedStores[1], {"GET", "shared"}).response == "db1\n");
    assert(run(registry, replayedStores[1], {"SMEMBERS", "tags"}).response == "isolated\n");
    std::filesystem::remove(multiDbAofPath);

    const std::string snapshotPath = "/tmp/swiftcache-command-tests.snapshot";
    const std::string checkpointAofPath = "/tmp/swiftcache-command-tests-checkpoint.aof";
    std::filesystem::remove(snapshotPath);
    std::filesystem::remove(checkpointAofPath);

    swiftcache::DataStore snapshotStore;
    assert(run(registry, snapshotStore, {"SET", "snap:string", "value"}).response == "OK\n");
    assert(run(registry, snapshotStore, {"RPUSH", "snap:list", "a", "b"}).response == "2\n");
    assert(run(registry, snapshotStore, {"HSET", "snap:hash", "field", "value"}).response == "1\n");
    assert(run(registry, snapshotStore, {"SADD", "snap:set", "one", "two"}).response == "2\n");

    swiftcache::SnapshotPersistence snapshot(snapshotPath);
    assert(snapshot.save(snapshotStore));

    swiftcache::DataStore loadedSnapshotStore;
    assert(snapshot.load(loadedSnapshotStore));
    assert(run(registry, loadedSnapshotStore, {"GET", "snap:string"}).response == "value\n");
    assert(run(registry, loadedSnapshotStore, {"LRANGE", "snap:list", "0", "-1"}).response == "a\nb\n");
    assert(run(registry, loadedSnapshotStore, {"HGET", "snap:hash", "field"}).response == "value\n");
    assert(run(registry, loadedSnapshotStore, {"SMEMBERS", "snap:set"}).response == "one\ntwo\n");

    std::deque<swiftcache::DataStore> snapshotStores(2);
    assert(run(registry, snapshotStores[0], {"SET", "shared", "db0"}).response == "OK\n");
    assert(run(registry, snapshotStores[1], {"SET", "shared", "db1"}).response == "OK\n");
    assert(snapshot.save(snapshotStores));

    std::deque<swiftcache::DataStore> loadedSnapshotStores(2);
    assert(snapshot.load(loadedSnapshotStores));
    assert(run(registry, loadedSnapshotStores[0], {"GET", "shared"}).response == "db0\n");
    assert(run(registry, loadedSnapshotStores[1], {"GET", "shared"}).response == "db1\n");

    swiftcache::AofPersistence checkpointAof(checkpointAofPath);
    assert(checkpointAof.append({"SET", "delta", "value"}));
    assert(std::filesystem::file_size(checkpointAofPath) > 0);
    assert(checkpointAof.checkpoint([&snapshot, &snapshotStore]() {
        return snapshot.save(snapshotStore);
    }));
    assert(std::filesystem::file_size(checkpointAofPath) == 0);
    std::filesystem::remove(snapshotPath);
    std::filesystem::remove(checkpointAofPath);

    return 0;
}
