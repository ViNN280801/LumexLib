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
 * @file LumexProtocolCompositeFields.hpp
 * @brief The fields made of other fields: `array_<T, N>`, `counted_<T,
 * CountField, Max>`, `computed_count_<T, Expr, Max>` with `count_from_<I>`,
 * `optional_tail_<F...>`, `by_length_<Min, A, B>`, `union_by_<Discriminant,
 * case_<V, L>...>` with `field_<I>` and `at_end_<D>`,
 * `union_by_context_<Key, case_<V, L>...>` and `nested_<Protocol, KeyField>`.
 * @details No count read from a message allocates: `counted_` and
 * `computed_count_` hold at most `Max` elements, and a larger count is
 * `limit_exceeded`. The fields that depend on the length of the message
 * (`optional_tail_`, `by_length_`, `union_by_<at_end_<D>, ...>`, `nested_`)
 * must be the last field of their group; the check is at compile time. A
 * field may depend on an earlier field of the same group (`count_from_<I>`,
 * `field_<I>`: the index in that group).
 */
#ifndef LUMEX_CORE_PROTOCOL_FIELD_COMPOSITE_FIELDS_HPP
#define LUMEX_CORE_PROTOCOL_FIELD_COMPOSITE_FIELDS_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

#include "lumex/core/protocol/error/LumexProtocolErrc.hpp"
#include "lumex/core/protocol/field/LumexProtocolContext.hpp"
#include "lumex/core/protocol/field/LumexProtocolFieldTraits.hpp"
#include "lumex/core/protocol/field/LumexProtocolIntegerFields.hpp"
#include "lumex/core/protocol/field/LumexProtocolNumeric.hpp"
#include "lumex/core/protocol/field/LumexProtocolRecord.hpp"
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
template <typename... F> struct field_pack
{
};

/// The largest value of an array field: values live on the stack of the
/// caller (a record is returned by value), so a capacity that makes a value
/// larger is refused at compile time.
LUMEX_CONSTEXPR std::size_t array_value_limit = 65536;

template <typename T, typename = void> struct is_indexable : std::false_type
{
};

template <typename T>
struct is_indexable<
    T, typename voider<decltype (std::declval<T const &> ().size (),
                                 std::declval<T const &> ()[0])>::type>
    : std::true_type
{
};

// Copies the elements of a container argument into the elements of an
// array value through the element field's assign.
template <typename ElementImpl, typename Value, typename Arg>
bool
assign_elements (Value *values, std::size_t capacity, std::size_t &count,
                 Arg const &argument)
{
  static_assert (is_indexable<Arg>::value,
                 "LUMEX_PROTOCOL: an array field takes its value type or a "
                 "container with size () and operator[]");
  std::size_t const size = static_cast<std::size_t> (argument.size ());
  if (size > capacity)
    return false;
  for (std::size_t i = 0; i < size; ++i)
    if (!ElementImpl::assign (values[i], argument[i]))
      return false;
  count = size;
  return true;
}

// The count a field value gives: an integer, or the raw value of a fixed
// point; variable_size for a negative value.
template <typename T>
std::size_t
to_count (T const &value) LUMEX_NOEXCEPT
{
  std::int64_t const raw = to_discriminant (value);
  return raw < 0 ? variable_size : static_cast<std::size_t> (raw);
}

// Sets a field value from a discriminant and tells whether it holds it.
template <typename V>
bool
from_discriminant (V &value, std::int64_t discriminant) LUMEX_NOEXCEPT
{
  using raw_type = typename raw_access<V>::raw_type;
  raw_access<V>::set (value, static_cast<raw_type> (discriminant));
  return to_discriminant (value) == discriminant;
}
} // namespace detail

