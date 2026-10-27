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
 * @file LumexProtocolOptions.hpp
 * @brief The row options of a protocol table as typed constant values:
 * `timeout_t`, `retries_t`, `stream_t`, `log_level_t`, `firmware_t`,
 * `model_t<N>`, `accepted_in_t<E, N>`, the flags `row_flags`, the set
 * `option_set<...>` and its queries `has_option`, `get_option`, `flags_of`.
 * @details A table writes the options of a row as `(timeout (500), retries
 * (2), dangerous)`; the names are members of `dsl::vocabulary`, so they need
 * no namespace in the table. `make_options` collects them into an
 * `option_set` that is a literal type: every query works in a constant
 * expression. Each option except the flags may appear once per row.
 */
#ifndef LUMEX_CORE_PROTOCOL_DSL_OPTIONS_HPP
#define LUMEX_CORE_PROTOCOL_DSL_OPTIONS_HPP

#include <cstddef>
#include <cstdint>
#include <type_traits>

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
namespace dsl
{
/// @brief The tag a word of the table vocabulary names as
/// `lumex_vocabulary_word` (an option type), so that the DSL can say which
/// word hid a type of the user's namespace.
struct vocabulary_word_tag
{
};

/// @brief The log level a row asks for (`log_level (level::debug)`); the
/// dialogue maps it to the level of its log.
enum class level : std::uint8_t
{
  trace,
  debug,
  info,
  warning,
  error,
  off
};

/// @brief The kinds of options, the keys of `has_option` and `get_option`.
namespace kind
{
/// `timeout (ms)`.
struct timeout
{
};
/// `retries (n)`.
struct retries
{
};
/// `stream (interval_ms)`.
struct stream
{
};
/// `log_level (level)`.
struct log_level
{
};
/// `firmware (from, to)`.
struct firmware
{
};
/// `model (names...)`.
struct model
{
};
/// `accepted_in (states...)`.
struct accepted_in
{
};
/// `idempotent`, `dangerous`, `deprecated`.
struct flag
{
};
/// Not an option (an error in a table).
struct none
{
};
} // namespace kind

/// @brief `timeout (ms)`: the reply must come within this many milliseconds.
struct timeout_t
{
  /// The kind of the option.
  typedef kind::timeout option_kind;
  /// A word of the table vocabulary (`timeout (ms)`).
  typedef vocabulary_word_tag lumex_vocabulary_word;
  /// Milliseconds.
  std::uint32_t milliseconds;

  /// @brief `timeout (ms)`.
  LUMEX_CONSTEXPR_CTOR explicit timeout_t (std::uint32_t ms) LUMEX_NOEXCEPT
      : milliseconds (ms)
  {
  }
};

/// @brief `retries (n)`: how many times the request is sent again after a
/// timeout.
struct retries_t
{
  /// The kind of the option.
  typedef kind::retries option_kind;
  /// A word of the table vocabulary (`retries (n)`).
  typedef vocabulary_word_tag lumex_vocabulary_word;
  /// Additional attempts.
  std::uint32_t count;

  /// @brief `retries (n)`.
  LUMEX_CONSTEXPR_CTOR explicit retries_t (std::uint32_t n) LUMEX_NOEXCEPT
      : count (n)
  {
  }
};

/// @brief `stream (interval_ms)`: the device sends this row by itself, every
/// `interval_ms` milliseconds (gaps are measured against it).
struct stream_t
{
  /// The kind of the option.
  typedef kind::stream option_kind;
  /// A word of the table vocabulary (`stream (interval_ms)`).
  typedef vocabulary_word_tag lumex_vocabulary_word;
  /// The expected interval between two messages, milliseconds.
  std::uint32_t interval_ms;

  /// @brief `stream (interval_ms)`.
  LUMEX_CONSTEXPR_CTOR explicit stream_t (std::uint32_t interval)
      LUMEX_NOEXCEPT : interval_ms (interval)
  {
  }
};

/// @brief `log_level (level)`: the level of the log record of this row.
struct log_level_t
{
  /// The kind of the option.
  typedef kind::log_level option_kind;
  /// A word of the table vocabulary (`log_level (level::debug)`).
  typedef vocabulary_word_tag lumex_vocabulary_word;
  /// The level.
  level value;

  /// @brief `log_level (level::...)`.
  LUMEX_CONSTEXPR_CTOR explicit log_level_t (level chosen) LUMEX_NOEXCEPT
      : value (chosen)
  {
  }
};

/// @brief `firmware (from, to)`: the row exists in firmware versions `from`
/// to `to` inclusive (the table chooses the encoding of a version number).
struct firmware_t
{
  /// The kind of the option.
  typedef kind::firmware option_kind;
  /// A word of the table vocabulary (`firmware (from, to)`).
  typedef vocabulary_word_tag lumex_vocabulary_word;
  /// First version with the row.
  std::uint32_t from;
  /// Last version with the row.
  std::uint32_t to;

