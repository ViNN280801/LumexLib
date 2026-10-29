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

#include <string>
#include <system_error>

#include "lumex/core/protocol/error/LumexProtocolErrc.hpp"
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
namespace core
{
namespace protocol
{
namespace error
{
namespace
{
class protocol_category_impl final : public std::error_category
{
public:
  char const *
  name () const LUMEX_NOEXCEPT override
  {
    return "lumex.protocol";
  }

  std::string
  message (int value) const override
  {
    // The switch is on the int: a value outside the enum is possible here.
    switch (value)
      {
      case static_cast<int> (protocol_errc::truncated):
        return "message shorter than its fields";
      case static_cast<int> (protocol_errc::trailing_bytes):
        return "bytes after the last field of a message";
      case static_cast<int> (protocol_errc::invalid_value):
        return "field value the field type cannot represent";
      case static_cast<int> (protocol_errc::unknown_frame):
        return "frame matches no row of the protocol";
      case static_cast<int> (protocol_errc::limit_exceeded):
        return "count larger than the capacity of its field";
      case static_cast<int> (protocol_errc::unknown_case):
        return "discriminant matches no case of the union";
      case static_cast<int> (protocol_errc::access_denied):
        return "the registry entry does not allow this access";
      case static_cast<int> (protocol_errc::not_supported):
        return "not available in the firmware or model of the session";
      case static_cast<int> (protocol_errc::duplicate_key):
        return "two entries of a table have the same key";
      case static_cast<int> (protocol_errc::out_of_limits):
        return "value outside the limits of the registry entry";
      case static_cast<int> (protocol_errc::timeout):
        return "no reply within the timeout";
      case static_cast<int> (protocol_errc::retries_exhausted):
        return "no reply after every retry";
      case static_cast<int> (protocol_errc::nak):
        return "the device answered with a negative acknowledgement";
      case static_cast<int> (protocol_errc::refused):
        return "the device refused the request";
      case static_cast<int> (protocol_errc::unexpected_reply):
        return "a reply other than the one expected";
      case static_cast<int> (protocol_errc::not_accepted_in_state):
        return "the device does not accept the request in its current state";
      case static_cast<int> (protocol_errc::link_lost):
        return "the link to the device was lost";
      case static_cast<int> (protocol_errc::not_confirmed):
        return "the device did not confirm the value it was sent";
      default:
        return "unknown protocol error";
      }
  }
};
} // namespace

std::error_category const &
protocol_category () LUMEX_NOEXCEPT
{
  // Never destroyed: an error code of this category may still be compared or
  // printed by a static destructor of another module at exit.
  static protocol_category_impl const *const instance
      = new protocol_category_impl ();
  return *instance;
}
} // namespace error
} // namespace protocol
} // namespace core
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
