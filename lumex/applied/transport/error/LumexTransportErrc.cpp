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

#include <cstring>
#include <string>
#include <system_error>

#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/detail/LumexAsioErrors.hpp"
#include "lumex/applied/transport/error/LumexTransportErrc.hpp"
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
namespace error
{
namespace
{
class transport_category_impl final : public std::error_category
{
public:
  char const *
  name () const LUMEX_NOEXCEPT override
  {
    return KTRANSPORT_CATEGORY_NAME;
  }

  std::string
  message (int value) const override
  {
    // The switch is on the int: a value outside the enum is possible here.
    switch (value)
      {
      case static_cast<int> (transport_errc::timeout):
        return "operation timed out";
      case static_cast<int> (transport_errc::cancelled):
        return "operation cancelled";
      case static_cast<int> (transport_errc::closed):
        return "transport closed";
      case static_cast<int> (transport_errc::not_open):
        return "transport not open";
      case static_cast<int> (transport_errc::disconnected):
        return "peer disconnected";
      case static_cast<int> (transport_errc::would_deadlock):
        return "synchronous call on the transport's own executor thread";
      case static_cast<int> (transport_errc::invalid_endpoint):
        return "invalid endpoint";
      case static_cast<int> (transport_errc::report_too_large):
        return "message larger than the device report";
      case static_cast<int> (transport_errc::bad_report_descriptor):
        return "unreadable HID report descriptor";
      case static_cast<int> (transport_errc::permission_denied):
        return "access to the device denied";
      default:
        return "unknown transport error";
      }
  }

  // The portable meaning of a value, so that a caller can compare a code
  // with std::errc without knowing the category: only where the standard
  // condition means the same thing. The other values have no portable
  // counterpart (closed, not_open and disconnected cover several errno
  // values or none) and stay in this category. A code still equals the
  // condition of its own value (make_error_condition (transport_errc)): the
  // standard comparison also asks the condition's category.
  std::error_condition
  default_error_condition (int value) const LUMEX_NOEXCEPT override
  {
    switch (value)
      {
      case static_cast<int> (transport_errc::timeout):
        return std::make_error_condition (std::errc::timed_out);
      case static_cast<int> (transport_errc::cancelled):
        return std::make_error_condition (std::errc::operation_canceled);
      case static_cast<int> (transport_errc::permission_denied):
        return std::make_error_condition (std::errc::permission_denied);
      default:
        return std::error_condition (value, *this);
      }
  }
};
} // namespace

std::error_category const &
transport_category () LUMEX_NOEXCEPT
{
  // Never destroyed: a code of this category may still be printed by a
  // static destructor at exit.
  static transport_category_impl const *const instance
      = new transport_category_impl ();
  return *instance;
}

bool
is_transport_error (std::error_code const &code,
                    transport_errc value) LUMEX_NOEXCEPT
{
  return code.value () == static_cast<int> (value)
         && (&code.category () == &transport_category ()
             || std::strcmp (code.category ().name (),
                             KTRANSPORT_CATEGORY_NAME)
                    == 0);
}
} // namespace error

namespace detail
{
std::error_code
from_asio (std::error_code const &code)
{
  if (!code)
    return code;
  if (code == asio::error::operation_aborted)
    return error::transport_errc::cancelled;
  if (code == asio::error::eof)
    return error::transport_errc::disconnected;
  // By value, not by category: on Windows the netdb and addrinfo categories
  // of Asio are its system category.
  if (code == asio::error::host_not_found
      || code == asio::error::host_not_found_try_again
      || code == asio::error::no_data || code == asio::error::no_recovery
      || code == asio::error::service_not_found
      || code == asio::error::socket_type_not_supported)
    return error::transport_errc::invalid_endpoint;
  // Asio reports operating system errors in its own "asio.system" category,
  // one object per copy of Asio in the process; the caller gets the same
  // value in std::system_category.
  if (code.category () == asio::system_category ())
    return std::error_code (code.value (), std::system_category ());
  return code;
}
} // namespace detail
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
