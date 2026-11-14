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

/**
 * @file LumexUtf8Validate.hpp
 * @brief Validation of UTF-8: the length of the well-formed prefix of a buffer
 * and a predicate for a whole buffer.
 * @details `utf8_valid_prefix_length` returns how many leading bytes of a
 * buffer are well-formed UTF-8 in the sense of Table 3-7 of the Unicode
 * standard (the shortest form of a scalar value: no overlong form, no
 * surrogate, nothing above U+10FFFF), and `is_valid_utf8` tells whether the
 * whole buffer is. They agree with `utf8_decoder` (`LumexUtf.hpp`), which
 * yields exactly the code points of the well-formed sequences and skips every
 * other byte: the prefix ends at the first byte the decoder would skip, so a
 * buffer is valid exactly when the decoder skips nothing. They only read and
 * allocate nothing. They are not a count made by the decoder: ASCII is read
 * eight bytes at a time, which is 2 to 9 times faster than the decoder with
 * `utf8_counter` on ASCII text; on text that is not ASCII (2-byte, 3-byte and
 * 4-byte sequences) the speed is about that of the decoder, 0.85 to 1.9 times
 * (4 MB of valid text, GCC 13, Clang 18 and cl.exe, see the CHANGELOG). A
 * faster path for such text is a SIMD routine behind the same functions. The
 * overloads for `char` serve text held in `std::string` and `char const *`,
 * and, where the compiler has it (C++20), those for `char8_t` serve
 * `std::u8string` and `u8` literals; a buffer of `char` or `char8_t` is read
 * as bytes.
 *
 * The public functions forward to a routine in `detail`. A faster routine for
 * a platform (SIMD) can be added there and chosen in the public functions
 * without a change for the callers: the contract above is all they rely on.
 * Header-only, works from C++11.
 */
#ifndef LUMEX_CORE_UNICODE_UTF_UTF8_VALIDATE_HPP
#define LUMEX_CORE_UNICODE_UTF_UTF8_VALIDATE_HPP

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

