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
 * @file LumexProtocolTextLayouts.hpp
 * @brief The text layouts: `fixed_width_text_layout<Radix>` (fields as
 * digits of a fixed width) and `tokenized_text_layout<Separator,
 * KeySeparator, Radix, Trim>` (a key prefix, then fields as tokens).
 * @details In `fixed_width_text_layout<16>` an integer of `n` bytes is `2n`
 * upper case hexadecimal digits of its two's complement bits (`i16 -1` is
 * `FFFF`), a floating field the digits of its IEEE 754 bits; in
 * `fixed_width_text_layout<10>` an integer is as many decimal digits as its
 * largest value has (plus a sign position when signed: `-005`), zero-padded;
 * a floating field is refused at compile time (use `dec_text_<I, F>`).
 *
 * `tokenized_text_layout` writes the key of the row, then
 * `KeySeparator`, then the fields separated by `Separator` (`XX,12,3.5`);
 * the terminator of the message is the frame codec's (a delimiter codec).
 * An integer is a decimal token (signed with `-`) or, with `Radix` 16, the
 * hexadecimal digits of its bits; a floating field a decimal token; a text
 * field a token of at most its width (no NUL in it). With `Trim` the spaces
 * around a token are ignored on decode (`XX, 12`) and a value that starts or
 * ends with a space is refused on encode (it would not read back). A token
 * may not contain `Separator`.
 */
#ifndef LUMEX_CORE_PROTOCOL_LAYOUT_TEXT_LAYOUTS_HPP
#define LUMEX_CORE_PROTOCOL_LAYOUT_TEXT_LAYOUTS_HPP

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include "lumex/core/protocol/dsl/LumexProtocolKey.hpp"
#include "lumex/core/protocol/error/LumexProtocolErrc.hpp"
#include "lumex/core/protocol/field/LumexProtocolNumeric.hpp"
#include "lumex/core/protocol/layout/LumexProtocolBinaryLayout.hpp"
#include "lumex/core/protocol/layout/LumexProtocolLayoutBase.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
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
namespace layout
{
namespace detail
{
/// The characters of a row key in a tokenized message: a text key as it is,
/// a character as itself, an integer or enumerator as a decimal number.
struct key_text
{
  char buffer[24];
  char const *data;
  std::size_t size;

  explicit key_text (::lumex::core::protocol::dsl::text_key const &key)
      LUMEX_NOEXCEPT : buffer (),
                       data (key.text),
                       size (key.size)
  {
  }

  explicit key_text (char key) LUMEX_NOEXCEPT : buffer (),
                                                data (buffer),
                                                size (1)
  {
    buffer[0] = key;
  }

  template <typename T>
  explicit key_text (T key,
                     typename std::enable_if<std::is_integral<T>::value
                                             || std::is_enum<T>::value>::type
                         * = nullptr) LUMEX_NOEXCEPT : buffer (),
                                                       data (buffer),
                                                       size (0)
  {
    std::int64_t const value = static_cast<std::int64_t> (key);
    std::uint64_t const magnitude
        = value < 0 ? (~static_cast<std::uint64_t> (value) + 1u)
                    : static_cast<std::uint64_t> (value);
    std::size_t at = 0;
    if (value < 0)
      buffer[at++] = '-';
    size = at + decimal_digits (magnitude, buffer + at);
  }

