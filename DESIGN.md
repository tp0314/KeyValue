# Design Notes

## Overview

This project is a key-value store built up in stages:

1. **Single-node core** -- in-memory store + WAL for durability (done)
2. **Networking** -- async TCP server, simple text protocol (in progress)
3. **Consensus** -- Raft-based replication across a cluster (in progress)

This document covers the reasoning behind the decisions made so far, not
just what was built.

## Concurrency model

The store is protected by a single `std::shared_mutex` covering the whole
map:

- `get()` takes a **shared** lock, so any number of reads can run
  concurrently.
- `put()` / `remove()` take a **unique** lock, so writes are fully
  serialized against everything else.

**Why one coarse lock instead of sharding?** A single-node store's write
throughput is already bounded by the WAL fsync on every write (see below),
which is far slower than the in-memory map update. Sharding the lock (e.g.
N buckets, each with its own mutex) would reduce *read* contention under
heavy concurrent reads, but wouldn't help writes, which are the actual
bottleneck. Coarse locking is the simplest correct thing that doesn't
leave performance on the table given that bottleneck -- it's a decision to
revisit if profiling later shows read contention actually matters.

## Durability: the write-ahead log

Every `put`/`remove` is logged to disk *before* it's applied to the
in-memory map ("log-before-apply"), and the log is fsync'd before the call
returns. That gives one specific guarantee: **if a caller has received
acknowledgement that a write succeeded, that write will still be there
after a crash and restart**, because either the log has the fsync'd record
or the call never returned in the first place.

### Log format

Records are binary and length-prefixed rather than a delimited text
format:

```
[ op: 1 byte ][ keyLen: 4 bytes ][ key bytes ][ valLen: 4 bytes ][ value bytes ]
```

A text format with a delimiter (e.g. comma or newline) would need
escaping logic for keys/values that happen to contain that delimiter, and
it's easy to get that escaping subtly wrong. Length-prefixing sidesteps
the problem entirely -- there's no character in a key or value that can
be misread as a record boundary.

### fsync on every write vs. batching

Right now every single write calls `fsync()`, which forces the OS to
flush the write out of the page cache and onto physical disk. This is the
safest and simplest option, but it means every write pays the cost of a
disk round-trip.

The standard next optimization is **group commit**: buffer several
pending writes, fsync once, and acknowledge all of them together. This
trades a small amount of added latency (waiting to batch) for much higher
write throughput. Not implemented yet -- noted here as the known next
step once there's a benchmark showing fsync-per-write is actually the
bottleneck, rather than optimizing blind.

### Torn writes

If the process crashes in the middle of writing a record (e.g. after the
op byte and key length are on disk but before the key bytes are), the log
ends with a partial, unreadable record. `WriteAheadLog::replay()` treats
any read failure -- at any point in a record -- as "this is the torn tail
of the log," logs nothing about it, and simply stops. Everything *before*
that point is still replayed normally.

This matters because the alternative (treating any read failure as fatal
corruption and refusing to start up) would mean a crash at exactly the
wrong instant could permanently prevent recovery -- turning a lost *last
write* into a lost *entire store*. Tested explicitly in
`tests/test_wal.cpp::test_replay_discards_torn_tail_record`, which
hand-crafts a truncated record to simulate this.

### What this WAL does *not* protect against

- Disk/media failure (single copy, single disk) -- that's what
  replication (Raft, stage 3) is for, not the WAL.
- Unbounded log growth: nothing compacts the log yet. A long-running
  store's log will grow forever. A snapshot-and-truncate mechanism
  (periodically dump the full map to a snapshot file, then truncate the
  WAL) is a known gap, not yet built.

## Roadmap / open items

- [ ] Async TCP server (Boost.Asio) with a simple text protocol
- [ ] Wire the network layer's write path through `KeyValueStore::put/remove`
- [ ] Raft integration for multi-node consensus and replication
- [ ] Snapshot + WAL compaction
- [ ] Benchmarks under concurrent load (see `benchmarks/`, not yet added)
