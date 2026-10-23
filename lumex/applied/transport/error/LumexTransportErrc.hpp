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
 * @file LumexTransportErrc.hpp
 * @brief The error codes of the transports: `transport_errc`, its category,
 * `is_transport_error` and the mapping of Asio's own codes.
 * @details The transport library is always static, so every binary that
 * links it (an executable, each DLL) has its own category object: a code made
 * in one binary compares equal to `transport_errc` only inside that binary.
 * `is_transport_error` also matches a code of another binary by the category
 * name. System errors pass unchanged (`std::system_category`); the codes of
 * Asio's own categories (`asio::error::operation_aborted`, `eof`, ...) are
 * translated at the boundary by `from_asio` and never reach a caller.
 *
 * The category maps `timeout`, `cancelled` and `permission_denied` to their
 * portable conditions (`std::errc::timed_out`, `operation_canceled`,
 * `permission_denied`), so `code == std::errc::permission_denied` holds for
 * them; the other values have no equivalent and compare only with
 * `transport_errc`.
 */
#ifndef LUMEX_APPLIED_TRANSPORT_ERROR_ERRC_HPP
#define LUMEX_APPLIED_TRANSPORT_ERROR_ERRC_HPP

#include <system_error>
#include <type_traits>

#include "lumex/core/utility/macros/LumexKeywords.hpp"

namespace lumex
{
namespace applied
{
namespace transport
{
namespace error
{
/// @brief Why a transport operation did not complete.
enum class transport_errc : int
{
  /// The deadline passed before the operation completed.
  timeout = 1,
  /// `cancel ()` or `close ()` ended the operation.
  cancelled,
  /// The transport was closed.
  closed,
  /// The operation needs an open transport.
  not_open,
  /// The peer went away (end of stream, reset, device removed).
  disconnected,
  /// A synchronous call from a thread that runs the transport's executor
  /// would wait for itself.
  would_deadlock,
  /// The address, host, port or device path cannot be used.
  invalid_endpoint,
  /// A message is larger than the device report can hold.
  report_too_large,
  /// The report descriptor of a HID device cannot be read.
  bad_report_descriptor,
  /// The operating system refused access to the device node: a POSIX open
  /// failed with `EACCES` or `EPERM`. Not produced on Windows, where
  /// `ERROR_ACCESS_DENIED` from opening a COM port means that the port is
  /// busy and stays a system error.
  permission_denied,
  /// The HID device has no output report with the message's report ID (a
  /// device that numbers no report has report 0 at most).
  unknown_report_id
};

/// @brief The last enumerator of `transport_errc`. Move it to the new value
/// whenever one is added after it: `statistics_t::errors` has one slot per
/// value up to it, and a `static_assert` there compares against it.
LUMEX_CONSTEXPR transport_errc KTRANSPORT_ERRC_LAST
    = transport_errc::unknown_report_id;

/// @brief The name of the category, also used by `is_transport_error`.
LUMEX_CONSTEXPR char const *KTRANSPORT_CATEGORY_NAME = "lumex.transport";

/// @brief The category of `transport_errc` in this binary.
std::error_category const &transport_category () LUMEX_NOEXCEPT;

/// @brief Makes the `std::error_code` of a `transport_errc`.
inline std::error_code
make_error_code (transport_errc value) LUMEX_NOEXCEPT
{
  return std::error_code (static_cast<int> (value), transport_category ());
}

/// @brief Makes the `std::error_condition` of a `transport_errc`.
inline std::error_condition
make_error_condition (transport_errc value) LUMEX_NOEXCEPT
{
  return std::error_condition (static_cast<int> (value),
                               transport_category ());
}

/**
 * @brief True when `code` is `value` of a transport category, of this binary
 * or of another one that links the transport library too.
 */
bool is_transport_error (std::error_code const &code,
                         transport_errc value) LUMEX_NOEXCEPT;
} // namespace error
} // namespace transport
} // namespace applied
} // namespace lumex

namespace std
{
template <>
struct is_error_code_enum<lumex::applied::transport::error::transport_errc>
    : true_type
{
};
} // namespace std

#endif // !LUMEX_APPLIED_TRANSPORT_ERROR_ERRC_HPP
