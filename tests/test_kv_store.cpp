#include <filesystem>
#include <iostream>
#include <memory>

#include "kv_store.h"
#include "test_util.h"
#include "wal.h"

using namespace kvstore;
namespace fs = std::filesystem;

void test_basic_put_get() {
    KeyValueStore store;
    store.put("a", "1");
    store.put("b", "2");
    CHECK_EQ(store.get("a").value_or(""), "1");
    CHECK_EQ(store.get("b").value_or(""), "2");
    CHECK(!store.get("missing").has_value());
}

void test_remove() {
    KeyValueStore store;
    store.put("a", "1");
    CHECK(store.remove("a"));
    CHECK(!store.get("a").has_value());
    CHECK(!store.remove("a"));
}

void test_overwrite() {
    KeyValueStore store;
    store.put("a", "1");
    store.put("a", "2");
    CHECK_EQ(store.get("a").value_or(""), "2");
}

void test_wal_recovery_survives_restart() {
    const std::string path = "test_recovery.wal";
    fs::remove(path);

    {
        auto wal = std::make_unique<WriteAheadLog>(path);
        KeyValueStore store(std::move(wal));
        store.put("x", "10");
        store.put("y", "20");
        store.remove("x");
    }

    {
        auto wal = std::make_unique<WriteAheadLog>(path);
        KeyValueStore store(std::move(wal));
        CHECK(!store.get("x").has_value());
        CHECK_EQ(store.get("y").value_or(""), "20");
        CHECK_EQ(store.size(), static_cast<size_t>(1));
    }

    fs::remove(path);
}

int main() {
    test_basic_put_get();
    test_remove();
    test_overwrite();
    test_wal_recovery_survives_restart();

    if (g_failures == 0) {
        std::cout << "All KeyValueStore tests passed.\n";
        return 0;
    }
    std::cerr << g_failures << " test(s) failed.\n";
    return 1;
}