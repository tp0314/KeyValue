#pragma once

#include <memory>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>

#include "wal.h"

namespace kvstore {

// Thread-safe in-memory key-value store with optional write-ahead-log
// backed durability.
//
// Concurrency model: a single std::shared_mutex guards the whole map.
// Reads take a shared (read) lock so concurrent get() calls can run in
// parallel; writes take a unique (exclusive) lock. This is a coarse-grained
// scheme -- one lock for the entire map rather than per-bucket striping --
// which is the right starting point for a single-node store where writes
// are already serialized through the WAL anyway. Sharding the lock (or the
// map) would be the next step if write contention became the bottleneck.
class KeyValueStore {
public:
    // If `wal` is non-null, every mutation is durably logged before it is
    // applied in memory, and the WAL is replayed immediately to rebuild
    // whatever state existed before the last shutdown/crash.
    explicit KeyValueStore(std::unique_ptr<WriteAheadLog> wal = nullptr);

    void put(const std::string& key, const std::string& value);
    bool remove(const std::string& key);
    std::optional<std::string> get(const std::string& key) const;
    size_t size() const;

    // Applies a mutation to memory WITHOUT writing it to this store's own
    // WAL. Used for WAL replay at startup, and later by the Raft layer,
    // whose replicated log is itself the durability mechanism -- logging
    // twice would be redundant.
    void applyPut(const std::string& key, const std::string& value);
    void applyRemove(const std::string& key);

private:
    void replayFromWal();

    std::unordered_map<std::string, std::string> db_;
    mutable std::shared_mutex rwMutex_;
    std::unique_ptr<WriteAheadLog> wal_;
};

}  // namespace kvstore