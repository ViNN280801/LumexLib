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
#include <cstring>
#include <type_traits>

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
  using type = T;

  static LUMEX_CONSTEXPR T
  make (T const &value) LUMEX_NOEXCEPT
  {
    return value;
  }
};

template <std::size_t N> struct key_maker<char[N]>
{
  using type = text_key;

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

/// @brief The tag a composite key type names as `lumex_composite_key`.
struct composite_key_tag
{
};

/// @brief What a `key_value_t` holds.
enum class key_kind : std::uint8_t
{
  /// `integer` holds an integer, a character or an enumerator, as its
  /// unsigned bits (never sign-extended) in `width` bytes.
  integer,
  /// `text` and `text_size` hold a text key.
  text,
  /// `integer` holds the packed bits of a composite key (a key type that
  /// names `lumex_composite_key`), in `width` bytes.
  composite,
  /// Another type; its value is reached through the command type only.
  other
};

/**
 * @brief The key of a row at run time, for the index and the logs.
 * @details An integer key is kept as the unsigned bits of its type in
 * `width` bytes: the character with the code 0xFF is `0xFF` in 1 byte and `-1`
 * as an `int` is `0xFFFFFFFF` in 4 bytes (they were the same 64-bit value when
 * the key was sign-extended). The index narrows or widens the key to the width
 * of the wire (`normalize_key`).
 */
struct key_value_t
{
  /// What the key holds.
  key_kind kind;
  /// The bytes of the key type for `integer` and `composite`, otherwise 0.
  std::uint8_t width;
  /// The key bits, zero-extended to 64 bits (`integer`, `composite`).
  std::uint64_t integer;
  /// The text key (`key_kind::text`), otherwise `nullptr`.
  char const *text;
  /// The number of characters of `text`.
  std::size_t text_size;
};

namespace detail
{
template <typename T, typename = void> struct key_voider
{
  using type = void;
};

template <typename T, typename = void>
struct is_composite_key : std::false_type
{
};

template <typename T>
struct is_composite_key<
    T, typename key_voider<typename T::lumex_composite_key>::type>
    : std::is_same<typename T::lumex_composite_key, composite_key_tag>
{
};

// The unsigned bits of an integer, enumerator or bool key.
template <typename T, bool Enum = std::is_enum<T>::value> struct key_bits
{
  static LUMEX_CONSTEXPR std::uint64_t
  get (T value) LUMEX_NOEXCEPT
  {
    return static_cast<std::uint64_t> (
        static_cast<typename std::make_unsigned<T>::type> (value));
  }
};

template <> struct key_bits<bool, false>
{
  static LUMEX_CONSTEXPR std::uint64_t
  get (bool value) LUMEX_NOEXCEPT
  {
    return value ? 1u : 0u;
  }
};

template <typename T> struct key_bits<T, true>
{
  static LUMEX_CONSTEXPR std::uint64_t
  get (T value) LUMEX_NOEXCEPT
  {
    return key_bits<typename std::underlying_type<T>::type>::get (
        static_cast<typename std::underlying_type<T>::type> (value));
  }
};

template <typename T>
LUMEX_CONSTEXPR key_value_t
to_key_value (T const &value, std::true_type, std::false_type) LUMEX_NOEXCEPT
{
  return key_value_t{ key_kind::integer,
                      static_cast<std::uint8_t> (sizeof (T)),
                      key_bits<T>::get (value), nullptr, 0 };
}

template <typename T>
LUMEX_CONSTEXPR key_value_t
to_key_value (T const &value, std::false_type, std::true_type) LUMEX_NOEXCEPT
{
  return key_value_t{ key_kind::composite,
                      static_cast<std::uint8_t> (T::lumex_key_width ()),
                      value.lumex_key_bits (), nullptr, 0 };
}

template <typename T>
LUMEX_CONSTEXPR key_value_t
to_key_value (T const &, std::false_type, std::false_type) LUMEX_NOEXCEPT
{
  return key_value_t{ key_kind::other, 0, 0, nullptr, 0 };
}

LUMEX_CONSTEXPR inline key_value_t
to_key_value (text_key const &value, std::false_type,
              std::false_type) LUMEX_NOEXCEPT
{
  return key_value_t{ key_kind::text, 0, 0, value.text, value.size };
}
} // namespace detail

/**
 * @brief The run-time form of a key made by `make_key`.
 * @details A composite key type (it names `using lumex_composite_key =
 * composite_key_tag;`) gives `static std::size_t lumex_key_width ()` (the
 * bytes it takes on the wire) and `std::uint64_t lumex_key_bits () const`
 * (its parts packed in wire order, most significant first).
 */
template <typename T>
LUMEX_CONSTEXPR key_value_t
key_value (T const &key) LUMEX_NOEXCEPT
{
  return detail::to_key_value (
      key,
      std::integral_constant<bool, std::is_integral<T>::value
                                       || std::is_enum<T>::value> (),
      std::integral_constant<bool, detail::is_composite_key<T>::value> ());
}

/// @brief The mask of the low `bytes` bytes of a 64-bit value.
LUMEX_CONSTEXPR inline std::uint64_t
width_mask (std::size_t bytes) LUMEX_NOEXCEPT
{
  return bytes >= 8 ? ~static_cast<std::uint64_t> (0)
                    : ((static_cast<std::uint64_t> (1) << (8u * bytes)) - 1u);
}

/**
 * @brief The key as the wire carries it: an integer or composite key is cut
 * to its low `wire_width` bytes (a character with the code 0xFF and the `int`
 * `-1` are the same key on a 1-byte wire). `wire_width` 0 keeps the key as it
 * is.
 */
LUMEX_CONSTEXPR inline key_value_t
normalize_key (key_value_t const &key, std::size_t wire_width) LUMEX_NOEXCEPT
{
  return (wire_width == 0
          || (key.kind != key_kind::integer
              && key.kind != key_kind::composite))
             ? key
             : key_value_t{ key.kind, static_cast<std::uint8_t> (wire_width),
                            key.integer & width_mask (wire_width), key.text,
                            key.text_size };
}

/// @brief Whether two keys are the same on the wire: the same kind, the same
/// bits and width, or the same characters.
inline bool
keys_equal (key_value_t const &left, key_value_t const &right) LUMEX_NOEXCEPT
{
  if (left.kind != right.kind)
    return false;
  if (left.kind == key_kind::integer || left.kind == key_kind::composite)
    return left.integer == right.integer && left.width == right.width;
  if (left.kind == key_kind::text)
    return left.text_size == right.text_size
           && (left.text_size == 0
               || std::memcmp (left.text, right.text, left.text_size) == 0);
  return false;
}
} // namespace dsl
} // namespace protocol
} // namespace core
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_PROTOCOL_DSL_KEY_HPP
