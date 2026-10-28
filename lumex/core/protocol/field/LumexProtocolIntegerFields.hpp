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
 * @file LumexProtocolIntegerFields.hpp
 * @brief The numeric fields: `integer_<Raw, Bytes, Order, Modifiers...>` and
 * its names (`u8_`, `i8_`, `u16_le_` ... `i64_be_`, the 24-bit `u24_le_`,
 * `i24_be_`, ...), `floating_` (`f32_le_` ... `f64_be_`) and
 * `sign_magnitude_<IntegerField>`.
 * @details A field describes the logical value (width, signedness, byte
 * order, scale, unit); the layout of the protocol decides how it looks on
 * the wire (bytes in a binary layout, digits in a text layout). A plain
 * field is written `u8_` or `u8_<>`, a scaled one
 * `u16_le_<scale<std::ratio<1, 100> >, unit::degree_celsius>`: the decoded
 * value is then a `fixed_point` whose `value ()` is the engineering value
 * and whose `raw` is the wire value.
 */
#ifndef LUMEX_CORE_PROTOCOL_FIELD_INTEGER_FIELDS_HPP
#define LUMEX_CORE_PROTOCOL_FIELD_INTEGER_FIELDS_HPP

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ratio>
#include <type_traits>

#include "lumex/core/protocol/error/LumexProtocolErrc.hpp"
#include "lumex/core/protocol/field/LumexProtocolFieldTraits.hpp"
#include "lumex/core/protocol/field/LumexProtocolModifiers.hpp"
#include "lumex/core/protocol/field/LumexProtocolNumeric.hpp"
#include "lumex/core/protocol/field/LumexProtocolValues.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#if __has_warning("-Wunsafe-buffer-usage-in-libc-call")
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-libc-call"
#endif
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
// The value of a numeric field: the raw type, or a fixed point when scaled.
template <typename Raw, typename Ratio> struct numeric_value
{
  using type = fixed_point<Raw, Ratio>;
};

template <typename Raw> struct numeric_value<Raw, std::ratio<1>>
{
  using type = Raw;
};

// assign () of a numeric field from an argument: an integer for a plain
// field, an engineering value (any arithmetic type) or the fixed point for a
// scaled one.
template <typename Value, typename Raw, std::size_t Bits, typename Ratio>
struct numeric_assign
{
  template <typename Arg>
  static bool
  apply (Value &value, Arg const &argument, std::true_type)
  {
    // An arithmetic argument: the engineering value.
    Raw raw = Raw ();
    if (!engineering_to_raw (static_cast<double> (argument), Ratio::num,
                             Ratio::den, Bits, raw))
      return false;
    raw_access<Value>::set (value, raw);
    return true;
  }

  template <typename Arg>
  static bool
  apply (Value &value, Arg const &argument, std::false_type)
  {
    static_assert (std::is_same<Arg, Value>::value,
                   "LUMEX_PROTOCOL: a scaled field takes its engineering "
                   "value (a number) or its fixed_point");
    value = argument;
    return true;
  }
};

template <typename Raw, std::size_t Bits>
struct numeric_assign<Raw, Raw, Bits, std::ratio<1>>
{
  template <typename Arg>
  static bool
  apply (Raw &value, Arg const &argument, std::true_type)
  {
    static_assert (std::is_integral<Arg>::value,
                   "LUMEX_PROTOCOL: an integer field without scale<> takes "
                   "an integer (give the field a scale<> for a fractional "
                   "value)");
    return integral_to_raw (argument, Bits, value);
  }

  template <typename Arg>
  static bool
  apply (Raw &value, Arg const &argument, std::false_type)
  {
    static_assert (std::is_same<Arg, Raw>::value,
                   "LUMEX_PROTOCOL: an integer field takes an integer");
    value = argument;
    return true;
  }
};
// The IEEE 754 bits of a float or a double.
template <typename Raw> struct floating_bits;

template <> struct floating_bits<float>
{
  static float
  from (std::uint64_t bits) LUMEX_NOEXCEPT
  {
    std::uint32_t const narrow = static_cast<std::uint32_t> (bits);
    float value = 0;
    std::memcpy (&value, &narrow, sizeof (value));
    return value;
  }

  static std::uint64_t
  to (float value) LUMEX_NOEXCEPT
  {
    std::uint32_t narrow = 0;
    std::memcpy (&narrow, &value, sizeof (value));
    return narrow;
  }
};

