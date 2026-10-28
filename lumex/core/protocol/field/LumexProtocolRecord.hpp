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
 * @file LumexProtocolRecord.hpp
 * @brief The decoded values of a group of fields: `record<F...>`, its access
 * `get<I> (record)`, the visit `visit_fields`, and the codec of a group
 * (`group_impl`) that decodes and encodes the fields in order.
 * @details The values of a row's request, of a layout and of a tail are a
 * `record` of the field types, one value per field (`value_of<F>::type`),
 * read with `get<I> (record)`. While a group is decoded, the record holds
 * the fields already read, so a field may depend on an earlier one of the
 * same group (`computed_count_`, `union_by_<field_<I>, ...>`).
 */
#ifndef LUMEX_CORE_PROTOCOL_FIELD_RECORD_HPP
#define LUMEX_CORE_PROTOCOL_FIELD_RECORD_HPP

#include <cstddef>
#include <type_traits>

#include "lumex/core/protocol/field/LumexProtocolFieldTraits.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"
#include "lumex/core/utility/sequence/LumexIndexSequence.hpp"

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
/// @brief A type passed to the visitor of `visit_fields`: the field type
/// `F` of the value.
template <typename F> struct field_type_tag
{
  /// The field type.
  using type = F;
};

namespace detail
{
template <std::size_t I, typename V> struct record_slot
{
  V value;
};

template <std::size_t I, typename V>
V &
get_slot (record_slot<I, V> &slot) LUMEX_NOEXCEPT
{
  return slot.value;
}

template <std::size_t I, typename V>
V const &
get_slot (record_slot<I, V> const &slot) LUMEX_NOEXCEPT
{
  return slot.value;
}

template <typename Sequence, typename... F> struct record_storage;

template <std::size_t... I, typename... F>
struct record_storage<::lumex::core::utility::sequence::index_sequence<I...>,
                      F...> : record_slot<I, typename value_of<F>::type>...
{
};

template <std::size_t I, typename... F> struct nth_field;

template <typename Head, typename... Tail> struct nth_field<0, Head, Tail...>
{
  using type = Head;
};

template <std::size_t I, typename Head, typename... Tail>
struct nth_field<I, Head, Tail...> : nth_field<I - 1, Tail...>
{
};
} // namespace detail

/**
 * @brief The values of the fields `F...`, one per field, in field order
 * (value-initialized: `record<F...> r = record<F...> ();`).
 */
template <typename... F>
struct record
    : detail::record_storage<
          ::lumex::core::utility::sequence::make_index_sequence<sizeof...(F)>,
          F...>
{
  /// The number of fields.
  using size = std::integral_constant<std::size_t, sizeof...(F)>;

  /// @brief The field type at `I`.
  template <std::size_t I> struct field_at : detail::nth_field<I, F...>
  {
  };
};

/// @brief The value of field `I` of a record.
template <std::size_t I, typename Record>
auto
get (Record &record) LUMEX_NOEXCEPT -> decltype (detail::get_slot<I> (record))
{
  return detail::get_slot<I> (record);
}

namespace detail
{
template <typename Sequence> struct record_compare;

template <std::size_t... I>
struct record_compare<::lumex::core::utility::sequence::index_sequence<I...>>
{
  template <typename Record>
  static bool
  equal (Record const &left, Record const &right)
  {
    bool same = true;
    bool const results[]
        = { true, (same = same && (get<I> (left) == get<I> (right)))... };
    (void)results;
    return same;
  }
};
} // namespace detail

/// @brief Equal when every field is equal.
template <typename... F>
bool
operator== (record<F...> const &left, record<F...> const &right)
{
  return detail::record_compare<
      ::lumex::core::utility::sequence::make_index_sequence<sizeof...(F)>>::
      equal (left, right);
}

/// @brief Not equal when a field differs.
template <typename... F>
bool
operator!= (record<F...> const &left, record<F...> const &right)
{
  return !(left == right);
}

namespace detail
{
template <typename Sequence, typename... F> struct record_visit;

template <std::size_t... I, typename... F>
struct record_visit<::lumex::core::utility::sequence::index_sequence<I...>,
                    F...>
{
  template <typename Record, typename Visitor>
  static void
  apply (Record &record, Visitor &visitor)
  {
    int const results[]
        = { 0, (visitor (I, field_type_tag<F> (), get<I> (record)), 0)... };
    (void)results;
  }
};

template <typename List> struct record_of_list;

template <template <typename...> class List, typename... F>
struct record_of_list<List<F...>>
{
  using type = record<F...>;
};
} // namespace detail