  key_text (key_text const &) = delete;
  key_text &operator= (key_text const &) = delete;
};

/// Whether `size` characters at `data` contain `c`.
inline bool
contains (char const *data, std::size_t size, char c) LUMEX_NOEXCEPT
{
  return size != 0 && std::memchr (data, c, size) != nullptr;
}
} // namespace detail

/**
 * @brief The fixed-width text layout: every field as characters of a fixed
 * width (see the file description).
 * @tparam Radix 16 (hexadecimal digits, the default) or 10 (decimal).
 */
template <unsigned Radix = 16> struct fixed_width_text_layout
{
  static_assert (Radix == 16 || Radix == 10,
                 "fixed_width_text_layout<Radix>: Radix is 16 or 10");

  /// @brief No alignment in a text.
  template <typename Impl> struct is_aligned : std::true_type
  {
  };

  /// @brief The number of characters of an integer of `bytes` bytes.
  static LUMEX_CONSTEXPR std::size_t
  integer_width (std::size_t bytes, bool is_signed) LUMEX_NOEXCEPT
  {
    return Radix == 16 ? 2u * bytes : detail::decimal_width (bytes, is_signed);
  }

  /// @brief Reads the fields of one message.
  class reader : public detail::reader_base
  {
  public:
    /// Hexadecimal digits carry the bits of an integer; decimal digits the
    /// number.
    using preserves_bits = std::integral_constant<bool, Radix == 16>;
    /// Not a tokenized text.
    using is_tokenized = std::false_type;
    /// Floating fields only as hexadecimal digits of their bits.
    using supports_floating = std::integral_constant<bool, Radix == 16>;

    /// @brief Reads `input`; `context` gives the session values.
    explicit reader (bytes_view_t input,
                     decode_context_t const *context = nullptr) LUMEX_NOEXCEPT
        : detail::reader_base (input, context)
    {
    }

    /// @brief The characters left.
    std::size_t
    remaining_units () const LUMEX_NOEXCEPT
    {
      return remaining_bytes ();
    }

    /// @brief An integer of `bytes` bytes as digits.
    bool
    read_integer (std::size_t bytes, byte_order, bool is_signed,
                  std::uint64_t &bits) LUMEX_NOEXCEPT
    {
      std::size_t const width = integer_width (bytes, is_signed);
      std::uint8_t const *data = nullptr;
      if (!take (width, data))
        return false;
      return parse (as_chars (data), width, bytes, is_signed, bits);
    }

    /// @brief The IEEE 754 bits of a float or a double as hexadecimal
    /// digits.
    bool
    read_floating (std::size_t bytes, byte_order order,
                   std::uint64_t &bits) LUMEX_NOEXCEPT
    {
      return read_integer (bytes, order, false, bits);
    }

    /// @brief `width` hexadecimal digits.
    bool
    read_hex_text (std::size_t width, std::uint64_t &value) LUMEX_NOEXCEPT
    {
      std::uint8_t const *data = nullptr;
      if (!take (width, data))
        return false;
      if (!detail::parse_hex (as_chars (data), width, value))
        return fail (
            ::lumex::core::protocol::error::protocol_errc::invalid_value);
      return true;
    }

    /// @brief A decimal number of a fixed width.
    bool
    read_decimal_text (std::size_t int_digits, std::size_t frac_digits,
                       std::int64_t &scaled) LUMEX_NOEXCEPT
    {
      std::size_t const width
          = int_digits + (frac_digits == 0 ? 0u : 1u + frac_digits);
      std::uint8_t const *data = nullptr;
      if (!take (width, data))
        return false;
      if (!detail::parse_fixed (as_chars (data), width, frac_digits, scaled)
          || !detail::fits_fixed (scaled, int_digits, frac_digits))
        return fail (
            ::lumex::core::protocol::error::protocol_errc::invalid_value);
      return true;
    }

    /// @brief One character.
    bool
    read_char (char &value) LUMEX_NOEXCEPT
    {
      std::uint8_t const *data = nullptr;
      if (!take (1, data))
        return false;
      value = static_cast<char> (data[0]);
      return true;
    }

    /// @brief `capacity` characters.
    bool
    read_text (std::size_t capacity, char const *&data,
               std::size_t &size) LUMEX_NOEXCEPT
    {
      std::uint8_t const *bytes = nullptr;
      if (!take (capacity, bytes))
        return false;
      data = as_chars (bytes);
      size = capacity;
      return true;
    }

    /// @brief The rest of the message as characters.
    bool
    read_token (char const *&data, std::size_t &size) LUMEX_NOEXCEPT
    {
      std::uint8_t const *bytes = nullptr;
      take_rest (bytes, size);
      data = as_chars (bytes);
      return true;
    }

    /// @brief The rest of the message.
    bool
    read_rest (std::uint8_t const *&data, std::size_t &size) LUMEX_NOEXCEPT
    {
      take_rest (data, size);
      return true;
    }

    /// @brief An integer of `bytes` bytes as digits at the end of the
    /// message.
    bool
    read_last_integer (std::size_t bytes, byte_order, bool is_signed,
                       std::uint64_t &bits) LUMEX_NOEXCEPT
    {
      std::size_t const width = integer_width (bytes, is_signed);
      std::uint8_t const *data = nullptr;
      if (!take_last (width, data))
        return false;
      return parse (as_chars (data), width, bytes, is_signed, bits);
    }

    /// @brief Whether the whole message was read; `trailing_bytes` if not.
    bool
    finish () LUMEX_NOEXCEPT
    {
      return remaining_bytes () == 0
                 ? true
                 : fail (::lumex::core::protocol::error::protocol_errc::
                             trailing_bytes);
    }

  private:
    bool
    parse (char const *data, std::size_t width, std::size_t bytes,
           bool is_signed, std::uint64_t &bits) LUMEX_NOEXCEPT
    {
      bool ok = false;
      if (Radix == 16)
        ok = detail::parse_hex (data, width, bits);
      else
        {
          // A fixed-width number may be padded with spaces on the left.
          std::size_t skip = 0;
          while (skip < width && detail::is_space (data[skip]))
            ++skip;
          std::uint64_t magnitude = 0;
          bool negative = false;
          ok = detail::parse_decimal (data + skip, width - skip, is_signed,
                                      magnitude, negative)
               && detail::decimal_to_bits (magnitude, negative, bytes,
                                           is_signed, bits);
        }
      return ok ? true
                : fail (::lumex::core::protocol::error::protocol_errc::
                            invalid_value);
    }
  };

  /// @brief Writes the fields of one message.
  class writer : public detail::writer_base
  {
  public:
    /// See `reader`.
    using preserves_bits = std::integral_constant<bool, Radix == 16>;
    /// Not a tokenized text.
    using is_tokenized = std::false_type;
    /// See `reader`.
    using supports_floating = std::integral_constant<bool, Radix == 16>;

    /// @brief Appends to `output`.
    explicit writer (bytes_t &output,
                     decode_context_t const *context = nullptr) LUMEX_NOEXCEPT
        : detail::writer_base (output, context)
    {
    }

    /// @brief An integer of `bytes` bytes as digits.
    bool
    write_integer (std::size_t bytes, byte_order, bool is_signed,
                   std::uint64_t bits)
    {
      char text[24] = {};
      std::size_t const width = integer_width (bytes, is_signed);
      bool const ok = Radix == 16 ? detail::format_hex (bits, width, text)
                                  : detail::format_decimal_fixed (
                                        bits, bytes, is_signed, width, text);
      if (!ok)
        return fail (
            ::lumex::core::protocol::error::protocol_errc::invalid_value);
      put (text, width);
      return true;
    }

    /// @brief The IEEE 754 bits of a float or a double as hexadecimal
    /// digits.
    bool
    write_floating (std::size_t bytes, byte_order order, std::uint64_t bits)
    {
      return write_integer (bytes, order, false, bits);
    }

    /// @brief `width` hexadecimal digits.
    bool
    write_hex_text (std::size_t width, std::uint64_t value)
    {
      char digits[16] = {};
      if (width > sizeof (digits)
          || !detail::format_hex (value, width, digits))
        return fail (
            ::lumex::core::protocol::error::protocol_errc::invalid_value);
      put (digits, width);
      return true;
    }

    /// @brief A decimal number of a fixed width.
    bool
    write_decimal_text (std::size_t int_digits, std::size_t frac_digits,
                        std::int64_t scaled)
    {
      char text[40] = {};
      if (int_digits + frac_digits > 18
          || !detail::format_fixed (scaled, int_digits, frac_digits, text))
        return fail (
            ::lumex::core::protocol::error::protocol_errc::invalid_value);
      put (text, int_digits + (frac_digits == 0 ? 0u : 1u + frac_digits));
      return true;
    }

    /// @brief One character.
    bool
    write_char (char value)
    {
      put (value);
      return true;
    }

    /// @brief `size` characters padded with NUL to `capacity`.
    bool
    write_text (std::size_t capacity, char const *data, std::size_t size)
    {
      if (size > capacity)
        return fail (
            ::lumex::core::protocol::error::protocol_errc::invalid_value);
      put (data, size);
      for (std::size_t i = size; i < capacity; ++i)
        put ('\0');
      return true;
    }

    /// @brief The characters of a token (the rest of the message).
    bool
    write_token (char const *data, std::size_t size)
    {
      put (data, size);
      return true;
    }

    /// @brief Bytes as they are (the rest of the message).
    bool
    write_rest (std::uint8_t const *data, std::size_t size)
    {
      put_bytes (data, size);
      return true;
    }
  };

  /// @brief No key prefix (the header carries the key).
  template <typename Row>
  static bool
  read_prefix (reader &) LUMEX_NOEXCEPT
  {
    return true;
  }

  /// @copydoc read_prefix
  template <typename Row>
  static bool
  write_prefix (writer &) LUMEX_NOEXCEPT
  {
    return true;
  }
};

/**
 * @brief The tokenized text layout: the key of the row, then the fields as
 * tokens (see the file description).
 * @tparam Separator Between two fields.
 * @tparam KeySeparator Between the key and the first field.
 * @tparam Radix 10 (decimal integers, the default) or 16 (hexadecimal bits).
 * @tparam Trim Whether spaces around a token are ignored on decode.
 */
template <char Separator = ',', char KeySeparator = Separator,
          unsigned Radix = 10, bool Trim = true>
struct tokenized_text_layout
{
  static_assert (Radix == 16 || Radix == 10,
                 "tokenized_text_layout: Radix is 10 or 16");

