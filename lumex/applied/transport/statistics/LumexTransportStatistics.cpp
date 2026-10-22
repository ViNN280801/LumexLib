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

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <system_error>

#include "lumex/applied/transport/error/LumexTransportErrc.hpp"
#include "lumex/applied/transport/statistics/LumexTransportStatistics.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

#if defined(__clang__)
// The library's clang-cl set: -Wall there is -Weverything (as in the other
// modules' sources).
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#if __has_warning("-Wunsafe-buffer-usage-in-libc-call")
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-libc-call"
#endif
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wswitch-enum"
#if __has_warning("-Wnrvo")
#pragma clang diagnostic ignored "-Wnrvo"
#endif
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#endif

namespace lumex
{
namespace applied
{
namespace transport
{
namespace statistics
{
namespace
{
std::memory_order const KRELAXED = std::memory_order_relaxed;

// The slot of a transport_errc value (also of another binary's category), or
// KERROR_CODE_COUNT for any other code.
std::size_t
error_slot (std::error_code const &error) LUMEX_NOEXCEPT
{
  int const value = error.value ();
  if (value < 1 || value > static_cast<int> (KERROR_CODE_COUNT))
    return KERROR_CODE_COUNT;
  if (&error.category () != &error::transport_category ()
      && std::strcmp (error.category ().name (),
                      error::KTRANSPORT_CATEGORY_NAME)
             != 0)
    return KERROR_CODE_COUNT;
  return static_cast<std::size_t> (value) - 1;
}

std::size_t
slot_of (error::transport_errc value) LUMEX_NOEXCEPT
{
  return static_cast<std::size_t> (value) - 1;
}

// Subtracts one unless the counter is zero (a reset may have run meanwhile).
void
decrement (std::atomic<std::uint64_t> &counter) LUMEX_NOEXCEPT
{
  std::uint64_t current = counter.load (KRELAXED);
  while (current != 0
         && !counter.compare_exchange_weak (current, current - 1, KRELAXED))
    {
    }
}
} // namespace

statistics_counters::statistics_counters () LUMEX_NOEXCEPT
    : bytes_read_ (0),
      bytes_written_ (0),
      reads_ (0),
      writes_ (0),
      messages_received_ (0),
      messages_sent_ (0),
      opens_ (0),
      other_errors_ (0),
      timeouts_ (0),
      cancellations_ (0),
      last_activity_ns_ (0)
{
  for (std::atomic<std::uint64_t> &slot : errors_)
    slot.store (0, KRELAXED);
}

void
statistics_counters::touch () LUMEX_NOEXCEPT
{
  std::chrono::nanoseconds const since_epoch
      = std::chrono::duration_cast<std::chrono::nanoseconds> (
          std::chrono::system_clock::now ().time_since_epoch ());
  std::uint64_t const now = static_cast<std::uint64_t> (since_epoch.count ());
  // Keep the latest: threads that finish out of order must not move it back.
  std::uint64_t current = last_activity_ns_.load (KRELAXED);
  while (current < now
         && !last_activity_ns_.compare_exchange_weak (current, now, KRELAXED))
    {
    }
}

void
statistics_counters::count_error (std::error_code const &error) LUMEX_NOEXCEPT
{
  std::size_t const slot = error_slot (error);
  if (slot == KERROR_CODE_COUNT)
    {
      other_errors_.fetch_add (1, KRELAXED);
      return;
    }
  errors_[slot].fetch_add (1, KRELAXED);
  if (slot == slot_of (error::transport_errc::timeout))
    timeouts_.fetch_add (1, KRELAXED);
  else if (slot == slot_of (error::transport_errc::cancelled))
    cancellations_.fetch_add (1, KRELAXED);
}

void
statistics_counters::add_open (std::error_code const &error) LUMEX_NOEXCEPT
{
  if (error)
    {
      count_error (error);
      return;
    }
  opens_.fetch_add (1, KRELAXED);
  touch ();
}

void
statistics_counters::add_read (std::error_code const &error,
                               std::size_t bytes) LUMEX_NOEXCEPT
{
  if (error)
    {
      count_error (error);
      return;
    }
  reads_.fetch_add (1, KRELAXED);
  bytes_read_.fetch_add (bytes, KRELAXED);
  touch ();
}

void
statistics_counters::add_write (std::error_code const &error,
                                std::size_t bytes) LUMEX_NOEXCEPT
{
  bytes_written_.fetch_add (bytes, KRELAXED);
  if (error)
    {
      count_error (error);
      if (bytes != 0)
        touch ();
      return;
    }
  writes_.fetch_add (1, KRELAXED);
  touch ();
}

void
statistics_counters::add_receive (std::error_code const &error,
                                  std::size_t bytes) LUMEX_NOEXCEPT
{
  if (error)
    {
      count_error (error);
      return;
    }
  messages_received_.fetch_add (1, KRELAXED);
  bytes_read_.fetch_add (bytes, KRELAXED);
  touch ();
}

void
statistics_counters::add_send (std::error_code const &error,
                               std::size_t bytes) LUMEX_NOEXCEPT
{
  if (error)
    {
      count_error (error);
      return;
    }
  messages_sent_.fetch_add (1, KRELAXED);
  bytes_written_.fetch_add (bytes, KRELAXED);
  touch ();
}

void
statistics_counters::add_error (std::error_code const &error) LUMEX_NOEXCEPT
{
  count_error (error);
}

void
statistics_counters::convert_cancel_to_timeout () LUMEX_NOEXCEPT
{
  decrement (cancellations_);
  decrement (errors_[slot_of (error::transport_errc::cancelled)]);
  timeouts_.fetch_add (1, KRELAXED);
  errors_[slot_of (error::transport_errc::timeout)].fetch_add (1, KRELAXED);
}

statistics_t
statistics_counters::snapshot () const LUMEX_NOEXCEPT
{
  statistics_t result;
  result.bytes_read = bytes_read_.load (KRELAXED);
  result.bytes_written = bytes_written_.load (KRELAXED);
  result.reads = reads_.load (KRELAXED);
  result.writes = writes_.load (KRELAXED);
  result.messages_received = messages_received_.load (KRELAXED);
  result.messages_sent = messages_sent_.load (KRELAXED);
  result.opens = opens_.load (KRELAXED);
  for (std::size_t slot = 0; slot < KERROR_CODE_COUNT; ++slot)
    result.errors[slot] = errors_[slot].load (KRELAXED);
  result.other_errors = other_errors_.load (KRELAXED);
  result.timeouts = timeouts_.load (KRELAXED);
  result.cancellations = cancellations_.load (KRELAXED);
  std::uint64_t const activity = last_activity_ns_.load (KRELAXED);
  if (activity != 0)
    result.last_activity = std::chrono::system_clock::time_point (
        std::chrono::duration_cast<std::chrono::system_clock::duration> (
            std::chrono::nanoseconds (static_cast<std::int64_t> (activity))));
  return result;
}

void
statistics_counters::reset () LUMEX_NOEXCEPT
{
  bytes_read_.store (0, KRELAXED);
  bytes_written_.store (0, KRELAXED);
  reads_.store (0, KRELAXED);
  writes_.store (0, KRELAXED);
  messages_received_.store (0, KRELAXED);
  messages_sent_.store (0, KRELAXED);
  opens_.store (0, KRELAXED);
  for (std::atomic<std::uint64_t> &slot : errors_)
    slot.store (0, KRELAXED);
  other_errors_.store (0, KRELAXED);
  timeouts_.store (0, KRELAXED);
  cancellations_.store (0, KRELAXED);
  last_activity_ns_.store (0, KRELAXED);
}
} // namespace statistics
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
