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
 * @file LumexProtocolValues.hpp
 * @brief The value types of decoded fields: `fixed_point`, `text_view`,
 * `fixed_string`, `bounded_array`, `alternatives`, and the values of the
 * composite fields (`tail_value`, `split_value`, `union_value`,
 * `nested_value`).
 * @details Decoding never allocates: every value has a capacity fixed by its
 * field type, and the views (`text_view`, `nested_value::payload`) point into
 * the decoded message, so they are valid while that buffer is. Every value
 * type is trivially copyable. A scaled number keeps its raw wire value
 * (`fixed_point::raw`) next to the engineering value (`value ()`), for the
 * log line of the index.
 */
#ifndef LUMEX_CORE_PROTOCOL_FIELD_VALUES_HPP
#define LUMEX_CORE_PROTOCOL_FIELD_VALUES_HPP

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <type_traits>

#include "lumex/core/span/LumexSpan"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#if __has_warning("-Wunsafe-buffer-usage-in-libc-call")
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-libc-call"
#endif
#pragma clang diagnostic ignored "-Wfloat-equal"
#endif

namespace lumex
{
namespace core
{
namespace protocol
{
namespace field
{
/// @brief A read-only view of bytes (the type of the framing payloads).
using bytes_view_t = ::lumex::core::span::view::span<std::uint8_t const>;

/**
 * @brief A number on the wire with a fixed scale: the raw value and the
 * engineering value `raw * Ratio`.
 * @tparam Raw The integer type of the raw value.
 * @tparam Ratio The scale (`std::ratio<1, 100>`: hundredths).
 */
template <typename Raw, typename Ratio> struct fixed_point
{
  /// The integer type of the raw value.
  using raw_type = Raw;
  /// The scale.
  using ratio = Ratio;

  /// The value on the wire.
  Raw raw;

  /// @brief The engineering value: `raw * Ratio::num / Ratio::den`.
  LUMEX_CONSTEXPR double
  value () const LUMEX_NOEXCEPT
  {
    return static_cast<double> (raw) * static_cast<double> (Ratio::num)
           / static_cast<double> (Ratio::den);
  }
};

/// @brief Equal when the raw values are equal.
template <typename Raw, typename Ratio>
LUMEX_CONSTEXPR bool
operator== (fixed_point<Raw, Ratio> const &left,
            fixed_point<Raw, Ratio> const &right) LUMEX_NOEXCEPT
{
  return left.raw == right.raw;
}

/// @brief Not equal when the raw values differ.
template <typename Raw, typename Ratio>
LUMEX_CONSTEXPR bool
operator!= (fixed_point<Raw, Ratio> const &left,
            fixed_point<Raw, Ratio> const &right) LUMEX_NOEXCEPT
{
  return !(left == right);
}

/**
 * @brief A read-only view of characters: a decoded `token_`, the text of a
 * `text_<N>`, an argument of a text field.
 */
class text_view
{
public:
  /// @brief The empty view.
  LUMEX_CONSTEXPR_CTOR
  text_view () LUMEX_NOEXCEPT : m_data (nullptr), m_size (0) {}

  /// @brief `size` characters at `data`.
  LUMEX_CONSTEXPR_CTOR
  text_view (char const *data, std::size_t size) LUMEX_NOEXCEPT
      : m_data (data),
        m_size (size)
  {
  }

  /// @brief The characters of a NUL-terminated string (an argument).
  text_view (char const *c_string)
      LUMEX_NOEXCEPT // NOLINT(google-explicit-constructor)
      : m_data (c_string),
        m_size (c_string == nullptr ? 0u : std::strlen (c_string))
  {
  }

  /// @brief The first character.
  LUMEX_CONSTEXPR char const *
  data () const LUMEX_NOEXCEPT
  {
    return m_data;
  }

  /// @brief The number of characters.
  LUMEX_CONSTEXPR std::size_t
  size () const LUMEX_NOEXCEPT
  {
    return m_size;
  }