/// @brief `N` elements of field `T` (a field or a layout).
template <typename T, std::size_t N> struct array_
{
  static_assert (N > 0, "array_<T, N>: N must be positive");

  /// The element implementation.
  using element_impl = typename impl_of<T>::type;
  /// The element value.
  using element_value = typename element_impl::value_type;
  /// Marks the type as a field.
  using lumex_protocol_field = field_tag;
  /// The value.
  using value_type = std::array<element_value, N>;
  static_assert (sizeof (value_type) <= detail::array_value_limit,
                 "LUMEX_PROTOCOL: array_<T, N>: the value is larger than 64 "
                 "KiB (it lives on the stack); use a smaller N or split the "
                 "message");
  /// `N` times the element, or `variable_size`.
  using binary_size = std::integral_constant<
      std::size_t, element_impl::binary_size::value == variable_size
                       ? variable_size
                       : N * element_impl::binary_size::value>;
  /// The alignment of the element.
  using binary_alignment = typename element_impl::binary_alignment;

  /// @brief The descriptor (`count` is `N`).
  static LUMEX_CONSTEXPR field_info_t
  info () LUMEX_NOEXCEPT
  {
    return detail::make_info (field_kind::array,
                              detail::info_size (binary_size::value),
                              byte_order::little, false, 1, 1, "", N);
  }

  /// @brief Reads the elements.
  template <bool Last, typename Reader, typename Siblings>
  static bool
  decode (Reader &reader, value_type &value, Siblings const &siblings)
  {
    for (std::size_t i = 0; i < N; ++i)
      if (!element_impl::template decode<false> (reader, value[i], siblings))
        return false;
    return true;
  }

  /// @brief Writes the elements.
  template <bool Last, typename Writer, typename Siblings>
  static bool
  encode (Writer &writer, value_type const &value, Siblings const &siblings)
  {
    for (std::size_t i = 0; i < N; ++i)
      if (!element_impl::template encode<false> (writer, value[i], siblings))
        return false;
    return true;
  }

  /// @brief Sets `value` from its value type or a container of `N`
  /// elements.
  template <typename Arg>
  static bool
  assign (value_type &value, Arg const &argument)
  {
    return assign_value (value, argument, std::is_same<Arg, value_type> ());
  }

private:
  static bool
  assign_value (value_type &value, value_type const &argument, std::true_type)
  {
    value = argument;
    return true;
  }

  template <typename Arg>
  static bool
  assign_value (value_type &value, Arg const &argument, std::false_type)
  {
    std::size_t count = 0;
    return detail::assign_elements<element_impl> (value.data (), N, count,
                                                  argument)
           && count == N;
  }
};

namespace detail
{
// The element part of counted_ and computed_count_.
template <typename T, std::size_t Max> struct bounded_elements
{
  static_assert (Max > 0, "the Max of an array field must be positive");

  using element_impl = typename impl_of<T>::type;
  using element_value = typename element_impl::value_type;
  using value_type = bounded_array<element_value, Max>;
  static_assert (sizeof (value_type) <= array_value_limit,
                 "LUMEX_PROTOCOL: counted_ / computed_count_: Max makes the "
                 "value larger than 64 KiB (it lives on the stack); use a "
                 "smaller Max or split the message");

  template <typename Reader, typename Siblings>
  static bool
  decode_elements (Reader &reader, value_type &value, std::size_t count,
                   Siblings const &siblings)
  {
    if (count == variable_size)
      return reader.fail (error::protocol_errc::invalid_value);
    if (count > Max)
      return reader.fail (error::protocol_errc::limit_exceeded);
    for (std::size_t i = 0; i < count; ++i)
      if (!element_impl::template decode<false> (reader, value.items[i],
                                                 siblings))
        return false;
    value.count = count;
    return true;
  }

  template <typename Writer, typename Siblings>
  static bool
  encode_elements (Writer &writer, value_type const &value,
                   Siblings const &siblings)
  {
    if (value.count > Max)
      return writer.fail (error::protocol_errc::invalid_value);
    for (std::size_t i = 0; i < value.count; ++i)
      if (!element_impl::template encode<false> (writer, value.items[i],
                                                 siblings))
        return false;
    return true;
  }

  template <typename Arg>
  static bool
  assign (value_type &value, Arg const &argument)
  {
    return assign_value (value, argument, std::is_same<Arg, value_type> ());
  }

  static bool
  assign_value (value_type &value, value_type const &argument, std::true_type)
  {
    if (argument.count > Max)
      return false;
    value = argument;
    return true;
  }

  template <typename Arg>
  static bool
  assign_value (value_type &value, Arg const &argument, std::false_type)
  {
    return assign_elements<element_impl> (value.items, Max, value.count,
                                          argument);
  }
};
} // namespace detail

/**
 * @brief A count (field `CountField`, an integer), then that many elements of
 * field `T`; at most `Max`.
 * @details The value is a `bounded_array<..., Max>`; a count above `Max` is
 * `limit_exceeded` on decode, a negative one `invalid_value`. On encode the
 * count is the size of the value.
 */
