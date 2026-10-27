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

#include <cctype>
#include <cstddef>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "lumex/applied/transport/hid/LumexHidDevice.hpp"
#include "lumex/applied/transport/hid/LumexHidSelectors.hpp"
#include "lumex/applied/transport/hid/detail/LumexHidapiSeam.hpp"
#include "lumex/applied/transport/session/LumexMessageSession.hpp"

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
namespace hid
{
namespace
{
char
upper (char c) LUMEX_NOEXCEPT
{
  return static_cast<char> (std::toupper (static_cast<unsigned char> (c)));
}

bool
equal_no_case (std::string const &lhs, std::string const &rhs)
{
  if (lhs.size () != rhs.size ())
    return false;
  for (std::size_t i = 0; i < lhs.size (); ++i)
    if (upper (lhs[i]) != upper (rhs[i]))
      return false;
  return true;
}

std::string
hex4 (unsigned value)
{
  char text[8];
  std::snprintf (text, sizeof text, "%04X", value);
  return text;
}

hid_enumeration_t
enumeration_over (std::shared_ptr<detail::hidapi_seam> const &seam)
{
  return [seam] ()
    { return detail::enumerate_hid_devices_with (*seam, hid_filter_t ()); };
}

hid_options_t
options_of (hid_selector_t const &selector, hid_enumeration_t enumeration)
{
  hid_options_t options = selector.options;
  options.vendor_id = 0;
  options.product_id = 0;
  options.serial_number.clear ();
  if (options.name.empty ())
    options.name = describe (selector);
  if (!selector.path.empty ())
    {
      options.path = selector.path;
      options.resolve_path = std::function<std::string ()> ();
    }
  else
    {
      // The device is found at each open, on the open's worker thread, not
      // here: a session runs its factory on its strand at every attempt.
      options.path.clear ();
      options.resolve_path = [selector, enumeration] ()
        { return find_hid_device (selector, enumeration ()); };
    }
  return options;
}
} // namespace

hid_enumeration_t
default_hid_enumeration ()
{
  return [] () { return enumerate_hid_devices (); };
}

bool
matches (hid_selector_t const &selector, hid_device_info_t const &device)
{
  if (!selector.path.empty ())
    return selector.path == device.path;
  if (selector.vendor_id == 0 && selector.product_id == 0
      && selector.serial_number.empty () && selector.interface_number < 0
      && selector.usage_page == 0 && selector.usage == 0)
    return false;
  if (selector.vendor_id != 0 && selector.vendor_id != device.vendor_id)
    return false;
  if (selector.product_id != 0 && selector.product_id != device.product_id)
    return false;
  if (!selector.serial_number.empty ()
      && !equal_no_case (selector.serial_number, device.serial_number))
    return false;
  if (selector.interface_number >= 0
      && selector.interface_number != device.interface_number)
    return false;
  if (selector.usage_page != 0 && selector.usage_page != device.usage_page)
    return false;
  if (selector.usage != 0 && selector.usage != device.usage)
    return false;
  return true;
}

std::string
find_hid_device (hid_selector_t const &selector,
                 std::vector<hid_device_info_t> const &devices)
{
  if (!selector.path.empty ())
    return selector.path;
  for (std::size_t i = 0; i < devices.size (); ++i)
    if (matches (selector, devices[i]))
      return devices[i].path;
  return std::string ();
}

std::string
describe (hid_selector_t const &selector)
{
  if (!selector.path.empty ())
    return selector.path;
  std::string text = "HID";
  if (selector.vendor_id != 0)
    text += " VID " + hex4 (selector.vendor_id);
  if (selector.product_id != 0)
    text += " PID " + hex4 (selector.product_id);
  if (!selector.serial_number.empty ())
    text += " SN " + selector.serial_number;
  if (selector.interface_number >= 0)
    text += " IF " + std::to_string (selector.interface_number);
  if (selector.usage_page != 0)
    text += " UP " + hex4 (selector.usage_page);
  if (selector.usage != 0)
    text += " U " + hex4 (selector.usage);
  return text;
}

session::message_session::factory_t
make_hid_factory (hid_selector_t const &selector,
                  hid_enumeration_t enumeration)
{
  if (!enumeration)
    enumeration = default_hid_enumeration ();
  hid_options_t const options = options_of (selector, std::move (enumeration));
  return [options] (executor_type const &executor)
    { return make_hid_device (executor, options); };
}

session::message_session::factory_t
detail::make_hid_factory_with (std::shared_ptr<hidapi_seam> seam,
                               hid_selector_t const &selector,
                               hid_enumeration_t enumeration)
{
  if (!enumeration)
    enumeration = enumeration_over (seam);
  hid_options_t const options = options_of (selector, std::move (enumeration));
  return [seam, options] (executor_type const &executor)
    { return make_hid_device_with (seam, executor, options); };
}
} // namespace hid
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
