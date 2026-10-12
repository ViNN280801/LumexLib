/*
 * Copyright (c) 2026 Vladislav Semykin <vladislav.semykin@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/**
 * @file LumexTestConfig.hpp
 * @brief Environment-driven configuration shared by the concurrency tests:
 * seed, thread counts, work scale, soak time and the build flavor.
 * @details Header-only, C++11, no dependency on the library under test, so
 * the tests of `core/atomic` and `core/hazard_pointer` read the same knobs:
 *
 * | Variable | Meaning | Default |
 * | --- | --- | --- |
 * | `LUMEX_TEST_SEED` | base seed of every pseudo-random schedule | 20261008 |
 * | `LUMEX_TEST_THREADS` | comma-separated thread counts, e.g. `1,2,4,8` |
 * 1,2,4,8 and one count above the cores | | `LUMEX_TEST_SCALE` | positive
 * integer, multiplies the work of every test | 1 | | `LUMEX_TEST_SOAK` |
 * non-zero lets the soak tests run | off | | `LUMEX_TEST_SOAK_SECONDS` |
 * length of one soak test | 20 | | `LUMEX_TEST_WATCHDOG_SECONDS` | time after
 * which a hung test aborts | 120 |
 *
 * A failing test prints the seed and the thread count (see
 * `LumexTestReplay.hpp`), so a failure is replayed with the same two
 * variables.
 */
#ifndef LUMEX_TESTS_SUPPORT_TEST_CONFIG_HPP
#define LUMEX_TESTS_SUPPORT_TEST_CONFIG_HPP

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

// Sanitizer detection: GCC defines __SANITIZE_*, Clang answers __has_feature.
#if defined(__SANITIZE_ADDRESS__)
#define LUMEX_TEST_HAS_ASAN 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
#define LUMEX_TEST_HAS_ASAN 1
#endif
#endif
#if !defined(LUMEX_TEST_HAS_ASAN)
#define LUMEX_TEST_HAS_ASAN 0
#endif

#if defined(__SANITIZE_THREAD__)
#define LUMEX_TEST_HAS_TSAN 1
#elif defined(__has_feature)
#if __has_feature(thread_sanitizer)
#define LUMEX_TEST_HAS_TSAN 1
#endif
#endif
#if !defined(LUMEX_TEST_HAS_TSAN)
#define LUMEX_TEST_HAS_TSAN 0
#endif

#if defined(__has_feature)
#if __has_feature(memory_sanitizer)
#define LUMEX_TEST_HAS_MSAN 1
#endif
#endif
#if !defined(LUMEX_TEST_HAS_MSAN)
#define LUMEX_TEST_HAS_MSAN 0
#endif

