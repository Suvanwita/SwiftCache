# Persistence

SwiftCache combines snapshot persistence with an append-only file (AOF). On startup, it restores the snapshot first and then replays the AOF delta before accepting clients.

## Snapshot Persistence

Snapshots are stored at:

```text
storage/swiftcache.snapshot
```

The snapshot worker runs periodically while the server is active. It writes the full in-memory datastore for every logical database to disk, including strings, lists, hashes, sets, creation timestamps, and TTL metadata.

Snapshot files are written through a temporary file and atomically renamed into place after a successful save.

## Manual Snapshots

Use `SAVE` to trigger the same snapshot/checkpoint flow manually:

```text
SAVE
OK
```

Use `LASTSAVE` to inspect the most recent successful background or manual snapshot timestamp:

```text
LASTSAVE
1717000000
```

`LASTSAVE` returns `0` if no snapshot has completed.

## AOF Persistence

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

Read-only commands such as `GET`, `TTL`, `KEYS`, `INFO`, `SMEMBERS`, `CONFIG GET`, `COMMAND STATS`, and `CLIENT LIST` are not written to the AOF.

## Checkpointing

After a successful snapshot, SwiftCache truncates the AOF so the log only contains mutations that happened after the latest snapshot.

Mutating commands are executed and appended under the same persistence lock, so periodic snapshots and manual `SAVE` calls can safely compact the AOF without losing or duplicating writes.

## Multi-DB Restore

Snapshots store every configured logical database. The AOF preserves logical DB context using `SELECT` markers before writes that target a non-current DB.

On startup:

1. SwiftCache loads the snapshot.
2. It replays the remaining AOF commands.
3. It applies each replayed write to the correct logical database.

## Verification Example

1. Start SwiftCache.
2. Write data:

```text
SET persisted value
OK
SADD tags cache systems
2
SAVE
OK
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

## Disabled Persistence

Disable AOF and snapshot persistence for a purely in-memory development server:

```sh
./SwiftCache --no-aof --no-snapshot
```

When snapshot persistence is disabled, `SAVE` returns an error because there is no snapshot target.