template <> struct floating_bits<double>
{
  static double
  from (std::uint64_t bits) LUMEX_NOEXCEPT
  {
    double value = 0;
    std::memcpy (&value, &bits, sizeof (value));
    return value;
  }

  static std::uint64_t
  to (double value) LUMEX_NOEXCEPT
  {
    std::uint64_t bits = 0;
    std::memcpy (&bits, &value, sizeof (value));
    return bits;
  }
};
} // namespace detail

/**
 * @brief An integer field of `Bytes` bytes (1 to 8) whose value has type
 * `Raw`.
 * @tparam Raw The integer type of the value (signed for a signed field).
 * @tparam Bytes The width on the wire; fewer bytes than `Raw` gives the
 * 24-bit fields.
 * @tparam Order The byte order in a binary layout.
 * @tparam Modifiers At most one `scale<Ratio>` and one unit.
 */
template <typename Raw, std::size_t Bytes, byte_order Order,
          typename... Modifiers>
struct integer_ : detail::check_modifiers<true, Modifiers...>
{
  static_assert (std::is_integral<Raw>::value
                     && !std::is_same<Raw, bool>::value,
                 "integer_: Raw must be an integer type");
  static_assert (Bytes >= 1 && Bytes <= sizeof (Raw),
                 "integer_: Bytes must be between 1 and sizeof (Raw)");

  /// Marks the type as a field.
  using lumex_protocol_field = field_tag;
  /// The integer type of the wire value.
  using raw_type = Raw;
  /// The factor of the engineering value (`std::ratio<1>` without a scale).
  using scale_type = typename detail::scale_of<Modifiers...>::type;
  /// The decoded value: `Raw`, or `fixed_point<Raw, scale_type>` when scaled.
  using value_type = typename detail::numeric_value<Raw, scale_type>::type;
  /// Bytes in a binary layout.
  using binary_size = std::integral_constant<std::size_t, Bytes>;
  /// The alignment the aligned binary layout checks: 2 for two bytes, 4 for
  /// four and eight, none for the others.
  using binary_alignment
      = std::integral_constant<std::size_t,
                               Bytes == 2 ? 2u : (Bytes >= 4 ? 4u : 1u)>;

  /// @brief The number of bytes on the wire.
  static LUMEX_CONSTEXPR std::size_t
  size () LUMEX_NOEXCEPT
  {
    return Bytes;
  }

  /// @brief The byte order on the wire.
  static LUMEX_CONSTEXPR byte_order
  order () LUMEX_NOEXCEPT
  {
    return Order;
  }

  /// @brief The unit symbol, `""` without a unit.
  static LUMEX_CONSTEXPR char const *
  unit () LUMEX_NOEXCEPT
  {
    return detail::unit_of<Modifiers...>::symbol ();
  }

  /// @brief The descriptor (`count` is 0).
  static LUMEX_CONSTEXPR field_info_t
  info () LUMEX_NOEXCEPT
  {
    return detail::make_info (field_kind::integer, Bytes, Order,
                              std::is_signed<Raw>::value, scale_type::num,
                              scale_type::den, unit (), 0u);
  }

  /// @brief The raw value of `bits` (the low `Bytes` bytes).
  static Raw
  from_bits (std::uint64_t bits) LUMEX_NOEXCEPT
  {
    return std::is_signed<Raw>::value
               ? static_cast<Raw> (detail::sign_extend (bits, 8u * Bytes))
               : static_cast<Raw> (bits & detail::width_mask (Bytes));
  }

  /// @brief The low `Bytes` bytes of raw value `raw`.
  static std::uint64_t
  to_bits (Raw raw) LUMEX_NOEXCEPT
  {
    return detail::to_bits (static_cast<std::int64_t> (raw), 8u * Bytes);
  }

  /// @brief Reads the field.
  template <bool Last, typename Reader, typename Siblings>
  static bool
  decode (Reader &reader, value_type &value, Siblings const &)
  {
    std::uint64_t bits = 0;
    if (!reader.read_integer (Bytes, Order, std::is_signed<Raw>::value, bits))
      return false;
    detail::raw_access<value_type>::set (value, from_bits (bits));
    return true;
  }

