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
 * @file LumexProtocolKeyReader.hpp
 * @brief Finding the key in a received frame: `key_at`, `key_at_digits`,
 * `tokenized_key`, `key_reader_of<P>` and `key_width_of<P>`.
 * @details The index looks a received frame up by its key. Where the key is
 * depends on the message format of the layer (its header type):
 *  - a tokenized text layout carries the key as the first token: it is read
 *    up to the key separator (`tokenized_key`);
 *  - a binary layout carries it in the header, by default in the first
 *    `key_width_of<P>` bytes, most significant first (`key_at<0, W>`);
 *  - a fixed-width hexadecimal layout carries it as the first `2 * W`
 *    digits (`key_at_digits<0, 2 * W, 16>`);
 *  - anything else, or a header with another position, names its own reader
 *    in the header type: `using key_reader = key_at<2, 1>;` or a type with
 *    `static bool read (bytes_view_t frame, key_read_t &out)`.
 * A binary or hexadecimal fixed-width layer must name the width
 * (`static constexpr std::size_t key_width = N;`) or its own `key_reader`:
 * the width of the key type is not the width on the wire (`0x1D` is an
 * `int`), so the index refuses to guess it (static_assert). The width of the
 * key on the wire is `P::header_type::key_width` when the header names it,
 * otherwise the widest key type of the table (an integer, an enumerator or a
 * composite key with its `lumex_key_width ()`). The
 * keys of the table are cut to that width (`normalize_key`), so that the
 * character with the code 0xFF and the integer `-1` are the same key on a
 * 1-byte wire.
 */
#ifndef LUMEX_CORE_PROTOCOL_INDEX_KEY_READER_HPP
#define LUMEX_CORE_PROTOCOL_INDEX_KEY_READER_HPP

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "lumex/core/protocol/dsl/LumexProtocolKey.hpp"
#include "lumex/core/protocol/dsl/LumexProtocolRow.hpp"
#include "lumex/core/protocol/dsl/LumexProtocolTypeList.hpp"
#include "lumex/core/protocol/field/LumexProtocolFieldTraits.hpp"
#include "lumex/core/protocol/field/LumexProtocolValues.hpp"
#include "lumex/core/protocol/layout/LumexProtocolBinaryLayout.hpp"
#include "lumex/core/protocol/layout/LumexProtocolCodec.hpp"
#include "lumex/core/protocol/layout/LumexProtocolLayoutBase.hpp"
#include "lumex/core/protocol/layout/LumexProtocolTextLayouts.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#endif

namespace lumex
{
namespace core
{
namespace protocol
{
namespace index
{
/// @brief The result of reading the key of a frame.
struct key_read_t
{
  /// The key as the frame carries it. A text key points into the frame.
  ::lumex::core::protocol::dsl::key_value_t key;
  /// Where the fields of the message start in the frame (after the header
  /// and the key of a binary layout; 0 for a tokenized layout, whose
  /// decoder reads the key prefix itself).
  std::size_t payload_offset;
};

/// @brief The byte order of the key bytes.
using key_byte_order = ::lumex::core::protocol::field::byte_order;

/**
 * @brief The key as `Width` bytes at `Offset` of the frame, in byte order
 * `Order` (most significant first by default). The bytes before and the key
 * itself belong to the header: the fields start after the key.
 */
template <std::size_t Offset, std::size_t Width,
          key_byte_order Order = key_byte_order::big>
struct key_at
{
  static_assert (Width >= 1 && Width <= 8,
                 "key_at<Offset, Width>: Width is 1 to 8 bytes");

  /// The key width on the wire.
  static LUMEX_CONSTEXPR std::size_t
  key_width () LUMEX_NOEXCEPT
  {
    return Width;
  }

  /// @brief Reads the key; false when the frame is shorter.
  static bool
  read (::lumex::core::protocol::layout::bytes_view_t frame,
        key_read_t &out) LUMEX_NOEXCEPT
  {
    if (frame.size () < Offset + Width)
      return false;
    std::uint64_t bits = 0;
    for (std::size_t i = 0; i < Width; ++i)
      {
        std::uint64_t const byte
            = frame.data ()[Offset
                            + (Order == key_byte_order::big ? i
                                                            : Width - 1u - i)];
        bits = (bits << 8u) | byte;
      }
    out.key = ::lumex::core::protocol::dsl::key_value_t{
      ::lumex::core::protocol::dsl::key_kind::integer,
      static_cast<std::uint8_t> (Width), bits, nullptr, 0
    };
    out.payload_offset = Offset + Width;
    return true;
  }
};

/**
 * @brief The key as `Digits` digits in radix `Radix` (10 or 16) at `Offset`
 * of the frame (the key of a fixed-width text layout).
 */
template <std::size_t Offset, std::size_t Digits, unsigned Radix = 16>
struct key_at_digits
{
  static_assert (Radix == 10 || Radix == 16,
                 "key_at_digits: Radix is 10 or 16");
  static_assert (Digits >= 1 && Digits <= 16,
                 "key_at_digits: Digits is 1 to 16");

