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
 * @brief The integer fields the DSL core needs: `integer_<Raw, Order,
 * Modifiers...>`, its names `u8_`, `u16_le_`, `u16_be_`, and the modifier
 * `scale<Ratio>`.
 * @details These describe a field (size, byte order, scale); encoding and
 * decoding, the other integer widths, signed and floating fields and units
 * come with the field set of the protocol layer. A plain field is written
 * `u8_<>`, a scaled one `u16_le_<scale<std::ratio<1, 100> > >` (the
 * engineering value is the raw value times the ratio).
 */
#ifndef LUMEX_CORE_PROTOCOL_FIELD_INTEGER_FIELDS_HPP
#define LUMEX_CORE_PROTOCOL_FIELD_INTEGER_FIELDS_HPP

#include <cstddef>
#include <cstdint>
#include <ratio>
#include <type_traits>

#include "lumex/core/framing/codec/LumexFramingCodec.hpp"
#include "lumex/core/protocol/field/LumexProtocolFieldTraits.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

namespace lumex
{
namespace core
{
namespace protocol
{
namespace field
{
/// @brief The byte order of a multi-byte field (the type of the framing
/// codecs).
typedef ::lumex::core::framing::codec::byte_order byte_order;

/**
 * @brief Field modifier: the engineering value is the raw value times
 * `Ratio` (a `std::ratio`).
 * @tparam Ratio For example `std::ratio<1, 100>` (hundredths).
 */
template <typename Ratio> struct scale
{
  /// Marks the type as a field modifier.
  typedef modifier_tag lumex_protocol_modifier;
  /// The factor, reduced (`std::ratio<2, 200>` is `std::ratio<1, 100>`).
  typedef typename Ratio::type ratio;
};

namespace detail
{
template <typename T> struct is_scale : std::false_type
{
};

template <typename Ratio> struct is_scale<scale<Ratio>> : std::true_type
{
};

// The ratio of the first scale<> among the modifiers, std::ratio<1> without
// one.
template <typename... Modifiers> struct scale_of
{
  typedef std::ratio<1> type;
};

template <typename Head, typename... Tail>
struct scale_of<Head, Tail...> : scale_of<Tail...>
{
};

template <typename Ratio, typename... Tail>
struct scale_of<scale<Ratio>, Tail...>
{
  typedef typename scale<Ratio>::ratio type;
};

template <bool... B> struct bool_pack
{
};

template <bool... B>
struct all_true : std::is_same<bool_pack<true, B...>, bool_pack<B..., true>>
{
};

template <typename... T> struct count_scales;

template <> struct count_scales<> : std::integral_constant<std::size_t, 0>
{
};

template <typename Head, typename... Tail>
struct count_scales<Head, Tail...>
    : std::integral_constant<std::size_t, (is_scale<Head>::value ? 1u : 0u)
                                              + count_scales<Tail...>::value>
{
};
} // namespace detail

/**
 * @brief An unsigned or signed integer field of `sizeof (Raw)` bytes.
 * @tparam Raw The integer type of the wire value.
 * @tparam Order The byte order on the wire.
 * @tparam Modifiers At most one `scale<Ratio>` (more modifiers come with
 * units).
 */
template <typename Raw, byte_order Order, typename... Modifiers>
struct integer_
{
  static_assert (std::is_integral<Raw>::value,
                 "integer_: Raw must be an integer type");
  static_assert (detail::all_true<is_modifier<Modifiers>::value...>::value,
                 "integer_: every argument after the byte order must be a "
                 "field modifier such as scale<std::ratio<1, 100> >");
  static_assert (detail::count_scales<Modifiers...>::value <= 1,
                 "integer_: give scale<> at most once");

  /// Marks the type as a field.
  typedef field_tag lumex_protocol_field;
  /// The wire value.
  typedef Raw raw_type;
  /// The factor of the engineering value (`std::ratio<1>` without a scale).
  typedef typename detail::scale_of<Modifiers...>::type scale_type;

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
};

/// @brief One unsigned byte.
template <typename... Modifiers>
using u8_ = integer_<std::uint8_t, byte_order::little, Modifiers...>;

/// @brief Two bytes, unsigned, least significant first.
template <typename... Modifiers>
using u16_le_ = integer_<std::uint16_t, byte_order::little, Modifiers...>;

/// @brief Two bytes, unsigned, most significant first.
template <typename... Modifiers>
using u16_be_ = integer_<std::uint16_t, byte_order::big, Modifiers...>;
} // namespace field
} // namespace protocol
} // namespace core
} // namespace lumex

#endif // !LUMEX_CORE_PROTOCOL_FIELD_INTEGER_FIELDS_HPP
