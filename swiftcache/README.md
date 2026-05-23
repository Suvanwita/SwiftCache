# SwiftCache

SwiftCache is a Redis-inspired C++17 datastore starter architecture. Phase 2 adds TTL and expiration while keeping the modular backend shape ready for persistence, Pub/Sub, richer data types, eviction, replication, and clustering.

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
- `INFO`

---

## Architecture

- `core/` owns command abstractions and registry dispatch.
- `commands/` contains isolated command implementations grouped by domain.
- `core/ExpiryWorker` removes expired keys in the background every second.
- `datastore/` owns the thread-safe in-memory key/value store and lazy expiration checks.
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
 totalKeys: 1,
 connectedClients: 1,
 totalCommands: 12,
 uptimeSeconds: 12
}
```

---

## Tests

```sh
make -C swiftcache test
```
