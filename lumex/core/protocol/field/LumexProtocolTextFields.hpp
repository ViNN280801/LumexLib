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
 * @file LumexProtocolTextFields.hpp
 * @brief The text fields: `ascii_hex_<Width>`, `dec_text_<IntDigits,
 * FracDigits>`, `char_`, `text_<N>` and `token_`.
 * @details In a binary or fixed-width layout these are characters of a fixed
 * width inside the message (`ascii_hex_<4>` is four hexadecimal digits,
 * `dec_text_<6, 3>` is `000500.000`, `text_<8>` is eight bytes padded with
 * NUL); in a tokenized layout each one is a token of the message (at most
 * the given width). `token_` is a token of a tokenized layout and the rest
 * of the message in the other layouts. The text values are views or fixed
 * strings: decoding never allocates.
 */
#ifndef LUMEX_CORE_PROTOCOL_FIELD_TEXT_FIELDS_HPP
#define LUMEX_CORE_PROTOCOL_FIELD_TEXT_FIELDS_HPP

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ratio>
#include <type_traits>

#include "lumex/core/protocol/error/LumexProtocolErrc.hpp"
#include "lumex/core/protocol/field/LumexProtocolFieldTraits.hpp"
#include "lumex/core/protocol/field/LumexProtocolIntegerFields.hpp"
#include "lumex/core/protocol/field/LumexProtocolModifiers.hpp"
#include "lumex/core/protocol/field/LumexProtocolNumeric.hpp"
#include "lumex/core/protocol/field/LumexProtocolValues.hpp"
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
namespace field
{
namespace detail
{
// The smallest unsigned type of at least Bits bits.
template <std::size_t Bits> struct unsigned_for
{
  typedef typename std::conditional<
      (Bits <= 8), std::uint8_t,
      typename std::conditional<
          (Bits <= 16), std::uint16_t,
          typename std::conditional<(Bits <= 32), std::uint32_t,
                                    std::uint64_t>::type>::type>::type type;
};

// 10 to the power N as an intmax_t constant.
template <std::size_t N>
struct power_of_ten
    : std::integral_constant<std::intmax_t, 10 * power_of_ten<N - 1>::value>
{
};

template <> struct power_of_ten<0> : std::integral_constant<std::intmax_t, 1>
{
};
} // namespace detail

/**
 * @brief `Width` hexadecimal digits (1 to 16): `ascii_hex_<4>` reads `"1A2F"`.
 * @details The value is unsigned (the smallest type of `4 * Width` bits), a
 * fixed point when the field takes a `scale<>`. On encode the digits are
 * upper case and zero-padded; a value wider than `Width` digits is
 * `invalid_value`.
 */
template <std::size_t Width, typename... Modifiers>
struct ascii_hex_ : detail::check_modifiers<true, Modifiers...>
{
  static_assert (Width >= 1 && Width <= 16,
                 "ascii_hex_<Width>: Width must be between 1 and 16 digits");

  /// Marks the type as a field.
  typedef field_tag lumex_protocol_field;
  /// The integer type of the raw value.
  typedef typename detail::unsigned_for<4 * Width>::type raw_type;
  /// The factor of the engineering value.
  typedef typename detail::scale_of<Modifiers...>::type scale_type;
  /// The value.
  typedef
      typename detail::numeric_value<raw_type, scale_type>::type value_type;
  /// Characters in a binary layout.
  typedef std::integral_constant<std::size_t, Width> binary_size;
  /// Characters need no alignment.
  typedef std::integral_constant<std::size_t, 1> binary_alignment;

  /// @brief The descriptor (`count` is the number of digits).
  static LUMEX_CONSTEXPR field_info_t
  info () LUMEX_NOEXCEPT
  {
    return detail::make_info (field_kind::hex_text, Width, byte_order::big,
                              false, scale_type::num, scale_type::den,
                              detail::unit_of<Modifiers...>::symbol (), Width);
  }

  /// @brief Reads the digits.
  template <bool Last, typename Reader, typename Siblings>
  static bool
  decode (Reader &reader, value_type &value, Siblings const &)
  {
    std::uint64_t number = 0;
    if (!reader.read_hex_text (Width, number))
      return false;
    detail::raw_access<value_type>::set (value,
                                         static_cast<raw_type> (number));
    return true;
  }

  /// @brief Writes the digits.
  template <bool Last, typename Writer, typename Siblings>
  static bool
  encode (Writer &writer, value_type const &value, Siblings const &)
  {
    return writer.write_hex_text (
        Width, static_cast<std::uint64_t> (
                   detail::raw_access<value_type>::get (value)));
  }

