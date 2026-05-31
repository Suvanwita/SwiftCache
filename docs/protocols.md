# Protocols

SwiftCache listens on `localhost:6379` by default and supports both simple inline commands and RESP array/bulk-string requests.

## Connecting

```sh
telnet localhost 6379
```

or:

```sh
nc localhost 6379
```

Inline clients receive a greeting before the first inline command response:

```text
Connected to SwiftCache
```

RESP clients do not receive the inline greeting, so they can parse the first server reply as a protocol response.

## Inline Protocol

Commands are line-oriented and space-delimited:

```text
COMMAND arg1 arg2
```

Example:

```text
SET name swiftcache
OK
GET name
swiftcache
```

Current inline parser support is intentionally simple. Values with spaces are not yet supported. For Pub/Sub messages or values that contain spaces, use a RESP client so the message can be sent as one bulk string.

## RESP Requests

SwiftCache accepts RESP array/bulk-string requests, the request format used by Redis clients:

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

Multi-line command responses are returned as RESP arrays where practical. Nil values are returned as RESP nil bulk strings.

## Authentication

If `--requirepass` or `requirepass` is configured, each client connection must authenticate before running commands:

```text
AUTH strong-password
OK
```

Unauthenticated clients receive:

```text
NOAUTH Authentication required
```

`AUTH` is connection-local and is not persisted.

## Client Metadata

SwiftCache tracks each active TCP connection with a client ID. Client metadata includes selected DB, protocol, authentication state, optional client name, subscription count, and connected age.

```text
CLIENT SETNAME worker-1
OK
CLIENT INFO
id=1 fd=4 db=0 protocol=inline authenticated=1 name=worker-1 subscriptions=0 age_seconds=8
```

## Pub/Sub Notes

Pub/Sub subscriptions are connection-local. They are not persisted to snapshots or the AOF.

For messages that contain spaces, use RESP so the message is sent as one bulk string:

```text
PUBLISH events "hello world"
```

The quoted inline form above is illustrative; the current inline parser does not preserve quotes as one argument.