  /// @brief No alignment in a text.
  template <typename Impl> struct is_aligned : std::true_type
  {
  };

  /// @brief Reads the tokens of one message.
  class reader : public detail::reader_base
  {
  public:
    /// Hexadecimal tokens carry the bits of an integer.
    using preserves_bits = std::integral_constant<bool, Radix == 16>;
    /// A tokenized text.
    using is_tokenized = std::true_type;
    /// Floating fields are decimal tokens.
    using supports_floating = std::true_type;

    /// @brief Reads `input`; `context` gives the session values.
    explicit reader (bytes_view_t input,
                     decode_context_t const *context = nullptr) LUMEX_NOEXCEPT
        : detail::reader_base (input, context),
          m_more (input.size () != 0),
          m_empty (input.size () == 0)
    {
    }

    /// @brief The tokens left.
    std::size_t
    remaining_units () const LUMEX_NOEXCEPT
    {
      if (!m_more)
        return 0;
      std::size_t count = 1;
      for (std::size_t i = m_position; i < m_end; ++i)
        if (static_cast<char> (m_data[i]) == Separator)
          ++count;
      return count;
    }

    /// @brief Reads and checks the key prefix; `unknown_frame` when it is
    /// not `key`.
    bool
    read_key (char const *key, std::size_t key_size) LUMEX_NOEXCEPT
    {
      std::size_t split = m_position;
      while (split < m_end
             && static_cast<char> (m_data[split]) != KeySeparator)
        ++split;
      char const *data = as_chars (m_data + m_position);
      std::size_t size = split - m_position;
      trim (data, size);
      m_empty = false;
      if (split < m_end)
        {
          m_position = split + 1u;
          m_more = true;
        }
      else
        {
          m_position = m_end;
          m_more = false;
        }
      if (size != key_size
          || (size != 0 && std::memcmp (data, key, size) != 0))
        return fail (
            ::lumex::core::protocol::error::protocol_errc::unknown_frame);
      return true;
    }

