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
 * @file LumexFramedStatistics.hpp
 * @brief The counters of a framed channel: `framed_statistics_t`, the
 * snapshot `framed_stream::statistics ()` returns, and `framed_counters`, the
 * atomics the framed adapters count in.
 * @details Spec G4 for the framing layer. The counters are
 * `std::atomic<std::uint64_t>` (no lock on the I/O path), incremented with
 * release order and read with acquire order; a snapshot reads them one by
 * one, so under traffic it is a set of values that were each true at some
 * moment, and a counter that was read sees what the counting thread did
 * before it counted (a frame is counted after it was delivered, so a
 * snapshot that shows it also shows the queue and `dropped_frames` settled).
 * `reset ()` during traffic is approximate, as for
 * `statistics::statistics_counters`. What is counted:
 * - `frames_in`: every complete frame the decoder recognized, whether it then
 *   went to a waiting reader, into the queue, or was dropped by the queue;
 * - `frames_out`: every frame written completely;
 * - `bytes_in`: the bytes of the wire the decoder was fed (the bytes of the
 *   chunks read, or of the datagrams received), framing overhead included;
 * - `bytes_out`: the bytes of the frames written completely, framing
 *   overhead included;
 * - `dropped_frames`: decoded frames the full frame queue dropped (the
 *   oldest one goes); a frame the decoder itself dropped (`frame_too_large`,
 *   `bad_checksum`, ...) is not counted here but in `errors`;
 * - `errors`: every framing error by its `framing_errc` value: a frame the
 *   decoder dropped, and a payload `encode` refused (`payload_too_large`);
 *   `other_errors` counts a code of another category that a stage reported;
 * - `timeouts`: synchronous `read_frame` / `write_frame` calls that ended by
 *   their deadline (counted once, as a timeout).
 *
 * The errors of the transport underneath (a failed read, a closed stream)
 * are counted by the transport itself (`statistics::statistics_t`). A
 * synchronous call refused with `would_deadlock` (made on a thread of the
 * executor) is counted by neither: the transports count their own refusals
 * of that kind, the adapter does not, because the call reached no operation
 * of the adapter.
 */
#ifndef LUMEX_APPLIED_TRANSPORT_FRAMED_FRAMED_STATISTICS_HPP
#define LUMEX_APPLIED_TRANSPORT_FRAMED_FRAMED_STATISTICS_HPP

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <system_error>

#include "lumex/core/framing/codec/LumexFramingErrc.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#endif

namespace lumex
{
namespace applied
{
namespace transport
{
namespace framed
{
/// @brief How many `framing_errc` values `framed_statistics_t::errors` holds:
/// one per value from 1 to `payload_too_large`.
LUMEX_CONSTEXPR std::size_t KFRAMING_ERROR_COUNT = 6;

static_assert (
    static_cast<int> (core::framing::codec::framing_errc::payload_too_large)
        == static_cast<int> (KFRAMING_ERROR_COUNT),
    "a new framing_errc value needs a slot in framed_statistics_t::errors: "
    "update KFRAMING_ERROR_COUNT");

/// @brief A snapshot of the counters of a framed adapter (see the file
/// description for what each field counts).
struct framed_statistics_t
{
  /// Complete frames the decoder recognized.
  std::uint64_t frames_in = 0;
  /// Frames written completely.
  std::uint64_t frames_out = 0;
  /// Wire bytes the decoder was fed.
  std::uint64_t bytes_in = 0;
  /// Wire bytes of the frames written completely.
  std::uint64_t bytes_out = 0;
  /// Decoded frames the full frame queue dropped.
  std::uint64_t dropped_frames = 0;
  /// Framing errors by `framing_errc` value, index `value - 1`; use
  /// `errors_of` (it returns 0 for a value outside the enum).
  std::array<std::uint64_t, KFRAMING_ERROR_COUNT> errors = { {} };
  /// Errors with a code that is not a `framing_errc` value.
  std::uint64_t other_errors = 0;
  /// Synchronous reads and writes that ended by their deadline.
  std::uint64_t timeouts = 0;

  /// @brief How many times `value` happened.
  std::uint64_t
  errors_of (core::framing::codec::framing_errc value) const LUMEX_NOEXCEPT
  {
    int const index = static_cast<int> (value) - 1;
    if (index < 0 || index >= static_cast<int> (KFRAMING_ERROR_COUNT))
      return 0;
    return errors[static_cast<std::size_t> (index)];
  }

  /// @brief Every error, `framing_errc` values and others.
  std::uint64_t
  total_errors () const LUMEX_NOEXCEPT
  {
    std::uint64_t total = other_errors;
    for (std::uint64_t const count : errors)
      total += count;
    return total;
  }
};

/**
 * @brief The atomic counters behind `framed_statistics_t`, owned by the
 * framed adapters. Every function is thread-safe and lock-free.
 */
class framed_counters
{
public:
  framed_counters () LUMEX_NOEXCEPT;

  framed_counters (framed_counters const &) = delete;
  framed_counters &operator= (framed_counters const &) = delete;

  /// @brief The decoder recognized a frame.
  void add_frame_in () LUMEX_NOEXCEPT;
  /// @brief A frame of `bytes` wire bytes was written completely.
  void add_frame_out (std::size_t bytes) LUMEX_NOEXCEPT;
  /// @brief The decoder was fed `bytes` bytes.
  void add_bytes_in (std::size_t bytes) LUMEX_NOEXCEPT;
  /// @brief The full frame queue dropped a frame.
  void add_dropped_frame () LUMEX_NOEXCEPT;
  /// @brief A framing error (a code of the framing category counts in its
  /// slot, any other in `other_errors`).
  void add_error (std::error_code const &error) LUMEX_NOEXCEPT;
  /// @brief The deadline of a synchronous call cancelled the adapter and the
  /// call ended with `timeout`.
  void convert_cancel_to_timeout () LUMEX_NOEXCEPT;

  /// @brief The current values.
  framed_statistics_t snapshot () const LUMEX_NOEXCEPT;
  /// @brief Sets every counter to zero.
  void reset () LUMEX_NOEXCEPT;

private:
  std::atomic<std::uint64_t> frames_in_;
  std::atomic<std::uint64_t> frames_out_;
  std::atomic<std::uint64_t> bytes_in_;
  std::atomic<std::uint64_t> bytes_out_;
  std::atomic<std::uint64_t> dropped_frames_;
  std::atomic<std::uint64_t> errors_[KFRAMING_ERROR_COUNT];
  std::atomic<std::uint64_t> other_errors_;
  std::atomic<std::uint64_t> timeouts_;
};
} // namespace framed
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_APPLIED_TRANSPORT_FRAMED_FRAMED_STATISTICS_HPP