  /// @brief Writes the field; `invalid_value` when the raw value does not
  /// fit `Bytes` bytes.
  template <bool Last, typename Writer, typename Siblings>
  static bool
  encode (Writer &writer, value_type const &value, Siblings const &)
  {
    Raw const raw = detail::raw_access<value_type>::get (value);
    if (!detail::raw_fits (raw, 8u * Bytes))
      return writer.fail (error::protocol_errc::invalid_value);
    return writer.write_integer (Bytes, Order, std::is_signed<Raw>::value,
                                 to_bits (raw));
  }

  /// @brief Sets `value` from an argument: an integer, or for a scaled field
  /// the engineering value or the fixed point; false when it does not fit.
  template <typename Arg>
  static bool
  assign (value_type &value, Arg const &argument)
  {
    return detail::numeric_assign<value_type, Raw, 8u * Bytes, scale_type>::
        apply (
            value, argument,
            std::integral_constant<bool, std::is_arithmetic<Arg>::value> ());
  }
};

/**
 * @brief An IEEE 754 field: `float` (4 bytes) or `double` (8 bytes).
 * @details The value is the engineering value already; the field takes a
 * unit but no `scale<>`.
 */
template <typename Raw, byte_order Order, typename... Modifiers>
struct floating_ : detail::check_modifiers<false, Modifiers...>
{
  static_assert ((std::is_same<Raw, float>::value && sizeof (float) == 4)
                     || (std::is_same<Raw, double>::value
                         && sizeof (double) == 8),
                 "floating_: Raw must be a 4-byte float or an 8-byte double");

  /// Marks the type as a field.
  using lumex_protocol_field = field_tag;
  /// The value type.
  using raw_type = Raw;
  /// The value.
  using value_type = Raw;
  /// Bytes in a binary layout.
  using binary_size = std::integral_constant<std::size_t, sizeof (Raw)>;
  /// The alignment the aligned binary layout checks.
  using binary_alignment = std::integral_constant<std::size_t, 4>;

  /// @brief The number of bytes on the wire.
  static LUMEX_CONSTEXPR std::size_t
  size () LUMEX_NOEXCEPT
  {
    return sizeof (Raw);
  }

  /// @brief The byte order on the wire.
  static LUMEX_CONSTEXPR byte_order
  order () LUMEX_NOEXCEPT
  {
    return Order;
  }

  /// @brief The descriptor.
  static LUMEX_CONSTEXPR field_info_t
  info () LUMEX_NOEXCEPT
  {
    return detail::make_info (field_kind::floating, sizeof (Raw), Order, true,
                              1, 1, detail::unit_of<Modifiers...>::symbol (),
                              0u);
  }

  /// @brief Reads the field.
  template <bool Last, typename Reader, typename Siblings>
  static bool
  decode (Reader &reader, value_type &value, Siblings const &)
  {
    static_assert (Reader::supports_floating::value,
                   "LUMEX_PROTOCOL: a floating field in a decimal "
                   "fixed-width layout: use dec_text_<I, F>");
    std::uint64_t bits = 0;
    if (!reader.read_floating (sizeof (Raw), Order, bits))
      return false;
    value = detail::floating_bits<Raw>::from (bits);
    return true;
  }

  /// @brief Writes the field.
  template <bool Last, typename Writer, typename Siblings>
  static bool
  encode (Writer &writer, value_type const &value, Siblings const &)
  {
    static_assert (Writer::supports_floating::value,
                   "LUMEX_PROTOCOL: a floating field in a decimal "
                   "fixed-width layout: use dec_text_<I, F>");
    std::uint64_t const bits = detail::floating_bits<Raw>::to (value);
    return writer.write_floating (sizeof (Raw), Order, bits);
  }

  /// @brief Sets `value` from any arithmetic argument.
  template <typename Arg>
  static bool
  assign (value_type &value, Arg const &argument)
  {
    static_assert (std::is_arithmetic<Arg>::value,
                   "LUMEX_PROTOCOL: a floating field takes a number");
    value = static_cast<Raw> (argument);
    return true;
  }
};

/**
 * @brief An integer whose top bit is the sign and whose other bits are the
 * magnitude: `sign_magnitude_<i32_be_>`.
 * @details The width, the byte order, the scale and the unit are those of
 * the integer field; the value is signed. A binary layout and a hexadecimal
 * text layout carry the sign-magnitude bits; a decimal text layout writes
 * the signed number. `-0` decodes to 0.
 */
