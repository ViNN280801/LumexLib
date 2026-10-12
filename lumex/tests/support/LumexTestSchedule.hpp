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
 * @file LumexTestSchedule.hpp
 * @brief Seeded random streams and schedule perturbation for concurrency
 * tests.
 * @details `SeededRandom` is a deterministic SplitMix64 stream, so a run is
 * reproduced from its seed. `Schedule` describes how a thread is disturbed
 * between its operations: not at all (tight loops, hot caches), by random
 * yields, by random short sleeps, or by flushing the caches so every
 * operation starts cold. `ScheduleKind` values are listed by
 * `all_schedule_kinds ()` so a test can run the same scenario under each.
 */
#ifndef LUMEX_TESTS_SUPPORT_TEST_SCHEDULE_HPP
#define LUMEX_TESTS_SUPPORT_TEST_SCHEDULE_HPP

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <thread>
#include <vector>

namespace lumex_test
{
/// Deterministic pseudo-random stream (SplitMix64).
class SeededRandom
{
public:
  explicit SeededRandom (std::uint64_t seed) : state_ (seed) {}

  /// The next 64 random bits.
  std::uint64_t
  next ()
  {
    state_ += 0x9E3779B97F4A7C15ull;
    std::uint64_t z = state_;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
  }

  /// A value in [0, bound); @p bound must be positive.
  std::uint32_t
  below (std::uint32_t bound)
  {
    return static_cast<std::uint32_t> ((next () >> 33) % bound);
  }

  /// True with the given probability in percent (0 to 100).
  bool
  chance (std::uint32_t percent)
  {
    return below (100u) < percent;
  }

private:
  std::uint64_t state_;
};

/// How a thread is disturbed between operations.
enum class ScheduleKind
{
  /// No disturbance: tight loops, hot caches, the most contention.
  tight,
  /// A yield at about one point in four.
  yielding,
  /// Random yields and short sleeps, rarely a long one.
  jittered,
  /// Every point flushes the data cache first (cold start of each operation),
  /// then yields now and then.
  cold
};

/// Every kind, in a fixed order.
inline std::vector<ScheduleKind>
all_schedule_kinds ()
{
  std::vector<ScheduleKind> kinds;
  kinds.push_back (ScheduleKind::tight);
  kinds.push_back (ScheduleKind::yielding);
  kinds.push_back (ScheduleKind::jittered);
  kinds.push_back (ScheduleKind::cold);
  return kinds;
}

/// Name of a kind, for messages and test names.
inline char const *
schedule_name (ScheduleKind kind)
{
  switch (kind)
    {
    case ScheduleKind::tight:
      return "tight";
    case ScheduleKind::yielding:
      return "yielding";
    case ScheduleKind::jittered:
      return "jittered";
    case ScheduleKind::cold:
      return "cold";
    }
  return "unknown";
}

/// Number of cache flushes so far (a test can tell that a schedule flushed).
inline std::atomic<unsigned long> &
cache_flush_count ()
{
  static std::atomic<unsigned long> count (0);
  return count;
}

/**
 * @brief Evicts the data cache of the calling core by streaming through a
 * buffer larger than a typical private cache level.
 * @details The buffer is per thread and lives as long as the thread. The
 * result is stored in an atomic so the loop is not removed.
 */
inline void
flush_data_cache ()
{
  cache_flush_count ().fetch_add (1, std::memory_order_relaxed);
  static thread_local std::vector<std::uint64_t> buffer;
  if (buffer.empty ())
    buffer.assign (std::size_t (1) << 17, 1u); // 1 MiB of 64-bit words
  std::uint64_t sum = 0;
  for (std::size_t i = 0; i < buffer.size (); i += 8)
    sum += buffer[i]++;
  // The sum is stored where the compiler cannot prove it unused.
  static std::atomic<std::uint64_t> sink (0);
  sink.store (sum, std::memory_order_relaxed);
}

/// Disturbs the calling thread at a point of its program, by the kind.
class Schedule
{
public:
  Schedule (ScheduleKind kind, std::uint64_t seed)
      : kind_ (kind), random_ (seed)
  {
  }

  /// The kind of this schedule.
  ScheduleKind
  kind () const
  {
    return kind_;
  }

  /// A point where another thread may run: call it between operations.
  void
  point ()
  {
    switch (kind_)
      {
      case ScheduleKind::tight:
        break;
      case ScheduleKind::yielding:
        if (random_.below (4u) == 0u)
          std::this_thread::yield ();
        break;
      case ScheduleKind::jittered:
        {
          std::uint32_t const roll = random_.below (100u);
          if (roll < 30u)
            std::this_thread::yield ();
          else if (roll < 45u)
            std::this_thread::sleep_for (
                std::chrono::microseconds (1 + random_.below (60u)));
          else if (roll == 99u)
            std::this_thread::sleep_for (std::chrono::milliseconds (1));
          break;
        }
      case ScheduleKind::cold:
        flush_data_cache ();
        if (random_.below (4u) == 0u)
          std::this_thread::yield ();
        break;
      }
  }

  /// The random stream of this schedule, for the thread's own decisions.
  SeededRandom &
  random ()
  {
    return random_;
  }

private:
  ScheduleKind kind_;
  SeededRandom random_;
};
} // namespace lumex_test

#endif // !LUMEX_TESTS_SUPPORT_TEST_SCHEDULE_HPP
