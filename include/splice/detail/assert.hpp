#pragma once

#include <cstdlib>
#include <print>

#if defined(NDEBUG)
#define SPLICE_ASSERT(cond, msg) ((void) 0)
#else
#define SPLICE_ASSERT(cond, msg)                                                                                       \
  do                                                                                                                   \
  {                                                                                                                    \
    if (!cond)                                                                                                         \
    {                                                                                                                  \
      std::println(stderr, "[splice] assertion failed: {}", msg);                                                      \
      std::abort();                                                                                                    \
    }                                                                                                                  \
  } while (0)
#endif
