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
 * @file LumexProtocolKey.hpp
 * @brief The key of a table row: `make_key`, `text_key`, and the run-time
 * form `key_value_t`.
 * @details The key is what identifies a message on the wire: a code
 * (`0x1D`), a character (`'A'`), an enumerator, or a text mnemonic
 * (`"AZ"`). `make_key` keeps integers, characters, enumerators and any other
 * literal type as they are and turns a string literal into a `text_key`, so
 * that the key of every row is a constant expression of its own type.
 */
#ifndef LUMEX_CORE_PROTOCOL_DSL_KEY_HPP
#define LUMEX_CORE_PROTOCOL_DSL_KEY_HPP

#include <cstddef>
#include <cstdint>
#include <type_traits>

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
namespace dsl
{
/// @brief A text key: the characters of a string literal, without the
/// terminating NUL.
struct text_key
{
  /// The first character (a string literal, never freed).
  char const *text;
  /// The number of characters.
  std::size_t size;
};

namespace detail
{
template <typename T> struct key_maker
{
  typedef T type;

  static LUMEX_CONSTEXPR T
  make (T const &value) LUMEX_NOEXCEPT
  {
    return value;
  }
};

template <std::size_t N> struct key_maker<char[N]>
{
  typedef text_key type;

  static LUMEX_CONSTEXPR text_key
  make (char const (&value)[N]) LUMEX_NOEXCEPT
  {
    return text_key{ value, N - 1 };
  }
};
} // namespace detail

/**
 * @brief The key of a row as a constant: a string literal becomes a
 * `text_key`, every other value keeps its type.
 */
template <typename T>
LUMEX_CONSTEXPR typename detail::key_maker<T>::type
make_key (T const &value) LUMEX_NOEXCEPT
{
  return detail::key_maker<T>::make (value);
}

/// @brief What a `key_value_t` holds.
enum class key_kind : std::uint8_t
{
  /// `integer` holds an integer, a character or an enumerator.
  integer,
  /// `text` and `text_size` hold a text key.
  text,
  /// Another type; its value is reached through the command type only.
  other
};

/// @brief The key of a row at run time, for the index and the logs.
struct key_value_t
{
  /// What the key holds.
  key_kind kind;
  /// The integer key, converted to 64 bits (`key_kind::integer`).
  std::uint64_t integer;
  /// The text key (`key_kind::text`), otherwise `nullptr`.
  char const *text;
  /// The number of characters of `text`.
  std::size_t text_size;
};

namespace detail
{
template <typename T>
LUMEX_CONSTEXPR key_value_t
to_key_value (T const &value, std::true_type) LUMEX_NOEXCEPT
{
  return key_value_t{ key_kind::integer, static_cast<std::uint64_t> (value),
                      nullptr, 0 };
}

template <typename T>
LUMEX_CONSTEXPR key_value_t
to_key_value (T const &, std::false_type) LUMEX_NOEXCEPT
{
  return key_value_t{ key_kind::other, 0, nullptr, 0 };
}

LUMEX_CONSTEXPR inline key_value_t
to_key_value (text_key const &value, std::false_type) LUMEX_NOEXCEPT
{
  return key_value_t{ key_kind::text, 0, value.text, value.size };
}
} // namespace detail

/// @brief The run-time form of a key made by `make_key`.
template <typename T>
LUMEX_CONSTEXPR key_value_t
key_value (T const &key) LUMEX_NOEXCEPT
{
  return detail::to_key_value (
      key, std::integral_constant<bool, std::is_integral<T>::value
                                            || std::is_enum<T>::value> ());
}
} // namespace dsl
} // namespace protocol
} // namespace core
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_PROTOCOL_DSL_KEY_HPP
