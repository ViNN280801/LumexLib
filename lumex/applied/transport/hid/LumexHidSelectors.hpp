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
 * @file LumexHidSelectors.hpp
 * @brief The selector of a HID device (spec G1): USB vendor and product ID,
 * serial number, interface number, usage page and usage, or the platform
 * path; it makes the factory of a `message_session` and is what
 * `session::device_watcher` watches for.
 * @details A selector names a device by what it is, not by where it is: a
 * device that came back under another path (the cable unplugged and
 * plugged, the node renumbered) is found again, because the factory looks
 * it up in the enumeration at every open. The enumeration (hidapi's
 * `hid_enumerate`) lists one record per HID interface and top-level
 * collection, so `interface_number`, `usage_page` and `usage` tell the
 * interfaces of one composite device apart. Like the serial selector, a
 * selector with no field set matches no device.
 *
 * The factory itself does not enumerate (a session runs its factory on its
 * strand at every attempt): the device it makes finds its path at each
 * open, on the open's worker thread (`hid_options_t::resolve_path`), and the
 * open fails with `transport_errc::invalid_endpoint` when no device
 * matches (a failed attempt, the session retries by its policy).
 */
#ifndef LUMEX_APPLIED_TRANSPORT_HID_HID_SELECTORS_HPP
#define LUMEX_APPLIED_TRANSPORT_HID_HID_SELECTORS_HPP

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "lumex/applied/transport/hid/LumexHidDevice.hpp"
#include "lumex/applied/transport/session/LumexMessageSession.hpp"
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
namespace hid
{
/// @brief The HID devices present now (the seam of the selectors and of
/// `session::device_watcher`; tests give a fake).
using hid_enumeration_t = std::function<std::vector<hid_device_info_t> ()>;

/// @brief `enumerate_hid_devices ()`: every HID interface, no device opened.
hid_enumeration_t default_hid_enumeration ();

/**
 * @brief A HID device to open. With `path` set, the rest is not looked at;
 * otherwise each non-zero or non-empty field must match, and a selector
 * with none set matches no device.
 */
struct hid_selector_t
{
  /// The platform path (`hid_device_info_t::path`), compared as it is.
  std::string path;
  /// USB vendor ID; 0 matches any.
  std::uint16_t vendor_id = 0;
  /// USB product ID; 0 matches any.
  std::uint16_t product_id = 0;
  /// The serial number (compared without case); empty matches any.
  std::string serial_number;
  /// The USB interface number; -1 matches any (a device whose interface
  /// number is unknown, -1, matches only -1).
  int interface_number = -1;
  /// The usage page of the top-level collection; 0 matches any.
  std::uint16_t usage_page = 0;
  /// The usage of the top-level collection; 0 matches any.
  std::uint16_t usage = 0;
  /// The rest of what the device is opened with (`name`, `read_slice`,
  /// `max_queued_reports`); the factory replaces its `path`, `vendor_id`,
  /// `product_id`, `serial_number` and `resolve_path`.
  hid_options_t options;
};

/// @brief True when `selector` names `device`.
bool matches (hid_selector_t const &selector, hid_device_info_t const &device);

/// @brief The path of the first of `devices` that `selector` names (the
/// path of the selector itself when it has one), or empty.
std::string find_hid_device (hid_selector_t const &selector,
                             std::vector<hid_device_info_t> const &devices);

/// @brief A readable form of the selector for logs ("HID VID 16E0 PID 0001
/// SN 123 IF 1 UP FF00 U 0001", or the path).
std::string describe (hid_selector_t const &selector);

/**
 * @brief The factory of a `message_session` over the HID device `selector`
 * names (see the file description). `enumeration` (empty:
 * `default_hid_enumeration ()`) runs on the worker thread of each open, so
 * it may run on several threads at once: it must be thread-safe.
 */
session::message_session::factory_t
make_hid_factory (hid_selector_t const &selector,
                  hid_enumeration_t enumeration = hid_enumeration_t ());

namespace detail
{
class hidapi_seam;

/// @brief `make_hid_factory` over `seam` (the devices it makes and, with an
/// empty `enumeration`, the enumeration use it); for the library's tests.
session::message_session::factory_t
make_hid_factory_with (std::shared_ptr<hidapi_seam> seam,
                       hid_selector_t const &selector,
                       hid_enumeration_t enumeration = hid_enumeration_t ());
} // namespace detail
} // namespace hid
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_APPLIED_TRANSPORT_HID_HID_SELECTORS_HPP
