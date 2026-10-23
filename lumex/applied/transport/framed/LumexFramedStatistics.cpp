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
#include <cstddef>
#include <cstdint>
#include <system_error>

#include "lumex/applied/transport/framed/LumexFramedStatistics.hpp"
#include "lumex/core/framing/codec/LumexFramingErrc.hpp"
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
namespace framed
{
namespace
{
// Increments release and snapshots acquire: a thread that reads a counter
// sees what the counting thread did before it (the adapter counts a frame
// after it delivered it, so a test or a monitor that waits for frames_in
// finds the queue and dropped_frames settled).
std::memory_order const KRELAXED = std::memory_order_relaxed;
std::memory_order const KRELEASE = std::memory_order_release;
std::memory_order const KACQUIRE = std::memory_order_acquire;

// The slot of a framing_errc value, or KFRAMING_ERROR_COUNT for any other
// code (another category, or a value outside the enum).
std::size_t
error_slot (std::error_code const &error) LUMEX_NOEXCEPT
{
  int const value = error.value ();
  if (error.category () != core::framing::codec::framing_category ()
      || value < 1 || value > static_cast<int> (KFRAMING_ERROR_COUNT))
    return KFRAMING_ERROR_COUNT;
  return static_cast<std::size_t> (value) - 1;
}
} // namespace

framed_counters::framed_counters () LUMEX_NOEXCEPT : frames_in_ (0),
                                                     frames_out_ (0),
                                                     bytes_in_ (0),
                                                     bytes_out_ (0),
                                                     dropped_frames_ (0),
                                                     other_errors_ (0),
                                                     timeouts_ (0)
{
  for (std::atomic<std::uint64_t> &slot : errors_)
    slot.store (0, KRELAXED);
}

void
framed_counters::add_frame_in () LUMEX_NOEXCEPT
{
  frames_in_.fetch_add (1, KRELEASE);
}

void
framed_counters::add_frame_out (std::size_t bytes) LUMEX_NOEXCEPT
{
  bytes_out_.fetch_add (static_cast<std::uint64_t> (bytes), KRELAXED);
  frames_out_.fetch_add (1, KRELEASE);
}

void
framed_counters::add_bytes_in (std::size_t bytes) LUMEX_NOEXCEPT
{
  bytes_in_.fetch_add (static_cast<std::uint64_t> (bytes), KRELEASE);
}

void
framed_counters::add_dropped_frame () LUMEX_NOEXCEPT
{
  dropped_frames_.fetch_add (1, KRELEASE);
}

void
framed_counters::add_error (std::error_code const &error) LUMEX_NOEXCEPT
{
  std::size_t const slot = error_slot (error);
  if (slot == KFRAMING_ERROR_COUNT)
    other_errors_.fetch_add (1, KRELEASE);
  else
    errors_[slot].fetch_add (1, KRELEASE);
}

void
framed_counters::convert_cancel_to_timeout () LUMEX_NOEXCEPT
{
  timeouts_.fetch_add (1, KRELEASE);
}

framed_statistics_t
framed_counters::snapshot () const LUMEX_NOEXCEPT
{
  framed_statistics_t result;
  result.frames_in = frames_in_.load (KACQUIRE);
  result.frames_out = frames_out_.load (KACQUIRE);
  result.bytes_in = bytes_in_.load (KACQUIRE);
  result.bytes_out = bytes_out_.load (KACQUIRE);
  result.dropped_frames = dropped_frames_.load (KACQUIRE);
  for (std::size_t i = 0; i < KFRAMING_ERROR_COUNT; ++i)
    result.errors[i] = errors_[i].load (KACQUIRE);
  result.other_errors = other_errors_.load (KACQUIRE);
  result.timeouts = timeouts_.load (KACQUIRE);
  return result;
}

void
framed_counters::reset () LUMEX_NOEXCEPT
{
  frames_in_.store (0, KRELAXED);
  frames_out_.store (0, KRELAXED);
  bytes_in_.store (0, KRELAXED);
  bytes_out_.store (0, KRELAXED);
  dropped_frames_.store (0, KRELAXED);
  for (std::atomic<std::uint64_t> &slot : errors_)
    slot.store (0, KRELAXED);
  other_errors_.store (0, KRELAXED);
  timeouts_.store (0, KRELAXED);
}
} // namespace framed
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
