cmake_minimum_required(VERSION 3.16)
project(kv_store LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra)

find_package(Threads REQUIRED)

add_library(kvstore_core
    src/kv_store.cpp
    src/wal.cpp
)
target_include_directories(kvstore_core PUBLIC include)
target_link_libraries(kvstore_core PUBLIC Threads::Threads)

add_executable(kv_store_demo src/main.cpp)
target_link_libraries(kv_store_demo PRIVATE kvstore_core)

enable_testing()
add_subdirectory(tests)
