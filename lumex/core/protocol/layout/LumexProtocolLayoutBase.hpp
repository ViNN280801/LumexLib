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
 * @file LumexProtocolLayoutBase.hpp
 * @brief What the three layouts share: the byte types, the reader and writer
 * bases (error state, session context, bounds) and the conversions of
 * numbers to and from hexadecimal and decimal text.
 * @details A layout `L` has `L::reader` and `L::writer`; the field types call
 * their primitives (see `field/LumexProtocolFieldTraits.hpp`):
 * - `read_integer (bytes, order, is_signed, bits)` /
 *   `write_integer (...)`: an integer of `bytes` bytes (its two's complement
 *   bits);
 * - `read_floating (bytes, order, bits)` / `write_floating (...)`: the IEEE
 *   754 bits of a float or a double;
 * - `read_hex_text (width, value)`, `read_decimal_text (int_digits,
 *   frac_digits, scaled)`, `read_char (c)`, `read_text (capacity, data,
 *   size)`, `read_token (data, size)`, `read_rest (data, size)` and their
 *   `write_*`;
 * - `read_last_integer (...)`: an integer at the end of the message;
 * - `remaining_units ()`: what is left in the unit of the layout: bytes
 *   (binary), characters (fixed width), tokens (tokenized);
 *   `units_written ()` of a writer counts the same way; `context ()`: the
 *   session values; `fail (code)`: records the first error and returns
 *   false.
 * A reader never reads past its input: every primitive checks the bounds
 * first and fails with `truncated`.
 */
#ifndef LUMEX_CORE_PROTOCOL_LAYOUT_BASE_HPP
#define LUMEX_CORE_PROTOCOL_LAYOUT_BASE_HPP

#include <clocale>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <system_error>
#include <vector>

#include "lumex/core/protocol/error/LumexProtocolErrc.hpp"
#include "lumex/core/protocol/field/LumexProtocolContext.hpp"
#include "lumex/core/protocol/field/LumexProtocolFieldTraits.hpp"
#include "lumex/core/protocol/field/LumexProtocolNumeric.hpp"
#include "lumex/core/protocol/field/LumexProtocolValues.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#if __has_warning("-Wlifetime-safety-suggestions")
// Clang 23 suggests [[clang::lifetimebound]] for every function that returns
// or stores a parameter (a suggestion, not a finding).
#pragma clang diagnostic ignored "-Wlifetime-safety-suggestions"
#endif
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#if __has_warning("-Wunsafe-buffer-usage-in-libc-call")
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-libc-call"
#endif
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#pragma clang diagnostic ignored "-Wfloat-equal"
#endif

#if defined(_MSC_VER) && !defined(__clang__)
#pragma warning(push)
// snprintf and strtod are the standard functions; the secure variants are
// not portable.
#pragma warning(disable : 4996)
#endif

