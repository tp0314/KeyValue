#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>

namespace kvstore {

enum class WalOpType : uint8_t {
    Put = 1,
    Remove = 2,
};

struct WalEntry {
    WalOpType op;
    std::string key;
    std::string value;  // empty for Remove
};

// Append-only, crash-recoverable log.
//
// Every entry is written and fsync'd to disk before the corresponding
// put()/remove() call on KeyValueStore returns. That ordering is what makes
// the store "log-before-apply": if the process dies at any point after a
// call returns, replaying this file on the next startup reconstructs
// exactly the state that had been acknowledged, no more and no less.
class WriteAheadLog {
public:
    explicit WriteAheadLog(std::string path);
    ~WriteAheadLog();

    WriteAheadLog(const WriteAheadLog&) = delete;
    WriteAheadLog& operator=(const WriteAheadLog&) = delete;

    void appendPut(const std::string& key, const std::string& value);
    void appendRemove(const std::string& key);

    // Reads every complete entry in the log file, in order, invoking
    // onEntry for each. If the file ends with a torn (partially written)
    // record -- e.g. the process crashed mid-write -- that final partial
    // record is silently discarded rather than treated as corruption.
    void replay(const std::function<void(const WalEntry&)>& onEntry);

private:
    void appendEntry(const WalEntry& entry);
    void openForAppend();

    std::string path_;
    int fd_ = -1;
    std::mutex mutex_;
};

}  // namespace kvstore
