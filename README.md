# Distributed Key-Value Store

This project was originally built a few months ago; I'm picking it back up
to reinforce my C++, systems, and concurrency skills, starting from a
single node and building outward.

**Status:** single-node core (storage + WAL) implemented and tested.
Networking and Raft consensus are in progress -- see `DESIGN.md` for the
overall architecture and roadmap of my choices. 

## Features (so far)

- Thread safe in memory store using a `std::shared_mutex` (concurrent
  reads, exclusive writes)
- Write ahead log (WAL) for durability: every write is fsync'd to disk
  before it's acknowledged, and the log is replayed on startup to recover
  state after a crash
- Binary, length prefixed log format (no delimiter/escaping issues with
  arbitrary key/value bytes)
- Graceful handling of a torn/partial write at the end of the log (the
  case where the process crashed mid append)

## Build

Requires CMake 3.16+ and a C++20 compiler.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

## Run

```bash
./build/kv_store_demo
```

Run it twice in a row from the same directory -- the second run will report
recovering the key written by the first, proving the WAL replay works.

## Test

```bash
cd build
ctest --output-on-failure
```

## Project layout

```
include/       Public headers
src/           Implementation
tests/         Unit tests (no external test framework dependency)
DESIGN.md      Architecture notes and design rationale
```