namespace lumex_test
{
/**
 * @brief Reads an integer environment variable.
 * @param name Name of the variable.
 * @param minimum Smallest accepted value.
 * @param maximum Largest accepted value.
 * @param fallback Returned when the variable is unset, malformed or out of
 * range.
 */
inline long
env_long (char const *name, long minimum, long maximum, long fallback)
{
  char const *text = std::getenv (name);
  if (text == nullptr || *text == '\0')
    return fallback;
  char *end = nullptr;
  long const value = std::strtol (text, &end, 10);
  if (end == text || value < minimum || value > maximum)
    return fallback;
  return value;
}

/// True when the variable is set to anything but "" and "0".
inline bool
env_flag (char const *name)
{
  char const *text = std::getenv (name);
  return text != nullptr && *text != '\0'
         && !(text[0] == '0' && text[1] == '\0');
}

/// Hardware threads of the machine, at least 1.
inline unsigned
hardware_threads ()
{
  unsigned const n = std::thread::hardware_concurrency ();
  return n == 0 ? 1u : n;
}

/// True when the build slows threads down by instrumentation (a sanitizer).
inline bool
instrumented_build ()
{
  return LUMEX_TEST_HAS_ASAN || LUMEX_TEST_HAS_TSAN || LUMEX_TEST_HAS_MSAN;
}

/**
 * @brief Divisor applied to the work of a test on a slow build.
 * @details ThreadSanitizer makes a test about ten times slower,
 * AddressSanitizer about two to three times, and a build without optimization
 * about three times; the counts shrink accordingly so every build finishes in
 * a similar time. The shape of a test (threads, interleavings) does not
 * change.
 */
inline int
slowdown_divisor ()
{
  int divisor = 1;
#if LUMEX_TEST_HAS_TSAN
  divisor = 6;
#elif LUMEX_TEST_HAS_ASAN || LUMEX_TEST_HAS_MSAN
  divisor = 3;
#endif
#if !defined(NDEBUG)
  divisor *= 2;
#endif
  return divisor;
}

/// Value of `LUMEX_TEST_SCALE` (1 to 1000), 1 when unset.
inline int
scale_factor ()
{
  return static_cast<int> (env_long ("LUMEX_TEST_SCALE", 1, 1000, 1));
}

/**
 * @brief Work of a test: @p base scaled by `LUMEX_TEST_SCALE` and divided by
 * the slowdown of the build, at least 1.
 */
inline int
scaled (int base)
{
  long const value
      = static_cast<long> (base) * scale_factor () / slowdown_divisor ();
  return value < 1 ? 1 : static_cast<int> (value);
}

/// Base seed: `LUMEX_TEST_SEED` or 20261008.
inline std::uint64_t
base_seed ()
{
  long const value = env_long ("LUMEX_TEST_SEED", 1, 2147483647L, 0);
  return value == 0 ? 20261008u : static_cast<std::uint64_t> (value);
}

/// True when `LUMEX_TEST_SEED` was given, i.e. a run is being replayed.
inline bool
seed_is_pinned ()
{
  return env_long ("LUMEX_TEST_SEED", 1, 2147483647L, 0) != 0;
}

/**
 * @brief Derives an independent seed from the base seed and two numbers
 * (SplitMix64 finalizer), so every test, thread and round has its own stream
 * and the whole run is reproduced by the base seed alone.
 */
inline std::uint64_t
derive_seed (std::uint64_t base, std::uint64_t a, std::uint64_t b = 0)
{
  std::uint64_t z = base + 0x9E3779B97F4A7C15ull * (a + 1)
                    + 0xBF58476D1CE4E5B9ull * (b + 1);
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
  return z ^ (z >> 31);
}

/**
 * @brief Thread counts of a test: `LUMEX_TEST_THREADS` or 1, 2, 4, 8 and one
 * count above the number of cores (at least 12, at most 32).
 * @param include_oversubscribed Adds the count above the cores to the
 * default list (a list from the variable is used as given).
 */
inline std::vector<int>
thread_counts (bool include_oversubscribed = true)
{
  std::vector<int> counts;
  char const *text = std::getenv ("LUMEX_TEST_THREADS");
  if (text != nullptr)
    {
      char const *p = text;
      while (*p != '\0')
        {
          char *end = nullptr;
          long const value = std::strtol (p, &end, 10);
          if (end == p)
            {
              ++p;
              continue;
            }
          if (value >= 1 && value <= 256)
            counts.push_back (static_cast<int> (value));
          p = end;
        }
    }
  if (counts.empty ())
    {
      counts.push_back (1);
      counts.push_back (2);
      counts.push_back (4);
      counts.push_back (8);
      if (include_oversubscribed)
        {
          int over = static_cast<int> (hardware_threads ()) + 4;
          counts.push_back (std::min (32, std::max (12, over)));
        }
    }
  return counts;
}

/// True when the soak tests were asked for (`LUMEX_TEST_SOAK`).
inline bool
soak_requested ()
{
  return env_flag ("LUMEX_TEST_SOAK");
}

/// Length of one soak test in seconds (`LUMEX_TEST_SOAK_SECONDS`, 1 to 86400).
inline int
soak_seconds ()
{
  return static_cast<int> (env_long ("LUMEX_TEST_SOAK_SECONDS", 1, 86400, 20));
}

/// Watchdog limit in seconds (`LUMEX_TEST_WATCHDOG_SECONDS`, 1 to 86400).
inline int
watchdog_limit_seconds ()
{
  return static_cast<int> (
      env_long ("LUMEX_TEST_WATCHDOG_SECONDS", 1, 86400, 120));
}

/// "name=value" list of the replay variables, for failure messages.
inline std::string
replay_text (std::uint64_t seed, int threads)
{
  return "replay with LUMEX_TEST_SEED=" + std::to_string (seed)
         + " LUMEX_TEST_THREADS=" + std::to_string (threads);
}
} // namespace lumex_test

#endif // !LUMEX_TESTS_SUPPORT_TEST_CONFIG_HPP
