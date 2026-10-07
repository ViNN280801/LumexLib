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
 * @file LumexUtf.hpp
 * @brief Transcoding policies for UTF-8, UTF-16, UTF-32, Latin-1 and
 * `wchar_t`: counters, writers and decoders that combine into a conversion.
 * @details A conversion is a decoder run with a policy. A decoder
 * (`utf8_decoder`, `utf16_decoder<SwapBytes>`, `utf32_decoder<SwapBytes>`,
 * `latin1_decoder`, `wchar_decoder`) reads a buffer of its unit type and calls
 * `Traits::low (result, code_point)` for a code point below U+10000 and
 * `Traits::high (result, code_point)` from U+10000 on, threading `result`
 * through the calls. With a counter (`utf8_counter`, `utf16_counter`,
 * `utf32_counter`) `result` is the size of the output; with a writer
 * (`utf8_writer`, `utf16_writer`, `utf32_writer`, `latin1_writer`) it is the
 * output pointer. So a conversion runs twice: once with the counter to size
 * the buffer, once with the writer to fill it. `wchar_selector<Size>` picks
 * the UTF-16 or UTF-32 set for the `wchar_t` of the platform.
 *
 * Invalid input is not an error: a decoder skips what it cannot decode.
 * Header-only, works from C++11, no allocation. `to_utf8` and `to_wide` in
 * `convert/LumexUnicodeConvert.hpp` are the ready-made string conversions.
 */
#ifndef LUMEX_CORE_UNICODE_UTF_HPP
#define LUMEX_CORE_UNICODE_UTF_HPP

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#if __has_warning("-Wunsafe-buffer-usage-in-libc-call")
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-libc-call"
#endif
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wswitch-enum"
#endif

#include <cstddef>
#include <cstdint>