template <typename IntegerField> struct sign_magnitude_
{
  /// The integer field that gives the width, order, scale and unit.
  using inner = typename impl_of<IntegerField>::type;
  /// The raw type: the signed type of the integer field.
  using raw_type = typename std::make_signed<typename inner::raw_type>::type;
  /// The factor of the engineering value.
  using scale_type = typename inner::scale_type;
  /// The value: signed, or a fixed point when scaled.
  using value_type =
      typename detail::numeric_value<raw_type, scale_type>::type;
  /// Marks the type as a field.
  using lumex_protocol_field = field_tag;
  /// Bytes in a binary layout.
  using binary_size = typename inner::binary_size;
  /// The alignment the aligned binary layout checks.
  using binary_alignment = typename inner::binary_alignment;

  /// @brief The descriptor.
  static LUMEX_CONSTEXPR field_info_t
  info () LUMEX_NOEXCEPT
  {
    return detail::make_info (field_kind::sign_magnitude, binary_size::value,
                              inner::order (), true, scale_type::num,
                              scale_type::den, inner::info ().unit, 0u);
  }

  /// @brief Reads the field.
  template <bool Last, typename Reader, typename Siblings>
  static bool
  decode (Reader &reader, value_type &value, Siblings const &)
  {
    std::size_t const bits_count = 8u * binary_size::value;
    std::uint64_t bits = 0;
    if (!reader.read_integer (binary_size::value, inner::order (),
                              !Reader::preserves_bits::value, bits))
      return false;
    std::int64_t result = 0;
    if (Reader::preserves_bits::value)
      {
        std::uint64_t const magnitude
            = bits & detail::low_bits (bits_count - 1u);
        bool const negative = ((bits >> (bits_count - 1u)) & 1u) != 0;
        result = negative ? -static_cast<std::int64_t> (magnitude)
                          : static_cast<std::int64_t> (magnitude);
      }
    else
      result = detail::sign_extend (bits, bits_count);
    // The magnitude of a sign-magnitude value never reaches the minimum of
    // two's complement, so a decimal text that does is refused.
    if (!fits_magnitude (result))
      return reader.fail (error::protocol_errc::invalid_value);
    detail::raw_access<value_type>::set (value,
                                         static_cast<raw_type> (result));
    return true;
  }

  /// @brief Writes the field; `invalid_value` when the magnitude does not fit
  /// the bits below the sign.
  template <bool Last, typename Writer, typename Siblings>
  static bool
  encode (Writer &writer, value_type const &value, Siblings const &)
  {
    std::int64_t const raw = static_cast<std::int64_t> (
        detail::raw_access<value_type>::get (value));
    if (!fits_magnitude (raw))
      return writer.fail (error::protocol_errc::invalid_value);
    std::size_t const bits_count = 8u * binary_size::value;
    std::uint64_t bits = 0;
    if (Writer::preserves_bits::value)
      bits = raw < 0 ? ((static_cast<std::uint64_t> (1) << (bits_count - 1u))
                        | static_cast<std::uint64_t> (-raw))
                     : static_cast<std::uint64_t> (raw);
    else
      bits = detail::to_bits (raw, bits_count);
    return writer.write_integer (binary_size::value, inner::order (),
                                 !Writer::preserves_bits::value, bits);
  }

  /// @brief Sets `value` from an argument (as an integer field of this
  /// width and scale).
  template <typename Arg>
  static bool
  assign (value_type &value, Arg const &argument)
  {
    if (!detail::numeric_assign<
            value_type, raw_type, 8u * binary_size::value,
            scale_type>::apply (value, argument,
                                std::integral_constant<
                                    bool, std::is_arithmetic<Arg>::value> ()))
      return false;
    return fits_magnitude (static_cast<std::int64_t> (
        detail::raw_access<value_type>::get (value)));
  }

private:
  static bool
  fits_magnitude (std::int64_t raw) LUMEX_NOEXCEPT
  {
    std::uint64_t const limit
        = detail::low_bits (8u * binary_size::value - 1u);
    return raw < 0 ? (raw >= -static_cast<std::int64_t> (limit))
                   : (static_cast<std::uint64_t> (raw) <= limit);
  }
};

