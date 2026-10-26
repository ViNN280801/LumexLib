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
 * @file LumexSystemError.hpp
 * @brief `format_system_error()`, the text of a system error code.
 * @details Header-only and usable from C++11. The code is `GetLastError ()`
 * on Windows (text from `FormatMessageW`, converted to UTF-8) and `errno`
 * elsewhere (text from `strerror`). The header includes `<windows.h>`
 * (defining `WIN32_LEAN_AND_MEAN` and `NOMINMAX` unless they are set) or
 * `<cstring>`.
 */
#ifndef LUMEX_CORE_UTILITY_OS_SYSTEM_ERROR_HPP
#define LUMEX_CORE_UTILITY_OS_SYSTEM_ERROR_HPP

#include <cstddef>
#include <string>

#include "lumex/core/utility/macros/LumexKeywords.hpp"
#include "lumex/core/utility/os/LumexCheckOS.hpp"

#ifdef LUMEX_OS_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cstring>
#endif

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace core
{
namespace utility
{
namespace os
{
#if defined(__clang__)
#pragma clang diagnostic push
// The fixed message buffer is indexed by the length FormatMessageW returned.
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#endif

#ifdef LUMEX_OS_WINDOWS
namespace detail
{
/**
 * @brief Converts `length` UTF-16 units of `text` to UTF-8.
 * @details A unit that cannot be decoded (an unpaired surrogate) becomes
 * U+FFFD. Exposed for the tests: the system texts of an English Windows are
 * ASCII and cannot show a wrong code page.
 * @return The UTF-8 text; empty when `length` is not positive.
 */
inline std::string
utf16_to_utf8 (wchar_t const *text, int length) LUMEX_NOEXCEPT
{
  std::string result;
  if (length <= 0)
    return result;
  int const size = ::WideCharToMultiByte (CP_UTF8, 0, text, length, nullptr, 0,
                                          nullptr, nullptr);
  if (size > 0)
    {
      result.resize (static_cast<std::size_t> (size));
      ::WideCharToMultiByte (CP_UTF8, 0, text, length, &result[0], size,
                             nullptr, nullptr);
    }
  return result;
}
} // namespace detail
#endif

/**
 * @brief Returns the text of a system error code, followed by the code.
 * @details `"Access is denied. (5)"` on Windows, `"Permission denied (13)"`
 * on POSIX. When the system has no text for the code the result is
 * `"error <code>"` (Windows) or `"errno <code>"` (POSIX). The text is in the
 * language of the system and in UTF-8 on every platform; the line break that
 * `FormatMessageW` appends is removed.
 * @param err_code `GetLastError ()` on Windows, `errno` on POSIX.
 * @return The text, or an empty string when `err_code` is 0.
 * @note `strerror` may share a buffer between threads on old C libraries;
 * the text is copied at once.
 */
inline std::string
format_system_error (int err_code) LUMEX_NOEXCEPT
{
  if (err_code == 0)
    return std::string ();
#ifdef LUMEX_OS_WINDOWS
  wchar_t buffer[256] = {};
  DWORD length = ::FormatMessageW (
      FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr,
      static_cast<DWORD> (err_code),
      MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT), buffer,
      static_cast<DWORD> (sizeof (buffer) / sizeof (buffer[0])), nullptr);
  while (length > 0
         && (buffer[length - 1] == L'\r' || buffer[length - 1] == L'\n'))
    --length;
  std::string const text
      = detail::utf16_to_utf8 (buffer, static_cast<int> (length));
  return text.empty () ? ("error " + std::to_string (err_code))
                       : (text + " (" + std::to_string (err_code) + ")");
#else
  char const *text = ::strerror (err_code);
  return text != nullptr
             ? (std::string (text) + " (" + std::to_string (err_code) + ")")
             : ("errno " + std::to_string (err_code));
#endif
}

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

} // namespace os
} // namespace utility
} // namespace core
} // namespace lumex

#endif // !LUMEX_CORE_UTILITY_OS_SYSTEM_ERROR_HPP
