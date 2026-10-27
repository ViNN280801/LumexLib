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
 * @file LumexTransportStatistics.hpp
 * @brief The counters of a channel: `statistics_t`, the snapshot a front
 * returns from `statistics ()`, and `statistics_counters`, the atomics the
 * base classes `stream_impl` and `message_impl` count in.
 * @details Counting happens in the base classes, so every transport (serial,
 * TCP, UDP, mocks) counts the same way. The counters are
 * `std::atomic<std::uint64_t>` updated with relaxed order: no lock on the I/O
 * path. A snapshot reads them one by one, so under traffic it is a set of
 * values that were each true at some moment, not one instant; after the
 * operations of the calling thread returned (a synchronous call, or the
 * completion handler ran), their effect is in the next snapshot.
 *
 * `reset_statistics ()` during traffic is approximate: an operation that is
 * in flight is counted after the reset, and the cancellation of a deadline
 * that was counted before the reset may turn into a timeout after it (the
 * timeout is counted, the cancellation is not subtracted below zero).
 *
 * What is counted, per completed operation:
 * - `opens`: every successful open completion, including an open of a stream
 *   that is already open (a no-op open);
 * - `reads` and `bytes_read`: a successful `read_some` and its bytes (a
 *   message transport counts `messages_received` and the payload bytes
 *   instead of `reads`);
 * - `writes` and `bytes_written`: a successful `write_some` or `write_all`
 *   (`write_all` is one write); the bytes of a failed `write_all` that were
 *   written before the error count in `bytes_written` (a message transport
 *   counts `messages_sent` and the payload bytes instead of `writes`);
 * - `errors`: every failed operation by its `transport_errc` value, also a
 *   synchronous call refused with `would_deadlock`; a code of another
 *   category (a system error) goes to `other_errors`;
 * - `timeouts`: every operation that ended with `transport_errc::timeout`:
 *   a synchronous call that ended by its deadline (the deadline cancels the
 *   operation, the call reports `timeout`; it counts once, as a timeout, not
 *   as a cancellation) and an asynchronous timeout of the transport itself,
 *   such as the bounded open of a serial port;
 * - `cancellations`: operations ended by `cancel ()` or `close ()`, also an
 *   open that another open in progress replaced;
 * - `last_activity`: the last successful open or transfer (the epoch when
 *   there was none); it keeps the latest time seen, so it does not move back
 *   when the wall clock (`system_clock`) is set back.
 *
 * `timeouts` and `cancellations` are also the `timeout` and `cancelled`
 * entries of `errors`.
 */
#ifndef LUMEX_APPLIED_TRANSPORT_STATISTICS_STATISTICS_HPP
#define LUMEX_APPLIED_TRANSPORT_STATISTICS_STATISTICS_HPP

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <system_error>

#include "lumex/applied/transport/error/LumexTransportErrc.hpp"
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
namespace statistics
{
/// @brief How many `transport_errc` values `statistics_t::errors` holds: one
/// per value from 1 to `error::KTRANSPORT_ERRC_LAST`.
LUMEX_CONSTEXPR std::size_t KERROR_CODE_COUNT = 11;

static_assert (static_cast<int> (error::KTRANSPORT_ERRC_LAST)
                   == static_cast<int> (KERROR_CODE_COUNT),
               "a new transport_errc value needs a slot in "
               "statistics_t::errors: update KERROR_CODE_COUNT");

/// @brief A snapshot of the counters of a `byte_stream` or a
/// `message_transport` (see the file description for what each field counts).
struct statistics_t
{
  /// Bytes received (payload bytes for a message transport).
  std::uint64_t bytes_read = 0;
  /// Bytes sent (payload bytes for a message transport).
  std::uint64_t bytes_written = 0;
  /// Successful `read_some` calls (byte streams only).
  std::uint64_t reads = 0;
  /// Successful `write_some` / `write_all` calls (byte streams only).
  std::uint64_t writes = 0;
  /// Messages received (message transports only).
  std::uint64_t messages_received = 0;
  /// Messages sent (message transports only).
  std::uint64_t messages_sent = 0;
  /// Successful opens.
  std::uint64_t opens = 0;
  /// Failed operations by `transport_errc` value, index `value - 1`; use
  /// `errors_of` (it returns 0 for a value outside the enum).
  std::array<std::uint64_t, KERROR_CODE_COUNT> errors = { {} };
  /// Failed operations with a code that is not a `transport_errc` value.
  std::uint64_t other_errors = 0;
  /// Operations that ended with `transport_errc::timeout`: synchronous calls
  /// that ended by their deadline and asynchronous timeouts of the transport.
  std::uint64_t timeouts = 0;
  /// Operations ended by `cancel ()` or `close ()`.
  std::uint64_t cancellations = 0;
  /// The last successful open or transfer; the epoch when there was none.
  /// The latest time seen: it does not move back if the wall clock is set
  /// back.
  std::chrono::system_clock::time_point last_activity;

