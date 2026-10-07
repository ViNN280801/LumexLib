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

/*
 * Portions of this file are derived from pugixml (https://pugixml.org), MIT
 * license, Copyright (c) 2006-2026 Arseny Kapoulkine. The full notice is in
 * THIRD-PARTY-NOTICES.md, which is installed with LumexLib.
 */

/**
 * @file LumexUnicodeConvert.hpp
 * @brief `to_utf8` and `to_wide`: conversion of a `wchar_t` string to UTF-8
 * and of a UTF-8 string to `wchar_t`, the same on every platform.
 * @details The functions do not depend on the C locale, the operating system
 * or the standard library facets (`mbstowcs`, `std::wstring_convert`,
 * `WideCharToMultiByte`): they run the decoders and policies of
 * `utf/LumexUtf.hpp` twice, once to count and once to write. A `wchar_t`
 * string is read as UTF-16 where `wchar_t` has 2 bytes (Windows) and as
 * UTF-32 where it has 4 (Linux, macOS). Invalid input is not an error: a
 * byte or a unit that cannot be decoded is skipped, so an unpaired surrogate
 * or a truncated UTF-8 sequence leaves no trace in the result. Every
 * function has an overload that takes a pointer and a length, which keeps
 * embedded zero characters.
 *
 * Header-only, works from C++11. The functions allocate the result string
 * and may throw `std::bad_alloc`.
 */
#ifndef LUMEX_CORE_UNICODE_CONVERT_HPP
#define LUMEX_CORE_UNICODE_CONVERT_HPP

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#if __has_warning("-Wunsafe-buffer-usage-in-libc-call")
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-libc-call"
#endif
#endif

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <string>

#include "lumex/core/unicode/utf/LumexUtf.hpp"
#include "lumex/core/utility/assert/LumexAssert.hpp"

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace core
{
namespace unicode
{
namespace convert
{
/**
 * @brief Converts `length` characters of a `wchar_t` string to UTF-8.
 * @param[in] str The characters; may be null only when `length` is 0.
 * @param[in] length The number of `wchar_t` units to convert, zero units
 * included.
 * @return The UTF-8 text. A unit that cannot be decoded (an unpaired
 * surrogate where `wchar_t` is 16 bits wide) is left out.
 */
inline std::string
to_utf8 (wchar_t const *str, std::size_t length)
{
  LUMEX_ASSERT (str != nullptr || length == 0);

  // first pass: get length in utf8 units
  std::size_t const size
      = utf::wchar_decoder::process (str, length, 0, utf::utf8_counter ());

  // allocate resulting string
  std::string result;
  result.resize (size);

  // second pass: convert to utf8
  if (size > 0)
    {
      std::uint8_t *const begin
          = reinterpret_cast<std::uint8_t *> (&result[0]);
      std::uint8_t *const end = utf::wchar_decoder::process (
          str, length, begin, utf::utf8_writer ());

      LUMEX_ASSERT (begin + size == end);
    }

  return result;
}

/**
 * @brief Converts a zero-terminated `wchar_t` string to UTF-8.
 * @param[in] str The string; must not be null.
 * @return The UTF-8 text, see `to_utf8 (wchar_t const *, std::size_t)`.
 */
inline std::string
to_utf8 (wchar_t const *str)
{
  LUMEX_ASSERT (str);

  return to_utf8 (str, std::wcslen (str));
}

/**
 * @brief Converts a `std::wstring` to UTF-8.
 * @param[in] str The string; embedded zero characters are converted.
 * @return The UTF-8 text, see `to_utf8 (wchar_t const *, std::size_t)`.
 */
inline std::string
to_utf8 (std::wstring const &str)
{
  return to_utf8 (str.c_str (), str.size ());
}

/**
 * @brief Converts `length` bytes of UTF-8 text to a `wchar_t` string.
 * @param[in] str The bytes; may be null only when `length` is 0.
 * @param[in] length The number of bytes to convert, zero bytes included.
 * @return The text as UTF-16 where `wchar_t` has 2 bytes and as UTF-32 where
 * it has 4. A byte that does not start a complete sequence is left out.
 */
inline std::wstring
to_wide (char const *str, std::size_t length)
{
  LUMEX_ASSERT (str != nullptr || length == 0);

  std::uint8_t const *const data
      = reinterpret_cast<std::uint8_t const *> (str);

  // first pass: get length in wchar_t units
  std::size_t const count
      = utf::utf8_decoder::process (data, length, 0, utf::wchar_counter ());

  // allocate resulting string
  std::wstring result;
  result.resize (count);

  // second pass: convert to wchar_t
  if (count > 0)
    {
      utf::wchar_writer::value_type const begin
          = reinterpret_cast<utf::wchar_writer::value_type> (&result[0]);
      utf::wchar_writer::value_type const end = utf::utf8_decoder::process (
          data, length, begin, utf::wchar_writer ());

      LUMEX_ASSERT (begin + count == end);
    }

  return result;
}

/**
 * @brief Converts a zero-terminated UTF-8 string to a `wchar_t` string.
 * @param[in] str The string; must not be null.
 * @return The text, see `to_wide (char const *, std::size_t)`.
 */
inline std::wstring
to_wide (char const *str)
{
  LUMEX_ASSERT (str);

  return to_wide (str, std::strlen (str));
}

/**
 * @brief Converts a UTF-8 `std::string` to a `wchar_t` string.
 * @param[in] str The string; embedded zero bytes are converted.
 * @return The text, see `to_wide (char const *, std::size_t)`.
 */
inline std::wstring
to_wide (std::string const &str)
{
  return to_wide (str.c_str (), str.size ());
}
} // namespace convert
} // namespace unicode
} // namespace core
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_UNICODE_CONVERT_HPP