template <typename T, typename CountField, std::size_t Max> struct counted_
{
  /// The elements.
  using elements = detail::bounded_elements<T, Max>;
  /// The count field implementation.
  using count_impl = typename impl_of<CountField>::type;
  /// The value of the count field.
  using count_value = typename count_impl::value_type;
  static_assert (std::is_integral<count_value>::value,
                 "counted_<T, CountField, Max>: CountField must hold an "
                 "integer (no scale<>)");

  /// Marks the type as a field.
  using lumex_protocol_field = field_tag;
  /// The value.
  using value_type = typename elements::value_type;
  /// The size depends on the message.
  using binary_size = std::integral_constant<std::size_t, variable_size>;
  /// The alignment of the count field.
  using binary_alignment = typename count_impl::binary_alignment;

  /// @brief The descriptor (`count` is `Max`).
  static LUMEX_CONSTEXPR field_info_t
  info () LUMEX_NOEXCEPT
  {
    return detail::make_info (field_kind::counted, 0, byte_order::little,
                              false, 1, 1, "", Max);
  }

  /// @brief Reads the count and the elements.
  template <bool Last, typename Reader, typename Siblings>
  static bool
  decode (Reader &reader, value_type &value, Siblings const &siblings)
  {
    count_value count = count_value ();
    if (!count_impl::template decode<false> (reader, count, siblings))
      return false;
    return elements::decode_elements (reader, value, detail::to_count (count),
                                      siblings);
  }

  /// @brief Writes the count and the elements.
  template <bool Last, typename Writer, typename Siblings>
  static bool
  encode (Writer &writer, value_type const &value, Siblings const &siblings)
  {
    count_value count = count_value ();
    if (value.count > Max || !count_impl::assign (count, value.count))
      return writer.fail (error::protocol_errc::invalid_value);
    return count_impl::template encode<false> (writer, count, siblings)
           && elements::encode_elements (writer, value, siblings);
  }

  /// @brief Sets `value` from its value type or a container of at most
  /// `Max` elements.
  template <typename Arg>
  static bool
  assign (value_type &value, Arg const &argument)
  {
    return elements::assign (value, argument);
  }
};

/**
 * @brief Elements of field `T` whose number `Expr::count (siblings)`
 * computes from the earlier fields of the group; at most `Max`.
 * @details `Expr` is a type with `template <class Siblings> static
 * std::size_t count (Siblings const &)` reading the earlier fields with
 * `field::get<I>`; returning `field::variable_size` means the earlier fields
 * are invalid (`invalid_value`). `count_from_<I>` takes the value of field
 * `I`. On encode the size of the value must equal the computed count.
 */
template <typename T, typename Expr, std::size_t Max> struct computed_count_
{
  /// The elements.
  using elements = detail::bounded_elements<T, Max>;

  /// Marks the type as a field.
  using lumex_protocol_field = field_tag;
  /// The value.
  using value_type = typename elements::value_type;
  /// The size depends on the message.
  using binary_size = std::integral_constant<std::size_t, variable_size>;
  /// The alignment of the element.
  using binary_alignment = typename elements::element_impl::binary_alignment;

  /// @brief The descriptor (`count` is `Max`).
  static LUMEX_CONSTEXPR field_info_t
  info () LUMEX_NOEXCEPT
  {
    return detail::make_info (field_kind::computed_count, 0,
                              byte_order::little, false, 1, 1, "", Max);
  }

  /// @brief Reads the computed number of elements.
  template <bool Last, typename Reader, typename Siblings>
  static bool
  decode (Reader &reader, value_type &value, Siblings const &siblings)
  {
    return elements::decode_elements (reader, value, Expr::count (siblings),
                                      siblings);
  }

  /// @brief Writes the elements; `invalid_value` when their number is not
  /// the computed one.
  template <bool Last, typename Writer, typename Siblings>
  static bool
  encode (Writer &writer, value_type const &value, Siblings const &siblings)
  {
    if (Expr::count (siblings) != value.count)
      return writer.fail (error::protocol_errc::invalid_value);
    return elements::encode_elements (writer, value, siblings);
  }

  /// @brief Sets `value` from its value type or a container.
  template <typename Arg>
  static bool
  assign (value_type &value, Arg const &argument)
  {
    return elements::assign (value, argument);
  }
};

