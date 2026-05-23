# SwiftCache

SwiftCache is a Redis-inspired in-memory datastore written in C++17. 

The current implementation supports strings, lists, hashes, sets, TTL, automatic expiration, append-only file persistence, basic server metrics, inline text commands, RESP requests, and multiple threaded TCP clients on `localhost:6379`.


## Features

- C++17 implementation
- TCP socket server on `localhost:6379`
- Inline text protocol for manual `telnet`/`nc` usage
- RESP array/bulk-string request parsing for Redis-style clients
- Multiple threaded client connections
- Graceful client disconnect handling
- Command registry pattern with one command per file
- Thread-safe in-memory datastore
- Typed values: string, list, hash, and set
- TTL support with lazy expiration on access
- Background expiry worker running once per second
- Append-only file persistence with startup replay
- Keyspace inspection with `KEYS`, `TYPE`, `SCAN`, `RENAME`, and `FLUSHDB`
- Server metrics through `INFO`
- CMake and Makefile build support
- Unit tests for core command behavior

## Project Structure

```text
swiftcache/
├── src/
│   ├── main.cpp
│   ├── core/
│   ├── networking/
│   ├── parser/
│   ├── commands/
│   │   ├── hash/
│   │   ├── key/
│   │   ├── list/
│   │   ├── set/
│   │   ├── string/
│   │   └── system/
│   ├── datastore/
│   └── utils/
├── tests/
├── storage/
├── CMakeLists.txt
├── Makefile
└── README.md
```

## Architecture

- `core/` defines command abstractions, the command registry, server metrics, expiry worker, and AOF persistence.
- `commands/` contains domain-specific command implementations grouped by data type.
- `datastore/` owns typed in-memory storage and synchronization.
- `parser/` converts client input into command tokens.
- `networking/` owns socket setup, accept loop, per-client handling, and protocol-aware response formatting.
- `storage/` is reserved for future persistence work.

The datastore uses `ValueObject` with typed payloads for strings, lists, hashes, and sets. All datastore operations are protected by a mutex, and expiration is enforced both lazily during access and actively by the expiry worker.

Mutating commands are appended to `storage/swiftcache.aof` as RESP frames after successful execution. On startup, SwiftCache replays the AOF before accepting clients.

## Requirements

- C++17 compiler
- CMake 3.10 or newer
- Make
- Linux/macOS-style socket environment
- `telnet`, `nc`, or any TCP client for manual testing

## Installation And Setup

Clone or open the repository, then build from the `swiftcache` directory:

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

Or use the Makefile from the repository root:

```sh
make -C swiftcache build
make -C swiftcache run
```

Run tests:

```sh
make -C swiftcache test
```

## Connecting And Protocols

SwiftCache listens on `localhost:6379`.

```sh
telnet localhost 6379
```

Inline clients receive a greeting before the first inline command response:

```text
Connected to SwiftCache
```

Commands are line-oriented and space-delimited:

```text
COMMAND arg1 arg2
```

Current parser support is intentionally simple. Values with spaces are not yet supported.

SwiftCache also accepts RESP array/bulk-string requests, the request format used by Redis clients:

```text
*3
$3
SET
$4
name
$10
swiftcache
```

RESP clients receive RESP-formatted replies:

```text
+OK
```

For RESP requests, SwiftCache does not send the inline greeting, so clients can parse the first server reply as a protocol response.

## Persistence

SwiftCache uses append-only file persistence for mutating commands. The AOF file is stored at:

```text
storage/swiftcache.aof
```

Logged commands include writes and keyspace mutations such as:

- `SET`, `MSET`, `DEL`
- `EXPIRE`, `PERSIST`
- `INCR`, `DECR`, `APPEND`
- `LPUSH`, `RPUSH`, `LPOP`, `RPOP`
- `HSET`, `HDEL`
- `SADD`, `SREM`
- `RENAME`, `FLUSHDB`

Read-only commands such as `GET`, `TTL`, `KEYS`, `INFO`, and `SMEMBERS` are not written to the AOF.

To verify persistence:

1. Start SwiftCache.
2. Write data:

```text
SET persisted value
OK
SADD tags cache systems
2
```

3. Stop the server.
4. Start it again.
5. Read the data:

```text
GET persisted
value
SMEMBERS tags
cache
systems
```

The AOF is intentionally simple and append-only. Compaction and snapshotting are future persistence improvements.

## Command Reference

### Server Commands

| Command | Description |
| --- | --- |
| `PING` | Returns `PONG`. |
| `INFO` | Returns server and datastore metrics. |

### Key Commands

