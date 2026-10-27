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
 * @file LumexProtocolFieldTraits.hpp
 * @brief What makes a type a field of a protocol row: `field_tag`,
 * `is_field`, the modifier tag `modifier_tag` / `is_modifier`, the
 * implementation lookup `impl_of` / `value_of`, and the run-time descriptor
 * `field_info_t` of a field type.
 * @details A field type declares `typedef field_tag lumex_protocol_field;`
 * and is its own implementation, or names its implementation as
 * `lumex_field_impl` (the integer names), or is a group of fields with a
 * `fields_type` (a `LUMEX_PROTOCOL_LAYOUT`, the field group of a row), whose
 * implementation is the record codec of `LumexProtocolRecord.hpp`. A name of
 * the table vocabulary that is usable without `<>` (`u8_`, `char_`) is a
 * carrier type naming its field as `lumex_named_field`; `resolve` maps it to
 * the field. A modifier (`scale<Ratio>`, a unit) declares `typedef
 * modifier_tag lumex_protocol_modifier;`.
 *
 * The implementation of a field (`impl_of<F>::type`) provides `value_type`
 * (the decoded value), `binary_size` and `binary_alignment` (integral
 * constants; `binary_size` is `variable_size` when the size depends on the
 * message), the constant `info ()`, and the static templates
 * `decode<Last> (reader, value, siblings)`, `encode<Last> (writer, value,
 * siblings)` and `assign (value, argument)` that the layouts call (see
 * `layout/LumexProtocolLayoutBase.hpp` for the reader and writer
 * primitives).
 */
#ifndef LUMEX_CORE_PROTOCOL_FIELD_TRAITS_HPP
#define LUMEX_CORE_PROTOCOL_FIELD_TRAITS_HPP

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "lumex/core/utility/bit/LumexByteOrder.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#endif