namespace lumex
{
namespace core
{
namespace protocol
{
namespace layout
{
/// @brief A read-only view of bytes: a message to decode.
typedef ::lumex::core::protocol::field::bytes_view_t bytes_view_t;

/// @brief An owning byte buffer: where a message is encoded (appended).
typedef std::vector<std::uint8_t> bytes_t;

/// @brief The session values a decoder may read.
typedef ::lumex::core::protocol::field::decode_context_t decode_context_t;

/// @brief The byte order of a multi-byte field.
typedef ::lumex::core::protocol::field::byte_order byte_order;

namespace detail
{
/// The value of a hexadecimal digit, -1 for another character.
LUMEX_CONSTEXPR inline int
hex_value (char c) LUMEX_NOEXCEPT
{
  return (c >= '0' && c <= '9')   ? c - '0'
         : (c >= 'A' && c <= 'F') ? c - 'A' + 10
         : (c >= 'a' && c <= 'f') ? c - 'a' + 10
                                  : -1;
}

/// Upper case hexadecimal digit of 0..15.
LUMEX_CONSTEXPR inline char
hex_digit (unsigned value) LUMEX_NOEXCEPT
{
  return "0123456789ABCDEF"[value & 15u];
}

/// Whether `c` is a space a text layout trims.
LUMEX_CONSTEXPR inline bool
is_space (char c) LUMEX_NOEXCEPT
{
  return c == ' ' || c == '\t';
}

/// Digits `[data, data + size)`, all hexadecimal, at most 16: the value.
inline bool
parse_hex (char const *data, std::size_t size,
           std::uint64_t &value) LUMEX_NOEXCEPT
{
  if (size == 0 || size > 16)
    return false;
  std::uint64_t result = 0;
  for (std::size_t i = 0; i < size; ++i)
    {
      int const digit = hex_value (data[i]);
      if (digit < 0)
        return false;
      result = (result << 4) | static_cast<std::uint64_t> (digit);
    }
  value = result;
  return true;
}

/// `width` upper case digits of `value`, zero-padded; false when the value
/// needs more digits.
inline bool
format_hex (std::uint64_t value, std::size_t width, char *out) LUMEX_NOEXCEPT
{
  if (width < 16 && (value >> (4u * width)) != 0)
    return false;
  for (std::size_t i = 0; i < width; ++i)
    out[width - 1u - i]
        = hex_digit (static_cast<unsigned> ((value >> (4u * i)) & 15u));
  return true;
}

/**
 * A decimal integer: an optional sign (`-` only when `allow_sign`) and
 * digits, the magnitude and the sign; false on overflow of 64 bits.
 * Leading zeros are fine, spaces are not.
 */
inline bool
parse_decimal (char const *data, std::size_t size, bool allow_sign,
               std::uint64_t &magnitude, bool &negative) LUMEX_NOEXCEPT
{
  std::size_t i = 0;
  negative = false;
  if (i < size && (data[i] == '-' || data[i] == '+'))
    {
      if (!allow_sign && data[i] == '-')
        return false;
      negative = data[i] == '-';
      ++i;
    }
  if (i == size)
    return false;
  std::uint64_t result = 0;
  for (; i < size; ++i)
    {
      if (data[i] < '0' || data[i] > '9')
        return false;
      std::uint64_t const digit = static_cast<std::uint64_t> (data[i] - '0');
      if (result > (~static_cast<std::uint64_t> (0) - digit) / 10u)
        return false;
      result = result * 10u + digit;
    }
  magnitude = result;
  return true;
}

/// The bits of a decimal integer of `bytes` bytes; false out of the range.
inline bool
decimal_to_bits (std::uint64_t magnitude, bool negative, std::size_t bytes,
                 bool is_signed, std::uint64_t &bits) LUMEX_NOEXCEPT
{
  std::size_t const width = 8u * bytes;
  if (!is_signed)
    {
      if ((negative && magnitude != 0)
          || !::lumex::core::protocol::field::detail::fits_unsigned_bits (
              magnitude, width))
        return false;
      bits = magnitude;
      return true;
    }
  std::uint64_t const limit = (static_cast<std::uint64_t> (1) << (width - 1u))
                              - (negative ? 0u : 1u);
  if (magnitude > limit)
    return false;
  bits = (negative ? (~magnitude + 1u) : magnitude)
         & ::lumex::core::protocol::field::detail::low_bits (width);
  return true;
}

/// The decimal digits of `value` into `out` (no padding): the count (at
/// most 20).
inline std::size_t
decimal_digits (std::uint64_t value, char *out) LUMEX_NOEXCEPT
{
  char reversed[20];
  std::size_t count = 0;
  do
    {
      reversed[count++] = static_cast<char> ('0' + value % 10u);
      value /= 10u;
    }
  while (value != 0);
  for (std::size_t i = 0; i < count; ++i)
    out[i] = reversed[count - 1u - i];
  return count;
}

/// The number of characters of a decimal integer of `bytes` bytes in a
/// fixed-width layout: the digits of the largest value, plus one for the
/// sign of a signed field.
LUMEX_CONSTEXPR inline std::size_t
decimal_width (std::size_t bytes, bool is_signed) LUMEX_NOEXCEPT
{
  return (bytes == 1   ? 3u
          : bytes == 2 ? 5u
          : bytes == 3 ? 8u
          : bytes == 4 ? 10u
          : bytes == 5 ? 13u
          : bytes == 6 ? 15u
          : bytes == 7 ? 17u
                       : 20u)
         + (is_signed ? 1u : 0u);
}

/// A decimal integer of `bits` (two's complement of `bytes` bytes when
/// signed) in exactly `width` characters, zero-padded, `-` first when
/// negative; false when it does not fit.
inline bool
format_decimal_fixed (std::uint64_t bits, std::size_t bytes, bool is_signed,
                      std::size_t width, char *out) LUMEX_NOEXCEPT
{
  bool negative = false;
  std::uint64_t magnitude = bits;
  if (is_signed)
    {
      std::int64_t const value
          = ::lumex::core::protocol::field::detail::sign_extend (bits,
                                                                 8u * bytes);
      negative = value < 0;
      magnitude = negative ? (~static_cast<std::uint64_t> (value) + 1u)
                           : static_cast<std::uint64_t> (value);
    }
  char digits[20];
  std::size_t const count = decimal_digits (magnitude, digits);
  std::size_t const room = width - (negative ? 1u : 0u);
  if (width == 0 || count > room)
    return false;
  std::size_t at = 0;
  if (negative)
    out[at++] = '-';
  for (std::size_t i = count; i < room; ++i)
    out[at++] = '0';
  for (std::size_t i = 0; i < count; ++i)
    out[at++] = digits[i];
  return true;
}

/// The value of 10 to the power `n` (n at most 18).
LUMEX_CONSTEXPR inline std::int64_t
power10 (std::size_t n) LUMEX_NOEXCEPT
{
  return n == 0 ? 1 : 10 * power10 (n - 1u);
}

/**
 * A decimal number with a fixed point: spaces, an optional sign, digits, an
 * optional point and at most `frac_digits` digits, spaces. The value in units
 * of the last of `frac_digits` digits; false for another text or more than
 * 18 significant digits.
 */
inline bool
parse_fixed (char const *data, std::size_t size, std::size_t frac_digits,
             std::int64_t &scaled) LUMEX_NOEXCEPT
{
  std::size_t first = 0;
  std::size_t last = size;
  while (first < last && is_space (data[first]))
    ++first;
  while (last > first && is_space (data[last - 1u]))
    --last;
  bool negative = false;
  if (first < last && (data[first] == '-' || data[first] == '+'))
    {
      negative = data[first] == '-';
      ++first;
    }
  std::uint64_t value = 0;
  std::size_t digits = 0;
  std::size_t fraction = 0;
  bool point = false;
  for (std::size_t i = first; i < last; ++i)
    {
      char const c = data[i];
      if (c == '.' && !point)
        {
          point = true;
          continue;
        }
      if (c < '0' || c > '9')
        return false;
      if (point)
        {
          if (fraction == frac_digits)
            return false;
          ++fraction;
        }
      if (value != 0 || c != '0')
        ++digits;
      if (digits > 18)
        return false;
      value = value * 10u + static_cast<std::uint64_t> (c - '0');
    }
  if (first == last || (last - first == 1u && point))
    return false;
  std::int64_t const factor = power10 (frac_digits - fraction);
  std::int64_t const magnitude = static_cast<std::int64_t> (value);
  if (magnitude > (std::numeric_limits<std::int64_t>::max) () / factor)
    return false;
  scaled = negative ? -(magnitude * factor) : magnitude * factor;
  return true;
}

/**
 * `scaled` (units of the last of `frac_digits` digits) as `int_digits`
 * digits, a point and `frac_digits` digits (no point without them),
 * zero-padded, `-` first when negative: exactly `width` characters; false
 * when it does not fit.
 */
inline bool
format_fixed (std::int64_t scaled, std::size_t int_digits,
              std::size_t frac_digits, char *out) LUMEX_NOEXCEPT
{
  std::size_t const width
      = int_digits + (frac_digits == 0 ? 0u : 1u + frac_digits);
  bool const negative = scaled < 0;
  std::uint64_t const magnitude
      = negative ? (~static_cast<std::uint64_t> (scaled) + 1u)
                 : static_cast<std::uint64_t> (scaled);
  std::uint64_t const factor
      = static_cast<std::uint64_t> (power10 (frac_digits));
  std::uint64_t const whole = magnitude / factor;
  std::uint64_t const part = magnitude % factor;
  char digits[20];
  std::size_t const count = decimal_digits (whole, digits);
  std::size_t const room = int_digits - (negative ? 1u : 0u);
  if (int_digits == 0 || (negative && int_digits < 2) || count > room)
    return false;
  std::size_t at = 0;
  if (negative)
    out[at++] = '-';
  for (std::size_t i = count; i < room; ++i)
    out[at++] = '0';
  for (std::size_t i = 0; i < count; ++i)
    out[at++] = digits[i];
  if (frac_digits != 0)
    {
      out[at++] = '.';
      for (std::size_t i = 0; i < frac_digits; ++i)
        {
          std::uint64_t const divisor
              = static_cast<std::uint64_t> (power10 (frac_digits - 1u - i));
          out[at++] = static_cast<char> ('0' + (part / divisor) % 10u);
        }
    }
  return at == width;
}

/// Whether `scaled` fits `int_digits` digits (one of them the sign when it
/// is negative) and `frac_digits` fraction digits: what `format_fixed` can
/// write, so a decoded number always encodes again.
inline bool
fits_fixed (std::int64_t scaled, std::size_t int_digits,
            std::size_t frac_digits) LUMEX_NOEXCEPT
{
  char text[40];
  return int_digits + frac_digits <= 18
         && format_fixed (scaled, int_digits, frac_digits, text);
}

/// The decimal point of the current C locale (`.` in the "C" locale).
inline char
locale_point () LUMEX_NOEXCEPT
{
  std::lconv const *const conventions = std::localeconv ();
  return conventions != nullptr && conventions->decimal_point != nullptr
                 && conventions->decimal_point[0] != '\0'
             ? conventions->decimal_point[0]
             : '.';
}

/// A decimal floating number (`-1.5e3`) as the bits of a float (4 bytes) or
/// a double (8 bytes); false for another text and for a number out of the
/// range of the type.
inline bool
parse_floating (char const *data, std::size_t size, std::size_t bytes,
                std::uint64_t &bits) LUMEX_NOEXCEPT
{
  char buffer[64];
  if (size == 0 || size >= sizeof (buffer))
    return false;
  char const point = locale_point ();
  for (std::size_t i = 0; i < size; ++i)
    {
      char const c = data[i];
      if (!((c >= '0' && c <= '9') || c == '.' || c == '-' || c == '+'
            || c == 'e' || c == 'E'))
        return false;
      buffer[i] = c == '.' ? point : c;
    }
  buffer[size] = '\0';
  char *end = nullptr;
  double const value = std::strtod (buffer, &end);
  // A number out of the range of the field is no value of it.
  if (end != buffer + size || !(value - value == 0.0))
    return false;
  if (bytes == 4)
    {
      // A double that rounds to the largest float (3.40282347e+38, the text
      // of FLT_MAX) is that float; only one that narrows to an infinity is
      // out of the range.
      float const narrow = static_cast<float> (value);
      if (!(narrow - narrow == 0.0f))
        return false;
      std::uint32_t word = 0;
      std::memcpy (&word, &narrow, sizeof (word));
      bits = word;
    }
  else
    std::memcpy (&bits, &value, sizeof (bits));
  return true;
}

/// The decimal text that reads back as the same float or double (`%.9g`,
/// `%.17g`), with `.` as the point: the number of characters; 0 for an
/// infinity or a NaN.
inline std::size_t
format_floating (std::uint64_t bits, std::size_t bytes,
                 char (&out)[32]) LUMEX_NOEXCEPT
{
  double value = 0;
  int precision = 17;
  if (bytes == 4)
    {
      std::uint32_t const word = static_cast<std::uint32_t> (bits);
      float narrow = 0;
      std::memcpy (&narrow, &word, sizeof (narrow));
      value = static_cast<double> (narrow);
      precision = 9;
    }
  else
    std::memcpy (&value, &bits, sizeof (value));
  // An infinity or a NaN has no decimal text a decoder accepts.
  if (!(value - value == 0.0))
    return 0;
  int const written
      = std::snprintf (out, sizeof (out), "%.*g", precision, value);
  if (written <= 0 || static_cast<std::size_t> (written) >= sizeof (out))
    return 0;
  char const point = locale_point ();
  for (int i = 0; i < written; ++i)
    if (out[i] == point)
      out[i] = '.';
  return static_cast<std::size_t> (written);
}

/// The common state of a reader: the input, the position, the session and
/// the first error.
class reader_base
{
public:
  reader_base (bytes_view_t input,
               decode_context_t const *context) LUMEX_NOEXCEPT
      : m_data (input.data ()),
        m_end (input.size ()),
        m_position (0),
        m_context (context),
        m_error ()
  {
  }

