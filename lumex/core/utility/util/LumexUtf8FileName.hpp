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
 * @file LumexUtf8FileName.hpp
 * @brief Opens a file stream by a UTF-8 file name, also on Windows.
 * @details Header-only and usable from C++11. A narrow name given to
 * `std::ifstream::open` is read in the ANSI code page on Windows, so a UTF-8
 * name with Cyrillic (a user profile) opens another file or none. `open ()`
 * gives the stream the UTF-16 name where the standard library takes one (the
 * MSVC library of cl.exe and clang-cl, libstdc++ with `_wfopen` on MinGW from
 * GCC 9, and libc++ with `_LIBCPP_HAS_OPEN_WITH_WCHAR`); with another Windows
 * library (libc++) it passes the narrow name after the same UTF-8 check; on
 * POSIX it passes the bytes as they are. On Windows a name that is not valid
 * UTF-8 is refused (the stream gets `failbit`) instead of opening the file of
 * the skipped bytes.
 *
 * It is for modules that must not depend on `lumex::filesystem` (the logger,
 * the JSON helper); a module that may depend on it uses
 * `lumex_filesystem::open_stream`, which does the same. The Win32 functions
 * of such a module take `to_wide ()`; text that the system gives back and
 * that names a file or a variable is read with the strict `from_wide ()`
 * (`to_utf8 ()` repairs, for text shown to a person).
 */
#ifndef LUMEX_CORE_UTILITY_UTF8_FILE_NAME_HPP
#define LUMEX_CORE_UTILITY_UTF8_FILE_NAME_HPP

#include <cstddef>
#include <fstream>
#include <ios>
#include <string>

#include "lumex/core/utility/macros/LumexKeywords.hpp"
#include "lumex/core/utility/os/LumexCheckOS.hpp"

// Windows only: the streams of the library take a wchar_t name (an extension
// of the MSVC library, of libstdc++ with _wfopen, and of libc++ where it
// defines _LIBCPP_HAS_OPEN_WITH_WCHAR: before LLVM 19 the macro is defined
// without a value, from LLVM 19 it is 0 or 1). The wchar_t overload of
// basic_filebuf::open is taken from libstdc++ of GCC 9 (_GLIBCXX_RELEASE,
// which clang with libstdc++ defines as well; __GNUC__ is 4 there): the
// headers of GCC 8 declare it, the static libstdc++.a of 8.3 defines it, but
// libstdc++-6.dll does not export it, so a shared link fails with an
// undefined reference. <fstream> comes first, it defines the library macros.
// The libc++ branch is not run in this repository's test matrix.
#define LUMEX_UTF8_FILE_NAME_WIDE_OPEN 0
#if defined(LUMEX_OS_WINDOWS)
#if defined(_MSC_VER) && !defined(_LIBCPP_VERSION)
#undef LUMEX_UTF8_FILE_NAME_WIDE_OPEN
#define LUMEX_UTF8_FILE_NAME_WIDE_OPEN 1
#elif defined(_GLIBCXX_HAVE__WFOPEN) && defined(_GLIBCXX_USE_WCHAR_T)         \
    && defined(_GLIBCXX_RELEASE) && _GLIBCXX_RELEASE >= 9
#undef LUMEX_UTF8_FILE_NAME_WIDE_OPEN
#define LUMEX_UTF8_FILE_NAME_WIDE_OPEN 1
#elif defined(_LIBCPP_VERSION) && defined(_LIBCPP_HAS_OPEN_WITH_WCHAR)
#if _LIBCPP_VERSION >= 190000
#if _LIBCPP_HAS_OPEN_WITH_WCHAR
#undef LUMEX_UTF8_FILE_NAME_WIDE_OPEN
#define LUMEX_UTF8_FILE_NAME_WIDE_OPEN 1
#endif
#else
#undef LUMEX_UTF8_FILE_NAME_WIDE_OPEN
#define LUMEX_UTF8_FILE_NAME_WIDE_OPEN 1
#endif
#endif
#endif