  /// @brief Sets `value` from an argument (an integer, or the engineering
  /// value of a scaled field).
  template <typename Arg>
  static bool
  assign (value_type &value, Arg const &argument)
  {
    return detail::
        numeric_assign<value_type, raw_type, 4 * Width, scale_type>::apply (
            value, argument,
            std::integral_constant<bool, std::is_arithmetic<Arg>::value> ());
  }
};

/**
 * @brief A decimal number with a fixed point: `IntDigits` digits, a point
 * and `FracDigits` digits (`dec_text_<6, 3>` is `000500.000`).
 * @details The value is a `fixed_point<std::int64_t, ...>` in units of the
 * last digit (times a `scale<>` modifier if given). On encode the number is
 * zero-padded to the full width; a negative number puts `-` in the first
 * position. On decode leading spaces, a sign and fewer fraction digits are
 * accepted; anything else is `invalid_value`. Without fraction digits there
 * is no point. At most 18 digits.
 */
template <std::size_t IntDigits, std::size_t FracDigits, typename... Modifiers>
struct dec_text_ : detail::check_modifiers<true, Modifiers...>
{
  static_assert (IntDigits >= 1, "dec_text_: give at least one digit");
  static_assert (IntDigits + FracDigits <= 18,
                 "dec_text_: at most 18 digits (a 64-bit value)");

  /// Marks the type as a field.
  typedef field_tag lumex_protocol_field;
  /// The raw value: the number in units of the last digit.
  typedef std::int64_t raw_type;
  /// The factor of the engineering value: one unit of the last digit,
  /// times the scale<> modifier.
  typedef std::ratio_multiply<
      std::ratio<1, detail::power_of_ten<FracDigits>::value>,
      typename detail::scale_of<Modifiers...>::type>
      scale_type;
  /// The value.
  typedef
      typename detail::numeric_value<raw_type, scale_type>::type value_type;
  /// Characters in a binary layout.
  typedef std::integral_constant<
      std::size_t, IntDigits + (FracDigits == 0 ? 0 : 1 + FracDigits)>
      binary_size;
  /// Characters need no alignment.
  typedef std::integral_constant<std::size_t, 1> binary_alignment;

  /// @brief The descriptor (`count` is the number of digits).
  static LUMEX_CONSTEXPR field_info_t
  info () LUMEX_NOEXCEPT
  {
    return detail::make_info (
        field_kind::decimal_text, binary_size::value, byte_order::big, true,
        scale_type::num, scale_type::den,
        detail::unit_of<Modifiers...>::symbol (), IntDigits + FracDigits);
  }

  /// @brief Reads the number.
  template <bool Last, typename Reader, typename Siblings>
  static bool
  decode (Reader &reader, value_type &value, Siblings const &)
  {
    std::int64_t number = 0;
    if (!reader.read_decimal_text (IntDigits, FracDigits, number))
      return false;
    detail::raw_access<value_type>::set (value, number);
    return true;
  }

  /// @brief Writes the number; `invalid_value` when it does not fit the
  /// width.
  template <bool Last, typename Writer, typename Siblings>
  static bool
  encode (Writer &writer, value_type const &value, Siblings const &)
  {
    return writer.write_decimal_text (
        IntDigits, FracDigits, detail::raw_access<value_type>::get (value));
  }

  /// @brief Sets `value` from an argument (the engineering value).
  template <typename Arg>
  static bool
  assign (value_type &value, Arg const &argument)
  {
    return detail::numeric_assign<value_type, raw_type, 64, scale_type>::
        apply (
            value, argument,
            std::integral_constant<bool, std::is_arithmetic<Arg>::value> ());
  }
};

/**
 * @brief One character (a code letter such as `'S'`).
 * @details In a tokenized layout, a token of exactly one character.
 */
struct char_
{
  /// Marks the type as a field.
  typedef field_tag lumex_protocol_field;
  /// The value.
  typedef char value_type;
  /// One byte in a binary layout.
  typedef std::integral_constant<std::size_t, 1> binary_size;
  /// No alignment.
  typedef std::integral_constant<std::size_t, 1> binary_alignment;

  /// @brief The descriptor.
  static LUMEX_CONSTEXPR field_info_t
  info () LUMEX_NOEXCEPT
  {
    return detail::make_info (field_kind::character, 1, byte_order::big, false,
                              1, 1, "", 1);
  }

  /// @brief Reads the character.
  template <bool Last, typename Reader, typename Siblings>
  static bool
  decode (Reader &reader, value_type &value, Siblings const &)
  {
    return reader.read_char (value);
  }

  /// @brief Writes the character.
  template <bool Last, typename Writer, typename Siblings>
  static bool
  encode (Writer &writer, value_type const &value, Siblings const &)
  {
    return writer.write_char (value);
  }