/// @brief The `Expr` of `computed_count_` that takes the value of field `I`
/// of the same group (an integer, or the raw value of a fixed point).
template <std::size_t I> struct count_from_
{
  /// @brief The value of field `I`; `variable_size` when negative.
  template <typename Siblings>
  static std::size_t
  count (Siblings const &siblings) LUMEX_NOEXCEPT
  {
    return detail::to_count (get<I> (siblings));
  }
};

/**
 * @brief Fields `F...` present when the message goes on after the fields
 * before them (a tail of newer firmware); must be the last field.
 * @details Absent when nothing is left (`present` is false); a tail that
 * starts but ends early is `truncated`. On encode the tail is written when
 * `present` is true.
 */
template <typename... F> struct optional_tail_
{
  static_assert (sizeof...(F) > 0, "optional_tail_: give at least one field");

  /// The fields of the tail.
  using group = group_impl<detail::field_pack<F...>>;
  /// Marks the type as a field.
  using lumex_protocol_field = field_tag;
  /// The value.
  using value_type = tail_value<record<F...>>;
  /// The size depends on the message.
  using binary_size = std::integral_constant<std::size_t, variable_size>;
  /// The alignment of the tail.
  using binary_alignment = typename group::binary_alignment;

  /// @brief The descriptor (`count` is the number of fields).
  static LUMEX_CONSTEXPR field_info_t
  info () LUMEX_NOEXCEPT
  {
    return detail::make_info (field_kind::optional_tail, 0, byte_order::little,
                              false, 1, 1, "", sizeof...(F));
  }

  /// @brief Reads the tail when the message goes on.
  template <bool Last, typename Reader, typename Siblings>
  static bool
  decode (Reader &reader, value_type &value, Siblings const &siblings)
  {
    static_assert (Last, "LUMEX_PROTOCOL: optional_tail_ must be the last "
                         "field of its group");
    value.present = reader.remaining_units () != 0;
    if (!value.present)
      return true;
    return group::template decode<Last> (reader, value.fields, siblings);
  }

  /// @brief Writes the tail when it is present.
  template <bool Last, typename Writer, typename Siblings>
  static bool
  encode (Writer &writer, value_type const &value, Siblings const &siblings)
  {
    static_assert (Last, "LUMEX_PROTOCOL: optional_tail_ must be the last "
                         "field of its group");
    if (!value.present)
      return true;
    return group::template encode<Last> (writer, value.fields, siblings);
  }

  /// @brief Sets `value` from its value type, or from the record of the
  /// tail (present).
  template <typename Arg>
  static bool
  assign (value_type &value, Arg const &argument)
  {
    return assign_value (value, argument, std::is_same<Arg, value_type> ());
  }

private:
  static bool
  assign_value (value_type &value, value_type const &argument, std::true_type)
  {
    value = argument;
    return true;
  }

  template <typename Arg>
  static bool
  assign_value (value_type &value, Arg const &argument, std::false_type)
  {
    static_assert (std::is_same<Arg, record<F...>>::value,
                   "LUMEX_PROTOCOL: optional_tail_ takes its tail_value or "
                   "the record of its fields");
    value.present = true;
    value.fields = argument;
    return true;
  }
};

/**
 * @brief Field `A` when fewer than `Min` units are left, field `B` when at
 * least `Min` are; must be the last field.
 * @details The unit is that of the layout: bytes in a binary layout,
 * characters in a fixed-width text layout (an integer of two bytes is four
 * hexadecimal characters), tokens in a tokenized one. `Min` therefore
 * belongs to one layout: a layer that binds the row to a layout with
 * another unit restates the row with its own `Min`. The value is
 * `alternatives<value of A, value of B>`: index 0 for `A`, 1 for `B`. On
 * encode the form written must land on its side of `Min`
 * (`invalid_value` otherwise), so that the message decodes to the same
 * form.
 */