  /// The key width on the wire in bytes (hexadecimal digits come in pairs).
  static LUMEX_CONSTEXPR std::size_t
  key_width () LUMEX_NOEXCEPT
  {
    return (Digits + 1u) / 2u;
  }

  /// @brief Reads the key; false when the frame is shorter or a character is
  /// no digit.
  static bool
  read (::lumex::core::protocol::layout::bytes_view_t frame,
        key_read_t &out) LUMEX_NOEXCEPT
  {
    if (frame.size () < Offset + Digits)
      return false;
    std::uint64_t bits = 0;
    for (std::size_t i = 0; i < Digits; ++i)
      {
        int const digit = ::lumex::core::protocol::layout::detail::hex_value (
            static_cast<char> (frame.data ()[Offset + i]));
        if (digit < 0 || static_cast<unsigned> (digit) >= Radix)
          return false;
        bits = bits * Radix + static_cast<unsigned> (digit);
      }
    out.key = ::lumex::core::protocol::dsl::key_value_t{
      ::lumex::core::protocol::dsl::key_kind::integer,
      static_cast<std::uint8_t> (key_width ()), bits, nullptr, 0
    };
    out.payload_offset = Offset + Digits;
    return true;
  }
};

/**
 * @brief The key of a tokenized message: the characters before the first
 * `KeySeparator` (all of the frame when there is none), spaces at the ends
 * dropped when `Trim`.
 */
template <char KeySeparator, bool Trim = true> struct tokenized_key
{
  /// @brief Reads the key; the message (with its key) is the payload.
  static bool
  read (::lumex::core::protocol::layout::bytes_view_t frame,
        key_read_t &out) LUMEX_NOEXCEPT
  {
    std::size_t end = 0;
    while (end < frame.size ()
           && static_cast<char> (frame.data ()[end]) != KeySeparator)
      ++end;
    std::size_t begin = 0;
    if (Trim)
      {
        while (begin < end && static_cast<char> (frame.data ()[begin]) == ' ')
          ++begin;
        while (end > begin
               && static_cast<char> (frame.data ()[end - 1u]) == ' ')
          --end;
      }
    out.key = ::lumex::core::protocol::dsl::key_value_t{
      ::lumex::core::protocol::dsl::key_kind::text, 0, 0,
      reinterpret_cast<char const *> (frame.data ()) + begin, end - begin
    };
    out.payload_offset = 0;
    return true;
  }
};

/// @brief A reader that finds no key: a frame is never matched to a row.
struct no_key_reader
{
  /// @brief Always false.
  static bool
  read (::lumex::core::protocol::layout::bytes_view_t,
        key_read_t &) LUMEX_NOEXCEPT
  {
    return false;
  }
};

namespace detail
{
template <typename T> struct key_voider
{
  using type = void;
};

template <typename H, typename = void> struct header_key_reader
{
  static LUMEX_CONSTEXPR bool
  present () LUMEX_NOEXCEPT
  {
    return false;
  }
};

template <typename H>
struct header_key_reader<H, typename key_voider<typename H::key_reader>::type>
{
  using type = typename H::key_reader;

