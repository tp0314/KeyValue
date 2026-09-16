#include <iostream>
#include <memory>
#include <thread>
#include <vector>

#include "kv_store.h"
#include "wal.h"

int main() {
    using namespace kvstore;

    auto wal = std::make_unique<WriteAheadLog>("kvstore.wal");
    KeyValueStore store(std::move(wal));

    std::cout << "Recovered " << store.size()
              << " key(s) from kvstore.wal on startup.\n";

    std::vector<std::thread> clients;
    clients.emplace_back([&]() { store.put("system_status", "online"); });
    for (int i = 0; i < 5; ++i) {
        clients.emplace_back([&]() {
            auto v = store.get("system_status");
            std::cout << "Thread " << std::this_thread::get_id()
                      << " read: " << v.value_or("<missing>") << "\n";
        });
    }
    for (auto& t : clients) t.join();

    std::cout << "Final size: " << store.size()
              << ". Run again to see recovery pick this up.\n";
    return 0;
}