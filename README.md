# SwiftCache

SwiftCache is a Redis-inspired in-memory datastore written in C++17. It supports typed data structures, logical databases, expiration, persistence, Pub/Sub, basic authentication, read-only mode, client management, and both inline and RESP-style TCP requests.

## Features

- C++17 TCP server on `localhost:6379` by default
- Inline text protocol for `telnet`/`nc` and RESP array/bulk-string request parsing
- Multiple threaded client connections with client inspection commands
- Optional password authentication with `AUTH`
- Configurable and runtime-switchable read-only mode
- Logical databases with `SELECT`, `DBSIZE`, `MOVE`, `FLUSHDB`, and `FLUSHALL`
- Typed values: string, list, hash, and set
- TTL support with lazy expiration and a background expiry worker
- Snapshot persistence, append-only file replay, manual `SAVE`, and `LASTSAVE`
- Optional key-count and estimated-memory eviction limits
- Eviction policies: `allkeys-lru`, `volatile-lru`, `ttl-priority`, and `random`
- Pub/Sub messaging with `SUBSCRIBE`, `PUBLISH`, and `UNSUBSCRIBE`
- Server, datastore, persistence, command, and client metrics through `INFO`
- Focused admin helpers such as `CONFIG GET`, `COMMAND STATS`, and `CLIENT LIST`

## Quick Start

Build from the `swiftcache` directory:

```sh
cd swiftcache
mkdir -p build
cd build
cmake ..
make
```

Run the server:

```sh
./SwiftCache
```

Run tests from the repository root:

```sh
make -C swiftcache test
```

## Example

```text
PING
PONG
SET name swiftcache
OK
GET name
swiftcache
SET token abc EX 5
OK
TTL token
5
DBSIZE
2
```

## Documentation

- [Configuration](docs/configuration.md)
- [Command Reference](docs/commands.md)
- [Persistence](docs/persistence.md)
- [Protocols](docs/protocols.md)

## Project Structure

```text
swiftcache/
├── src/
│   ├── main.cpp
│   ├── core/
│   ├── networking/
│   ├── parser/
│   ├── commands/
│   ├── datastore/
│   └── utils/
├── tests/
├── storage/
├── CMakeLists.txt
└── Makefile
docs/
├── commands.md
├── configuration.md
├── persistence.md
└── protocols.md
```

## Requirements

- C++17 compiler
- CMake 3.10 or newer
- Make
- Linux/macOS-style socket environment
- `telnet`, `nc`, or any TCP client for manual testing

## Architecture

- `core/` defines command abstractions, command metadata, the command registry, server metrics, expiry worker, snapshot persistence, and AOF persistence.
- `commands/` contains domain-specific command implementations grouped by data type.
- `datastore/` owns typed in-memory storage, expiration, eviction, and synchronization.
- `parser/` converts inline and RESP requests into command tokens.
- `networking/` owns socket setup, accept loop, per-client handling, Pub/Sub routing, client metadata, and protocol-aware response formatting.
- `storage/` stores persistence files created by SwiftCache.
