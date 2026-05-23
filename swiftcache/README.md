# SwiftCache

SwiftCache is a Redis-inspired C++17 datastore starter architecture. Phase 1 focuses on a small command surface and a modular backend shape that can grow into TTL, persistence, Pub/Sub, richer data types, eviction, replication, and clustering.

This is intentionally a systems/backend project, not a CRUD application.

## Phase 1 commands

- `PING`
- `SET key value`
- `GET key`
- `DEL key`
- `EXISTS key`
- `INFO`

## Architecture

- `core/` owns command abstractions and registry dispatch.
- `commands/` contains isolated command implementations grouped by domain.
- `datastore/` owns the thread-safe in-memory key/value store.
- `parser/` converts client input into command tokens.
- `networking/` owns the TCP socket server and threaded client handling.
- `storage/` is reserved for future persistence work.

## Build

```sh
mkdir build
cd build
cmake ..
make
./MiniRedis
```

Or from the repository root:

```sh
make -C swiftcache run
```

## Try it

SwiftCache listens on `localhost:6379`.

```sh
telnet localhost 6379
```

Expected greeting:

```text
Connected to MiniRedis
```

Example session:

```text
PING
PONG
SET name swiftcache
OK
GET name
swiftcache
EXISTS name
1
DEL name
1
GET name
(nil)
INFO
# Server
swiftcache_version:0.1.0
uptime_in_seconds:12

# Keyspace
keys:0
```

## Tests

```sh
make -C swiftcache test
```