  /// @brief `firmware (from, to)`.
  LUMEX_CONSTEXPR_CTOR
  firmware_t (std::uint32_t first, std::uint32_t last) LUMEX_NOEXCEPT
      : from (first),
        to (last)
  {
  }
};

/// @brief `model ("A", "B")`: the row exists in these models only.
template <std::size_t N> struct model_t
{
  /// The kind of the option.
  typedef kind::model option_kind;
  /// The model names (string literals).
  char const *names[N];
};

/// @brief `accepted_in (s1, s2)`: the device accepts the row in these states
/// only (`E` is the protocol's state enumeration).
template <typename E, std::size_t N> struct accepted_in_t
{
  /// The kind of the option.
  typedef kind::accepted_in option_kind;
  /// The state enumeration.
  typedef E state_type;
  /// The states.
  E states[N];
};

/**
 * @brief The flags of a row: `idempotent` (sending it twice is harmless, it
 * may be retried), `dangerous` (it changes the device irreversibly and needs
 * a confirmation), `deprecated` (kept for old firmware).
 */
struct row_flags
{
  /// The flag values, combined with `|` by `flags_of`.
  enum value : unsigned
  {
    /// Repeating the row changes nothing.
    idempotent = 1u,
    /// The row changes the device irreversibly.
    dangerous = 2u,
    /// The row is kept for old devices.
    deprecated = 4u
  };
};

namespace detail
{
template <typename T> struct options_voider
{
  typedef void type;
};

template <typename T, typename = void> struct kind_of
{
  typedef kind::none type;
};

template <typename T>
struct kind_of<T, typename options_voider<typename T::option_kind>::type>
{
  typedef typename T::option_kind type;
};

template <> struct kind_of<row_flags::value, void>
{
  typedef kind::flag type;
};

template <bool... B> struct option_bools
{
};

template <bool... B>
struct options_all
    : std::is_same<option_bools<true, B...>, option_bools<B..., true>>
{
};

template <typename K, typename... T> struct count_kind;

template <typename K>
struct count_kind<K> : std::integral_constant<std::size_t, 0>
{
};

template <typename K, typename Head, typename... Tail>
struct count_kind<K, Head, Tail...>
    : std::integral_constant<
          std::size_t,
          (std::is_same<typename kind_of<Head>::type, K>::value ? 1u : 0u)
              + count_kind<K, Tail...>::value>
{
};

// The index of the first option of kind K; sizeof... (T) when there is none.
template <typename K, std::size_t I, typename... T> struct index_of_kind;

template <typename K, std::size_t I>
struct index_of_kind<K, I> : std::integral_constant<std::size_t, I>
{
};

template <typename K, std::size_t I, typename Head, typename... Tail>
struct index_of_kind<K, I, Head, Tail...>
    : std::conditional<std::is_same<typename kind_of<Head>::type, K>::value,
                       std::integral_constant<std::size_t, I>,
                       index_of_kind<K, I + 1, Tail...>>::type
{
};

template <std::size_t I, typename... T> struct option_at
{
  typedef void type;
};

template <typename Head, typename... Tail> struct option_at<0, Head, Tail...>
{
  typedef Head type;
};

template <std::size_t I, typename Head, typename... Tail>
struct option_at<I, Head, Tail...> : option_at<I - 1, Tail...>
{
};

template <std::size_t I, typename T> struct option_slot
{
  T value;
};

template <typename Indices, typename... T> struct option_storage;

template <std::size_t... I, typename... T>
struct option_storage<::lumex::core::utility::sequence::index_sequence<I...>,
                      T...> : option_slot<I, T>...
{
  LUMEX_CONSTEXPR_CTOR
  option_storage (T const &...values) LUMEX_NOEXCEPT
      : option_slot<I, T>{ values }...
  {
  }
};

template <typename T> struct flag_bits
{
  static LUMEX_CONSTEXPR unsigned
  get (T const &) LUMEX_NOEXCEPT
  {
    return 0u;
  }
};

template <> struct flag_bits<row_flags::value>
{
  static LUMEX_CONSTEXPR unsigned
  get (row_flags::value flag) LUMEX_NOEXCEPT
  {
    return static_cast<unsigned> (flag);
  }
};

LUMEX_CONSTEXPR inline unsigned
or_all () LUMEX_NOEXCEPT
{
  return 0u;
}

template <typename... Rest>
LUMEX_CONSTEXPR unsigned
or_all (unsigned first, Rest... rest) LUMEX_NOEXCEPT
{
  return first | or_all (rest...);
}

template <std::size_t... I, typename... T>
LUMEX_CONSTEXPR unsigned
flags_of_storage (
    option_storage<::lumex::core::utility::sequence::index_sequence<I...>,
                   T...> const &storage) LUMEX_NOEXCEPT
{
  return or_all (flag_bits<T>::get (
      static_cast<option_slot<I, T> const &> (storage).value)...);
}
} // namespace detail

/**
 * @brief The options of one row, in the order the table wrote them.
 * @details A literal type: `C::options ()` of a command type is a constant
 * expression. Rejects at compile time a value that is not an option and an
 * option given twice (flags excepted).
 */
template <typename... T>
struct option_set
    : detail::option_storage<
          ::lumex::core::utility::sequence::make_index_sequence<sizeof...(T)>,
          T...>
{
  static_assert (
      detail::options_all<!std::is_same<typename detail::kind_of<T>::type,
                                        kind::none>::value...>::value,
      "LUMEX_PROTOCOL: a row option must be timeout (ms), retries (n), "
      "stream (interval_ms), log_level (level::...), firmware (from, to), "
      "model (\"name\", ...), accepted_in (state, ...), idempotent, "
      "dangerous, deprecated, or an option type of another layer (a type "
      "naming option_kind)");
  static_assert (
      detail::options_all<(
          std::is_same<typename detail::kind_of<T>::type, kind::flag>::value
          || detail::count_kind<typename detail::kind_of<T>::type, T...>::value
                 == 1)...>::value,
      "LUMEX_PROTOCOL: a row option is given twice");

  /// @brief Stores the options.
  LUMEX_CONSTEXPR_CTOR
  option_set (T const &...values) LUMEX_NOEXCEPT
      : detail::option_storage<::lumex::core::utility::sequence::
                                   make_index_sequence<sizeof...(T)>,
                               T...> (values...)
  {
  }
};

/// @brief Collects the options of a row (the table's option group).
template <typename... T>
LUMEX_CONSTEXPR option_set<T...>
make_options (T const &...values) LUMEX_NOEXCEPT
{
  return option_set<T...> (values...);
}

/// @brief Whether an option set holds an option of kind `K` (a type trait).
template <typename K, typename Set> struct has_option_type;

/// @copydoc has_option_type
template <typename K, typename... T>
struct has_option_type<K, option_set<T...>>
    : std::integral_constant<bool, (detail::count_kind<K, T...>::value > 0)>
{
};

/**
 * @brief Whether `options` holds an option of kind `K`.
 * @tparam K A tag of `dsl::kind`: `has_option<kind::timeout> (C::options ())`.
 */
template <typename K, typename... T>
LUMEX_CONSTEXPR bool
has_option (option_set<T...> const &) LUMEX_NOEXCEPT
{
  return has_option_type<K, option_set<T...>>::value;
}

namespace detail
{
template <typename K, typename... T> struct option_getter
{
  static_assert (count_kind<K, T...>::value > 0,
                 "LUMEX_PROTOCOL: get_option of an option the row does not "
                 "have (check has_option first)");
  typedef
      typename option_at<index_of_kind<K, 0, T...>::value, T...>::type type;
};
} // namespace detail

/**
 * @brief The option of kind `K` (the first one for `kind::flag`).
 * @details Does not compile when the set has no such option.
 * @return A copy: `get_option<kind::timeout> (C::options ()).milliseconds`.
 */
template <typename K, typename... T>
LUMEX_CONSTEXPR typename detail::option_getter<K, T...>::type
get_option (option_set<T...> const &options) LUMEX_NOEXCEPT
{
  return static_cast<detail::option_slot<
      detail::index_of_kind<K, 0, T...>::value,
      typename detail::option_getter<K, T...>::type> const &> (options)
      .value;
}

/// @brief The option at position `I` of an option set (in table order).
template <std::size_t I, typename... T>
LUMEX_CONSTEXPR typename detail::option_at<I, T...>::type const &
option_value_at (option_set<T...> const &options) LUMEX_NOEXCEPT
{
  return static_cast<detail::option_slot<
      I, typename detail::option_at<I, T...>::type> const &> (options)
      .value;
}

/// @brief The flags of a row combined: `flags_of (C::options ()) &
/// row_flags::dangerous`.
template <typename... T>
LUMEX_CONSTEXPR unsigned
flags_of (option_set<T...> const &options) LUMEX_NOEXCEPT
{
  return detail::flags_of_storage (options);
}
} // namespace dsl
} // namespace protocol
} // namespace core
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_PROTOCOL_DSL_OPTIONS_HPP
