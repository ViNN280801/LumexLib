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
 * @file LumexProtocolNumeric.hpp
 * @brief The integer arithmetic the field types share: masks of a wire
 * width, sign extension, range checks, the conversion of an engineering
 * value to a raw value, and the gather and scatter of the bits of a mask.
 * @details Internal to `core/protocol` (namespace `field::detail`); every
 * function is total: a value that does not fit is reported, never wrapped.
 */
#ifndef LUMEX_CORE_PROTOCOL_FIELD_NUMERIC_HPP
#define LUMEX_CORE_PROTOCOL_FIELD_NUMERIC_HPP

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

#include "lumex/core/protocol/field/LumexProtocolValues.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
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
namespace detail
{
/// The low `bits` bits set (0 to 64).
LUMEX_CONSTEXPR inline std::uint64_t
low_bits (std::size_t bits) LUMEX_NOEXCEPT
{
  return bits >= 64u ? ~static_cast<std::uint64_t> (0)
                     : (static_cast<std::uint64_t> (1) << bits) - 1u;
}

/// The bits of a value `bytes` wide (1 to 8).
LUMEX_CONSTEXPR inline std::uint64_t
width_mask (std::size_t bytes) LUMEX_NOEXCEPT
{
  return low_bits (8u * bytes);
}

/// The two's complement value of the low `bits` bits (1 to 64).
inline std::int64_t
sign_extend (std::uint64_t value, std::size_t bits) LUMEX_NOEXCEPT
{
  std::uint64_t const masked = value & low_bits (bits);
  std::uint64_t const sign = static_cast<std::uint64_t> (1) << (bits - 1u);
  if ((masked & sign) == 0)
    return static_cast<std::int64_t> (masked);
  // The magnitude of a negative value, without overflow for the minimum.
  std::uint64_t const magnitude = ((~masked) & low_bits (bits)) + 1u;
  return magnitude == (static_cast<std::uint64_t> (1) << 63)
             ? (std::numeric_limits<std::int64_t>::min) ()
             : -static_cast<std::int64_t> (magnitude);
}

/// Whether `value` fits `bits` bits unsigned.
LUMEX_CONSTEXPR inline bool
fits_unsigned_bits (std::uint64_t value, std::size_t bits) LUMEX_NOEXCEPT
{
  return (value & ~low_bits (bits)) == 0;
}

/// Whether `value` fits `bits` bits in two's complement.
LUMEX_CONSTEXPR inline bool
fits_signed_bits (std::int64_t value, std::size_t bits) LUMEX_NOEXCEPT
{
  return bits >= 64u
             ? true
             : (value >= -static_cast<std::int64_t> (
                    static_cast<std::uint64_t> (1) << (bits - 1u))
                && value <= static_cast<std::int64_t> (low_bits (bits - 1u)));
}

/// The low `bits` bits of the two's complement of `value`.
LUMEX_CONSTEXPR inline std::uint64_t
to_bits (std::int64_t value, std::size_t bits) LUMEX_NOEXCEPT
{
  return static_cast<std::uint64_t> (value) & low_bits (bits);
}

template <typename Raw, typename Arg>
bool
integral_to_raw_signed (Arg argument, std::size_t bits, Raw &raw,
                        std::true_type) LUMEX_NOEXCEPT
{
  std::int64_t const value = static_cast<std::int64_t> (argument);
  if (std::is_signed<Raw>::value
          ? !fits_signed_bits (value, bits)
          : (value < 0
             || !fits_unsigned_bits (static_cast<std::uint64_t> (value),
                                     bits)))
    return false;
  raw = static_cast<Raw> (value);
  return true;
}

template <typename Raw, typename Arg>
bool
integral_to_raw_signed (Arg argument, std::size_t bits, Raw &raw,
                        std::false_type) LUMEX_NOEXCEPT
{
  std::uint64_t const value = static_cast<std::uint64_t> (argument);
  if (std::is_signed<Raw>::value ? (value > low_bits (bits - 1u))
                                 : !fits_unsigned_bits (value, bits))
    return false;
  raw = static_cast<Raw> (value);
  return true;
}

/**
 * An integer argument (any integral type) as a raw value of `Raw` that fits
 * a wire width of `bits` bits: false when it does not fit.
 */
template <typename Raw, typename Arg>
bool
integral_to_raw (Arg argument, std::size_t bits, Raw &raw) LUMEX_NOEXCEPT
{
  return integral_to_raw_signed (
      argument, bits, raw,
      std::integral_constant<bool, std::is_signed<Arg>::value> ());
}

/// Whether raw value `raw` of a field fits its wire width of `bits` bits.
template <typename Raw>
bool
raw_fits (Raw raw, std::size_t bits) LUMEX_NOEXCEPT
{
  return std::is_signed<Raw>::value
             ? fits_signed_bits (static_cast<std::int64_t> (raw), bits)
             : fits_unsigned_bits (static_cast<std::uint64_t> (raw), bits);
}

/**
 * The raw value of engineering value `value` for a scale `num / den` and a
 * wire width of `bits` bits: rounded to the nearest raw value (halves away
 * from zero); false for NaN, an infinity or a value out of the range.
 */
template <typename Raw>
bool
engineering_to_raw (double value, std::intmax_t num, std::intmax_t den,
                    std::size_t bits, Raw &raw) LUMEX_NOEXCEPT
{
  if (!(value == value))
    return false;
  double const scaled
      = value * static_cast<double> (den) / static_cast<double> (num);
  double const rounded
      = scaled < 0 ? -std::floor (-scaled + 0.5) : std::floor (scaled + 0.5);
  // The bounds of the width as doubles: exact up to 2^63, and the upper
  // bound is exclusive (2^bits or 2^(bits - 1) is a power of two).
  double const top = std::ldexp (
      1.0, static_cast<int> (bits) - (std::is_signed<Raw>::value ? 1 : 0));
  double const bottom = std::is_signed<Raw>::value ? -top : 0.0;
  if (!(rounded >= bottom && rounded < top))
    return false;
  raw = rounded < 0 ? static_cast<Raw> (static_cast<std::int64_t> (rounded))
                    : static_cast<Raw> (static_cast<std::uint64_t> (rounded));
  return true;
}

/// The number of bits set.
LUMEX_CONSTEXPR inline std::size_t
bit_count (std::uint64_t value) LUMEX_NOEXCEPT
{
  return value == 0 ? 0u : 1u + bit_count (value & (value - 1u));
}

/// The position of the lowest bit set (64 for zero).
LUMEX_CONSTEXPR inline std::size_t
lowest_bit (std::uint64_t value) LUMEX_NOEXCEPT
{
  return value == 0 ? 64u
                    : ((value & 1u) != 0 ? 0u : 1u + lowest_bit (value >> 1));
}

/// The bits of `word` under `mask`, packed to the low end in mask order.
inline std::uint64_t
gather_bits (std::uint64_t word, std::uint64_t mask) LUMEX_NOEXCEPT
{
  std::uint64_t result = 0;
  std::size_t out = 0;
  for (std::size_t bit = 0; bit < 64u; ++bit)
    {
      std::uint64_t const one = static_cast<std::uint64_t> (1) << bit;
      if ((mask & one) == 0)
        continue;
      if ((word & one) != 0)
        result |= static_cast<std::uint64_t> (1) << out;
      ++out;
    }
  return result;
}

/// The low bits of `value` spread over the bits of `mask` (the inverse of
/// `gather_bits`).
inline std::uint64_t
scatter_bits (std::uint64_t value, std::uint64_t mask) LUMEX_NOEXCEPT
{
  std::uint64_t result = 0;
  std::size_t in = 0;
  for (std::size_t bit = 0; bit < 64u; ++bit)
    {
      std::uint64_t const one = static_cast<std::uint64_t> (1) << bit;
      if ((mask & one) == 0)
        continue;
      if ((value & (static_cast<std::uint64_t> (1) << in)) != 0)
        result |= one;
      ++in;
    }
  return result;
}

/// The raw value of a numeric field value: the integer, or the raw member of
/// a fixed point.
template <typename T> struct raw_access
{
  using raw_type = T;

  static T
  get (T value) LUMEX_NOEXCEPT
  {
    return value;
  }

  static void
  set (T &value, T raw) LUMEX_NOEXCEPT
  {
    value = raw;
  }
};

template <typename Raw, typename Ratio>
struct raw_access<fixed_point<Raw, Ratio>>
{
  using raw_type = Raw;

  static Raw
  get (fixed_point<Raw, Ratio> const &value) LUMEX_NOEXCEPT
  {
    return value.raw;
  }

  static void
  set (fixed_point<Raw, Ratio> &value, Raw raw) LUMEX_NOEXCEPT
  {
    value.raw = raw;
  }
};

/// A value as a discriminant: integers, characters, enumerators and the raw
/// value of a fixed point.
template <typename T>
std::int64_t
to_discriminant (T const &value) LUMEX_NOEXCEPT
{
  return static_cast<std::int64_t> (raw_access<T>::get (value));
}

/// Whether value `value` of a field equals discriminant `discriminant`.
template <typename T>
bool
equals_discriminant (T const &value, std::int64_t discriminant) LUMEX_NOEXCEPT
{
  return to_discriminant (value) == discriminant;
}
} // namespace detail
} // namespace field
} // namespace protocol
} // namespace core
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_PROTOCOL_FIELD_NUMERIC_HPP