#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace core
{
namespace unicode
{
namespace utf
{
namespace detail
{
/**
 * @brief The portable routine behind `utf8_valid_prefix_length`.
 * @details Not part of the public interface: call `utf8_valid_prefix_length`.
 * @param[in] data Bytes to check; may be null when `size` is 0.
 * @param[in] size Number of bytes.
 * @return The length of the well-formed prefix, in the range `0 .. size`.
 */
inline std::size_t
utf8_valid_prefix_length_scalar (std::uint8_t const *data,
                                 std::size_t size) LUMEX_NOEXCEPT
{
  std::size_t i = 0;

  while (i < size)
    {
      std::uint8_t const lead = data[i];

      // 0xxxxxxx
      if (lead < 0x80)
        {
          ++i;

          while (size - i >= 8)
            {
              std::uint64_t word = 0;
              std::memcpy (&word, data + i, sizeof (word));
              if ((word & 0x8080808080808080ULL) != 0)
                break;
              i += 8;
            }

          // the rest of the word that holds a non-ASCII byte, or of the data
          while (i < size && data[i] < 0x80)
            ++i;

          continue;
        }

      std::size_t const rest = size - i;

      // 110xxxxx 10xxxxxx; C0 and C1 would be overlong forms
      if (lead - 0xC2U < 0x1EU)
        {
          if (rest >= 2 && (data[i + 1] & 0xC0) == 0x80)
            {
              i += 2;
              continue;
            }

          return i;
        }

      // 1110xxxx 10xxxxxx 10xxxxxx; E0 takes A0..BF as the second byte (no
      // overlong form) and ED takes 80..9F (no surrogate)
      if (lead - 0xE0U < 0x10U)
        {
          if (rest >= 3 && ((data[i + 1] ^ 0x80) | (data[i + 2] ^ 0x80)) < 0x40
              && (lead - 0xE1U < 0xCU || lead - 0xEEU < 2U
                  || (lead == 0xE0 ? data[i + 1] >= 0xA0
                                   : data[i + 1] < 0xA0)))
            {
              i += 3;
              continue;
            }

          return i;
        }

      // 11110xxx and three 10xxxxxx; F0 takes 90..BF as the second byte (no
      // overlong form) and F4 takes 80..8F (nothing above U+10FFFF)
      if (lead - 0xF0U < 5U)
        {
          if (rest >= 4
              && ((data[i + 1] ^ 0x80) | (data[i + 2] ^ 0x80)
                  | (data[i + 3] ^ 0x80))
                     < 0x40
              && (lead != 0xF0 || data[i + 1] >= 0x90)
              && (lead != 0xF4 || data[i + 1] < 0x90))
            {
              i += 4;
              continue;
            }

          return i;
        }

      // a continuation byte, or a lead byte F5..FF
      return i;
    }

  return size;
}
} // namespace detail

/**
 * @brief The length of the longest prefix of a buffer that is well-formed
 * UTF-8.
 * @details The prefix is made of whole well-formed sequences (Table 3-7 of the
 * Unicode standard). The result is `size` when the whole buffer is well formed
 * and otherwise the offset of the first byte that starts no well-formed
 * sequence (a stray continuation byte, `C0`, `C1`, `F5`..`FF`, an overlong
 * form, a surrogate, a value above U+10FFFF, or a sequence cut short by the
 * end of the buffer or by a byte that is not a continuation byte), which is
 * where `utf8_decoder` starts to skip. A U+0000 byte is valid. The buffer is
 * only read, and not past `size`.
 * @param[in] data Bytes to check; may be null when `size` is 0.
 * @param[in] size Number of bytes.
 * @return The length of the well-formed prefix in bytes, `0 .. size`.
 * @note Exception-safety: nothrow. Thread-safety: yes, no shared state.
 */
LUMEX_ATTRIBUTE_NODISCARD (
    "The length of the valid prefix is the only result of the check")
inline std::size_t
utf8_valid_prefix_length (std::uint8_t const *data,
                          std::size_t size) LUMEX_NOEXCEPT
{
  return detail::utf8_valid_prefix_length_scalar (data, size);
}

/**
 * @brief The same for a buffer of `char`, which is read as bytes.
 * @param[in] data Characters to check; may be null when `size` is 0.
 * @param[in] size Number of characters.
 * @return The length of the well-formed prefix in characters, `0 .. size`.
 */
LUMEX_ATTRIBUTE_NODISCARD (
    "The length of the valid prefix is the only result of the check")
inline std::size_t
utf8_valid_prefix_length (char const *data, std::size_t size) LUMEX_NOEXCEPT
{
  return detail::utf8_valid_prefix_length_scalar (
      reinterpret_cast<std::uint8_t const *> (data), size);
}

#if LUMEX_HAS_CHAR8_T
/**
 * @brief The same for a buffer of `char8_t` (C++20), which is read as bytes.
 * @param[in] data Code units to check; may be null when `size` is 0.
 * @param[in] size Number of code units.
 * @return The length of the well-formed prefix in code units, `0 .. size`.
 */
LUMEX_ATTRIBUTE_NODISCARD (
    "The length of the valid prefix is the only result of the check")
inline std::size_t
utf8_valid_prefix_length (char8_t const *data, std::size_t size) LUMEX_NOEXCEPT
{
  return detail::utf8_valid_prefix_length_scalar (
      reinterpret_cast<std::uint8_t const *> (data), size);
}
#endif

/**
 * @brief Tells whether a whole buffer is well-formed UTF-8.
 * @details The same as `utf8_valid_prefix_length (data, size) == size`. An
 * empty buffer is valid.
 * @param[in] data Bytes to check; may be null when `size` is 0.
 * @param[in] size Number of bytes.
 * @return `true` when `utf8_decoder` would skip no byte of the buffer.
 * @note Exception-safety: nothrow. Thread-safety: yes, no shared state.
 */
LUMEX_ATTRIBUTE_NODISCARD ("Whether the buffer is valid UTF-8 is the only "
                           "result of the check")
inline bool
is_valid_utf8 (std::uint8_t const *data, std::size_t size) LUMEX_NOEXCEPT
{
  return utf8_valid_prefix_length (data, size) == size;
}

/**
 * @brief The same for a buffer of `char`, which is read as bytes.
 * @param[in] data Characters to check; may be null when `size` is 0.
 * @param[in] size Number of characters.
 * @return `true` when the whole buffer is well-formed UTF-8.
 */
LUMEX_ATTRIBUTE_NODISCARD ("Whether the buffer is valid UTF-8 is the only "
                           "result of the check")
inline bool
is_valid_utf8 (char const *data, std::size_t size) LUMEX_NOEXCEPT
{
  return utf8_valid_prefix_length (data, size) == size;
}

#if LUMEX_HAS_CHAR8_T
/**
 * @brief The same for a buffer of `char8_t` (C++20), which is read as bytes.
 * @param[in] data Code units to check; may be null when `size` is 0.
 * @param[in] size Number of code units.
 * @return `true` when the whole buffer is well-formed UTF-8.
 */
LUMEX_ATTRIBUTE_NODISCARD ("Whether the buffer is valid UTF-8 is the only "
                           "result of the check")
inline bool
is_valid_utf8 (char8_t const *data, std::size_t size) LUMEX_NOEXCEPT
{
  return utf8_valid_prefix_length (data, size) == size;
}
#endif

} // namespace utf
} // namespace unicode
} // namespace core
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_UNICODE_UTF_UTF8_VALIDATE_HPP