    /// @brief An integer token of a field of `bytes` bytes.
    bool
    read_integer (std::size_t bytes, byte_order, bool is_signed,
                  std::uint64_t &bits) LUMEX_NOEXCEPT
    {
      char const *data = nullptr;
      std::size_t size = 0;
      if (!next (data, size))
        return false;
      return parse_integer (data, size, bytes, is_signed, bits);
    }

    /// @brief A decimal token as the bits of a float or a double.
    bool
    read_floating (std::size_t bytes, byte_order,
                   std::uint64_t &bits) LUMEX_NOEXCEPT
    {
      char const *data = nullptr;
      std::size_t size = 0;
      if (!next (data, size))
        return false;
      if (!detail::parse_floating (data, size, bytes, bits))
        return fail (
            ::lumex::core::protocol::error::protocol_errc::invalid_value);
      return true;
    }

    /// @brief A token of at most `width` hexadecimal digits.
    bool
    read_hex_text (std::size_t width, std::uint64_t &value) LUMEX_NOEXCEPT
    {
      char const *data = nullptr;
      std::size_t size = 0;
      if (!next (data, size))
        return false;
      if (size > width || !detail::parse_hex (data, size, value))
        return fail (
            ::lumex::core::protocol::error::protocol_errc::invalid_value);
      return true;
    }