template <std::size_t Min, typename A, typename B> struct by_length_
{
  /// The short form.
  using short_impl = typename impl_of<A>::type;
  /// The long form.
  using long_impl = typename impl_of<B>::type;
  /// Marks the type as a field.
  using lumex_protocol_field = field_tag;
  /// The value.
  using value_type = alternatives<typename short_impl::value_type,
                                  typename long_impl::value_type>;
  /// The size depends on the message.
  using binary_size = std::integral_constant<std::size_t, variable_size>;
  /// The larger alignment of the two.
  using binary_alignment
      = std::integral_constant<std::size_t,
                               (short_impl::binary_alignment::value
                                > long_impl::binary_alignment::value)
                                   ? short_impl::binary_alignment::value
                                   : long_impl::binary_alignment::value>;

  /// @brief The descriptor (`count` is `Min`).
  static LUMEX_CONSTEXPR field_info_t
  info () LUMEX_NOEXCEPT
  {
    return detail::make_info (field_kind::by_length, 0, byte_order::little,
                              false, 1, 1, "", Min);
  }

  /// @brief Reads the form the length chooses.
  template <bool Last, typename Reader, typename Siblings>
  static bool
  decode (Reader &reader, value_type &value, Siblings const &siblings)
  {
    static_assert (Last, "LUMEX_PROTOCOL: by_length_ must be the last field "
                         "of its group");
    if (reader.remaining_units () < Min)
      return short_impl::template decode<Last> (
          reader, value.template emplace<0> (), siblings);
    return long_impl::template decode<Last> (
        reader, value.template emplace<1> (), siblings);
  }

  /// @brief Writes the form the value holds.
  template <bool Last, typename Writer, typename Siblings>
  static bool
  encode (Writer &writer, value_type const &value, Siblings const &siblings)
  {
    static_assert (Last, "LUMEX_PROTOCOL: by_length_ must be the last field "
                         "of its group");
    std::size_t const start = writer.units_written ();
    bool const ok = value.index () == 0
                        ? short_impl::template encode<Last> (
                              writer, value.template get<0> (), siblings)
                        : long_impl::template encode<Last> (
                              writer, value.template get<1> (), siblings);
    if (!ok)
      return false;
    bool const is_long = writer.units_written () - start >= Min;
    if (is_long != (value.index () == 1))
      return writer.fail (error::protocol_errc::invalid_value);
    return true;
  }

  /// @brief Sets `value` from its value type.
  template <typename Arg>
  static bool
  assign (value_type &value, Arg const &argument)
  {
    static_assert (std::is_same<Arg, value_type>::value,
                   "LUMEX_PROTOCOL: by_length_ is given as its alternatives");
    value = argument;
    return true;
  }
};

/// @brief A case of `union_by_` and `union_by_context_`: field (layout) `L`
/// when the discriminant is `Value`.
template <std::int64_t Value, typename L> struct case_
{
  /// The discriminant of the case.
  static LUMEX_CONSTEXPR std::int64_t
  value () LUMEX_NOEXCEPT
  {
    return Value;
  }

  /// The field of the case.
  using layout_type = L;
};

/// @brief The discriminant of `union_by_` is field `I` of the same group,
/// read before the union.
template <std::size_t I> struct field_
{
};

/// @brief The discriminant of `union_by_` is integer field `D` at the end of
/// the message, after the case (the union is then the last field).
template <typename D> struct at_end_
{
  /// The discriminant field.
  using field_type = D;
};

namespace detail
{
template <typename T> struct is_case : std::false_type
{
};

template <std::int64_t Value, typename L>
struct is_case<case_<Value, L>> : std::true_type
{
};

// Decodes and encodes the case at a run-time position among Cases.
template <std::size_t K, typename... Cases> struct case_switch;

template <std::size_t K> struct case_switch<K>
{
  template <bool Last, typename Reader, typename Value, typename Siblings>
  static bool
  decode (Reader &reader, Value &, std::int64_t, Siblings const &)
  {
    return reader.fail (error::protocol_errc::unknown_case);
  }

  template <bool Last, typename Writer, typename Value, typename Siblings>
  static bool
  encode (Writer &writer, Value const &, Siblings const &)
  {
    return writer.fail (error::protocol_errc::invalid_value);
  }

  static bool
  value_at (std::size_t, std::int64_t &) LUMEX_NOEXCEPT
  {
    return false;
  }
};

template <std::size_t K, typename Head, typename... Tail>
struct case_switch<K, Head, Tail...>
{
  using impl = typename impl_of<typename Head::layout_type>::type;

  template <bool Last, typename Reader, typename Value, typename Siblings>
  static bool
  decode (Reader &reader, Value &value, std::int64_t discriminant,
          Siblings const &siblings)
  {
    if (discriminant != Head::value ())
      return case_switch<K + 1, Tail...>::template decode<Last> (
          reader, value, discriminant, siblings);
    return impl::template decode<Last> (
        reader, value.cases.template emplace<K> (), siblings);
  }

