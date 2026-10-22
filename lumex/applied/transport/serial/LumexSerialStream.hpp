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
 * @file LumexSerialStream.hpp
 * @brief The serial port as a `byte_stream`.
 * @details The port is opened by the bounded open of `applied/serial` (a
 * Windows worker thread cancelled with `CancelSynchronousIo`, POSIX
 * `O_NONBLOCK`), so an open never waits past its deadline, also for a paired
 * but absent Bluetooth port. The handle is then given to `asio::serial_port`:
 * reads and writes are overlapped (Windows) or non-blocking (POSIX), and
 * `cancel ()` and `close ()` end them from any thread. An open in progress
 * is not ended by `cancel ()` nor by a terminal cancellation: it ends with
 * the bounded open, at the latest after `open_deadline`. The settings are the
 * `serial_port_settings_t` of `applied/serial`; a value the platform cannot
 * set (mark or space parity, 1.5 stop bits outside Windows, data bits outside
 * 5..8) fails the open with `std::errc::not_supported`.
 */
#ifndef LUMEX_APPLIED_TRANSPORT_SERIAL_SERIAL_STREAM_HPP
#define LUMEX_APPLIED_TRANSPORT_SERIAL_SERIAL_STREAM_HPP

#include <chrono>
#include <functional>
#include <string>
#include <system_error>

#include "lumex/applied/serial/probe/LumexSerialProber.hpp"
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
namespace serial
{
/// @brief The serial settings: baud, data bits, parity, stop bits, flow.
typedef lumex::applied::serial::probe::serial_port_settings_t
    serial_settings_t;

/// @brief What `make_serial_stream` opens.
struct serial_stream_options_t
{
  /// The port: "COM3", "\\\\.\\COM12", "/dev/ttyUSB0".
  std::string path;
  /// The settings applied after the open.
  serial_settings_t settings;
  /// The longest an open may take (the bounded open of the device); a
  /// synchronous `open (deadline)` may end earlier with `timeout`.
  deadline_t open_deadline = deadline_t (3000);
  /// When set, finds the port at each open, on the open's worker thread
  /// (not on the stream's executor): its result replaces `path` for that
  /// open, and `path` is only the name for logs. An empty result, or a
  /// `std::system_error` it throws, fails the open with `invalid_endpoint`
  /// (or the error's code). For a port found through the enumeration by
  /// its USB identity (`session::make_serial_factory`). It runs on a new
  /// thread at each open, also concurrently with the opens of other streams:
  /// it must be thread-safe; the open's deadline does not bound it (one that
  /// hangs keeps its detached thread).
  std::function<std::string ()> resolve_path;
};

/**
 * @brief The path the operating system opens: on Windows a "COMn" name gets
 * the "\\\\.\\" prefix (which "COM10" and above need); others are kept.
 */
std::string serial_open_path (std::string const &path);

/**
 * @brief The error of settings the current platform cannot apply:
 * `std::errc::not_supported`, or a value-initialized code when they can be.
 */
std::error_code validate_serial_settings (serial_settings_t const &settings);

/// @brief A serial port stream on `executor` (closed; call `open`).
stream::byte_stream
make_serial_stream (executor_type const &executor,
                    serial_stream_options_t const &options);
} // namespace serial
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_APPLIED_TRANSPORT_SERIAL_SERIAL_STREAM_HPP