/**
 * @brief Calls `visitor (index, field_type_tag<F> (), value)` for every field
 * of `record` in order (what the decoded log line and the export walk).
 */
template <typename... F, typename Visitor>
void
visit_fields (record<F...> const &record, Visitor &&visitor)
{
  detail::record_visit<
      ::lumex::core::utility::sequence::make_index_sequence<sizeof...(F)>,
      F...>::apply (record, visitor);
}

/// @brief `record_of<List>::type`: the record of a field list
/// (`dsl::field_list<F...>`, `type_list<F...>`).
template <typename List> struct record_of : detail::record_of_list<List>
{
};

namespace detail
{
template <std::size_t... S> struct size_sum;

template <> struct size_sum<> : std::integral_constant<std::size_t, 0>
{
};

template <std::size_t Head, std::size_t... Tail>
struct size_sum<Head, Tail...>
    : std::integral_constant<std::size_t,
                             (Head == variable_size
                              || size_sum<Tail...>::value == variable_size)
                                 ? variable_size
                                 : Head + size_sum<Tail...>::value>
{
};

template <std::size_t... S> struct size_max;

template <> struct size_max<> : std::integral_constant<std::size_t, 1>
{
};

template <std::size_t Head, std::size_t... Tail>
struct size_max<Head, Tail...>
    : std::integral_constant<std::size_t, (Head > size_max<Tail...>::value)
                                              ? Head
                                              : size_max<Tail...>::value>
{
};

template <bool Last, typename Sequence, typename... F> struct record_codec;

template <bool Last, std::size_t... I, typename... F>
struct record_codec<
    Last, ::lumex::core::utility::sequence::index_sequence<I...>, F...>
{
  template <typename Reader, typename Record>
  static bool
  decode (Reader &reader, Record &record)
  {
    bool ok = true;
    bool const results[] = {
      true,
      (ok = ok && impl_of<F>::type::template decode < Last
            && I + 1 == sizeof...(F) > (reader, get<I> (record), record))...
    };
    (void)results;
    return ok;
  }

  template <typename Writer, typename Record>
  static bool
  encode (Writer &writer, Record const &record)
  {
    bool ok = true;
    bool const results[] = {
      true,
      (ok = ok && impl_of<F>::type::template encode < Last
            && I + 1 == sizeof...(F) > (writer, get<I> (record), record))...
    };
    (void)results;
    return ok;
  }
};
} // namespace detail

/**
 * @brief The codec of a group of fields: a `LUMEX_PROTOCOL_LAYOUT`, the
 * field group of a row (`dsl::field_list<F...>`), a tail.
 * @details The value is `record<F...>`; the fields are decoded in order,
 * the last one learns that it is last when the group is.
 */
template <template <typename...> class List, typename... F>
struct group_impl<List<F...>>
{
  /// The value: one value per field.
  using value_type = record<F...>;
  /// The sum of the field sizes, or `variable_size`.
  using binary_size
      = detail::size_sum<impl_of<F>::type::binary_size::value...>;
  /// The largest alignment of a field.
  using binary_alignment
      = detail::size_max<impl_of<F>::type::binary_alignment::value...>;
  /// The field types.
  using fields_type = List<F...>;

  /// @brief The descriptor (kind `group`, `count` the number of fields).
  static LUMEX_CONSTEXPR field_info_t
  info () LUMEX_NOEXCEPT
  {
    return detail::make_info (
        field_kind::group, detail::info_size (binary_size::value),
        byte_order::little, false, 1, 1, "", sizeof...(F));
  }

  /// @brief Decodes every field into `value`.
  template <bool Last, typename Reader, typename Siblings>
  static bool
  decode (Reader &reader, value_type &value, Siblings const &)
  {
    return detail::record_codec<
        Last,
        ::lumex::core::utility::sequence::make_index_sequence<sizeof...(F)>,
        F...>::decode (reader, value);
  }

  /// @brief Encodes every field of `value`.
  template <bool Last, typename Writer, typename Siblings>
  static bool
  encode (Writer &writer, value_type const &value, Siblings const &)
  {
    return detail::record_codec<
        Last,
        ::lumex::core::utility::sequence::make_index_sequence<sizeof...(F)>,
        F...>::encode (writer, value);
  }

  /// @brief A group is given as its record.
  template <typename Arg>
  static bool
  assign (value_type &value, Arg const &argument)
  {
    static_assert (std::is_convertible<Arg, value_type>::value,
                   "LUMEX_PROTOCOL: a group of fields (a layout) is given as "
                   "its record (field::record_of<...>::type)");
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

#endif // !LUMEX_CORE_PROTOCOL_FIELD_RECORD_HPP