  template <bool Last, typename Writer, typename Value, typename Siblings>
  static bool
  encode (Writer &writer, Value const &value, Siblings const &siblings)
  {
    if (value.cases.index () != K)
      return case_switch<K + 1, Tail...>::template encode<Last> (writer, value,
                                                                 siblings);
    return impl::template encode<Last> (writer, value.cases.template get<K> (),
                                        siblings);
  }

  static bool
  value_at (std::size_t index, std::int64_t &discriminant) LUMEX_NOEXCEPT
  {
    if (index != K)
      return case_switch<K + 1, Tail...>::value_at (index, discriminant);
    discriminant = Head::value ();
    return true;
  }
};

// Where union_by_ finds its discriminant: a leading field, a field at the
// end, an earlier field of the group.
template <typename D> struct discriminant_source
{
  using impl = typename impl_of<D>::type;
  using value_type = typename impl::value_type;
  using binary_alignment = typename impl::binary_alignment;

  template <bool Last, typename Reader, typename Siblings>
  static bool
  read (Reader &reader, std::int64_t &discriminant, Siblings const &siblings)
  {
    value_type value = value_type ();
    if (!impl::template decode<false> (reader, value, siblings))
      return false;
    discriminant = to_discriminant (value);
    return true;
  }

  template <typename Writer, typename Siblings>
  static bool
  write_before (Writer &writer, std::int64_t discriminant,
                Siblings const &siblings)
  {
    value_type value = value_type ();
    if (!from_discriminant (value, discriminant))
      return writer.fail (error::protocol_errc::invalid_value);
    return impl::template encode<false> (writer, value, siblings);
  }

  template <typename Writer, typename Siblings>
  static bool
  write_after (Writer &, std::int64_t, Siblings const &)
  {
    return true;
  }
};

template <typename D> struct discriminant_source<at_end_<D>>
{
  using impl = typename impl_of<D>::type;
  using value_type = typename impl::value_type;
  using binary_alignment = typename impl::binary_alignment;
  static_assert (is_plain_integer<impl>::value,
                 "LUMEX_PROTOCOL: the discriminant at_end_<D> is an integer "
                 "field without scale<>");

  template <bool Last, typename Reader, typename Siblings>
  static bool
  read (Reader &reader, std::int64_t &discriminant, Siblings const &)
  {
    static_assert (Last, "LUMEX_PROTOCOL: union_by_<at_end_<D>, ...> must be "
                         "the last field of its group");
    std::uint64_t bits = 0;
    if (!reader.read_last_integer (impl::binary_size::value, impl::order (),
                                   std::is_signed<value_type>::value, bits))
      return false;
    discriminant = static_cast<std::int64_t> (impl::from_bits (bits));
    return true;
  }

  template <typename Writer, typename Siblings>
  static bool
  write_before (Writer &, std::int64_t, Siblings const &)
  {
    return true;
  }

  template <typename Writer, typename Siblings>
  static bool
  write_after (Writer &writer, std::int64_t discriminant,
               Siblings const &siblings)
  {
    value_type value = value_type ();
    if (!from_discriminant (value, discriminant))
      return writer.fail (error::protocol_errc::invalid_value);
    return impl::template encode<true> (writer, value, siblings);
  }
};

template <std::size_t I> struct discriminant_source<field_<I>>
{
  using binary_alignment = std::integral_constant<std::size_t, 1>;

  template <bool Last, typename Reader, typename Siblings>
  static bool
  read (Reader &, std::int64_t &discriminant, Siblings const &siblings)
  {
    discriminant = to_discriminant (get<I> (siblings));
    return true;
  }

  template <typename Writer, typename Siblings>
  static bool
  write_before (Writer &writer, std::int64_t discriminant,
                Siblings const &siblings)
  {
    if (to_discriminant (get<I> (siblings)) != discriminant)
      return writer.fail (error::protocol_errc::invalid_value);
    return true;
  }

  template <typename Writer, typename Siblings>
  static bool
  write_after (Writer &, std::int64_t, Siblings const &)
  {
    return true;
  }
};

template <typename... Cases> struct union_cases
{
  static_assert (sizeof...(Cases) > 0, "give at least one case_<V, L>");
  static_assert (values_all<is_case<Cases>::value...>::value,
                 "LUMEX_PROTOCOL: the cases of a union are case_<value, "
                 "layout>");

  using alternatives_type = alternatives<
      typename impl_of<typename Cases::layout_type>::type::value_type...>;
  using value_type = union_value<alternatives_type>;
  using switch_type = case_switch<0, Cases...>;
};
} // namespace detail

