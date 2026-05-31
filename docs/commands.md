# Command Reference

Commands are case-insensitive. Inline commands are space-delimited; use RESP requests when a value or Pub/Sub message needs spaces.

## Server And Admin

| Command | Description |
| --- | --- |
| `AUTH password` | Authenticates the current connection when password authentication is enabled. |
| `PING` | Returns `PONG`. |
| `INFO` | Returns server, persistence, command, client, and datastore metrics. |
| `CONFIG GET key` | Returns runtime configuration pairs for `databases`, `readonly`, `max_keys`, `max_memory`, `eviction_policy`, `requirepass`, or `*`. |
| `COMMAND STATS` | Returns per-command execution counters. |
| `READONLY [ON\|OFF]` | Returns read-only status as `1` or `0`, or toggles read-only mode at runtime. |
| `SAVE` | Writes a foreground snapshot immediately and checkpoints the AOF when enabled. |
| `LASTSAVE` | Returns the Unix timestamp of the last successful snapshot, or `0` if none has completed. |

## Client Commands

| Command | Description |
| --- | --- |
| `CLIENT LIST` | Returns active client connections with ID, DB, protocol, auth state, name, subscription count, and connected age. |
| `CLIENT INFO` | Returns the same metadata for the current connection only. |
| `CLIENT COUNT` | Returns the current number of connected clients. |
| `CLIENT SETNAME name` | Sets a debug name for the current connection. |
| `CLIENT GETNAME` | Returns the current connection name or `(nil)`. |
| `CLIENT KILL id` | Closes the client connection with the matching ID. Returns `1` if found, otherwise `0`. |

Example:

```text
CLIENT SETNAME worker-1
OK
CLIENT INFO
id=1 fd=4 db=0 protocol=inline authenticated=1 name=worker-1 subscriptions=0 age_seconds=8
CLIENT COUNT
1
```

## Pub/Sub

Pub/Sub subscriptions are connection-local and are not persisted to snapshots or the AOF.

| Command | Description |
| --- | --- |
| `SUBSCRIBE channel [channel ...]` | Subscribes the current client connection to one or more channels. |
| `PUBLISH channel message` | Sends a message to all clients subscribed to the channel. Returns the number of clients that received it. |
| `UNSUBSCRIBE [channel ...]` | Removes the current client connection from one or more channels. With no channel, removes all subscriptions. |

## Keys And Databases

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

Example:

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

## TTL

| Command | Description |
| --- | --- |
| `SET key value EX seconds` | Sets a string key with TTL. |
| `EXPIRE key seconds` | Adds or replaces TTL for an existing key. |
| `TTL key` | Returns remaining TTL, `-1` for no TTL, or `-2` for missing keys. |
| `PERSIST key` | Removes TTL from a key. |

## Strings

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

## Lists

| Command | Description |
| --- | --- |
| `LPUSH key value [value ...]` | Pushes one or more values to the left side of a list. |
| `RPUSH key value [value ...]` | Pushes one or more values to the right side of a list. |
| `LPOP key` | Pops from the left side of a list. |
| `RPOP key` | Pops from the right side of a list. |
| `LRANGE key start stop` | Returns an inclusive list range. Negative indexes are supported. |

## Hashes

| Command | Description |
| --- | --- |
| `HSET key field value` | Sets a field in a hash. Returns `1` for new field, `0` for update. |
| `HGET key field` | Gets a field value or `(nil)`. |
| `HDEL key field` | Deletes a field. Returns `1` if removed, otherwise `0`. |
| `HEXISTS key field` | Returns `1` if the field exists, otherwise `0`. |
| `HGETALL key` | Returns field/value pairs, one item per line. |

## Sets

| Command | Description |
| --- | --- |
| `SADD key member [member ...]` | Adds members to a set. Returns count of newly added members. |
| `SREM key member [member ...]` | Removes members from a set. Returns count removed. |
| `SISMEMBER key member` | Returns `1` if member exists, otherwise `0`. |
| `SMEMBERS key` | Returns all set members, one per line. |
| `SCARD key` | Returns set cardinality. |

## Read-Only Mode

When read-only mode is active, write commands are rejected with:

```text
ERR server is read-only
```

Reads, `AUTH`, `SELECT`, `SUBSCRIBE`, `UNSUBSCRIBE`, `READONLY`, `SAVE`, `LASTSAVE`, `CONFIG GET`, `COMMAND STATS`, and `CLIENT` inspection commands remain available. `PUBLISH` is rejected because it produces a runtime side effect.
