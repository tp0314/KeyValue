add_executable(test_kv_store test_kv_store.cpp)
target_link_libraries(test_kv_store PRIVATE kvstore_core)
add_test(NAME kv_store_tests COMMAND test_kv_store)

add_executable(test_wal test_wal.cpp)
target_link_libraries(test_wal PRIVATE kvstore_core)
add_test(NAME wal_tests COMMAND test_wal)
