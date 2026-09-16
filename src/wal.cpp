#include "wal.h"

#include <fcntl.h>

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

#include <fstream>
#include <stdexcept>
#include <system_error>

namespace kvstore {

namespace {

// Writes `len` bytes from `data`, retrying on partial writes and EINTR.
// A single ::write() call is not guaranteed to write everything you hand
// it -- this loop is what makes appendEntry's writes actually complete.
void writeAll(int fd, const char* data, size_t len) {
    size_t written = 0;
    while (written < len) {
        ssize_t n = ::write(fd, data + written, len - written);
        if (n < 0) {
            if (errno == EINTR) continue;
            throw std::system_error(errno, std::generic_category(), "WAL write failed");
        }
        written += static_cast<size_t>(n);
    }
}

}  // namespace

WriteAheadLog::WriteAheadLog(std::string path) : path_(std::move(path)) {
    openForAppend();
}

WriteAheadLog::~WriteAheadLog() {
    if (fd_ >= 0) {
        ::close(fd_);
    }
}

void WriteAheadLog::openForAppend() {
    fd_ = ::open(path_.c_str(), O_WRONLY | O_APPEND | O_CREAT, 0644);
    if (fd_ < 0) {
        throw std::system_error(errno, std::generic_category(),
                                 "failed to open WAL file: " + path_);
    }
}

void WriteAheadLog::appendPut(const std::string& key, const std::string& value) {
    appendEntry(WalEntry{WalOpType::Put, key, value});
}

void WriteAheadLog::appendRemove(const std::string& key) {
    appendEntry(WalEntry{WalOpType::Remove, key, ""});
}

// On-disk record layout (all integers are native-endian, fine since this
// file is only ever read back on the machine that wrote it):
//
//   [ op : 1 byte ][ keyLen : 4 bytes ][ key bytes ][ valLen : 4 bytes ][ value bytes ]
//
// Binary + length-prefixed rather than a delimited text format so keys or
// values containing '\n', '\t', etc. can never corrupt the log.
void WriteAheadLog::appendEntry(const WalEntry& entry) {
    std::lock_guard<std::mutex> lock(mutex_);

    const uint8_t op = static_cast<uint8_t>(entry.op);
    const uint32_t keyLen = static_cast<uint32_t>(entry.key.size());
    const uint32_t valLen = static_cast<uint32_t>(entry.value.size());

    writeAll(fd_, reinterpret_cast<const char*>(&op), sizeof(op));
    writeAll(fd_, reinterpret_cast<const char*>(&keyLen), sizeof(keyLen));
    writeAll(fd_, entry.key.data(), keyLen);
    writeAll(fd_, reinterpret_cast<const char*>(&valLen), sizeof(valLen));
    writeAll(fd_, entry.value.data(), valLen);

    // fsync forces the write out of the OS page cache and onto the
    // physical disk. Without it, "durable" here would only mean
    // "survives a process crash" -- a power loss could still lose
    // acknowledged writes that the OS hadn't flushed yet. This is the
    // classic WAL durability-vs-throughput trade-off: fsync'ing every
    // single write is the safest option and the simplest to reason
    // about, at the cost of one disk round-trip per write. Batching
    // several writes per fsync is the usual next optimization.
#if defined(_WIN32)
    // MinGW/Windows has no fsync(); _commit() is the direct equivalent --
    // it flushes a file descriptor's OS buffers to disk with the same
    // guarantee.
    if (::_commit(fd_) != 0) {
#else
    if (::fsync(fd_) != 0) {
#endif
        throw std::system_error(errno, std::generic_category(), "WAL fsync failed");
    }
}

void WriteAheadLog::replay(const std::function<void(const WalEntry&)>& onEntry) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::ifstream in(path_, std::ios::binary);
    if (!in.is_open()) {
        return;  // No log file yet -- fresh store, nothing to recover.
    }

    while (true) {
        uint8_t op = 0;
        in.read(reinterpret_cast<char*>(&op), sizeof(op));
        if (!in) break;  // Clean EOF between records.

        uint32_t keyLen = 0;
        in.read(reinterpret_cast<char*>(&keyLen), sizeof(keyLen));
        if (!in) break;  // Torn write: process died mid-append. Discard.

        std::string key(keyLen, '\0');
        in.read(key.data(), keyLen);
        if (!in) break;

        uint32_t valLen = 0;
        in.read(reinterpret_cast<char*>(&valLen), sizeof(valLen));
        if (!in) break;

        std::string value(valLen, '\0');
        in.read(value.data(), valLen);
        if (!in) break;

        onEntry(WalEntry{static_cast<WalOpType>(op), std::move(key), std::move(value)});
    }
}

}  // namespace kvstore