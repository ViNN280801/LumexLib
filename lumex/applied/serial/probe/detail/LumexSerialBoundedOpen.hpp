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
 * @file LumexSerialBoundedOpen.hpp
 * @brief Internal bounded open shared by the serial prober and the port
 * enumeration.
 *
 * @details The helper opens a port so that the caller never waits longer
 * than the deadline and classifies the outcome into a `serial_port_state`.
 * A paired but disconnected Bluetooth virtual COM port blocks inside
 * `CreateFile` in the RFCOMM connect, so the Windows branch runs the open
 * on a worker thread and cancels it with `CancelSynchronousIo`; the POSIX
 * branch opens with `O_NONBLOCK`. The enumeration and `LumexSerialProber`
 * are the only callers. Consumers must not include this header.
 */
#ifndef LUMEX_APPLIED_SERIAL_PROBE_DETAIL_SERIAL_BOUNDED_OPEN_HPP
#define LUMEX_APPLIED_SERIAL_PROBE_DETAIL_SERIAL_BOUNDED_OPEN_HPP

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#endif

#include "lumex/LumexExport.hpp"

#include <chrono>
#include <cstdint>
#include <string>

#include "lumex/applied/serial/enumeration/LumexSerialPortEnumeration.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"
#include "lumex/core/utility/os/LumexCheckOS.hpp"

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
#if defined(LUMEX_OS_WINDOWS)
/**
 * @brief Native handle type; `HANDLE` without including `Windows.h`.
 */
using native_serial_handle_t = void *;
#else
/**
 * @brief Native handle type: a file descriptor.
 */
using native_serial_handle_t = int;
#endif

/**
 * @brief The platform value that means "no handle".
 */
inline native_serial_handle_t
invalid_native_serial_handle () LUMEX_NOEXCEPT
{
#if defined(LUMEX_OS_WINDOWS)
  return reinterpret_cast<void *> (static_cast<std::intptr_t> (-1));
#else
  return -1;
#endif
}

/**
 * @brief How `bounded_open_serial_port` ended.
 */
enum class open_outcome
{
  opened,    ///< The port is open; `handle` is valid.
  timed_out, ///< The open did not return within the deadline.
  failed     ///< The platform refused the open; see `error_code`.
};

/**
 * @brief Result of one bounded open.
 */
struct open_result_t
{
  open_outcome outcome = open_outcome::failed; ///< What happened.
  native_serial_handle_t handle = invalid_native_serial_handle ();
  ///< Open handle on `opened`; the caller closes it through
  ///< `close_serial_handle`.
  int error_code = 0;       ///< `GetLastError` / `errno`; 0 when none.
  std::string system_error; ///< Formatted error or timeout text.
};

/**
 * @brief Opens `open_path` so the caller never waits past `deadline`.
 *
 * @param open_path Platform path (`"\\\\.\\COM3"`, `"/dev/ttyUSB0"`).
 * @param platform_open_flags `CreateFileA` `dwFlagsAndAttributes` on
 * Windows (`0` for a classification probe, `FILE_FLAG_OVERLAPPED` for the
 * transport); POSIX ignores it and always opens
 * `O_RDWR | O_NOCTTY | O_NONBLOCK`.
 * @param deadline Budget for the open; a non-positive deadline reports
 * `timed_out` without touching the port.
 * @return `opened` with the handle to close; `timed_out` with the timeout
 * text (no handle is owned by the caller); `failed` with the platform
 * error.
 * @note On Windows a cancelled or abandoned worker closes a late handle
 * itself, so a `timed_out` result never owns one.
 */
open_result_t bounded_open_serial_port (std::string const &open_path,
                                        std::uint32_t platform_open_flags,
                                        std::chrono::milliseconds deadline);

/**
 * @brief Closes `handle`; a no-op for the invalid value.
 */
void close_serial_handle (native_serial_handle_t handle) LUMEX_NOEXCEPT;

/**
 * @brief Maps an open outcome to the enumeration state.
 * @param result Result of `bounded_open_serial_port`.
 * @return `available` for `opened`; `unresponsive` for `timed_out`; `busy`
 * for access-denied / sharing-violation / `EBUSY` / `EACCES` failures;
 * `free` otherwise.
 */
LUMEX_PUBLIC_API enumeration::serial_port_state
state_from_open_result (open_result_t const &result) LUMEX_NOEXCEPT;

} // namespace detail
} // namespace probe
} // namespace serial
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_APPLIED_SERIAL_PROBE_DETAIL_SERIAL_BOUNDED_OPEN_HPP