  /// Records `code` unless an error is recorded already; returns false.
  bool
  fail (::lumex::core::protocol::error::protocol_errc code) LUMEX_NOEXCEPT
  {
    if (!m_error)
      m_error = ::lumex::core::protocol::error::make_error_code (code);
    return false;
  }

  /// The first error, or no error.
  std::error_code
  error () const LUMEX_NOEXCEPT
  {
    return m_error;
  }

  /// The session values, `nullptr` without a session.
  decode_context_t const *
  context () const LUMEX_NOEXCEPT
  {
    return m_context;
  }

  /// The offset of the next unread byte.
  std::size_t
  position () const LUMEX_NOEXCEPT
  {
    return m_position;
  }

protected:
  std::size_t
  remaining_bytes () const LUMEX_NOEXCEPT
  {
    return m_end - m_position;
  }

  /// The next `count` bytes; `truncated` when fewer are left.
  bool
  take (std::size_t count, std::uint8_t const *&data) LUMEX_NOEXCEPT
  {
    if (count > remaining_bytes ())
      return fail (::lumex::core::protocol::error::protocol_errc::truncated);
    data = m_data + m_position;
    m_position += count;
    return true;
  }

  /// The last `count` bytes, which the reader then no longer reads.
  bool
  take_last (std::size_t count, std::uint8_t const *&data) LUMEX_NOEXCEPT
  {
    if (count > remaining_bytes ())
      return fail (::lumex::core::protocol::error::protocol_errc::truncated);
    m_end -= count;
    data = m_data + m_end;
    return true;
  }