  /// @brief Whether the view has no characters.
  LUMEX_CONSTEXPR bool
  empty () const LUMEX_NOEXCEPT
  {
    return m_size == 0;
  }

private:
  char const *m_data;
  std::size_t m_size;
};

/// @brief Equal when both views hold the same characters.
inline bool
operator== (text_view const &left, text_view const &right) LUMEX_NOEXCEPT
{
  return left.size () == right.size ()
         && (left.size () == 0
             || std::memcmp (left.data (), right.data (), left.size ()) == 0);
}

/// @brief Not equal when the characters differ.
inline bool
operator!= (text_view const &left, text_view const &right) LUMEX_NOEXCEPT
{
  return !(left == right);
}

/**
 * @brief A text of at most `N` characters stored in place (the value of
 * `text_<N>`): no allocation.
 */
template <std::size_t N> struct fixed_string
{
  static_assert (N > 0, "fixed_string<N>: N must be positive");

  /// The characters; those after `length` are NUL.
  char chars[N];
  /// The number of characters.
  std::size_t length;

  /// @brief The characters as a view.
  text_view
  view () const LUMEX_NOEXCEPT
  {
    return text_view (chars, length);
  }

  /// @brief The number of characters.
  std::size_t
  size () const LUMEX_NOEXCEPT
  {
    return length;
  }

  /// @brief Replaces the text; false (and no change) when `text` is longer
  /// than `N` or holds a NUL (the padding of the wire: it would end the
  /// text).
  bool
  assign (text_view const &text) LUMEX_NOEXCEPT
  {
    if (text.size () > N
        || (text.size () != 0
            && std::memchr (text.data (), '\0', text.size ()) != nullptr))
      return false;
    std::memset (chars, 0, N);
    if (text.size () != 0)
      std::memcpy (chars, text.data (), text.size ());
    length = text.size ();
    return true;
  }
};

/// @brief Equal when both texts hold the same characters.
template <std::size_t N>
bool
operator== (fixed_string<N> const &left,
            fixed_string<N> const &right) LUMEX_NOEXCEPT
{
  return left.view () == right.view ();
}

/// @brief Not equal when the characters differ.
template <std::size_t N>
bool
operator!= (fixed_string<N> const &left,
            fixed_string<N> const &right) LUMEX_NOEXCEPT
{
  return !(left == right);
}

/**
 * @brief Up to `Max` elements stored in place (the value of `counted_` and
 * `computed_count_`): a count read from a message never allocates.
 */
template <typename T, std::size_t Max> struct bounded_array
{
  static_assert (Max > 0, "bounded_array<T, Max>: Max must be positive");

  /// The element type.
  using value_type = T;

  /// The elements; those after `count` are value-initialized.
  T items[Max];
  /// The number of elements.
  std::size_t count;

  /// @brief The number of elements.
  std::size_t
  size () const LUMEX_NOEXCEPT
  {
    return count;
  }

  /// @brief The largest number of elements.
  static LUMEX_CONSTEXPR std::size_t
  capacity () LUMEX_NOEXCEPT
  {
    return Max;
  }

  /// @brief The first element.
  T const *
  begin () const LUMEX_NOEXCEPT
  {
    return items;
  }

  /// @brief One past the last element.
  T const *
  end () const LUMEX_NOEXCEPT
  {
    return items + count;
  }

  /// @brief Element `index` (below `size ()`).
  T const &
  operator[] (std::size_t index) const LUMEX_NOEXCEPT
  {
    return items[index];
  }

  /// @copydoc operator[]
  T &
  operator[] (std::size_t index) LUMEX_NOEXCEPT
  {
    return items[index];
  }

  /// @brief Appends `value`; false when the array is full.
  bool
  push_back (T const &value) LUMEX_NOEXCEPT
  {
    if (count >= Max)
      return false;
    items[count] = value;
    ++count;
    return true;
  }
};

/// @brief Equal when both arrays hold equal elements.
template <typename T, std::size_t Max>
bool
operator== (bounded_array<T, Max> const &left,
            bounded_array<T, Max> const &right)
{
  if (left.count != right.count)
    return false;
  for (std::size_t i = 0; i < left.count; ++i)
    if (!(left.items[i] == right.items[i]))
      return false;
  return true;
}

/// @brief Not equal when the elements differ.
template <typename T, std::size_t Max>
bool
operator!= (bounded_array<T, Max> const &left,
            bounded_array<T, Max> const &right)
{
  return !(left == right);
}

namespace detail
{
template <bool... B> struct value_bools
{
};

template <bool... B>
struct values_all
    : std::is_same<value_bools<true, B...>, value_bools<B..., true>>
{
};

template <typename... T> union alternative_union;

template <> union alternative_union<>
{
};

template <typename Head, typename... Tail>
union alternative_union<Head, Tail...>
{
  Head head;
  alternative_union<Tail...> tail;