/**
 * @brief One of several layouts chosen by a discriminant: `union_by_<u8_,
 * case_<1, a_t>, case_<2, b_t> >`.
 * @details The discriminant is a field read before the case (`union_by_<D,
 * ...>`), an earlier field of the same group (`union_by_<field_<I>,
 * ...>`), or an integer field at the end of the message
 * (`union_by_<at_end_<D>, ...>`). The value is a `union_value`: the
 * discriminant and the case (`cases.index ()` is the position of the case).
 * A discriminant no case has is `unknown_case`. On encode the discriminant
 * is the value of the case held; with `field_<I>` field `I` must already
 * hold it (`invalid_value` otherwise).
 */
template <typename Discriminant, typename... Cases> struct union_by_
{
  /// The cases.
  using cases = detail::union_cases<Cases...>;
  /// Where the discriminant comes from.
  using source = detail::discriminant_source<Discriminant>;

  /// Marks the type as a field.
  using lumex_protocol_field = field_tag;
  /// The value.
  using value_type = typename cases::value_type;
  /// The size depends on the case.
  using binary_size = std::integral_constant<std::size_t, variable_size>;
  /// The alignment of the discriminant.
  using binary_alignment = typename source::binary_alignment;

  /// @brief The descriptor (`count` is the number of cases).
  static LUMEX_CONSTEXPR field_info_t
  info () LUMEX_NOEXCEPT
  {
    return detail::make_info (field_kind::union_by, 0, byte_order::little,
                              false, 1, 1, "", sizeof...(Cases));
  }

  /// @brief Reads the discriminant and the case it chooses.
  template <bool Last, typename Reader, typename Siblings>
  static bool
  decode (Reader &reader, value_type &value, Siblings const &siblings)
  {
    std::int64_t discriminant = 0;
    if (!source::template read<Last> (reader, discriminant, siblings))
      return false;
    value.discriminant = discriminant;
    return cases::switch_type::template decode<Last> (reader, value,
                                                      discriminant, siblings);
  }

  /// @brief Writes the discriminant of the case held and the case.
  template <bool Last, typename Writer, typename Siblings>
  static bool
  encode (Writer &writer, value_type const &value, Siblings const &siblings)
  {
    std::int64_t discriminant = 0;
    if (!cases::switch_type::value_at (value.cases.index (), discriminant))
      return writer.fail (error::protocol_errc::invalid_value);
    return source::write_before (writer, discriminant, siblings)
           && cases::switch_type::template encode<Last> (writer, value,
                                                         siblings)
           && source::write_after (writer, discriminant, siblings);
  }

  /// @brief Sets `value` from its value type.
  template <typename Arg>
  static bool
  assign (value_type &value, Arg const &argument)
  {
    static_assert (std::is_same<Arg, value_type>::value,
                   "LUMEX_PROTOCOL: a union is given as its union_value");
    value = argument;
    return true;
  }
};

/**
 * @brief One of several layouts chosen by a session value:
 * `union_by_context_<device_type, case_<1, a_t>, case_<2, b_t> >` where
 * `device_type` is a `LUMEX_PROTOCOL_CONTEXT_KEY`.
 * @details The decoder reads the value from the `decode_context_t` it was
 * given; without one, or when the session does not know the key, the
 * message is `unknown_case`. On encode, a context that knows the key must
 * agree with the case held (`invalid_value` otherwise).
 */