// The narrow fallback asks the process code page. The header is used without
// linking any library of LumexLib (the JSON helper is header-only), and it
// must not pull <windows.h> and its macros into its includers, so the one
// function is declared here, exactly as <windows.h> declares it, unless that
// header came first (a second declaration is a -Wredundant-decls error under
// -Werror; one that follows ours is in a system header and is not reported).
#if defined(LUMEX_OS_WINDOWS) && !LUMEX_UTF8_FILE_NAME_WIDE_OPEN              \
    && !defined(_WINDOWS_) && !defined(_INC_WINDOWS) && !defined(_WINNLS_)
extern "C" __declspec (dllimport) unsigned int __stdcall GetACP (void);
#endif

// clang-cl reads -Wall as -Weverything and the release builds add /WX, so the
// header carries its own diagnostics block (it is included before the pragmas
// of the .cpp files that use it). The lifetime-safety analysis of clang 23
// reports the output parameters that these functions clear and refill.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#if __has_warning("-Wlifetime-safety-invalidation")
#pragma clang diagnostic ignored "-Wlifetime-safety-invalidation"
#endif
#endif

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace core
{
namespace utility
{
namespace utf8_file_name
{
/**
 * @brief Whether `text` is strictly valid UTF-8.
 * @details No stray or truncated sequence, no overlong form, no surrogate and
 * nothing above U+10FFFF.
 * @param text The bytes.
 * @return `true` for valid UTF-8 (an empty text is valid).
 */
inline bool
is_valid (std::string const &text) LUMEX_NOEXCEPT
{
  std::size_t index = 0;
  std::size_t const size = text.size ();
  while (index < size)
    {
      unsigned char const lead = static_cast<unsigned char> (text[index]);
      std::size_t length = 1;
      unsigned char second_low = 0x80U;
      unsigned char second_high = 0xBFU;
      if (lead <= 0x7FU)
        {
          ++index;
          continue;
        }
      if (lead >= 0xC2U && lead <= 0xDFU)
        length = 2;
      else if (lead >= 0xE0U && lead <= 0xEFU)
        {
          length = 3;
          if (lead == 0xE0U)
            second_low = 0xA0U; // not overlong
          else if (lead == 0xEDU)
            second_high = 0x9FU; // not a surrogate
        }
      else if (lead >= 0xF0U && lead <= 0xF4U)
        {
          length = 4;
          if (lead == 0xF0U)
            second_low = 0x90U; // not overlong
          else if (lead == 0xF4U)
            second_high = 0x8FU; // not above U+10FFFF
        }
      else
        return false;
      if (index + length > size)
        return false;
      unsigned char const second
          = static_cast<unsigned char> (text[index + 1]);
      if (second < second_low || second > second_high)
        return false;
      for (std::size_t k = 2; k < length; ++k)
        if ((static_cast<unsigned char> (text[index + k]) & 0xC0U) != 0x80U)
          return false;
      index += length;
    }
  return true;
}

/**
 * @brief Converts strictly valid UTF-8 to `wchar_t` text.
 * @details UTF-16 where `wchar_t` has 2 bytes (Windows), UTF-32 where it has
 * 4. Unlike `MultiByteToWideChar` with the default flags it never writes
 * U+FFFD and unlike a lenient converter it never skips a byte: the name of
 * the skipped bytes would be the name of another file.
 * @param utf8 The UTF-8 text.
 * @param wide Receives the result; empty when the function returns `false`.
 * @return `true` on success, `false` when `utf8` is not valid UTF-8.
 */
inline bool
to_wide (std::string const &utf8, std::wstring &wide)
{
  wide.clear ();
  if (!is_valid (utf8))
    return false;
  std::size_t index = 0;
  std::size_t const size = utf8.size ();
  wide.reserve (size);
  while (index < size)
    {
      unsigned char const lead = static_cast<unsigned char> (utf8[index]);
      unsigned long code = lead;
      std::size_t length = 1;
      if (lead >= 0xF0U)
        {
          length = 4;
          code = lead & 0x07U;
        }
      else if (lead >= 0xE0U)
        {
          length = 3;
          code = lead & 0x0FU;
        }
      else if (lead >= 0xC0U)
        {
          length = 2;
          code = lead & 0x1FU;
        }
      for (std::size_t k = 1; k < length; ++k)
        code = (code << 6U)
               | (static_cast<unsigned char> (utf8[index + k]) & 0x3FU);
      index += length;
      if (sizeof (wchar_t) == 2 && code > 0xFFFFUL)
        {
          code -= 0x10000UL;
          wide.push_back (static_cast<wchar_t> (0xD800UL + (code >> 10U)));
          wide.push_back (static_cast<wchar_t> (0xDC00UL + (code & 0x3FFUL)));
        }
      else
        wide.push_back (static_cast<wchar_t> (code));
    }
  return true;
}

namespace detail
{
/// Appends the UTF-8 form of the scalar value `code` to `utf8`.
inline void
append_utf8 (unsigned long code, std::string &utf8)
{
  if (code < 0x80UL)
    utf8.push_back (static_cast<char> (code));
  else if (code < 0x800UL)
    {
      utf8.push_back (static_cast<char> (0xC0UL | (code >> 6U)));
      utf8.push_back (static_cast<char> (0x80UL | (code & 0x3FUL)));
    }
  else if (code < 0x10000UL)
    {
      utf8.push_back (static_cast<char> (0xE0UL | (code >> 12U)));
      utf8.push_back (static_cast<char> (0x80UL | ((code >> 6U) & 0x3FUL)));
      utf8.push_back (static_cast<char> (0x80UL | (code & 0x3FUL)));
    }
  else
    {
      utf8.push_back (static_cast<char> (0xF0UL | (code >> 18U)));
      utf8.push_back (static_cast<char> (0x80UL | ((code >> 12U) & 0x3FUL)));
      utf8.push_back (static_cast<char> (0x80UL | ((code >> 6U) & 0x3FUL)));
      utf8.push_back (static_cast<char> (0x80UL | (code & 0x3FUL)));
    }
}

/// The one wide to UTF-8 loop. A unit that is not a scalar value (an
/// unpaired surrogate, a value above U+10FFFF) is replaced with U+FFFD when
/// `replace` is set and ends the conversion with `false` otherwise.
inline bool
wide_to_utf8 (std::wstring const &wide, std::string &utf8, bool replace)
{
  utf8.clear ();
  std::size_t const size = wide.size ();
  utf8.reserve (size);
  std::size_t index = 0;
  while (index < size)
    {
      unsigned long code = static_cast<unsigned long> (wide[index++]);
      if (sizeof (wchar_t) == 2 && code >= 0xD800UL && code <= 0xDBFFUL
          && index < size
          && static_cast<unsigned long> (wide[index]) >= 0xDC00UL
          && static_cast<unsigned long> (wide[index]) <= 0xDFFFUL)
        {
          code = 0x10000UL + ((code - 0xD800UL) << 10U)
                 + (static_cast<unsigned long> (wide[index++]) - 0xDC00UL);
        }
      else if ((code >= 0xD800UL && code <= 0xDFFFUL) || code > 0x10FFFFUL)
        {
          if (!replace)
            {
              utf8.clear ();
              return false;
            }
          code = 0xFFFDUL;
        }
      append_utf8 (code, utf8);
    }
  return true;
}
} // namespace detail

/**
 * @brief Converts `wchar_t` text (UTF-16 or UTF-32) to UTF-8.
 * @details For text that is shown to a person or only compared (a module file
 * name in a message). A unit that is not a scalar value (an unpaired
 * surrogate, a value above U+10FFFF) becomes U+FFFD. Text that names a file or
 * a variable goes through `from_wide ()`, which refuses it instead.
 * @param wide The text.
 * @return The UTF-8 text.
 */
inline std::string
to_utf8 (std::wstring const &wide)
{
  std::string utf8;
  detail::wide_to_utf8 (wide, utf8, true);
  return utf8;
}

/**
 * @brief Converts `wchar_t` text (UTF-16 or UTF-32) to UTF-8, strictly.
 * @details The inverse of `to_wide ()`, for text that the system gives back
 * and that names a file, a directory or a variable (a module path, an
 * environment value). An unpaired surrogate or a value above U+10FFFF is
 * refused: a replacement character would name another file.
 * @param wide The text, `size` units long (it may hold NUL units).
 * @param size The number of units.
 * @param utf8 Receives the result; empty when the function returns `false`.
 * @return `true` on success, `false` when `wide` is not valid UTF-16/32.
 */
inline bool
from_wide (wchar_t const *wide, std::size_t size, std::string &utf8)
{
  if (wide == nullptr && size != 0)
    {
      utf8.clear ();
      return false;
    }
  // Indexed through a std::wstring, never through the raw pointer.
  return detail::wide_to_utf8 (wide == nullptr ? std::wstring ()
                                               : std::wstring (wide, size),
                               utf8, false);
}

/// `from_wide ()` of a `std::wstring`.
inline bool
from_wide (std::wstring const &wide, std::string &utf8)
{
  return detail::wide_to_utf8 (wide, utf8, false);
}

/**
 * @brief Whether a narrow stream open reads `name` as the file it names.
 * @details Only the Windows build whose library has no wide open
 * (`LUMEX_UTF8_FILE_NAME_WIDE_OPEN == 0`) can lose: the C runtime reads the
 * narrow name in the ANSI code page, so a UTF-8 name with a byte of 0x80 or
 * more opens another file unless the process code page is UTF-8. Such a name
 * is refused there (false); an ASCII name, and every name elsewhere, is fine.
 * @param name The UTF-8 file name.
 * @return `true` when the narrow name is read as it is written.
 */
inline bool
narrow_name_is_faithful (std::string const &name)
{
#if defined(LUMEX_OS_WINDOWS) && !LUMEX_UTF8_FILE_NAME_WIDE_OPEN
  for (std::size_t index = 0; index < name.size (); ++index)
    if (static_cast<unsigned char> (name[index]) >= 0x80U)
      return GetACP () == 65001U; // CP_UTF8
  return true;
#else
  static_cast<void> (name);
  return true;
#endif
}

/**
 * @brief Opens `stream` (an `std::ifstream`, `std::ofstream` or
 * `std::fstream`) by a UTF-8 file name.
 * @details See the file comment for the branches. A name that is not valid
 * UTF-8 is refused on Windows only (on POSIX every byte string is a name).
 * @param stream The stream to open; it must not be open.
 * @param name The UTF-8 file name.
 * @param mode The open mode.
 * @return `true` when the stream is open. On `false` the stream has `failbit`
 * set. Does not throw, unless the stream has an exception mask for `failbit`.
 */
template <typename Stream>
inline bool
open (Stream &stream, std::string const &name, std::ios_base::openmode mode)
{
#if defined(LUMEX_OS_WINDOWS)
  std::wstring wide;
  if (!to_wide (name, wide))
    {
      stream.setstate (std::ios_base::failbit);
      return false;
    }
#if LUMEX_UTF8_FILE_NAME_WIDE_OPEN
  stream.open (wide.c_str (), mode);
#else
  if (!narrow_name_is_faithful (name))
    {
      stream.setstate (std::ios_base::failbit);
      return false;
    }
  stream.open (name.c_str (), mode);
#endif
#else
  stream.open (name.c_str (), mode);
#endif
  return stream.is_open ();
}
} // namespace utf8_file_name
} // namespace utility
} // namespace core
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_UTILITY_UTF8_FILE_NAME_HPP