    /// @brief A decimal token with at most `frac_digits` fraction digits and
    /// at most `int_digits` integer digits.
    bool
    read_decimal_text (std::size_t int_digits, std::size_t frac_digits,
                       std::int64_t &scaled) LUMEX_NOEXCEPT
    {
      char const *data = nullptr;
      std::size_t size = 0;
      if (!next (data, size))
        return false;
      std::int64_t value = 0;
      if (!detail::parse_fixed (data, size, frac_digits, value)
          || !detail::fits_fixed (value, int_digits, frac_digits))
        return fail (
            ::lumex::core::protocol::error::protocol_errc::invalid_value);
      scaled = value;
      return true;
    }

    /// @brief A token of one character.
    bool
    read_char (char &value) LUMEX_NOEXCEPT
    {
      char const *data = nullptr;
      std::size_t size = 0;
      if (!next (data, size))
        return false;
      if (size != 1)
        return fail (
            ::lumex::core::protocol::error::protocol_errc::invalid_value);
      value = data[0];
      return true;
    }

    /// @brief A token of at most `capacity` characters; a NUL has no
    /// place in a text token (`invalid_value`).
    bool
    read_text (std::size_t capacity, char const *&data,
               std::size_t &size) LUMEX_NOEXCEPT
    {
      if (!next (data, size))
        return false;
      if (size > capacity || detail::contains (data, size, '\0'))
        return fail (
            ::lumex::core::protocol::error::protocol_errc::invalid_value);
      return true;
    }

    /// @brief The next token.
    bool
    read_token (char const *&data, std::size_t &size) LUMEX_NOEXCEPT
    {
      return next (data, size);
    }

    /// @brief The rest of the message, separators included.
    bool
    read_rest (std::uint8_t const *&data, std::size_t &size) LUMEX_NOEXCEPT
    {
      take_rest (data, size);
      m_more = false;
      return true;
    }

    /// @brief The last token as an integer of `bytes` bytes.
    bool
    read_last_integer (std::size_t bytes, byte_order, bool is_signed,
                       std::uint64_t &bits) LUMEX_NOEXCEPT
    {
      if (!m_more)
        return fail (::lumex::core::protocol::error::protocol_errc::truncated);
      std::size_t split = m_end;
      while (split > m_position
             && static_cast<char> (m_data[split - 1u]) != Separator)
        --split;
      char const *data = as_chars (m_data + split);
      std::size_t size = m_end - split;
      if (split > m_position)
        m_end = split - 1u;
      else
        {
          m_end = m_position;
          m_more = false;
        }
      trim (data, size);
      return parse_integer (data, size, bytes, is_signed, bits);
    }