template <typename Key, typename... Cases> struct union_by_context_
{
  /// The cases.
  using cases = detail::union_cases<Cases...>;

  /// Marks the type as a field.
  using lumex_protocol_field = field_tag;
  /// The value.
  using value_type = typename cases::value_type;
  /// The size depends on the case.
  using binary_size = std::integral_constant<std::size_t, variable_size>;
  /// No alignment of its own.
  using binary_alignment = std::integral_constant<std::size_t, 1>;

  /// @brief The descriptor (`count` is the number of cases).
  static LUMEX_CONSTEXPR field_info_t
  info () LUMEX_NOEXCEPT
  {
    return detail::make_info (field_kind::union_by_context, 0,
                              byte_order::little, false, 1, 1, "",
                              sizeof...(Cases));
  }

  /// @brief The key of the session value.
  static LUMEX_CONSTEXPR char const *
  key () LUMEX_NOEXCEPT
  {
    return Key::context_key ();
  }

  /// @brief Reads the case the session value chooses.
  template <bool Last, typename Reader, typename Siblings>
  static bool
  decode (Reader &reader, value_type &value, Siblings const &siblings)
  {
    std::int64_t discriminant = 0;
    decode_context_t const *const context = reader.context ();
    if (context == nullptr || !context->find (key (), discriminant))
      return reader.fail (error::protocol_errc::unknown_case);
    value.discriminant = discriminant;
    return cases::switch_type::template decode<Last> (reader, value,
                                                      discriminant, siblings);
  }

  /// @brief Writes the case held.
  template <bool Last, typename Writer, typename Siblings>
  static bool
  encode (Writer &writer, value_type const &value, Siblings const &siblings)
  {
    std::int64_t discriminant = 0;
    if (!cases::switch_type::value_at (value.cases.index (), discriminant))
      return writer.fail (error::protocol_errc::invalid_value);
    std::int64_t session = 0;
    decode_context_t const *const context = writer.context ();
    if (context != nullptr && context->find (key (), session)
        && session != discriminant)
      return writer.fail (error::protocol_errc::invalid_value);
    return cases::switch_type::template encode<Last> (writer, value, siblings);
  }

  /// @brief Sets `value` from its value type.
  template <typename Arg>
  static bool
  assign (value_type &value, Arg const &argument)
  {
    static_assert (std::is_same<Arg, value_type>::value,
                   "LUMEX_PROTOCOL: a union is given as its union_value");
    value = argument;
    return true;
  }
};

/**
 * @brief A message of another protocol inside this one: its key (field
 * `KeyField`), then the rest of the message (a tunnel); must be the last
 * field.
 * @details The value is a `nested_value`: the key and the bytes after it.
 * `layout::decode_nested<Protocol, Row> (value)` decodes the payload with
 * the inner protocol's layout once the key tells which row it is.
 */
template <typename Protocol, typename KeyField> struct nested_
{
  /// The inner protocol.
  using protocol_type = Protocol;
  /// The key field implementation.
  using key_impl = typename impl_of<KeyField>::type;

  /// Marks the type as a field.
  using lumex_protocol_field = field_tag;
  /// The value.
  using value_type = nested_value<typename key_impl::value_type>;
  /// The size depends on the message.
  using binary_size = std::integral_constant<std::size_t, variable_size>;
  /// The alignment of the key.
  using binary_alignment = typename key_impl::binary_alignment;

  /// @brief The descriptor.
  static LUMEX_CONSTEXPR field_info_t
  info () LUMEX_NOEXCEPT
  {
    return detail::make_info (field_kind::nested, 0, byte_order::little, false,
                              1, 1, "", 0);
  }

  /// @brief Reads the key and keeps the rest of the message.
  template <bool Last, typename Reader, typename Siblings>
  static bool
  decode (Reader &reader, value_type &value, Siblings const &siblings)
  {
    static_assert (Last, "LUMEX_PROTOCOL: nested_ must be the last field of "
                         "its group");
    if (!key_impl::template decode<false> (reader, value.key, siblings))
      return false;
    std::uint8_t const *data = nullptr;
    std::size_t size = 0;
    if (!reader.read_rest (data, size))
      return false;
    value.payload = bytes_view_t (data, size);
    return true;
  }

  /// @brief Writes the key and the payload.
  template <bool Last, typename Writer, typename Siblings>
  static bool
  encode (Writer &writer, value_type const &value, Siblings const &siblings)
  {
    static_assert (Last, "LUMEX_PROTOCOL: nested_ must be the last field of "
                         "its group");
    return key_impl::template encode<false> (writer, value.key, siblings)
           && writer.write_rest (value.payload.data (), value.payload.size ());
  }

  /// @brief Sets `value` from its value type.
  template <typename Arg>
  static bool
  assign (value_type &value, Arg const &argument)
  {
    static_assert (std::is_same<Arg, value_type>::value,
                   "LUMEX_PROTOCOL: nested_ is given as its nested_value");
    value = argument;
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

#endif // !LUMEX_CORE_PROTOCOL_FIELD_COMPOSITE_FIELDS_HPP
