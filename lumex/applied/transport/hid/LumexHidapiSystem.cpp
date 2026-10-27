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

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include "lumex/applied/transport/hid/LumexHidDevice.hpp"
#include "lumex/applied/transport/hid/detail/LumexHidapi.hpp"
#include "lumex/applied/transport/hid/detail/LumexHidapiSeam.hpp"

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
namespace detail
{
namespace
{
// hid_enumerate and hid_open_path (hidapi's lazy hid_init) are not
// thread-safe. Never destroyed: an open may still run at exit.
std::mutex &
hidapi_mutex ()
{
  static std::mutex *const mutex = new std::mutex ();
  return *mutex;
}

void
clear_os_error ()
{
#if defined(_WIN32)
  ::SetLastError (0);
#else
  errno = 0;
#endif
}

// The error of the operating system right after a failed hidapi call;
// `fallback` when it left none.
std::error_code
os_error_or (std::errc fallback)
{
#if defined(_WIN32)
  int const code = static_cast<int> (::GetLastError ());
#else
  int const code = errno;
#endif
  if (code != 0)
    return std::error_code (code, std::system_category ());
  return std::make_error_code (fallback);
}

void
append_utf8 (std::string &out, std::uint32_t code)
{
  if (code < 0x80U)
    out += static_cast<char> (code);
  else if (code < 0x800U)
    {
      out += static_cast<char> (0xC0U | (code >> 6));
      out += static_cast<char> (0x80U | (code & 0x3FU));
    }
  else if (code < 0x10000U)
    {
      out += static_cast<char> (0xE0U | (code >> 12));
      out += static_cast<char> (0x80U | ((code >> 6) & 0x3FU));
      out += static_cast<char> (0x80U | (code & 0x3FU));
    }
  else
    {
      out += static_cast<char> (0xF0U | (code >> 18));
      out += static_cast<char> (0x80U | ((code >> 12) & 0x3FU));
      out += static_cast<char> (0x80U | ((code >> 6) & 0x3FU));
      out += static_cast<char> (0x80U | (code & 0x3FU));
    }
}

class system_handle final : public hidapi_handle
{
public:
  explicit system_handle (::hid_device *device) : device_ (device) {}

  int
  read_timeout (std::uint8_t *data, std::size_t size, int milliseconds,
                std::error_code &error) override
  {
    clear_os_error ();
    int const read = ::hid_read_timeout (device_, data, size, milliseconds);
    if (read < 0)
      error = os_error_or (std::errc::io_error);
    return read;
  }

  int
  write (std::uint8_t const *data, std::size_t size,
         std::error_code &error) override
  {
    clear_os_error ();
    int const written = ::hid_write (device_, data, size);
    if (written < 0)
      error = os_error_or (std::errc::io_error);
    return written;
  }

  int
  report_descriptor (std::uint8_t *data, std::size_t size,
                     std::error_code &error) override
  {
    clear_os_error ();
    int const read = ::hid_get_report_descriptor (device_, data, size);
    if (read < 0)
      error = os_error_or (std::errc::io_error);
    return read;
  }

  std::string
  last_error_text () override
  {
    return wide_to_utf8 (::hid_error (device_));
  }

  void
  close () override
  {
    ::hid_close (device_);
    device_ = nullptr;
  }

private:
  ::hid_device *device_;
};

class system_seam final : public hidapi_seam
{
public:
  std::vector<hid_device_info_t>
  enumerate (std::uint16_t vendor_id, std::uint16_t product_id) override
  {
    std::vector<hid_device_info_t> found;
    std::lock_guard<std::mutex> const lock (hidapi_mutex ());
    ::hid_device_info *const list = ::hid_enumerate (vendor_id, product_id);
    for (::hid_device_info const *it = list; it != nullptr; it = it->next)
      {
        hid_device_info_t info;
        info.path = it->path != nullptr ? it->path : "";
        info.vendor_id = it->vendor_id;
        info.product_id = it->product_id;
        info.serial_number = wide_to_utf8 (it->serial_number);
        info.manufacturer = wide_to_utf8 (it->manufacturer_string);
        info.product = wide_to_utf8 (it->product_string);
        info.release_number = it->release_number;
        info.usage_page = it->usage_page;
        info.usage = it->usage;
        info.interface_number = it->interface_number;
        found.push_back (info);
      }
    ::hid_free_enumeration (list);
    return found;
  }

  std::unique_ptr<hidapi_handle>
  open_path (std::string const &path, std::error_code &error) override
  {
    std::lock_guard<std::mutex> const lock (hidapi_mutex ());
    clear_os_error ();
    ::hid_device *const device = ::hid_open_path (path.c_str ());
    if (device == nullptr)
      {
        error = os_error_or (std::errc::no_such_device);
        return std::unique_ptr<hidapi_handle> ();
      }
    return std::unique_ptr<hidapi_handle> (new system_handle (device));
  }
};
} // namespace

std::string
wide_to_utf8 (wchar_t const *text)
{
  std::string out;
  if (text == nullptr)
    return out;
  for (std::size_t i = 0; text[i] != 0; ++i)
    {
      std::uint32_t code = static_cast<std::uint32_t> (text[i]);
      if (sizeof (wchar_t) == 2 && code >= 0xD800U && code <= 0xDBFFU)
        {
          std::uint32_t const low = static_cast<std::uint32_t> (text[i + 1]);
          if (low >= 0xDC00U && low <= 0xDFFFU)
            {
              code = 0x10000U + ((code - 0xD800U) << 10) + (low - 0xDC00U);
              ++i;
            }
          else
            code = 0xFFFDU;
        }
      else if ((code >= 0xD800U && code <= 0xDFFFU) || code > 0x10FFFFU)
        code = 0xFFFDU;
      append_utf8 (out, code);
    }
  return out;
}

std::shared_ptr<hidapi_seam>
system_hidapi ()
{
  static std::shared_ptr<hidapi_seam> const *const seam
      = new std::shared_ptr<hidapi_seam> (std::make_shared<system_seam> ());
  return *seam;
}
} // namespace detail
} // namespace hid
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
