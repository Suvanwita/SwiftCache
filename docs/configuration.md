# Configuration

SwiftCache can be configured with CLI arguments, a config file, or both. When both are used, the config file is loaded first and CLI arguments override it.

## CLI Options

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
| `--eviction-policy <policy>` | Supported values: `none`, `allkeys-lru`, `volatile-lru`, `ttl-priority`, `random`. |
| `--no-aof` | Disables AOF startup replay and command appends. |
| `--no-snapshot` | Disables snapshot startup restore and background snapshot saves. |
| `--help` | Prints available options. |

## Config File

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

## Common Modes

Run with custom network and persistence paths:

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

Disable persistence for a purely in-memory development server:

```sh
./SwiftCache --no-aof --no-snapshot
```

Run with a bounded cache policy:

```sh
./SwiftCache --max-keys 10000 --eviction-policy allkeys-lru
```

## Eviction Policies

Eviction is disabled by default. Set `max_keys`, `max_memory`, or both, then choose a policy.

| Policy | Behavior |
| --- | --- |
| `none` | Does not evict keys. Limits are ignored if this policy is active. |
| `allkeys-lru` | Evicts the least recently accessed key from all keys. |
| `volatile-lru` | Evicts the least recently accessed key only among keys with TTL. If no TTL keys exist, the limit may remain exceeded. |
| `ttl-priority` | Evicts keys with the nearest expiration time first. If no TTL keys exist, the limit may remain exceeded. |
| `random` | Evicts random keys. |

`max_memory` and `MEMORY USAGE` use SwiftCache's internal estimate of stored key/value data. They are useful for cache pressure behavior and key-level inspection, but they are not the same as operating-system RSS memory.

## Runtime Configuration

Use `CONFIG GET key` to inspect selected runtime configuration:

```text
CONFIG GET readonly
readonly
false
CONFIG GET requirepass
requirepass
******
```

Supported keys are `databases`, `readonly`, `max_keys`, `max_memory`, `eviction_policy`, `requirepass`, and `*`.