#include "lumex/core/utility/bit/LumexBit.hpp"

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace core
{
namespace unicode
{
namespace utf
{
/**
 * @brief Counting policy: the size in bytes of the UTF-8 form of what a
 * decoder yields.
 * @details A decoder (`utf8_decoder`, `utf16_decoder`, ...) calls `low` for
 * every code point below U+10000 and `high` for every code point from U+10000
 * on, and threads `result` through the calls. This policy adds 1, 2 or 3
 * bytes for `low` and 4 for `high`. Run it first to size the buffer, then run
 * `utf8_writer` over the same input.
 */
struct utf8_counter
{
  using value_type = std::size_t;

  static value_type
  low (value_type result,
       std::uint32_t chr) // NOLINT(bugprone-easily-swappable-parameters)
  {
    // U+0000..U+007F
    if (chr < 0x80)
      return result + 1;

    // U+0080..U+07FF
    if (chr < 0x800)
      return result + 2;

    // U+0800..U+FFFF
    return result + 3;
  }

  static value_type
  high (value_type result, std::uint32_t /* unused */)
  {
    // U+10000..U+10FFFF
    return result + 4;
  }
};

/**
 * @brief Counting policy: the number of 16-bit units in the UTF-16 form (1
 * for a code point below U+10000, 2 for a surrogate pair).
 */
struct utf16_counter
{
  using value_type = std::size_t;

  static value_type
  low (value_type result, std::uint32_t /* unused */)
  {
    return result + 1;
  }

  static value_type
  high (value_type result, std::uint32_t /* unused */)
  {
    return result + 2;
  }
};

/**
 * @brief Counting policy: the number of 32-bit units in the UTF-32 form, one
 * per code point.
 */
struct utf32_counter
{
  using value_type = std::size_t;

  static value_type
  low (value_type result, std::uint32_t /* unused */)
  {
    return result + 1;
  }

  static value_type
  high (value_type result, std::uint32_t /* unused */)
  {
    return result + 1;
  }
};

/**
 * @brief Writing policy: stores the UTF-8 bytes of each code point.
 * @details `value_type` is the output pointer; `low`, `high` and `any` write
 * the bytes of one code point at `result` and return the pointer behind them.
 * `any` picks `low` or `high` by the value. The buffer must hold the size a
 * `utf8_counter` run over the same input returned.
 */
struct utf8_writer
{
  using value_type = std::uint8_t *;

  static value_type
  low (value_type result,
       std::uint32_t chr) // NOLINT(bugprone-easily-swappable-parameters)
  {
    // U+0000..U+007F
    if (chr < 0x80)
      {
        *result = static_cast<std::uint8_t> (chr);
        return result + 1;
      }
    // U+0080..U+07FF
    if (chr < 0x800)
      {
        result[0] = static_cast<std::uint8_t> (0xC0 | (chr >> 6));
        result[1] = static_cast<std::uint8_t> (0x80 | (chr & 0x3F));
        return result + 2;
      }
    // U+0800..U+FFFF
    result[0] = static_cast<std::uint8_t> (0xE0 | (chr >> 12));
    result[1] = static_cast<std::uint8_t> (0x80 | ((chr >> 6) & 0x3F));
    result[2] = static_cast<std::uint8_t> (0x80 | (chr & 0x3F));
    return result + 3;
  }

  static value_type
  high (value_type result, std::uint32_t chr)
  {
    // U+10000..U+10FFFF
    result[0] = static_cast<std::uint8_t> (0xF0 | (chr >> 18));
    result[1] = static_cast<std::uint8_t> (0x80 | ((chr >> 12) & 0x3F));
    result[2] = static_cast<std::uint8_t> (0x80 | ((chr >> 6) & 0x3F));
    result[3] = static_cast<std::uint8_t> (0x80 | (chr & 0x3F));
    return result + 4;
  }

  static value_type
  any (value_type result, std::uint32_t chr)
  {
    return (chr < 0x10000) ? low (result, chr) : high (result, chr);
  }
};

/**
 * @brief Writing policy: stores the UTF-16 units of each code point, with a
 * surrogate pair from U+10000 on, in the byte order of the machine.
 */
struct utf16_writer
{
  using value_type = std::uint16_t *;

  static value_type
  low (value_type result,
       std::uint32_t chr) // NOLINT(bugprone-easily-swappable-parameters)
  {
    *result = static_cast<std::uint16_t> (chr);

    return result + 1;
  }

  static value_type
  high (value_type result,
        std::uint32_t chr) // NOLINT(bugprone-easily-swappable-parameters)
  {
    std::uint32_t msh = (chr - 0x10000U) >> 10;
    std::uint32_t lsh = (chr - 0x10000U) & 0x3ff;

    result[0] = static_cast<std::uint16_t> (0xD800 + msh);
    result[1] = static_cast<std::uint16_t> (0xDC00 + lsh);

    return result + 2;
  }

  static value_type
  any (value_type result,
       std::uint32_t chr) // NOLINT(bugprone-easily-swappable-parameters)
  {
    return (chr < 0x10000) ? low (result, chr) : high (result, chr);
  }
};

/**
 * @brief Writing policy: stores each code point as one 32-bit unit, in the
 * byte order of the machine.
 */
struct utf32_writer
{
  using value_type = std::uint32_t *;

  static value_type
  low (value_type result, std::uint32_t chr)
  {
    *result = chr;

    return result + 1;
  }

  static value_type
  high (value_type result, std::uint32_t chr)
  {
    *result = chr;

    return result + 1;
  }

  static value_type
  any (value_type result, std::uint32_t chr)
  {
    *result = chr;

    return result + 1;
  }
};

/**
 * @brief Writing policy: stores a code point as one Latin-1 byte, and `?` for
 * a code point above U+00FF.
 */
struct latin1_writer
{
  using value_type = std::uint8_t *;

  static value_type
  low (value_type result, std::uint32_t chr)
  {
    *result = static_cast<std::uint8_t> (chr > 255 ? '?' : chr);

    return result + 1;
  }

  static value_type
  high (value_type result, std::uint32_t /* chr */)
  {
    *result = '?';

    return result + 1;
  }
};

/**
 * @brief Decoder of a UTF-8 buffer.
 * @details Calls `Traits::low` for a code point below U+10000 and
 * `Traits::high` from U+10000 on. A byte that does not start a complete
 * sequence (a stray continuation byte, a truncated sequence, a lead byte from
 * 0xF8) is skipped, and the decoder goes on with the next byte; it does not
 * report the error, and it does not reject overlong forms or surrogates. Runs
 * of ASCII are read four bytes at a time when the data is aligned.
 */
struct utf8_decoder
{
  using type = std::uint8_t;

  template <typename Traits>
  static typename Traits::value_type
  process (std::uint8_t const *data, std::size_t size,
           typename Traits::value_type result, Traits /* unused */)
  {
    std::uint8_t const utf8_byte_mask = 0x3f;

    while (size)
      {
        std::uint8_t lead = *data;

        // 0xxxxxxx -> U+0000..U+007F
        if (lead < 0x80)
          {
            result = Traits::low (result, lead);
            data += 1;
            size -= 1;

            // process aligned single-byte (ascii) blocks
            if ((reinterpret_cast<std::uintptr_t> (data) & 3) == 0)
              {
                // round-trip through void* to silence 'cast increases required
                // alignment of target type' warnings
                while (
                    size >= 4
                    && (*static_cast<std::uint32_t const *> (
                            static_cast< // NOLINT(bugprone-casting-through-void)
                                void const *> (data))
                        & 0x80808080)
                           == 0)
                  {
                    result = Traits::low (result, data[0]);
                    result = Traits::low (result, data[1]);
                    result = Traits::low (result, data[2]);
                    result = Traits::low (result, data[3]);
                    data += 4;
                    size -= 4;
                  }
              }
          }
        // 110xxxxx -> U+0080..U+07FF
        else if (static_cast<unsigned int> (lead - 0xC0) < 0x20 && size >= 2
                 && (data[1] & 0xc0) == 0x80)
          {
            result = Traits::low (result, static_cast<std::uint32_t> (
                                              ((lead & ~0xC0) << 6)
                                              | (data[1] & utf8_byte_mask)));
            data += 2;
            size -= 2;
          }
        // 1110xxxx -> U+0800-U+FFFF
        else if (static_cast<unsigned int> (lead - 0xE0) < 0x10 && size >= 3
                 && (data[1] & 0xc0) == 0x80 && (data[2] & 0xc0) == 0x80)
          {
            result = Traits::low (
                result,
                static_cast<std::uint32_t> (((lead & ~0xE0) << 12)
                                            | ((data[1] & utf8_byte_mask) << 6)
                                            | (data[2] & utf8_byte_mask)));
            data += 3;
            size -= 3;
          }
        // 11110xxx -> U+10000..U+10FFFF
        else if (static_cast<unsigned int> (lead - 0xF0) < 0x08 && size >= 4
                 && (data[1] & 0xc0) == 0x80 && (data[2] & 0xc0) == 0x80
                 && (data[3] & 0xc0) == 0x80)
          {
            result = Traits::high (result,
                                   static_cast<std::uint32_t> (
                                       ((lead & ~0xF0) << 18)
                                       | ((data[1] & utf8_byte_mask) << 12)
                                       | ((data[2] & utf8_byte_mask) << 6)
                                       | (data[3] & utf8_byte_mask)));
            data += 4;
            size -= 4;
          }
        // 10xxxxxx or 11111xxx -> invalid
        else
          {
            data += 1;
            size -= 1;
          }
      }

    return result;
  }
};

/**
 * @brief Decoder of a UTF-16 buffer.
 * @details `process (data, size, result, traits)` walks `size` 16-bit units
 * and calls `Traits::low` for a unit outside the surrogate range and
 * `Traits::high` for a valid surrogate pair, threading `result` through the
 * calls and returning its final value. An unpaired surrogate is skipped. With
 * `SwapBytes` set, every unit is byte swapped first, to read a UTF-16 buffer
 * of the other byte order.
 * @tparam SwapBytes Swap the bytes of every unit before decoding.
 */
template <bool SwapBytes> struct utf16_decoder
{
  using type = std::uint16_t;

  template <typename Traits>
  static typename Traits::value_type
  process (std::uint16_t const *data, std::size_t size,
           typename Traits::value_type result, Traits /*unused*/)
  {
    while (size)
      {
        std::uint16_t lead
            = SwapBytes ? utility::bit::byte_swap (*data) : *data;

        // U+0000..U+D7FF
        if (lead < 0xD800)
          { // NOLINT(bugprone-branch-clone)
            result = Traits::low (result, lead);
            data += 1;
            size -= 1;
          }
        // U+E000..U+FFFF
        else if (static_cast<unsigned int> (lead - 0xE000) < 0x2000)
          {
            result = Traits::low (result, lead);
            data += 1;
            size -= 1;
          }
        // surrogate pair lead
        else if (static_cast<unsigned int> (lead - 0xD800) < 0x400
                 && size >= 2)
          {
            std::uint16_t next
                = SwapBytes ? utility::bit::byte_swap (data[1]) : data[1];

            if (static_cast<unsigned int> (next - 0xDC00) < 0x400)
              {
                result = Traits::high (
                    result,
                    static_cast<std::uint32_t> (
                        0x10000 + ((lead & 0x3ff) << 10) + (next & 0x3ff)));
                data += 2;
                size -= 2;
              }
            else
              {
                data += 1;
                size -= 1;
              }
          }
        else
          {
            data += 1;
            size -= 1;
          }
      }

    return result;
  }
};

/**
 * @brief Decoder of a UTF-32 buffer: `Traits::low` for a code point below
 * U+10000 and `Traits::high` from U+10000 on. With `SwapBytes` set, every unit
 * is byte swapped first. It does not validate the values.
 * @tparam SwapBytes Swap the bytes of every unit before decoding.
 */
template <bool SwapBytes> struct utf32_decoder
{
  using type = std::uint32_t;

  template <typename Traits>
  static typename Traits::value_type
  process (std::uint32_t const *data, std::size_t size,
           typename Traits::value_type result, Traits /* unused */)
  {
    while (size)
      {
        std::uint32_t lead
            = SwapBytes ? utility::bit::byte_swap (*data) : *data;

        // U+0000..U+FFFF
        if (lead < 0x10000)
          {
            result = Traits::low (result, lead);
            data += 1;
            size -= 1;
          }
        // U+10000..U+10FFFF
        else
          {
            result = Traits::high (result, lead);
            data += 1;
            size -= 1;
          }
      }

    return result;
  }
};

/**
 * @brief Decoder of a Latin-1 (ISO 8859-1) buffer: every byte is a code
 * point, passed to `Traits::low`.
 */
struct latin1_decoder
{
  using type = std::uint8_t;

  template <typename Traits>
  static typename Traits::value_type
  process (std::uint8_t const *data, std::size_t size,
           typename Traits::value_type result, Traits /* unused */)
  {
    while (size)
      {
        result = Traits::low (result, *data);
        data += 1;
        size -= 1;
      }

    return result;
  }
};

/**
 * @brief The counting policy, writing policy, decoder and unit type that match
 * a `wchar_t` of `Size` bytes (2: UTF-16, 4: UTF-32), as members `type`,
 * `counter`, `writer` and `decoder`.
 */
template <std::size_t Size> struct wchar_selector;
template <> struct wchar_selector<2>
{
  using type = std::uint16_t;
  using counter = utf16_counter;
  using writer = utf16_writer;
  using decoder = utf16_decoder<false>;
};

template <> struct wchar_selector<4>
{
  using type = std::uint32_t;
  using counter = utf32_counter;
  using writer = utf32_writer;
  using decoder = utf32_decoder<false>;
};

/// @brief Counting policy for the `wchar_t` of this platform.
using wchar_counter = wchar_selector<sizeof (wchar_t)>::counter;
/// @brief Writing policy for the `wchar_t` of this platform.
using wchar_writer = wchar_selector<sizeof (wchar_t)>::writer;

/**
 * @brief Decoder of a `wchar_t` buffer: `utf16_decoder<false>` where
 * `wchar_t` has 2 bytes (Windows) and `utf32_decoder<false>` where it has 4.
 */
struct wchar_decoder
{
  using type = wchar_t;

  template <typename Traits>
  static typename Traits::value_type
  process (wchar_t const *data, std::size_t size,
           typename Traits::value_type result, Traits traits)
  {
    using decoder = wchar_selector<sizeof (wchar_t)>::decoder;

    return decoder::process (
        reinterpret_cast<typename decoder::type const *> (data), size, result,
        traits);
  }
};

} // namespace utf
} // namespace unicode
} // namespace core
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_UNICODE_UTF_HPP
