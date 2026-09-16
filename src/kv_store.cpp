#include "kv_store.h"

namespace kvstore {

KeyValueStore::KeyValueStore(std::unique_ptr<WriteAheadLog> wal)
    : wal_(std::move(wal)) {
    if (wal_) {
        replayFromWal();
    }
}

void KeyValueStore::replayFromWal() {
    // Runs during construction, before any other thread can hold a
    // reference to `this`, so no locking is needed here.
    wal_->replay([this](const WalEntry& entry) {
        switch (entry.op) {
            case WalOpType::Put:
                db_[entry.key] = entry.value;
                break;
            case WalOpType::Remove:
                db_.erase(entry.key);
                break;
        }
    });
}

void KeyValueStore::put(const std::string& key, const std::string& value) {
    std::unique_lock<std::shared_mutex> lock(rwMutex_);
    if (wal_) {
        wal_->appendPut(key, value);
    }
    db_[key] = value;
}

bool KeyValueStore::remove(const std::string& key) {
    std::unique_lock<std::shared_mutex> lock(rwMutex_);
    if (wal_) {
        wal_->appendRemove(key);
    }
    return db_.erase(key) > 0;
}

std::optional<std::string> KeyValueStore::get(const std::string& key) const {
    std::shared_lock<std::shared_mutex> lock(rwMutex_);
    auto it = db_.find(key);
    if (it != db_.end()) {
        return it->second;
    }
    return std::nullopt;
}

size_t KeyValueStore::size() const {
    std::shared_lock<std::shared_mutex> lock(rwMutex_);
    return db_.size();
}

void KeyValueStore::applyPut(const std::string& key, const std::string& value) {
    std::unique_lock<std::shared_mutex> lock(rwMutex_);
    db_[key] = value;
}

void KeyValueStore::applyRemove(const std::string& key) {
    std::unique_lock<std::shared_mutex> lock(rwMutex_);
    db_.erase(key);
}

}  // namespace kvstore