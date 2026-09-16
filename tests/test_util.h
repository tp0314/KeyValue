#pragma once

#include <iostream>

// Minimal assert-and-continue helpers -- no external test framework
// dependency, deliberately. CHECK keeps running after a failure and
// reports everything that's wrong, rather than aborting at the first one.
inline int g_failures = 0;

#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            std::cerr << "CHECK FAILED: " << #cond << " at " << __FILE__ \
                      << ":" << __LINE__ << "\n";                       \
            ++g_failures;                                               \
        }                                                                \
    } while (0)

#define CHECK_EQ(a, b)                                                  \
    do {                                                                \
        auto va = (a);                                                 \
        auto vb = (b);                                                 \
        if (!(va == vb)) {                                              \
            std::cerr << "CHECK_EQ FAILED: " << #a << " != " << #b      \
                      << " at " << __FILE__ << ":" << __LINE__ << "\n"; \
            ++g_failures;                                               \
        }                                                                \
    } while (0)