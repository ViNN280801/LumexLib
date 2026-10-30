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
 * @file LumexMockStream.hpp
 * @brief `mock_stream`: a `byte_stream` driven by a test (or a simulator)
 * instead of a device.
 * @details The control object (`mock_stream_control`) is thread-safe: `feed`
 * makes bytes available to reads (a read blocked on an empty stream ends as
 * soon as bytes arrive), `fail_*` makes the next operation end with an error,
 * `fail_write_after` lets some writes succeed and fails the next one,
 * `disconnect` ends the pending and later reads with `disconnected`,
 * `set_write_chunk` limits how many bytes one write takes (to exercise
 * `write_all`), `written` returns every byte the stream wrote. A read waits
 * for bytes indefinitely: a test bounds it with a deadline, `cancel` or
 * `close`. Several reads may be pending at once: they are served in the order
 * they were started.
 *
 * The scripting members let a simulator drive the mock: `on_write` sees every
 * accepted chunk on the strand of the mock (so a reply can be fed from
 * inside), `feed_after` delivers bytes after a delay of the mock's timer
 * source (a `virtual_clock` moves it), `set_read_chunk` limits how many bytes
 * one read takes, `hold_writes` / `release_writes` keep writes pending (a
 * frozen device) and let them go in order, `complete_next_read_empty` ends
 * one read with no bytes and no error. These settings belong to the script,
 * not to a connection: scripted empty reads, `hold_writes`, the read chunk,
 * the write chunk, the observer and the deliveries of `feed_after` outlive
 * `close` and a new open (an empty read scripted before a close ends the
 * first read after the reopen). An observer that throws leaves the write it
 * saw complete; when it runs during `release_writes` the remaining held
 * writes are flushed by a new step before the exception goes on. Available
 * with `LUMEX_TRANSPORT_WITH_TESTING`.
 */
#ifndef LUMEX_APPLIED_TRANSPORT_TESTING_MOCK_STREAM_HPP
#define LUMEX_APPLIED_TRANSPORT_TESTING_MOCK_STREAM_HPP

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

#include "lumex/applied/transport/stream/LumexByteStream.hpp"
#include "lumex/applied/transport/stream/LumexStreamTypes.hpp"
#include "lumex/applied/transport/testing/LumexMockScript.hpp"

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
namespace testing
{
class mock_stream_impl;

/// @brief The script side of a mock stream (thread-safe).
class mock_stream_control
{
public:
  explicit mock_stream_control (std::shared_ptr<mock_stream_impl> impl);

  /// @brief Makes `bytes` available to reads.
  void feed (std::vector<std::uint8_t> const &bytes);
  /// @brief The next open fails with `error`.
  void fail_open (std::error_code error);
  /// @brief The next open does not complete until a cancel or a close ends
  /// it with `cancelled` (a device that never answers the open).
  void hold_next_open ();
  /// @brief The next read fails with `error`.
  void fail_next_read (std::error_code error);
  /// @brief The next write fails with `error`.
  void fail_next_write (std::error_code error);
  /// @brief After `writes` more writes that succeed, the next write fails
  /// with `error` (so a `write_all` in chunks fails on a later chunk).
  void fail_write_after (std::size_t writes, std::error_code error);
  /// @brief The pending and later reads end with `disconnected` (bytes fed
  /// before are read first), and so do the writes held by `hold_writes`.
  void disconnect ();
  /// @brief At most `bytes` per write (0: no limit, the default).
  void set_write_chunk (std::size_t bytes);
  /// @brief Every byte written so far.
  std::vector<std::uint8_t> written () const;
  /// @brief How many times the stream was opened.
  std::size_t open_count () const;

  /// @brief Sets the observer of the accepted writes (see
  /// `write_observer_t`); an empty one removes it. It sees each chunk the
  /// mock took (as many bytes as `set_write_chunk` allowed), in order.
  void on_write (write_observer_t observer);
  /// @brief Makes `bytes` available to reads after `delay` of the mock's
  /// timer source; deliveries with the same time arrive in the order they
  /// were scheduled. A delivery waits on the clock it was scheduled with and
  /// survives `cancel`, `close` and `disconnect`; it ends when the mock is
  /// destroyed.
  void feed_after (std::chrono::milliseconds delay,
                   std::vector<std::uint8_t> const &bytes);
  /// @brief Ends the next read with 0 bytes and no error after `delay` of
  /// the mock's timer source (`complete_next_read_empty` then); ordered with
  /// `feed_after` and `disconnect_after` by time and, for equal times, by the
  /// order of the calls.
  void empty_read_after (std::chrono::milliseconds delay);
  /// @brief `disconnect ()` after `delay` of the mock's timer source (the
  /// rules of `empty_read_after`). A new open clears the disconnect, as it
  /// does for `disconnect ()`.
  void disconnect_after (std::chrono::milliseconds delay);
  /// @brief Every open that starts before `duration` of the mock's timer
  /// source has passed from now fails with `error` (a cable that is plugged
  /// back later); a zero or negative duration clears the window. `fail_open`
  /// is served first.
  void fail_opens_for (std::chrono::milliseconds duration,
                       std::error_code error);
  /// @brief At most `bytes` per read (0: no limit, the default); the rest
  /// stays for the next read.
  void set_read_chunk (std::size_t bytes);
  /// @brief From now on writes stay pending (they are neither recorded nor
  /// completed) until `release_writes`, `cancel` or `close`; the latter two
  /// end them with `cancelled`.
  void hold_writes ();
  /// @brief Lets the held writes go, in the order they were started, on the
  /// strand of the mock (after the call returns, when the strand runs); later
  /// writes queue behind them until they are done.
  void release_writes ();
  /// @brief The next read ends with 0 bytes and no error (a pending one now),
  /// also when bytes are available; the stream stays open.
  void complete_next_read_empty ();

private:
  std::shared_ptr<mock_stream_impl> impl_;
};

/// @brief A mock stream: the stream to hand to the code under test and the
/// control of the test.
struct mock_stream
{
  stream::byte_stream stream;
  mock_stream_control control;
};

/// @brief Makes a mock stream on `executor` with the log name `name` and
/// the steady timer source.
mock_stream make_mock_stream (executor_type const &executor,
                              std::string const &name = "mock");

/// @brief Makes a mock stream on `executor` with `options` (the name and the
/// timer source of the scripted deliveries).
mock_stream make_mock_stream (executor_type const &executor,
                              mock_options_t const &options);
} // namespace testing
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_APPLIED_TRANSPORT_TESTING_MOCK_STREAM_HPP