  /// Everything left.
  void
  take_rest (std::uint8_t const *&data, std::size_t &size) LUMEX_NOEXCEPT
  {
    data = m_data + m_position;
    size = remaining_bytes ();
    m_position = m_end;
  }

  static char const *
  as_chars (std::uint8_t const *data) LUMEX_NOEXCEPT
  {
    return reinterpret_cast<char const *> (data);
  }

  std::uint8_t const *m_data;
  std::size_t m_end;
  std::size_t m_position;

private:
  decode_context_t const *m_context;
  std::error_code m_error;
};

/// The common state of a writer: the output, the session and the first
/// error.
class writer_base
{
public:
  writer_base (bytes_t &output, decode_context_t const *context) LUMEX_NOEXCEPT
      : m_output (output),
        m_context (context),
        m_error ()
  {
  }

  /// Records `code` unless an error is recorded already; returns false.
  bool
  fail (::lumex::core::protocol::error::protocol_errc code) LUMEX_NOEXCEPT
  {
    if (!m_error)
      m_error = ::lumex::core::protocol::error::make_error_code (code);
    return false;
  }

  /// The first error, or no error.
  std::error_code
  error () const LUMEX_NOEXCEPT
  {
    return m_error;
  }

  /// The session values, `nullptr` without a session.
  decode_context_t const *
  context () const LUMEX_NOEXCEPT
  {
    return m_context;
  }