namespace lumex
{
namespace core
{
namespace protocol
{
namespace field
{
/// @brief The byte order of a multi-byte field (the type the framing codecs
/// use too).
typedef ::lumex::core::utility::bit::byte_order byte_order;

/// @brief The tag a field type names as `lumex_protocol_field`.
struct field_tag
{
};

/// @brief The tag a field modifier names as `lumex_protocol_modifier`.
struct modifier_tag
{
};

/// @brief `binary_size` of a field whose size depends on the message.
LUMEX_CONSTEXPR std::size_t variable_size = static_cast<std::size_t> (-1);

namespace detail
{
template <typename T> struct voider
{
  typedef void type;
};

template <typename T, typename = void> struct named_field
{
  typedef T type;
};

template <typename T>
struct named_field<T, typename voider<typename T::lumex_named_field>::type>
{
  typedef typename T::lumex_named_field type;
};

template <typename T, typename = void> struct has_field_tag : std::false_type
{
};

template <typename T>
struct has_field_tag<T,
                     typename voider<typename T::lumex_protocol_field>::type>
    : std::is_same<typename T::lumex_protocol_field, field_tag>
{
};
} // namespace detail

/**
 * @brief `resolve<T>::type`: the field a name of the table vocabulary stands
 * for (`u8_` written without `<>` is a carrier of `u8_<>`); any other type
 * is itself.
 */
template <typename T> struct resolve : detail::named_field<T>
{
};

/// @brief Whether `T` is a field type (it names `lumex_protocol_field`, or is
/// a vocabulary name of one).
template <typename T>
struct is_field : detail::has_field_tag<typename resolve<T>::type>
{
};

/// @brief Whether `T` is a field modifier (it names
/// `lumex_protocol_modifier`).
template <typename T, typename = void> struct is_modifier : std::false_type
{
};

/// @copydoc is_modifier
template <typename T>
struct is_modifier<
    T, typename detail::voider<typename T::lumex_protocol_modifier>::type>
    : std::is_same<typename T::lumex_protocol_modifier, modifier_tag>
{
};

/// @brief The record codec of a group of fields (defined in
/// `LumexProtocolRecord.hpp`).
template <typename FieldList> struct group_impl;

namespace detail
{
template <typename T, typename = void> struct has_impl : std::false_type
{
};

template <typename T>
struct has_impl<T, typename voider<typename T::lumex_field_impl>::type>
    : std::true_type
{
};

template <typename T, typename = void> struct has_fields : std::false_type
{
};

template <typename T>
struct has_fields<T, typename voider<typename T::fields_type>::type>
    : std::true_type
{
};

template <typename T, bool Impl, bool Fields> struct impl_select
{
  typedef T type;
};

template <typename T, bool Fields> struct impl_select<T, true, Fields>
{
  typedef typename T::lumex_field_impl type;
};

template <typename T> struct impl_select<T, false, true>
{
  typedef group_impl<typename T::fields_type> type;
};
} // namespace detail

/**
 * @brief `impl_of<F>::type`: the implementation of field `F` (see the file
 * description).
 */
template <typename F> struct impl_of
{
  /// The field the name stands for.
  typedef typename resolve<F>::type field_type;
  /// Its implementation.
  typedef typename detail::impl_select<
      field_type, detail::has_impl<field_type>::value,
      detail::has_fields<field_type>::value>::type type;
};

/// @brief `value_of<F>::type`: the decoded value of field `F`.
template <typename F> struct value_of
{
  /// The value type.
  typedef typename impl_of<F>::type::value_type type;
};

/// @brief What a field is, for the descriptors of the index and the export.
enum class field_kind : std::uint8_t
{
  /// An integer of 1 to 8 bytes (`u8_`, `i24_be_`, ...).
  integer,
  /// An IEEE 754 number (`f32_le_`, `f64_be_`).
  floating,
  /// An integer whose top bit is the sign (`sign_magnitude_<...>`).
  sign_magnitude,
  /// Hexadecimal digits of a fixed width (`ascii_hex_<W>`).
  hex_text,
  /// Decimal digits with a fixed point (`dec_text_<I, F>`).
  decimal_text,
  /// One character (`char_`).
  character,
  /// A text of a fixed capacity, NUL-padded (`text_<N>`).
  text,
  /// A token of a tokenized text, or the rest of a message (`token_`).
  token,
  /// A run of bits of a word (`bits_<...>`).
  bits,
  /// The bits of a mask of a word, compacted (`mask_<...>`).
  mask,
  /// An enumeration stored as an integer (`enum_<...>`).
  enumeration,
  /// A fixed number of elements (`array_<T, N>`).
  array,
  /// A count, then that many elements (`counted_<...>`).
  counted,
  /// A number of elements computed from earlier fields
  /// (`computed_count_<...>`).
  computed_count,
  /// Fields present only when the message goes on (`optional_tail_<...>`).
  optional_tail,
  /// One of two layouts chosen by the length (`by_length_<...>`).
  by_length,
  /// One of several layouts chosen by a discriminant (`union_by_<...>`).
  union_by,
  /// One of several layouts chosen by a session value
  /// (`union_by_context_<...>`).
  union_by_context,
  /// Several values packed into one word (`split_<...>`).
  split,
  /// A message of another protocol (`nested_<...>`).
  nested,
  /// A group of fields (a layout).
  group,
  /// The key of an entry of a registry (`registry::registry_key_<...>`).
  registry_key,
  /// The value of an entry of a registry, checked against the entry
  /// (`registry::registry_value_<...>`).
  registry_value
};

/**
 * @brief The descriptor of a field type: what the run-time index,
 * `describe` and the export need without the type
 * (`impl_of<F>::type::info ()`, a constant expression).
 */
struct field_info_t
{
  /// What the field is.
  field_kind kind;
  /// Bytes in a binary layout, 0 when the size depends on the message.
  std::size_t binary_size;
  /// The byte order (integer and floating fields).
  byte_order order;
  /// Whether the value is signed.
  bool is_signed;
  /// The engineering value is the raw value times `scale_num / scale_den`.
  std::intmax_t scale_num;
  /// See `scale_num`.
  std::intmax_t scale_den;
  /// The unit symbol (ASCII: `"uL"`, `"degC"`), `""` without a unit.
  char const *unit;
  /// By kind: the element count or capacity, the digit count, the bit
  /// count, the number of fields or cases; 0 when it does not apply.
  std::size_t count;
};

namespace detail
{
LUMEX_CONSTEXPR inline field_info_t
make_info (field_kind kind, std::size_t binary_size, byte_order order,
           bool is_signed, std::intmax_t scale_num, std::intmax_t scale_den,
           char const *unit, std::size_t count) LUMEX_NOEXCEPT
{
  return field_info_t{ kind,      binary_size, order, is_signed,
                       scale_num, scale_den,   unit,  count };
}

LUMEX_CONSTEXPR inline std::size_t
info_size (std::size_t binary_size) LUMEX_NOEXCEPT
{
  return binary_size == variable_size ? 0u : binary_size;
}
} // namespace detail
} // namespace field
} // namespace protocol
} // namespace core
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_PROTOCOL_FIELD_TRAITS_HPP
