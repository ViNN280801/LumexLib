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
 * @file LumexHidapiSeam.hpp
 * @brief The seam between `hid_device` and hidapi: the calls the transport
 * makes, behind an interface that the tests replace with a fake.
 * @details Not a public API: the functions with a seam exist for the
 * library's tests. `system_hidapi ()` is the real hidapi; it serializes
 * `hid_enumerate` and `hid_open_path` (hidapi's lazy initialization and
 * enumeration are not thread-safe) and never calls `hid_exit`.
 */
#ifndef LUMEX_APPLIED_TRANSPORT_HID_DETAIL_HIDAPI_SEAM_HPP
#define LUMEX_APPLIED_TRANSPORT_HID_DETAIL_HIDAPI_SEAM_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

#include "lumex/applied/transport/hid/LumexHidDevice.hpp"
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
namespace detail
{
/// @brief An open hidapi device. The destructor makes no hidapi call: a
/// handle that was not closed is abandoned.
class hidapi_handle
{
public:
  virtual ~hidapi_handle ();

  /// @brief `hid_read_timeout`: the bytes read, 0 when the slice passed, -1
  /// on failure (`error` set).
  virtual int read_timeout (std::uint8_t *data, std::size_t size,
                            int milliseconds, std::error_code &error)
      = 0;
  /// @brief `hid_write`: `data[0]` is the report ID (0 when unnumbered).
  virtual int write (std::uint8_t const *data, std::size_t size,
                     std::error_code &error)
      = 0;
  /// @brief `hid_get_report_descriptor`.
  virtual int report_descriptor (std::uint8_t *data, std::size_t size,
                                 std::error_code &error)
      = 0;
  /// @brief `hid_error` of the handle, in UTF-8.
  virtual std::string last_error_text () = 0;
  /// @brief `hid_close`; called once.
  virtual void close () = 0;
};

/// @brief The hidapi functions without a handle.
class hidapi_seam
{
public:
  virtual ~hidapi_seam ();

  /// @brief `hid_enumerate (vendor_id, product_id)` (0 matches any).
  virtual std::vector<hid_device_info_t> enumerate (std::uint16_t vendor_id,
                                                    std::uint16_t product_id)
      = 0;
  /// @brief `hid_open_path`; null with `error` set on failure.
  virtual std::unique_ptr<hidapi_handle> open_path (std::string const &path,
                                                    std::error_code &error)
      = 0;
};

/// @brief A hidapi string (UTF-16 on Windows, UTF-32 elsewhere) in UTF-8;
/// an unpaired surrogate or a value above U+10FFFF becomes U+FFFD, null
/// becomes empty.
std::string wide_to_utf8 (wchar_t const *text);

/// @brief The real hidapi (one shared instance).
std::shared_ptr<hidapi_seam> system_hidapi ();

/// @brief `enumerate_hid_devices` over `seam`.
std::vector<hid_device_info_t>
enumerate_hid_devices_with (hidapi_seam &seam, hid_filter_t const &filter);

/// @brief How many reports and read failures of an already closed
/// connection reached the device's strand and were dropped there (a test
/// probe of the reader's stop checks; 0 for another transport).
std::size_t stale_events (hid_device const &device);

/// @brief `make_hid_device` over `seam`.
hid_device make_hid_device_with (std::shared_ptr<hidapi_seam> seam,
                                 executor_type const &executor,
                                 hid_options_t const &options);
} // namespace detail
} // namespace hid
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_APPLIED_TRANSPORT_HID_DETAIL_HIDAPI_SEAM_HPP