  /// The units written so far: bytes (binary), characters (fixed width).
  std::size_t
  units_written () const LUMEX_NOEXCEPT
  {
    return m_output.size ();
  }

protected:
  void
  put (char c)
  {
    m_output.push_back (static_cast<std::uint8_t> (c));
  }

  void
  put (char const *data, std::size_t size)
  {
    m_output.insert (m_output.end (),
                     reinterpret_cast<std::uint8_t const *> (data),
                     reinterpret_cast<std::uint8_t const *> (data) + size);
  }

  void
  put_bytes (std::uint8_t const *data, std::size_t size)
  {
    m_output.insert (m_output.end (), data, data + size);
  }

  bytes_t &m_output;

private:
  decode_context_t const *m_context;
  std::error_code m_error;
};

/// The `bytes` bytes of integer bits in `order`.
inline void
store_bits (std::uint64_t bits, std::size_t bytes, byte_order order,
            std::uint8_t *out) LUMEX_NOEXCEPT
{
  for (std::size_t i = 0; i < bytes; ++i)
    {
      std::size_t const shift
          = 8u * (order == byte_order::big ? bytes - 1u - i : i);
      out[i] = static_cast<std::uint8_t> ((bits >> shift) & 0xFFu);
    }
}

/// The integer bits of `bytes` bytes in `order`.
inline std::uint64_t
load_bits (std::uint8_t const *in, std::size_t bytes,
           byte_order order) LUMEX_NOEXCEPT
{
  std::uint64_t bits = 0;
  for (std::size_t i = 0; i < bytes; ++i)
    {
      std::size_t const shift
          = 8u * (order == byte_order::big ? bytes - 1u - i : i);
      bits |= static_cast<std::uint64_t> (in[i]) << shift;
    }
  return bits;
}
} // namespace detail
} // namespace layout
} // namespace protocol
} // namespace core
} // namespace lumex

#if defined(_MSC_VER) && !defined(__clang__)
#pragma warning(pop)
#endif

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_PROTOCOL_LAYOUT_BASE_HPP