  static LUMEX_CONSTEXPR bool
  present () LUMEX_NOEXCEPT
  {
    return true;
  }
};

template <typename H, typename = void> struct header_key_width
{
  static LUMEX_CONSTEXPR std::size_t
  get () LUMEX_NOEXCEPT
  {
    return 0;
  }
};

template <typename H>
struct header_key_width<H, typename std::enable_if<(H::key_width > 0)>::type>
{
  static LUMEX_CONSTEXPR std::size_t
  get () LUMEX_NOEXCEPT
  {
    return H::key_width;
  }
};

// The bytes of the largest integer or composite key type of the table.
template <typename... Rows> struct widest_key;

template <> struct widest_key<>
{
  static LUMEX_CONSTEXPR std::size_t
  get () LUMEX_NOEXCEPT
  {
    return 0;
  }
};

// The bytes a key type takes on the wire: its size for an integer or an
// enumerator, lumex_key_width () for a composite key, 0 for the rest.
template <typename T,
          bool Composite
          = ::lumex::core::protocol::dsl::detail::is_composite_key<T>::value>
struct key_type_width
{
  static LUMEX_CONSTEXPR std::size_t
  get () LUMEX_NOEXCEPT
  {
    return (std::is_integral<T>::value || std::is_enum<T>::value) ? sizeof (T)
                                                                  : 0u;
  }
};

template <typename T> struct key_type_width<T, true>
{
  static LUMEX_CONSTEXPR std::size_t
  get () LUMEX_NOEXCEPT
  {
    return T::lumex_key_width ();
  }
};

template <typename Head, typename... Tail> struct widest_key<Head, Tail...>
{
  static LUMEX_CONSTEXPR std::size_t
  own () LUMEX_NOEXCEPT
  {
    return key_type_width<typename Head::key_type>::get ();
  }

  // The width of the rest is computed once: evaluating it in both arms of
  // the comparison doubled the steps per row (2^n for n rows), and a table
  // of about twenty rows ran into the constexpr step limit of the compiler.
  static LUMEX_CONSTEXPR std::size_t
  get () LUMEX_NOEXCEPT
  {
    return larger (own (), widest_key<Tail...>::get ());
  }

private:
  static LUMEX_CONSTEXPR std::size_t
  larger (std::size_t first, std::size_t second) LUMEX_NOEXCEPT
  {
    return first > second ? first : second;
  }
};

template <typename List> struct widest_of_list;

template <typename... Rows>
struct widest_of_list<::lumex::core::protocol::dsl::type_list<Rows...>>
    : widest_key<Rows...>
{
};

template <typename Layout, std::size_t Width> struct default_reader
{
  using type = no_key_reader;
};

template <typename Policy, std::size_t Width>
struct default_reader<::lumex::core::protocol::layout::binary_layout<Policy>,
                      Width>
{
  using type = key_at<0, Width>;
};

template <std::size_t Width>
struct default_reader<
    ::lumex::core::protocol::layout::fixed_width_text_layout<16>, Width>
{
  using type = key_at_digits<0, 2 * Width, 16>;
};

template <char Separator, char KeySeparator, unsigned Radix, bool Trim,
          std::size_t Width>
struct default_reader<::lumex::core::protocol::layout::tokenized_text_layout<
                          Separator, KeySeparator, Radix, Trim>,
                      Width>
{
  using type = tokenized_key<KeySeparator, Trim>;
};
} // namespace detail

/**
 * @brief The width of the key of protocol `P` on the wire: the
 * `key_width` constant of its header type, otherwise the bytes of the
 * widest integer key type of its table (1 without an integer key).
 */
template <typename P>
struct key_width_of
    : std::integral_constant<
          std::size_t,
          detail::header_key_width<typename P::header_type>::get () != 0
              ? detail::header_key_width<typename P::header_type>::get ()
              : (detail::widest_of_list<typename ::lumex::core::protocol::dsl::
                                            commands_of<P>::type>::get ()
                         != 0
                     ? detail::widest_of_list<
                           typename ::lumex::core::protocol::dsl::commands_of<
                               P>::type>::get ()
                     : 1u)>
{
};

/**
 * @brief The reader of the key of the frames of protocol `P` (see the file
 * description).
 */
// Layouts whose default reader needs the width of the key (the digits of a
// hexadecimal key are twice its bytes): the header names it, or names the
// reader itself.
template <typename Layout> struct layout_needs_key_width : std::false_type
{
};

template <typename Policy>
struct layout_needs_key_width<
    ::lumex::core::protocol::layout::binary_layout<Policy>> : std::true_type
{
};

template <>
struct layout_needs_key_width<
    ::lumex::core::protocol::layout::fixed_width_text_layout<16>>
    : std::true_type
{
};

// Whether the key width is known where the layout needs it.
template <typename P>
struct key_width_known
    : std::integral_constant<
          bool,
          detail::header_key_reader<typename P::header_type>::present ()
              || detail::header_key_width<typename P::header_type>::get () != 0
              || !layout_needs_key_width<
                  typename ::lumex::core::protocol::layout::layout_of<
                      P>::type>::value>
{
};

template <typename P>
struct key_reader_of
    : std::conditional<
          detail::header_key_reader<typename P::header_type>::present (),
          detail::header_key_reader<typename P::header_type>,
          detail::default_reader<
              typename ::lumex::core::protocol::layout::layout_of<P>::type,
              key_width_of<P>::value>>::type
{
  static_assert (
      key_width_known<P>::value,
      "LUMEX_PROTOCOL: the header type of a binary or hexadecimal "
      "fixed-width layer names the width of the key on the wire "
      "(static constexpr std::size_t key_width = N;) or its own key_reader "
      "(using key_reader = index::key_at<Offset, N>;): the width of the "
      "key type (an int for 0x1D) is not the width on the wire");
};
} // namespace index
} // namespace protocol
} // namespace core
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_PROTOCOL_INDEX_KEY_READER_HPP