    /// @brief Whether every token was read; `trailing_bytes` if not.
    bool
    finish () LUMEX_NOEXCEPT
    {
      return !m_more ? true
                     : fail (::lumex::core::protocol::error::protocol_errc::
                                 trailing_bytes);
    }

  private:
    static void
    trim (char const *&data, std::size_t &size) LUMEX_NOEXCEPT
    {
      if (!Trim)
        return;
      while (size != 0 && detail::is_space (data[0]))
        {
          ++data;
          --size;
        }
      while (size != 0 && detail::is_space (data[size - 1u]))
        --size;
    }

    bool
    next (char const *&data, std::size_t &size) LUMEX_NOEXCEPT
    {
      // An empty message without a key is one empty token when the layout
      // reads one (what a writer makes of one empty token).
      if (m_empty)
        {
          m_empty = false;
          data = as_chars (m_data + m_position);
          size = 0;
          return true;
        }
      if (!m_more)
        return fail (::lumex::core::protocol::error::protocol_errc::truncated);
      std::size_t split = m_position;
      while (split < m_end && static_cast<char> (m_data[split]) != Separator)
        ++split;
      data = as_chars (m_data + m_position);
      size = split - m_position;
      if (split < m_end)
        m_position = split + 1u;
      else
        {
          m_position = m_end;
          m_more = false;
        }
      trim (data, size);
      return true;
    }

    bool
    parse_integer (char const *data, std::size_t size, std::size_t bytes,
                   bool is_signed, std::uint64_t &bits) LUMEX_NOEXCEPT
    {
      bool ok = false;
      if (Radix == 16)
        ok = size <= 2u * bytes && detail::parse_hex (data, size, bits);
      else
        {
          std::uint64_t magnitude = 0;
          bool negative = false;
          ok = detail::parse_decimal (data, size, is_signed, magnitude,
                                      negative)
               && detail::decimal_to_bits (magnitude, negative, bytes,
                                           is_signed, bits);
        }
      return ok ? true
                : fail (::lumex::core::protocol::error::protocol_errc::
                            invalid_value);
    }

    bool m_more;
    bool m_empty;
  };

  /// @brief Writes the key and the tokens of one message.
  class writer : public detail::writer_base
  {
  public:
    /// See `reader`.
    using preserves_bits = std::integral_constant<bool, Radix == 16>;
    /// A tokenized text.
    using is_tokenized = std::true_type;
    /// Floating fields are decimal tokens.
    using supports_floating = std::true_type;

    /// @brief Appends to `output`.
    explicit writer (bytes_t &output,
                     decode_context_t const *context = nullptr) LUMEX_NOEXCEPT
        : detail::writer_base (output, context),
          m_pending (false),
          m_separator (Separator),
          m_tokens (0)
    {
    }

    /// @brief The tokens written so far (the unit of the layout).
    std::size_t
    units_written () const LUMEX_NOEXCEPT
    {
      return m_tokens;
    }

    /// @brief Writes the key; the first field follows after
    /// `KeySeparator`. `invalid_value` when the key holds the separator.
    bool
    write_key (char const *key, std::size_t key_size)
    {
      if (detail::contains (key, key_size, KeySeparator))
        return fail (
            ::lumex::core::protocol::error::protocol_errc::invalid_value);
      put (key, key_size);
      m_pending = true;
      m_separator = KeySeparator;
      return true;
    }

    /// @brief An integer token.
    bool
    write_integer (std::size_t bytes, byte_order, bool is_signed,
                   std::uint64_t bits)
    {
      char text[24] = {};
      std::size_t size = 0;
      if (Radix == 16)
        {
          std::size_t digits = 1;
          while (digits < 16 && (bits >> (4u * digits)) != 0)
            ++digits;
          detail::format_hex (bits, digits, text);
          size = digits;
        }
      else
        {
          std::uint64_t magnitude = bits;
          if (is_signed)
            {
              std::int64_t const value
                  = ::lumex::core::protocol::field::detail::sign_extend (
                      bits, 8u * bytes);
              if (value < 0)
                {
                  text[size++] = '-';
                  magnitude = ~static_cast<std::uint64_t> (value) + 1u;
                }
            }
          size += detail::decimal_digits (magnitude, text + size);
        }
      return token (text, size);
    }

