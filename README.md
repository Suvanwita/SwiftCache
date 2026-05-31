# SwiftCache

SwiftCache is a Redis-inspired in-memory datastore written in C++17. 

The current implementation supports strings, lists, hashes, sets, logical databases, optional password authentication, read-only mode, TTL, automatic expiration, snapshot persistence, append-only file persistence, basic server metrics, Pub/Sub messaging, inline text commands, RESP requests, and multiple threaded TCP clients on a configurable TCP host/port.


## Features

- C++17 implementation
- TCP socket server on `localhost:6379`
- Configurable bind host, port, password authentication, read-only mode, logical database count, persistence paths, persistence modes, and eviction policy
- Inline text protocol for manual `telnet`/`nc` usage
- RESP array/bulk-string request parsing for Redis-style clients
- Multiple threaded client connections
- Graceful client disconnect handling
- Command registry pattern with one command per file
- Thread-safe in-memory datastore
- Typed values: string, list, hash, and set
- TTL support with lazy expiration on access
- Background expiry worker running once per second
- Periodic snapshot persistence with startup restore
- Append-only file persistence with startup replay
- Pub/Sub messaging with `SUBSCRIBE`, `PUBLISH`, and `UNSUBSCRIBE`
- Optional key-count and estimated-memory eviction limits
- Eviction policies: `allkeys-lru`, `volatile-lru`, `ttl-priority`, and `random`
- Keyspace inspection with `DBSIZE`, `MEMORY USAGE`, `KEYS`, `TYPE`, `SCAN`, `RENAME`, `MOVE`, `FLUSHDB`, and `FLUSHALL`
- Connection-local logical database selection with `SELECT`
- Optional password authentication with `AUTH`
- Configurable and runtime-switchable read-only mode with `READONLY`
- Expanded server, persistence, and datastore metrics through `INFO`
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

- `core/` defines command abstractions, the command registry, server metrics, expiry worker, snapshot persistence, and AOF persistence.
- `commands/` contains domain-specific command implementations grouped by data type.
- `datastore/` owns typed in-memory storage and synchronization.
- `parser/` converts client input into command tokens.
- `networking/` owns socket setup, accept loop, per-client handling, and protocol-aware response formatting.
- `storage/` stores persistence files created by SwiftCache.

The datastore uses `ValueObject` with typed payloads for strings, lists, hashes, and sets. All datastore operations are protected by a mutex, and expiration is enforced both lazily during access and actively by the expiry worker.

SwiftCache loads `storage/swiftcache.snapshot` first, then replays the remaining commands from `storage/swiftcache.aof` before accepting clients. Snapshots store every configured logical database, and AOF replay restores writes into their selected database. Mutating commands are executed and appended under the same persistence lock, so periodic snapshots can safely compact the AOF without losing or duplicating writes.

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

Run with custom server and persistence settings:

```sh
./SwiftCache --host 0.0.0.0 --port 6380 --aof storage/dev.aof --snapshot storage/dev.snapshot
```

Run with password authentication enabled:

```sh
./SwiftCache --host 0.0.0.0 --requirepass strong-password
```

Run in read-only mode for maintenance:

```sh
./SwiftCache --readonly
```

Run with a custom number of logical databases:

```sh
./SwiftCache --databases 32
```

Disable persistence modes when you want a purely in-memory development server:

```sh
./SwiftCache --no-aof --no-snapshot
```

Run with a bounded cache policy:

```sh
./SwiftCache --max-keys 10000 --eviction-policy allkeys-lru
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

## Configuration

SwiftCache can be configured with CLI arguments, a config file, or both. When both are used, the config file is loaded first and CLI arguments override it.

### CLI Options

| Option | Description |
| --- | --- |
| `--host <host>` | Bind host. Defaults to `localhost`. Use `0.0.0.0` or `*` to listen on all IPv4 interfaces. |
| `--port <port>` | Bind port. Defaults to `6379`. |
| `--aof <path>` | Append-only file path. Defaults to `storage/swiftcache.aof`. |
| `--snapshot <path>` | Snapshot file path. Defaults to `storage/swiftcache.snapshot`. |
| `--config <path>` | Loads a key/value config file before applying CLI overrides. |
| `--requirepass <password>` | Requires clients to send `AUTH password` before other commands. Unset by default. |
| `--readonly` | Starts the server in read-only mode. |
| `--databases <count>` | Number of logical databases. Defaults to `16`. |
| `--max-keys <count>` | Evicts keys when the datastore grows above this key count. `0` disables the key-count limit. |
| `--max-memory <bytes>` | Evicts keys when estimated stored data grows above this size. `0` disables the memory limit. |
| `--eviction-policy <policy>` | Eviction policy. Supported values: `none`, `allkeys-lru`, `volatile-lru`, `ttl-priority`, `random`. |
| `--no-aof` | Disables AOF startup replay and command appends. |
| `--no-snapshot` | Disables snapshot startup restore and background snapshot saves. |
| `--help` | Prints available options. |

### Config File

Config files use `key=value` lines. Blank lines and `#` comments are ignored.

```text
host=localhost
port=6379
aof=storage/swiftcache.aof
snapshot=storage/swiftcache.snapshot
aof_enabled=true
snapshot_enabled=true
requirepass=
readonly=false
databases=16
max_keys=0
max_memory=0
eviction_policy=none
```

Run with a config file:

```sh
./SwiftCache --config swiftcache.conf
```

Override one value from the config file:

```sh
./SwiftCache --config swiftcache.conf --port 6380
```

### Eviction Policies

Eviction is disabled by default. Set `max_keys`, `max_memory`, or both, then choose a policy.

| Policy | Behavior |
| --- | --- |
| `none` | Does not evict keys. Limits are ignored if this policy is active. |
| `allkeys-lru` | Evicts the least recently accessed key from all keys. |
| `volatile-lru` | Evicts the least recently accessed key only among keys with TTL. If no TTL keys exist, the limit may remain exceeded. |
| `ttl-priority` | Evicts keys with the nearest expiration time first. If no TTL keys exist, the limit may remain exceeded. |
| `random` | Evicts random keys. |

`max_memory` and `MEMORY USAGE` use SwiftCache's internal estimate of stored key/value data. They are useful for cache pressure behavior and key-level inspection, but they are not the same as operating-system RSS memory.

## Connecting And Protocols

SwiftCache listens on `localhost:6379` by default.

```sh
telnet localhost 6379
```

Inline clients receive a greeting before the first inline command response:

```text
Connected to SwiftCache
```

If `--requirepass` or `requirepass` is configured, each client connection must authenticate before running commands:

```text
AUTH strong-password
OK
```

Unauthenticated clients receive `NOAUTH Authentication required`. `AUTH` is connection-local and is not persisted.

When read-only mode is active, write commands are rejected with `ERR server is read-only`. Reads, `AUTH`, `SELECT`, `SUBSCRIBE`, `UNSUBSCRIBE`, and `READONLY` remain available. `PUBLISH` is rejected because it produces a runtime side effect.

Commands are line-oriented and space-delimited:

```text
COMMAND arg1 arg2
```

Current parser support is intentionally simple. Values with spaces are not yet supported.
For Pub/Sub messages that contain spaces, use a RESP client so the message can be sent as one bulk string.

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

SwiftCache combines snapshot persistence with an append-only file.

Snapshots are stored at:

```text
storage/swiftcache.snapshot
```

The snapshot worker runs periodically while the server is active. It writes the full in-memory datastore for every logical database to disk, including strings, lists, hashes, sets, creation timestamps, and TTL metadata. After a successful snapshot, SwiftCache truncates the AOF so the log only contains mutations that happened after the latest snapshot.

Use `SAVE` to trigger the same snapshot/checkpoint flow manually. `LASTSAVE` returns the Unix timestamp of the most recent successful background or manual snapshot, or `0` if no snapshot has completed.

The AOF file is stored at:

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
- `RENAME`, `MOVE`, `FLUSHDB`, `FLUSHALL`
- `SELECT` markers when writes target a different logical database

Read-only commands such as `GET`, `TTL`, `KEYS`, `INFO`, and `SMEMBERS` are not written to the AOF.

On startup, SwiftCache restores the snapshot first and then replays the AOF delta. `SELECT` markers in the AOF preserve which logical database each write targeted. This gives the server faster recovery than replaying the entire command history every time.

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

Snapshot files are written through a temporary file and atomically renamed into place after a successful save.

