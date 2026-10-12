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
 * @file LumexSerialProber.hpp
 * @brief Bounded serial port probing: open a port, send a request and wait
 * for a caller-defined response inside one deadline.
 *
 * @details Declares the `SerialProber` session class and its result types.
 * Every open, write and read is bounded by a caller-supplied deadline
 * (default `Constants::KDEFAULT_PROBE_DEADLINE_MS`); the class never waits
 * past it, including on Windows virtual COM ports whose open blocks inside
 * `CreateFile` (a paired but disconnected Bluetooth SPP/LE device). The
 * response is collected until the caller's predicate confirms it, the
 * response cap is reached or the deadline expires.
 *
 * The class is a move-only RAII session and is not thread-safe. Link
 * <tt>%lumex::serial</tt> (compiled library). Works from C++11.
 */
#ifndef LUMEX_APPLIED_SERIAL_PROBE_HPP
#define LUMEX_APPLIED_SERIAL_PROBE_HPP

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#endif

#include "lumex/LumexExport.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/core/utility/macros/LumexConstantMacros.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace applied
{
/**
 * @brief Cross-platform helpers for naming/resolving serial (and
 * serial-adjacent) communication ports.
 */
namespace serial
{
/**
 * @brief Bounded probing of the device behind a serial port.
 */
namespace probe
{
/**
 * @brief Compile-time defaults of the probe contract.
 */
namespace Constants
{
/**
 * @brief Default overall probe deadline in milliseconds.
 * @details Shared by the open and the exchange of one `probe` call; every
 * method takes the deadline as a parameter and falls back to this value.
 */
LUMEX_CONST_NUM std::uint32_t KDEFAULT_PROBE_DEADLINE_MS = 500U;

/**
 * @brief Default cap of the collected response, in bytes.
 * @details A response that reaches the cap stops the read loop with the
 * `incomplete` outcome when the predicate has not confirmed it.
 */
LUMEX_CONST_NUM std::size_t KDEFAULT_PROBE_MAX_RESPONSE_BYTES = 8192U;
} // namespace Constants

/**
 * @brief Parity the prober configures on the port.
 */
enum class serial_parity
{
  none, ///< No parity bit.
  odd,  ///< Odd parity.
  even, ///< Even parity.
  mark, ///< Mark parity: Windows; Linux only where `CMSPAR` exists.
  space ///< Space parity: Windows; Linux only where `CMSPAR` exists.
};

/**
 * @brief Number of stop bits the prober configures on the port.
 */
enum class serial_stop_bits
{
  one,            ///< One stop bit.
  one_point_five, ///< 1.5 stop bits: Windows only.
  two             ///< Two stop bits.
};

/**
 * @brief Flow control the prober configures on the port.
 */
enum class serial_flow_control
{
  none,     ///< No flow control.
  software, ///< XON/XOFF.
  hardware  ///< RTS/CTS.
};

/**
 * @brief Serial port settings applied when the port is opened.
 *
 * @details The defaults match the common instrument configuration
 * (9600 baud, 8 data bits, no parity, one stop bit, no flow control).
 * A combination the current platform cannot configure (for example
 * 1.5 stop bits outside Windows) makes `open` fail with `open_failed`
 * and an explanatory `system_error`; no exception is thrown.
 */
struct serial_port_settings_t
{
  std::uint32_t baud_rate = 9600U; ///< Bits per second.
  std::uint8_t data_bits = 8U;     ///< Data bits per character (5 to 8).
  serial_parity parity = serial_parity::none;         ///< Parity bit mode.
  serial_stop_bits stop_bits = serial_stop_bits::one; ///< Stop bits.
  serial_flow_control flow_control
      = serial_flow_control::none; ///< Flow control mode.
};

/**
 * @brief Outcome of one `SerialProber::probe` call.
 */
enum class serial_probe_status
{
  responded,    ///< The predicate confirmed the collected response.
  incomplete,   ///< Bytes arrived, but the predicate never confirmed them
                ///< before the deadline or the response cap.
  no_response,  ///< The deadline expired without a single response byte.
  open_timeout, ///< The bounded open did not return within the deadline.
  open_failed,  ///< The open failed: absent or busy port, unsupported
                ///< settings or a configuration error; see `system_error`.
  io_error      ///< The request was not written in full, or a read failed.
};

/**
 * @brief Result of one `SerialProber::probe` call.
 */
struct serial_probe_result_t
{
  serial_probe_status status = serial_probe_status::no_response;
  ///< What happened; see `serial_probe_status`.

  std::vector<std::uint8_t> response;
  ///< Bytes collected before the loop stopped; empty when none arrived.

  std::chrono::milliseconds elapsed = std::chrono::milliseconds (0);
  ///< Time the call took, from the open (when one was needed) to the last
  ///< read.

  std::string system_error;
  ///< Formatted platform error or validation text; empty when none.
};

/**
 * @brief Outcome of `SerialProber::open`, `write` and `read`.
 */
enum class serial_io_status
{
  ok,                ///< The operation completed.
  deadline_exceeded, ///< The deadline expired first.
  failed,            ///< The platform reported an error; see `system_error`.
  not_open           ///< The port is not open; call `open` or `probe` first.
};

/**
 * @brief Result of `SerialProber::open` and `SerialProber::write`.
 */
struct serial_io_result_t
{
  serial_io_status status = serial_io_status::failed; ///< What happened.
  std::size_t bytes_transferred = 0U; ///< Bytes written; 0 for `open`.
  std::string system_error; ///< Formatted platform error; empty when none.
};

/**
 * @brief Result of `SerialProber::read`.
 */
struct serial_read_result_t
{
  serial_io_status status = serial_io_status::failed; ///< What happened.
  std::vector<std::uint8_t> data; ///< Bytes read; empty when none arrived.
  std::string system_error; ///< Formatted platform error; empty when none.
};

/**
 * @brief RAII session that opens one serial port in bounded time, sends a
 * request and waits for the device response.
 *
 * @details Typical use:
 * @code
 * lumex::applied::serial::probe::SerialProber prober (port_path, settings);
 * auto result = prober.probe (
 *     request,
 *     [] (std::vector<std::uint8_t> const &response)
 *     { return response.size () >= 20U; },
 *     std::chrono::milliseconds (500));
 * if (result.status == serial_probe_status::responded)
 *   consume (result.response);
 * @endcode
 *
 * `probe` opens the port when it is closed and keeps it open, so a session
 * can follow it with `read` / `write`; `close` (or the destructor) releases
 * the port. Every wait is bounded by the per-call deadline: on Windows the
 * open itself runs on a worker thread and a stuck open (a paired but
 * disconnected Bluetooth virtual COM port) is reported as `open_timeout`
 * without blocking the caller; the port is then closed by the worker if it
 * ever opens.
 *
 * @note Not thread-safe: one instance is one session on one port.
 * @note Move-only; a moved-from object owns no port and can be destroyed.
 * @note Failures are reported through result statuses; the only exceptions
 * are a throwing `response_complete` predicate (propagated unchanged) and
 * allocation failures of the response buffer.
 */
class LUMEX_PUBLIC_API SerialProber
{
public:
  /**
   * @brief Creates a session for `port_path`; the port is not opened yet.
   * @param port_path Path or name the platform open accepts (`"COM3"` /
   * `"\\\\.\\COM3"` on Windows, `"/dev/ttyUSB0"` on POSIX).
   * @param settings Settings applied when the port is opened.
   */
  SerialProber (std::string port_path, serial_port_settings_t const &settings);

  /// @brief Closes the port when it is open.
  ~SerialProber ();

  /// @brief Not copyable: the session owns one open port.
  SerialProber (SerialProber const &) = delete;
  /// @brief Not copyable: the session owns one open port.
  SerialProber &operator= (SerialProber const &) = delete;

  /// @brief Moves the session, including an open port, into `other`.
  SerialProber (SerialProber &&other) LUMEX_NOEXCEPT;
  /// @brief Moves the session, including an open port, into `other`.
  SerialProber &operator= (SerialProber &&other) LUMEX_NOEXCEPT;

  /**
   * @brief Opens the port with the configured settings inside `deadline`.
   *
   * @param deadline Overall budget for the open; `probe` shares its own
   * budget with this call. A non-positive deadline means the budget is
   * already exhausted.
   * @return `ok` when the port is open; `deadline_exceeded` when the open
   * did not return in time; `failed` when the platform refused the open or
   * the settings are unsupported on this platform (see `system_error`).
   * @note Calling `open` on an open session is a no-op that returns `ok`.
   * @note On Windows the open runs on a worker thread; the caller never
   * waits past `deadline`.
   */
  LUMEX_ATTRIBUTE_NODISCARD ("The caller must read the open outcome.")
  serial_io_result_t
  open (std::chrono::milliseconds deadline
        = std::chrono::milliseconds (Constants::KDEFAULT_PROBE_DEADLINE_MS));

  /**
   * @brief Closes the port; safe to call on a closed session.
   */
  void close () LUMEX_NOEXCEPT;

  /**
   * @brief True while the port is open.
   */
  LUMEX_ATTRIBUTE_NODISCARD ("The open state drives the next call.")
  bool is_open () const LUMEX_NOEXCEPT;

  /**
   * @brief Opens the port when needed, sends `request` and collects the
   * response until `response_complete` confirms it.
   *
   * @param request Bytes written after the input/output buffers are
   * purged; empty means "wait for unsolicited data".
   * @param response_complete Called with the accumulated response after
   * every read chunk; `true` stops the loop with `responded`. It is never
   * called with an empty vector; its exceptions propagate unchanged.
   * @param deadline Overall budget for the open (when the session is
   * closed), the purge, the write and the reads; not per read.
   * @param max_response_bytes Response cap; a value below 1 is clamped to
   * 1. When the cap is reached and the predicate has not confirmed, the
   * outcome is `incomplete`.
   * @return The outcome, the collected bytes, the elapsed time and the
   * formatted platform error (see `serial_probe_status`).
   * @note The port stays open after the call; use `close` or the
   * destructor to release it.
   * @note A request that is not written in full (platform error or
   * deadline) yields `io_error`.
   */
  LUMEX_ATTRIBUTE_NODISCARD ("The caller must read the probe outcome.")
  serial_probe_result_t
  probe (std::vector<std::uint8_t> const &request,
         std::function<bool (std::vector<std::uint8_t> const &)> const
             &response_complete,
         std::chrono::milliseconds deadline
         = std::chrono::milliseconds (Constants::KDEFAULT_PROBE_DEADLINE_MS),
         std::size_t max_response_bytes
         = Constants::KDEFAULT_PROBE_MAX_RESPONSE_BYTES);

  /**
   * @brief Writes every byte of `data` or stops at the deadline.
   *
   * @param data Bytes to send.
   * @param deadline Overall budget for the write.
   * @return `ok` with `bytes_transferred == data.size ()` when every byte
   * left; `deadline_exceeded` when the budget ran out first (the partial
   * count stays in `bytes_transferred`); `failed` on a platform error;
   * `not_open` when the session has no open port.
   */
  LUMEX_ATTRIBUTE_NODISCARD ("The caller must read the write outcome.")
  serial_io_result_t
  write (std::vector<std::uint8_t> const &data,
         std::chrono::milliseconds deadline
         = std::chrono::milliseconds (Constants::KDEFAULT_PROBE_DEADLINE_MS));

  /**
   * @brief Reads once: returns as soon as at least one byte is available
   * or the deadline expires.
   *
   * @param deadline Overall budget for the read.
   * @return `ok` with the bytes that arrived; `deadline_exceeded` with
   * empty `data` when nothing arrived in time; `failed` on a platform
   * error; `not_open` when the session has no open port.
   */
  LUMEX_ATTRIBUTE_NODISCARD ("The caller must read the read outcome.")
  serial_read_result_t
  read (std::chrono::milliseconds deadline
        = std::chrono::milliseconds (Constants::KDEFAULT_PROBE_DEADLINE_MS));

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace probe
} // namespace serial
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_APPLIED_SERIAL_PROBE_HPP
