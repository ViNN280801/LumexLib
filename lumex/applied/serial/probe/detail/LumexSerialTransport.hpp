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
 * @file LumexSerialTransport.hpp
 * @brief Internal transport seam of the serial prober: the operations the
 * probe engine performs on one port.
 *
 * @details Not part of the public API. The header is header-only so the
 * engine and the test doubles compile against it without exported symbols;
 * the platform transports live in `LumexSerialProber.cpp`. Consumers must
 * not include this header.
 */
#ifndef LUMEX_APPLIED_SERIAL_PROBE_DETAIL_SERIAL_TRANSPORT_HPP
#define LUMEX_APPLIED_SERIAL_PROBE_DETAIL_SERIAL_TRANSPORT_HPP

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#endif

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace applied
{
namespace serial
{
namespace probe
{
namespace detail
{
/**
 * @brief Outcome of one transport operation.
 */
enum class transport_status
{
  ok,                ///< The operation completed.
  deadline_exceeded, ///< The deadline expired first.
  failed             ///< The platform reported an error.
};

/**
 * @brief Result of one transport operation.
 */
struct transport_result_t
{
  transport_status status = transport_status::failed; ///< What happened.
  std::size_t bytes = 0U; ///< Bytes written or read on `ok`.
  int error_code
      = 0; ///< Platform error (`GetLastError` / `errno`), 0 if none.
  std::string system_error; ///< Formatted platform error; empty when none.
};

/**
 * @brief Operations the probe engine needs from one port.
 *
 * @details `open` performs the bounded open and applies the settings. The
 * Windows implementation runs the open on a worker thread and never blocks
 * the caller past the deadline. `write` loops until every byte is out or
 * the deadline or an error stops it. `read` returns as soon as at least one
 * byte arrived (`status == ok`, `bytes > 0`); `deadline_exceeded` is
 * reported only with `bytes == 0`; `failed` reports an OS error. `purge`
 * flushes the input and output buffers; the engine ignores its failure.
 */
class serial_transport_t
{
public:
  virtual ~serial_transport_t () = default;

  /// @brief True while the port is open.
  virtual bool is_open () const = 0;

  /// @brief Bounded open with the configured settings.
  virtual transport_result_t open (std::chrono::milliseconds deadline) = 0;

  /// @brief Closes the port when it is open.
  virtual void close () = 0;

  /// @brief Writes `size` bytes or stops at the deadline.
  virtual transport_result_t write (std::uint8_t const *data, std::size_t size,
                                    std::chrono::milliseconds deadline)
      = 0;

  /// @brief Reads at least one byte or stops at the deadline.
  virtual transport_result_t read (std::uint8_t *buffer, std::size_t capacity,
                                   std::chrono::milliseconds deadline)
      = 0;

  /// @brief Flushes the input and output buffers.
  virtual void purge () = 0;
};

} // namespace detail
} // namespace probe
} // namespace serial
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_APPLIED_SERIAL_PROBE_DETAIL_SERIAL_TRANSPORT_HPP
