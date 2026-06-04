// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Yann Poirier

#pragma once
#include <cmath>
#include <cstdio>

// Tiny dependency-free test harness: each test executable's main() returns
// g_failures (0 = pass). Keeps `ctest` runnable with no network/framework.
inline int g_failures = 0;

#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      ++g_failures;                                                       \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);         \
    }                                                                     \
  } while (0)

#define CHECK_NEAR(a, b, eps)                                             \
  do {                                                                    \
    const double _a = (a), _b = (b);                                      \
    if (std::fabs(_a - _b) > (eps)) {                                     \
      ++g_failures;                                                       \
      std::printf("FAIL %s:%d: |%g - %g| > %g\n", __FILE__, __LINE__, _a, \
                  _b, static_cast<double>(eps));                          \
    }                                                                     \
  } while (0)