    /// @brief A decimal token of a float or a double.
    bool
    write_floating (std::size_t bytes, byte_order, std::uint64_t bits)
    {
      char text[32] = {};
      std::size_t const size = detail::format_floating (bits, bytes, text);
      if (size == 0)
        return fail (
            ::lumex::core::protocol::error::protocol_errc::invalid_value);
      return token (text, size);
    }

    /// @brief `width` hexadecimal digits.
    bool
    write_hex_text (std::size_t width, std::uint64_t value)
    {
      char digits[16] = {};
      if (width > sizeof (digits)
          || !detail::format_hex (value, width, digits))
        return fail (
            ::lumex::core::protocol::error::protocol_errc::invalid_value);
      return token (digits, width);
    }

    /// @brief A decimal number of the full width.
    bool
    write_decimal_text (std::size_t int_digits, std::size_t frac_digits,
                        std::int64_t scaled)
    {
      char text[40] = {};
      if (int_digits + frac_digits > 18
          || !detail::format_fixed (scaled, int_digits, frac_digits, text))
        return fail (
            ::lumex::core::protocol::error::protocol_errc::invalid_value);
      return token (text,
                    int_digits + (frac_digits == 0 ? 0u : 1u + frac_digits));
    }

    /// @brief A token of one character.
    bool
    write_char (char value)
    {
      return token (&value, 1);
    }

    /// @brief A token of at most `capacity` characters.
    bool
    write_text (std::size_t capacity, char const *data, std::size_t size)
    {
      if (size > capacity)
        return fail (
            ::lumex::core::protocol::error::protocol_errc::invalid_value);
      return token (data, size);
    }

    /// @brief A token.
    bool
    write_token (char const *data, std::size_t size)
    {
      return token (data, size);
    }

    /// @brief Bytes as they are (the rest of the message).
    bool
    write_rest (std::uint8_t const *data, std::size_t size)
    {
      separate ();
      put_bytes (data, size);
      // A reader counts the separators of the rest as tokens.
      for (std::size_t i = 0; i < size; ++i)
        if (static_cast<char> (data[i]) == Separator)
          ++m_tokens;
      return true;
    }

  private:
    void
    separate ()
    {
      if (m_pending)
        put (m_separator);
      m_pending = true;
      m_separator = Separator;
      ++m_tokens;
    }

    // A token never holds the separator, and with Trim it neither starts
    // nor ends with a space (the decoder would drop it).
    bool
    token (char const *data, std::size_t size)
    {
      if (detail::contains (data, size, Separator)
          || (Trim && size != 0
              && (detail::is_space (data[0])
                  || detail::is_space (data[size - 1u]))))
        return fail (
            ::lumex::core::protocol::error::protocol_errc::invalid_value);
      separate ();
      put (data, size);
      return true;
    }

    bool m_pending;
    char m_separator;
    std::size_t m_tokens;
  };

  /// @brief Reads and checks the key of `Row`; `unknown_frame` when the
  /// message has another key.
  template <typename Row>
  static bool
  read_prefix (reader &input) LUMEX_NOEXCEPT
  {
    detail::key_text const key (Row::key ());
    return input.read_key (key.data, key.size);
  }

  /// @brief Writes the key of `Row`.
  template <typename Row>
  static bool
  write_prefix (writer &output)
  {
    detail::key_text const key (Row::key ());
    return output.write_key (key.data, key.size);
  }
};
} // namespace layout
} // namespace protocol
} // namespace core
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_PROTOCOL_LAYOUT_TEXT_LAYOUTS_HPP
