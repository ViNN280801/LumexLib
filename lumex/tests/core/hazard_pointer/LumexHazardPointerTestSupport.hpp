/*
 * Copyright (c) 2026 Vladislav Semykin <vladislav.semykin@gmail.com>
 *
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of
 * charge, to any person obtaining a copy
 * of this software and associated
 * documentation files (the "Software"), to deal
 * in the Software without
 * restriction, including without limitation the rights
 * to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the
 * Software, and to permit persons to whom the Software is
 * furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice
 * and this permission notice shall be included in
 * all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT
 * WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO
 * THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH
 * THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/**
 * @file LumexHazardPointerTestSupport.hpp
 * @brief Shared fixtures of the hazard pointer tests.
 * @details `counters_t` counts constructions and deletions of the test
 * objects, `poisoned_node` is the protected payload of the stress tests (its
 * two words are each other's complement and its destructor overwrites them, so
 * a reader that uses a deleted node sees a broken pair or a poisoned value),
 * and the helpers read the thread count and the work scale from the
 * environment so that one binary runs with several thread counts, and shrink
 * the work under a sanitizer.
 */
#ifndef LUMEX_TESTS_CORE_HAZARD_POINTER_HAZARD_POINTER_TEST_SUPPORT_HPP
#define LUMEX_TESTS_CORE_HAZARD_POINTER_HAZARD_POINTER_TEST_SUPPORT_HPP

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/hazard_pointer/LumexHazardPointer"

#if defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__)
#define LUMEX_HP_TEST_SANITIZED 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer)
#define LUMEX_HP_TEST_SANITIZED 1
#endif
#endif
#if !defined(LUMEX_HP_TEST_SANITIZED)
#define LUMEX_HP_TEST_SANITIZED 0
#endif

// ThreadSanitizer alone: the unprotected structures of the ABA tests race on
// purpose (they read nodes the allocator hands out again), which it reports.
#if defined(__SANITIZE_THREAD__)
#define LUMEX_HP_TEST_TSAN 1
#elif defined(__has_feature)
#if __has_feature(thread_sanitizer)
#define LUMEX_HP_TEST_TSAN 1
#endif
#endif
#if !defined(LUMEX_HP_TEST_TSAN)
#define LUMEX_HP_TEST_TSAN 0
#endif

namespace lumex_hp_test
{
namespace hp = lumex::core::hazard_pointer;

/// Counts how many test objects were made and deleted.
struct counters_t
{
  std::atomic<int> constructed;
  std::atomic<int> deleted;

  counters_t () : constructed (0), deleted (0) {}
};

/// A hazard-protectable object that counts its life and poisons itself.
struct counted_node : hp::hazard_pointer_obj_base<counted_node>
{
  counted_node (counters_t &the_counters, int the_value)
      : counters (&the_counters), value (the_value)
  {
    the_counters.constructed.fetch_add (1);
  }

  counted_node (counted_node const &other)
      : hp::hazard_pointer_obj_base<counted_node> (other),
        counters (other.counters), value (other.value)
  {
    counters->constructed.fetch_add (1);
  }

  counted_node &
  operator= (counted_node const &other)
  {
    value = other.value;
    return *this;
  }

  ~counted_node ()
  {
    value = -1;
    counters->deleted.fetch_add (1);
  }

  counters_t *counters;
  int value;
};

/// The payload of the stress tests: `second == ~first` while it is alive.
struct poisoned_node : hp::hazard_pointer_obj_base<poisoned_node>
{
  /// Number of live nodes.
  static std::atomic<long> &
  alive ()
  {
    static std::atomic<long> count (0);
    return count;
  }

  explicit poisoned_node (std::uint64_t v) : first (v), second (~v)
  {
    alive ().fetch_add (1);
  }

  ~poisoned_node ()
  {
    first = 0xDEADBEEFDEADBEEFull;
    second = 0xDEADBEEFDEADBEEFull;
    alive ().fetch_sub (1);
  }

  /// True when the pair is intact (the node is alive and was not reused).
  bool
  intact () const
  {
    return second == ~first && first != 0xDEADBEEFDEADBEEFull;
  }

  std::uint64_t first;
  std::uint64_t second;
};

/// Reads an unsigned number from the environment, `fallback` when unset.
inline unsigned
env_number (char const *name, unsigned fallback)
{
  char const *text = std::getenv (name);
  if (text == nullptr || *text == '\0')
    {
      return fallback;
    }
  return static_cast<unsigned> (std::strtoul (text, nullptr, 10));
}

/// Threads of a stress test: LUMEX_HP_THREADS or the hardware count (2..8).
inline unsigned
stress_threads ()
{
  unsigned count = std::thread::hardware_concurrency ();
  count = count < 2 ? 2 : (count > 8 ? 8 : count);
  return env_number ("LUMEX_HP_THREADS", count);
}

/// Work scale of a stress test: LUMEX_HP_SCALE (percent, default 100), and a
/// tenth of that under a sanitizer.
inline unsigned
stress_scale (unsigned iterations)
{
  unsigned percent = env_number ("LUMEX_HP_SCALE", 100);
  if (LUMEX_HP_TEST_SANITIZED)
    {
      percent /= 10;
    }
  unsigned scaled = static_cast<unsigned> (
      static_cast<unsigned long long> (iterations) * percent / 100);
  return scaled < 10 ? 10 : scaled;
}
} // namespace lumex_hp_test

#endif // !LUMEX_TESTS_CORE_HAZARD_POINTER_HAZARD_POINTER_TEST_SUPPORT_HPP