  /// @brief How many operations failed with `value`.
  std::uint64_t
  errors_of (error::transport_errc value) const LUMEX_NOEXCEPT
  {
    int const index = static_cast<int> (value) - 1;
    if (index < 0 || index >= static_cast<int> (KERROR_CODE_COUNT))
      return 0;
    return errors[static_cast<std::size_t> (index)];
  }

  /// @brief Every failed operation, `transport_errc` values and others.
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
 * @brief The atomic counters behind `statistics_t`, owned by `stream_impl`
 * and `message_impl`. Every function is thread-safe and lock-free.
 */
class statistics_counters
{
public:
  statistics_counters () LUMEX_NOEXCEPT;

  statistics_counters (statistics_counters const &) = delete;
  statistics_counters &operator= (statistics_counters const &) = delete;

  /// @brief An open completed with `error` (success when it is empty).
  void add_open (std::error_code const &error) LUMEX_NOEXCEPT;
  /// @brief A read completed with `error` and `bytes`.
  void add_read (std::error_code const &error,
                 std::size_t bytes) LUMEX_NOEXCEPT;
  /// @brief A write completed with `error`; `bytes` were written (also before
  /// an error).
  void add_write (std::error_code const &error,
                  std::size_t bytes) LUMEX_NOEXCEPT;
  /// @brief A message receive completed with `error` and `bytes` of payload.
  void add_receive (std::error_code const &error,
                    std::size_t bytes) LUMEX_NOEXCEPT;
  /// @brief A message send completed with `error`; `bytes` of payload were
  /// sent on success.
  void add_send (std::error_code const &error,
                 std::size_t bytes) LUMEX_NOEXCEPT;
  /// @brief An error that never reached an operation (`would_deadlock`).
  void add_error (std::error_code const &error) LUMEX_NOEXCEPT;
  /// @brief The deadline of a synchronous call cancelled its operation: the
  /// cancellation already counted becomes a timeout.
  void convert_cancel_to_timeout () LUMEX_NOEXCEPT;

  /// @brief The current values (see the file description).
  statistics_t snapshot () const LUMEX_NOEXCEPT;
  /// @brief Sets every counter to zero.
  void reset () LUMEX_NOEXCEPT;

private:
  void count_error (std::error_code const &error) LUMEX_NOEXCEPT;
  void touch () LUMEX_NOEXCEPT;

  std::atomic<std::uint64_t> bytes_read_;
  std::atomic<std::uint64_t> bytes_written_;
  std::atomic<std::uint64_t> reads_;
  std::atomic<std::uint64_t> writes_;
  std::atomic<std::uint64_t> messages_received_;
  std::atomic<std::uint64_t> messages_sent_;
  std::atomic<std::uint64_t> opens_;
  std::atomic<std::uint64_t> errors_[KERROR_CODE_COUNT];
  std::atomic<std::uint64_t> other_errors_;
  std::atomic<std::uint64_t> timeouts_;
  std::atomic<std::uint64_t> cancellations_;
  // Nanoseconds since the system clock's epoch; 0: no activity yet.
  std::atomic<std::uint64_t> last_activity_ns_;
};
} // namespace statistics
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_APPLIED_TRANSPORT_STATISTICS_STATISTICS_HPP
