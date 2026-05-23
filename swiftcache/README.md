# SwiftCache

SwiftCache is a Redis-inspired C++17 datastore starter architecture. It now supports core string, list, hash, set, TTL, and expiration operations while keeping the modular backend shape ready for persistence, Pub/Sub, richer data types, eviction, replication, and clustering.

This is intentionally a systems/backend project, not a CRUD application.

---

## Commands

- `PING`
- `SET key value`
- `SET key value EX seconds`
- `GET key`
- `DEL key`
- `EXISTS key`
- `EXPIRE key seconds`
- `TTL key`
- `PERSIST key`
- `INCR key`
- `DECR key`
- `APPEND key value`
- `STRLEN key`
- `MGET key [key ...]`
- `MSET key value [key value ...]`
- `LPUSH key value [value ...]`
- `RPUSH key value [value ...]`
- `LPOP key`
- `RPOP key`
- `LRANGE key start stop`
- `HSET key field value`
- `HGET key field`
- `HDEL key field`
- `HEXISTS key field`
- `HGETALL key`
- `SADD key member [member ...]`
- `SREM key member [member ...]`
- `SISMEMBER key member`
- `SMEMBERS key`
- `SCARD key`
- `INFO`

---

## Architecture

- `core/` owns command abstractions and registry dispatch.
- `commands/` contains isolated command implementations grouped by domain.
- `core/ExpiryWorker` removes expired keys in the background every second.
- `datastore/` owns the thread-safe in-memory key/value store, typed string/list/hash/set values, and lazy expiration checks.
- `parser/` converts client input into command tokens.
- `networking/` owns the TCP socket server and threaded client handling.
- `storage/` is reserved for future persistence work.

---

## Build

```sh
mkdir build
cd build
cmake ..
make
./SwiftCache
```

Or from the repository root:

```sh
make -C swiftcache run
```

---

## Try it

SwiftCache listens on `localhost:6379`.

```sh
telnet localhost 6379
```

Expected greeting:

```text
Connected to SwiftCache
```

Example session:

```text
PING
PONG
SET name swiftcache
OK
APPEND name -store
16
STRLEN name
16
MSET visits 10 mode fast
OK
INCR visits
11
DECR visits
10
MGET name visits missing
swiftcache-store
10
(nil)
LPUSH queue b a
2
RPUSH queue c d
4
LRANGE queue 0 -1
a
b
c
d
LPOP queue
a
RPOP queue
d
HSET user:1 name ada
1
HSET user:1 role architect
1
HGET user:1 name
ada
HEXISTS user:1 role
1
HGETALL user:1
name
ada
role
architect
HDEL user:1 role
1
SADD tags fast cache fast
2
SADD tags systems
1
SCARD tags
3
SISMEMBER tags cache
1
SMEMBERS tags
cache
fast
systems
SREM tags fast
1
SET token abc EX 60
OK
TTL token
60
EXPIRE token 120
1
PERSIST token
1
TTL token
-1
GET name
swiftcache
EXISTS name
1
DEL name
1
GET name
(nil)
INFO
{
 totalKeys: 6,
 connectedClients: 1,
 totalCommands: 35,
 uptimeSeconds: 12
}
```

---

## Tests

```sh
make -C swiftcache test
```