  LUMEX_CONSTEXPR_CTOR
  alternative_union () LUMEX_NOEXCEPT : head () {}
};

template <std::size_t I> struct alternative_at
{
  template <typename Union>
  static auto
  get (Union &storage) LUMEX_NOEXCEPT
      -> decltype (alternative_at<I - 1>::get (storage.tail))
  {
    return alternative_at<I - 1>::get (storage.tail);
  }
};

template <> struct alternative_at<0>
{
  template <typename Union>
  static auto
  get (Union &storage) LUMEX_NOEXCEPT -> decltype ((storage.head))
  {
    return storage.head;
  }
};

template <std::size_t I, typename... T> struct nth_value;

template <typename Head, typename... Tail> struct nth_value<0, Head, Tail...>
{
  using type = Head;
};

template <std::size_t I, typename Head, typename... Tail>
struct nth_value<I, Head, Tail...> : nth_value<I - 1, Tail...>
{
};
} // namespace detail

/**
 * @brief One of the values `T...` (the value of `union_by_`,
 * `union_by_context_` and `by_length_`): a small variant of trivially
 * copyable types.
 * @details `index ()` tells which one is held; `get<I> ()` reads it (the
 * caller checks the index first), `emplace<I> ()` switches to a
 * value-initialized `I`-th value.
 */
template <typename... T> class alternatives
{
  static_assert (sizeof...(T) > 0, "alternatives<T...>: give a type");
  static_assert (
      detail::values_all<std::is_trivially_copyable<T>::value...>::value,
      "alternatives<T...>: every value type must be trivially copyable");

public:
  /// @brief The type of alternative `I`.
  template <std::size_t I> struct type_at : detail::nth_value<I, T...>
  {
  };

  /// @brief Holds a value-initialized first alternative.
  alternatives () LUMEX_NOEXCEPT : m_index (0), m_storage () {}

  /// @brief Which alternative is held (0-based, in the order of `T...`).
  std::size_t
  index () const LUMEX_NOEXCEPT
  {
    return m_index;
  }

  /// @brief Alternative `I` (only when `index () == I`).
  template <std::size_t I>
  typename type_at<I>::type const &
  get () const LUMEX_NOEXCEPT
  {
    return detail::alternative_at<I>::get (m_storage);
  }

  /// @copydoc get
  template <std::size_t I>
  typename type_at<I>::type &
  get () LUMEX_NOEXCEPT
  {
    return detail::alternative_at<I>::get (m_storage);
  }

  /// @brief Switches to a value-initialized alternative `I` and returns it.
  template <std::size_t I>
  typename type_at<I>::type &
  emplace () LUMEX_NOEXCEPT
  {
    using value_type = typename type_at<I>::type;
    value_type &slot = detail::alternative_at<I>::get (m_storage);
    ::new (static_cast<void *> (&slot)) value_type ();
    m_index = I;
    return slot;
  }

  /// @brief Switches to alternative `I` holding `value`.
  template <std::size_t I>
  void
  set (typename type_at<I>::type const &value) LUMEX_NOEXCEPT
  {
    using value_type = typename type_at<I>::type;
    value_type &slot = detail::alternative_at<I>::get (m_storage);
    ::new (static_cast<void *> (&slot)) value_type (value);
    m_index = I;
  }

private:
  std::size_t m_index;
  detail::alternative_union<T...> m_storage;
};

namespace detail
{
template <std::size_t I, std::size_t N> struct alternatives_equal
{
  template <typename A>
  static bool
  apply (A const &left, A const &right)
  {
    return left.index () == I
               ? (left.template get<I> () == right.template get<I> ())
               : alternatives_equal<I + 1, N>::apply (left, right);
  }
};

template <std::size_t N> struct alternatives_equal<N, N>
{
  template <typename A>
  static bool
  apply (A const &, A const &)
  {
    return false;
  }
};
} // namespace detail

/// @brief Equal when both hold the same alternative with equal values.
template <typename... T>
bool
operator== (alternatives<T...> const &left, alternatives<T...> const &right)
{
  return left.index () == right.index ()
         && detail::alternatives_equal<0, sizeof...(T)>::apply (left, right);
}

/// @brief Not equal otherwise.
template <typename... T>
bool
operator!= (alternatives<T...> const &left, alternatives<T...> const &right)
{
  return !(left == right);
}

/**
 * @brief The value of `optional_tail_<F...>`: whether the message carried the
 * tail, and its fields.
 */
template <typename Record> struct tail_value
{
  /// Whether the tail was present (on encode: whether to write it).
  bool present;
  /// The fields of the tail (value-initialized when absent).
  Record fields;
};

/// @brief Equal when both are absent, or both present with equal fields.
template <typename Record>
bool
operator== (tail_value<Record> const &left, tail_value<Record> const &right)
{
  return left.present == right.present
         && (!left.present || left.fields == right.fields);
}

/// @brief Not equal otherwise.
template <typename Record>
bool
operator!= (tail_value<Record> const &left, tail_value<Record> const &right)
{
  return !(left == right);
}

/**
 * @brief The value of `split_<Word, Parts...>`: the raw word and the parts
 * read from it.
 * @details On encode the parts are written into the raw word; the bits of
 * the word that no part covers keep their value, so a decoded word encodes
 * back to the same bits.
 */
template <typename Raw, typename Record> struct split_value
{
  /// The word as it was on the wire.
  Raw raw;
  /// The parts, in the order of the split.
  Record parts;
};

/// @brief Equal when the raw words and the parts are equal.
template <typename Raw, typename Record>
bool
operator== (split_value<Raw, Record> const &left,
            split_value<Raw, Record> const &right)
{
  return left.raw == right.raw && left.parts == right.parts;
}

/// @brief Not equal otherwise.
template <typename Raw, typename Record>
bool
operator!= (split_value<Raw, Record> const &left,
            split_value<Raw, Record> const &right)
{
  return !(left == right);
}

/**
 * @brief The value of `union_by_` and `union_by_context_`: the discriminant
 * that chose the case and the value of the case.
 * @details `cases.index ()` is the position of the case in the field type;
 * `discriminant` is its value (written on encode from the case, not from
 * this member).
 */
template <typename Alternatives> struct union_value
{
  /// The value that chose the case.
  std::int64_t discriminant;
  /// The case.
  Alternatives cases;
};

/// @brief Equal when the same case holds equal values.
template <typename Alternatives>
bool
operator== (union_value<Alternatives> const &left,
            union_value<Alternatives> const &right)
{
  return left.discriminant == right.discriminant && left.cases == right.cases;
}

/// @brief Not equal otherwise.
template <typename Alternatives>
bool
operator!= (union_value<Alternatives> const &left,
            union_value<Alternatives> const &right)
{
  return !(left == right);
}

/**
 * @brief The value of `nested_<Protocol, KeyField>`: the key of the inner
 * message and its bytes, for a decode with the inner protocol
 * (`layout::decode_nested`).
 */
template <typename Key> struct nested_value
{
  /// The key of the inner message.
  Key key;
  /// The inner message after its key (a view into the decoded buffer).
  bytes_view_t payload;
};

/// @brief Equal when the keys and the payload bytes are equal.
template <typename Key>
bool
operator== (nested_value<Key> const &left, nested_value<Key> const &right)
{
  return left.key == right.key && left.payload.size () == right.payload.size ()
         && (left.payload.size () == 0
             || std::memcmp (left.payload.data (), right.payload.data (),
                             left.payload.size ())
                    == 0);
}

/// @brief Not equal otherwise.
template <typename Key>
bool
operator!= (nested_value<Key> const &left, nested_value<Key> const &right)
{
  return !(left == right);
}
} // namespace field
} // namespace protocol
} // namespace core
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_PROTOCOL_FIELD_VALUES_HPP