namespace detail
{
// Whether Impl is an integer field without scale<> (the word of the bit
// fields, a discriminant at the end).
template <typename Impl> struct is_plain_integer : std::false_type
{
};

template <typename Raw, std::size_t Bytes, byte_order Order,
          typename... Modifiers>
struct is_plain_integer<integer_<Raw, Bytes, Order, Modifiers...>>
    : std::is_same<
          typename integer_<Raw, Bytes, Order, Modifiers...>::scale_type,
          std::ratio<1>>
{
};
} // namespace detail

/// @brief One byte, unsigned.
template <typename... Modifiers>
using u8_ = integer_<std::uint8_t, 1, byte_order::little, Modifiers...>;
/// @brief One byte, signed.
template <typename... Modifiers>
using i8_ = integer_<std::int8_t, 1, byte_order::little, Modifiers...>;
/// @brief Two bytes, unsigned, least significant first.
template <typename... Modifiers>
using u16_le_ = integer_<std::uint16_t, 2, byte_order::little, Modifiers...>;
/// @brief Two bytes, unsigned, most significant first.
template <typename... Modifiers>
using u16_be_ = integer_<std::uint16_t, 2, byte_order::big, Modifiers...>;
/// @brief Two bytes, signed, least significant first.
template <typename... Modifiers>
using i16_le_ = integer_<std::int16_t, 2, byte_order::little, Modifiers...>;
/// @brief Two bytes, signed, most significant first.
template <typename... Modifiers>
using i16_be_ = integer_<std::int16_t, 2, byte_order::big, Modifiers...>;
/// @brief Three bytes, unsigned, least significant first.
template <typename... Modifiers>
using u24_le_ = integer_<std::uint32_t, 3, byte_order::little, Modifiers...>;
/// @brief Three bytes, unsigned, most significant first.
template <typename... Modifiers>
using u24_be_ = integer_<std::uint32_t, 3, byte_order::big, Modifiers...>;
/// @brief Three bytes, signed, least significant first.
template <typename... Modifiers>
using i24_le_ = integer_<std::int32_t, 3, byte_order::little, Modifiers...>;
/// @brief Three bytes, signed, most significant first.
template <typename... Modifiers>
using i24_be_ = integer_<std::int32_t, 3, byte_order::big, Modifiers...>;
/// @brief Four bytes, unsigned, least significant first.
template <typename... Modifiers>
using u32_le_ = integer_<std::uint32_t, 4, byte_order::little, Modifiers...>;
/// @brief Four bytes, unsigned, most significant first.
template <typename... Modifiers>
using u32_be_ = integer_<std::uint32_t, 4, byte_order::big, Modifiers...>;
/// @brief Four bytes, signed, least significant first.
template <typename... Modifiers>
using i32_le_ = integer_<std::int32_t, 4, byte_order::little, Modifiers...>;
/// @brief Four bytes, signed, most significant first.
template <typename... Modifiers>
using i32_be_ = integer_<std::int32_t, 4, byte_order::big, Modifiers...>;
/// @brief Eight bytes, unsigned, least significant first.
template <typename... Modifiers>
using u64_le_ = integer_<std::uint64_t, 8, byte_order::little, Modifiers...>;
/// @brief Eight bytes, unsigned, most significant first.
template <typename... Modifiers>
using u64_be_ = integer_<std::uint64_t, 8, byte_order::big, Modifiers...>;
/// @brief Eight bytes, signed, least significant first.
template <typename... Modifiers>
using i64_le_ = integer_<std::int64_t, 8, byte_order::little, Modifiers...>;
/// @brief Eight bytes, signed, most significant first.
template <typename... Modifiers>
using i64_be_ = integer_<std::int64_t, 8, byte_order::big, Modifiers...>;
/// @brief A 4-byte IEEE 754 number, least significant byte first.
template <typename... Modifiers>
using f32_le_ = floating_<float, byte_order::little, Modifiers...>;
/// @brief A 4-byte IEEE 754 number, most significant byte first.
template <typename... Modifiers>
using f32_be_ = floating_<float, byte_order::big, Modifiers...>;
/// @brief An 8-byte IEEE 754 number, least significant byte first.
template <typename... Modifiers>
using f64_le_ = floating_<double, byte_order::little, Modifiers...>;
/// @brief An 8-byte IEEE 754 number, most significant byte first.
template <typename... Modifiers>
using f64_be_ = floating_<double, byte_order::big, Modifiers...>;
} // namespace field
} // namespace protocol
} // namespace core
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_PROTOCOL_FIELD_INTEGER_FIELDS_HPP