  /// @brief Sets `value` from a `char`.
  template <typename Arg>
  static bool
  assign (value_type &value, Arg const &argument)
  {
    static_assert (std::is_same<Arg, char>::value,
                   "LUMEX_PROTOCOL: char_ takes a char");
    value = argument;
    return true;
  }
};

/**
 * @brief A text of at most `N` characters: `N` bytes padded with NUL in a
 * binary or fixed-width layout, a token of at most `N` characters in a
 * tokenized one.
 * @details The value is a `fixed_string<N>` (the characters before the
 * first NUL). A longer text, or one holding a NUL, is `invalid_value` on
 * encode and refused by `assign`.
 */
template <std::size_t N> struct text_
{
  static_assert (N >= 1, "text_<N>: N must be positive");

  /// Marks the type as a field.
  typedef field_tag lumex_protocol_field;
  /// The value.
  typedef fixed_string<N> value_type;
  /// Bytes in a binary layout.
  typedef std::integral_constant<std::size_t, N> binary_size;
  /// No alignment.
  typedef std::integral_constant<std::size_t, 1> binary_alignment;

  /// @brief The descriptor (`count` is the capacity).
  static LUMEX_CONSTEXPR field_info_t
  info () LUMEX_NOEXCEPT
  {
    return detail::make_info (field_kind::text, N, byte_order::big, false, 1,
                              1, "", N);
  }

  /// @brief Reads the text.
  template <bool Last, typename Reader, typename Siblings>
  static bool
  decode (Reader &reader, value_type &value, Siblings const &)
  {
    char const *data = nullptr;
    std::size_t size = 0;
    if (!reader.read_text (N, data, size))
      return false;
    std::size_t length = 0;
    while (length < size && data[length] != '\0')
      ++length;
    if (!value.assign (text_view (data, length)))
      return reader.fail (error::protocol_errc::invalid_value);
    return true;
  }

  /// @brief Writes the text.
  template <bool Last, typename Writer, typename Siblings>
  static bool
  encode (Writer &writer, value_type const &value, Siblings const &)
  {
    if (value.length > N
        || (value.length != 0
            && std::memchr (value.chars, '\0', value.length) != nullptr))
      return writer.fail (error::protocol_errc::invalid_value);
    return writer.write_text (N, value.chars, value.length);
  }

  /// @brief Sets `value` from a text (a string literal, a `text_view`);
  /// false when it is longer than `N`.
  template <typename Arg>
  static bool
  assign (value_type &value, Arg const &argument)
  {
    return assign_text (value, argument, std::is_same<Arg, value_type> ());
  }

private:
  template <typename Arg>
  static bool
  assign_text (value_type &value, Arg const &argument, std::false_type)
  {
    static_assert (std::is_convertible<Arg, text_view>::value,
                   "LUMEX_PROTOCOL: text_<N> takes a text (a string literal "
                   "or a field::text_view)");
    return value.assign (text_view (argument));
  }

  static bool
  assign_text (value_type &value, value_type const &argument, std::true_type)
  {
    value = argument;
    return true;
  }
};

/**
 * @brief A token of a tokenized layout; the rest of the message in a binary
 * or fixed-width layout (there it must be the last field).
 * @details The value is a `text_view` into the decoded message.
 */
struct token_
{
  /// Marks the type as a field.
  typedef field_tag lumex_protocol_field;
  /// The value.
  typedef text_view value_type;
  /// The size depends on the message.
  typedef std::integral_constant<std::size_t, variable_size> binary_size;
  /// No alignment.
  typedef std::integral_constant<std::size_t, 1> binary_alignment;

  /// @brief The descriptor.
  static LUMEX_CONSTEXPR field_info_t
  info () LUMEX_NOEXCEPT
  {
    return detail::make_info (field_kind::token, 0, byte_order::big, false, 1,
                              1, "", 0);
  }

  /// @brief Reads the token.
  template <bool Last, typename Reader, typename Siblings>
  static bool
  decode (Reader &reader, value_type &value, Siblings const &)
  {
    static_assert (Last || Reader::is_tokenized::value,
                   "LUMEX_PROTOCOL: token_ outside a tokenized layout reads "
                   "the rest of the message, so it must be the last field");
    char const *data = nullptr;
    std::size_t size = 0;
    if (!reader.read_token (data, size))
      return false;
    value = text_view (data, size);
    return true;
  }

  /// @brief Writes the token.
  template <bool Last, typename Writer, typename Siblings>
  static bool
  encode (Writer &writer, value_type const &value, Siblings const &)
  {
    static_assert (Last || Writer::is_tokenized::value,
                   "LUMEX_PROTOCOL: token_ outside a tokenized layout reads "
                   "the rest of the message, so it must be the last field");
    return writer.write_token (value.data (), value.size ());
  }

  /// @brief Sets `value` from a text.
  template <typename Arg>
  static bool
  assign (value_type &value, Arg const &argument)
  {
    static_assert (std::is_convertible<Arg, text_view>::value,
                   "LUMEX_PROTOCOL: token_ takes a text (a string literal or "
                   "a field::text_view)");
    value = text_view (argument);
    return true;
  }
};
} // namespace field
} // namespace protocol
} // namespace core
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_PROTOCOL_FIELD_TEXT_FIELDS_HPP