## Command Reference

### Server Commands

| Command | Description |
| --- | --- |
| `AUTH password` | Authenticates the current connection when password authentication is enabled. |
| `PING` | Returns `PONG`. |
| `INFO` | Returns server, persistence, command, client, and datastore metrics. |
| `CONFIG GET key` | Returns runtime configuration pairs for `databases`, `readonly`, `max_keys`, `max_memory`, `eviction_policy`, `requirepass`, or `*`. |
| `READONLY [ON\|OFF]` | Returns read-only status as `1` or `0`, or toggles read-only mode at runtime. |
| `SAVE` | Writes a foreground snapshot immediately and checkpoints the AOF when enabled. |
| `LASTSAVE` | Returns the Unix timestamp of the last successful snapshot, or `0` if none has completed. |

### Pub/Sub Commands

Pub/Sub subscriptions are connection-local and are not persisted to snapshots or the AOF.

| Command | Description |
| --- | --- |
| `SUBSCRIBE channel [channel ...]` | Subscribes the current client connection to one or more channels. |
| `PUBLISH channel message` | Sends a message to all clients subscribed to the channel. Returns the number of clients that received it. |
| `UNSUBSCRIBE [channel ...]` | Removes the current client connection from one or more channels. With no channel, removes all subscriptions. |

### Key Commands

| Command | Description |
| --- | --- |
| `SELECT index` | Selects a logical database for the current connection. Defaults to database `0`. |
| `DBSIZE` | Returns the number of live keys in the current logical database. |
| `MEMORY USAGE key` | Returns SwiftCache's estimated bytes used by a key, or `(nil)` for a missing key. |
| `DEL key` | Deletes a key. Returns `1` if removed, otherwise `0`. |
| `EXISTS key` | Returns `1` if the key exists and is not expired, otherwise `0`. |
| `KEYS [pattern]` | Returns keys matching a glob-style pattern. Defaults to `*`. |
| `TYPE key` | Returns `string`, `list`, `hash`, `set`, or `none`. |
| `RENAME source destination` | Renames an existing key while preserving its value and TTL. |
| `MOVE key db` | Moves a key from the current logical database to another one. Returns `1` if moved, otherwise `0`. |
| `FLUSHDB` | Removes all keys from the current datastore. |
| `FLUSHALL` | Removes all keys from every logical database. |
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
 estimatedBytes: 0,
 aofSizeBytes: 0,
 evictedKeys: 0,
 expiredKeys: 0,
 connectedClients: 1,
 peakConnectedClients: 1,
 totalCommands: 2,
 rejectedCommands: 0,
 lastSnapshotUnixSeconds: 0,
 commandCounts: {
  INFO: 1,
  PING: 1
 },
 uptimeSeconds: 4
}
```

```text
LASTSAVE
0
SAVE
OK
LASTSAVE
1717000000
CONFIG GET readonly
readonly
false
CONFIG GET requirepass
requirepass
******
```

```text
SET name swiftcache
OK
EXISTS name
1
TYPE name
string
MEMORY USAGE name
248
DBSIZE
1
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

```text
SET db0 value
OK
SELECT 1
OK
SET db1 value
OK
FLUSHALL
OK
DBSIZE
0
SELECT 0
OK
DBSIZE
0
```

```text
SET shared zero
OK
SELECT 1
OK
SET shared one
OK
GET shared
one
SELECT 0
OK
GET shared
zero
```

```text
SET job queued
OK
MOVE job 1
1
GET job
(nil)
SELECT 1
OK
GET job
queued
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

### Pub/Sub

Open one client and subscribe to a channel:

```text
SUBSCRIBE news
subscribe
news
1
```

Open another client and publish a message:

```text
PUBLISH news launch
1
```

The subscribed client receives:

```text
message
news
launch
```

Unsubscribe when finished:

```text
UNSUBSCRIBE news
unsubscribe
news
0
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

The current tests cover inline parsing, RESP request parsing, strings, TTL, lists, hashes, sets, keyspace commands, AOF replay, snapshot save/load, and AOF checkpoint truncation.

## Roadmap

Potential next phases:

- Authentication
- Replication starter
- Transactions
- Thread pool or event-loop networking
- Integration tests over TCP
