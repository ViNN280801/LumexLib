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
 * `close`. Available with `LUMEX_TRANSPORT_WITH_TESTING`.
 */
#ifndef LUMEX_APPLIED_TRANSPORT_TESTING_MOCK_STREAM_HPP
#define LUMEX_APPLIED_TRANSPORT_TESTING_MOCK_STREAM_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

#include "lumex/applied/transport/stream/LumexByteStream.hpp"
#include "lumex/applied/transport/stream/LumexStreamTypes.hpp"

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
  /// @brief The pending and later reads end with `disconnected`.
  void disconnect ();
  /// @brief At most `bytes` per write (0: no limit, the default).
  void set_write_chunk (std::size_t bytes);
  /// @brief Every byte written so far.
  std::vector<std::uint8_t> written () const;
  /// @brief How many times the stream was opened.
  std::size_t open_count () const;

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

/// @brief Makes a mock stream on `executor` with the log name `name`.
mock_stream make_mock_stream (executor_type const &executor,
                              std::string const &name = "mock");
} // namespace testing
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_APPLIED_TRANSPORT_TESTING_MOCK_STREAM_HPP