| Command | Description |
| --- | --- |
| `DEL key` | Deletes a key. Returns `1` if removed, otherwise `0`. |
| `EXISTS key` | Returns `1` if the key exists and is not expired, otherwise `0`. |
| `KEYS [pattern]` | Returns keys matching a glob-style pattern. Defaults to `*`. |
| `TYPE key` | Returns `string`, `list`, `hash`, `set`, or `none`. |
| `RENAME source destination` | Renames an existing key while preserving its value and TTL. |
| `FLUSHDB` | Removes all keys from the current datastore. |
| `SCAN 0 [MATCH pattern]` | Returns cursor `0` and a sorted snapshot of matching keys. |

### TTL Commands

| Command | Description |
| --- | --- |
| `SET key value EX seconds` | Sets a string key with TTL. |
| `EXPIRE key seconds` | Adds or replaces TTL for an existing key. |
| `TTL key` | Returns remaining TTL, `-1` for no TTL, or `-2` for missing keys. |
| `PERSIST key` | Removes TTL from a key. |

### String Commands

| Command | Description |
| --- | --- |
| `SET key value` | Sets a string value. |
| `GET key` | Gets a string value or `(nil)`. |
| `INCR key` | Increments an integer string by one. |
| `DECR key` | Decrements an integer string by one. |
| `APPEND key value` | Appends to a string and returns the new length. |
| `STRLEN key` | Returns string length, or `0` for missing keys. |
| `MGET key [key ...]` | Gets multiple string values. |
| `MSET key value [key value ...]` | Sets multiple string values. |

### List Commands

| Command | Description |
| --- | --- |
| `LPUSH key value [value ...]` | Pushes one or more values to the left side of a list. |
| `RPUSH key value [value ...]` | Pushes one or more values to the right side of a list. |
| `LPOP key` | Pops from the left side of a list. |
| `RPOP key` | Pops from the right side of a list. |
| `LRANGE key start stop` | Returns an inclusive list range. Negative indexes are supported. |

### Hash Commands

| Command | Description |
| --- | --- |
| `HSET key field value` | Sets a field in a hash. Returns `1` for new field, `0` for update. |
| `HGET key field` | Gets a field value or `(nil)`. |
| `HDEL key field` | Deletes a field. Returns `1` if removed, otherwise `0`. |
| `HEXISTS key field` | Returns `1` if the field exists, otherwise `0`. |
| `HGETALL key` | Returns field/value pairs, one item per line. |

### Set Commands

| Command | Description |
| --- | --- |
| `SADD key member [member ...]` | Adds members to a set. Returns count of newly added members. |
| `SREM key member [member ...]` | Removes members from a set. Returns count removed. |
| `SISMEMBER key member` | Returns `1` if member exists, otherwise `0`. |
| `SMEMBERS key` | Returns all set members, one per line. |
| `SCARD key` | Returns set cardinality. |

## Example Sessions

### Server And Keys

```text
PING
PONG
INFO
{
 totalKeys: 0,
 connectedClients: 1,
 totalCommands: 2,
 uptimeSeconds: 4
}
```

```text
SET name swiftcache
OK
EXISTS name
1
TYPE name
string
KEYS n*
name
RENAME name project:name
OK
SCAN 0 MATCH project:*
0
project:name
DEL name
0
DEL project:name
1
GET project:name
(nil)
```

```text
SET temp value
OK
FLUSHDB
OK
KEYS
```

### Strings

```text
SET name swiftcache
OK
GET name
swiftcache
APPEND name -store
16
STRLEN name
16
```

```text
MSET visits 10 mode fast
OK
INCR visits
11
DECR visits
10
MGET visits mode missing
10
fast
(nil)
```

### TTL And Expiration

```text
SET token abc EX 5
OK
GET token
abc
TTL token
5
```

After five seconds:

```text
GET token
(nil)
TTL token
-2
```

```text
SET session active
OK
EXPIRE session 120
1
TTL session
120
PERSIST session
1
TTL session
-1
```

### Lists

```text
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
LRANGE queue 0 -1
b
c
```

### Hashes

```text
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
```

### Sets

```text
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
SMEMBERS tags
cache
systems
```

## Error Behavior

SwiftCache returns simple text errors:

```text
ERR unknown command 'COMMAND'
ERR wrong number of arguments for SET
ERR wrong type for SADD
ERR value is not an integer
```

Missing values generally return `(nil)` for read commands and `0` for delete/remove/existence checks.

## Development

Add new commands by creating a command class under the relevant `src/commands/<domain>/` directory and registering it in `src/commands/CommandFactory.cpp`.

For new data types, extend `ValueObject` and add typed operations to `DataStore`. Keep command classes thin: they should validate arguments, call the datastore, and format responses.

## Tests

```sh
make -C swiftcache test
```

The current tests cover inline parsing, RESP request parsing, strings, TTL, lists, hashes, sets, keyspace commands, and AOF replay through the command registry.

## Roadmap

Potential next phases:

- Snapshot persistence
- Pub/Sub
- Authentication
- LRU/LFU eviction
- Replication starter
- Thread pool or event-loop networking
- Integration tests over TCP
