/*
 * Copyright (c) 2026 Vladislav Semykin <vladislav.semykin@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/**
 * @file LumexBit.hpp
 * @brief Bit helpers. `count_leading_zeros` (C++11) counts the leading zeros
 * of an unsigned integer of 1, 2, 4 or 8 bytes, an analogue of C++20
 * `std::countl_zero`. `byte_swap` (C++11) reverses the byte order of an
 * integral value, an analogue of C++23 `std::byteswap`. `is_little_endian`
 * (C++11) tells whether the machine stores the least significant byte first.
 * @details `count_leading_zeros` uses `_BitScanReverse` with MSVC (including
 * clang-cl) and `__builtin_clz` / `__builtin_clzll` with GCC and Clang. A
 * zero value returns the width; those intrinsics do not define that case.
 * `byte_swap` accepts every integral type except `bool` whose size is 1, 2, 4
 * or 8 bytes. From C++20, where concepts, `std::bit_cast`, `std::ranges` and
 * `std::is_constant_evaluated` exist, it uses the compiler's byte swap
 * builtin for 2, 4 and 8 bytes (`__builtin_bswap*` with GCC and Clang,
 * `_byteswap_*` with MSVC) at run time and reverses the bytes of a
 * `std::bit_cast` copy in a constant expression. Below C++20 it is written
 * with shifts and masks, so it is `constexpr` from C++11 and compilers turn
 * it into one byte swap instruction. Both forms give the same result for the
 * same value.
 *
 * The byte swap and the byte-order probe serve code that reads or writes data
 * of a fixed byte order, such as the UTF-16 and UTF-32 transcoders of the
 * `unicode` module. `byte_swap` below C++20 and `is_little_endian` replace
 * helpers of the XML module; both functions are Lumex's own code (the XML
 * module itself is derived from pugixml, see `THIRD-PARTY-NOTICES.md`).
 */

// NOLINTBEGIN(readability-identifier-length,
// cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

#ifndef LUMEX_CORE_UTILITY_BIT_HPP
#define LUMEX_CORE_UTILITY_BIT_HPP

#include <algorithm>
#include <array>
#if __cplusplus > 201703L && defined(__has_include)
#if __has_include(<bit>)
#include <bit> // for std::bit_cast
#endif
#endif
#include <cstddef> // for std::byte
#include <cstdint>
#include <cstring> // for std::memcpy
#include <memory>  // for std::addressof
#include <type_traits>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

namespace lumex
{
namespace core
{
namespace utility
{
namespace bit
{

/**
 * @brief Counts the leading zero bits of an unsigned integer (C++11).
 * @details Analogue of C++20 `std::countl_zero` for an unsigned integer of
 * 1, 2, 4 or 8 bytes. `bool` and wider types, including `unsigned __int128`,
 * are not in the overload set: the intrinsics below scan 32 or 64 bits, and
 * a wider value would be truncated. A zero value returns the bit width.
 * The function is not `constexpr`: the MSVC intrinsics are not constant
 * expressions on the toolchains this library supports.
 * @tparam NumericType Unsigned integer, not `bool`, of 1, 2, 4 or 8 bytes.
 * @param[in] value Input value.
 * @return The number of consecutive zero bits from the most significant bit,
 *         in the range `0 .. sizeof(NumericType) * 8`.
 * @note Exception-safety: nothrow (the function is `noexcept`).
 * @note Thread-safety: yes, no shared state is used.
 */
template <typename NumericType>
typename std::enable_if<
    std::is_integral<NumericType>::value
        && std::is_unsigned<NumericType>::value
        && !std::is_same<bool, NumericType>::value
        && (sizeof (NumericType) == 1 || sizeof (NumericType) == 2
            || sizeof (NumericType) == 4 || sizeof (NumericType) == 8),
    std::uint8_t>::type
count_leading_zeros (NumericType value) LUMEX_NOEXCEPT
{
  // __builtin_clz(0) is undefined, and _BitScanReverse(0) reports no bit.
  if (value == 0)
    return static_cast<std::uint8_t> (sizeof (NumericType) * 8);

#if defined(_MSC_VER)
  // clang-cl defines __clang__ as well as _MSC_VER. This branch is first
  // so this compiler keeps the MSVC intrinsic.
  unsigned long index = 0;

  if (sizeof (NumericType) <= 4)
    {
      _BitScanReverse (std::addressof (index),
                       static_cast<std::uint32_t> (value));
      // Index of the highest set bit (0 is the least significant bit).
      return static_cast<std::uint8_t> ((sizeof (NumericType) * 8) - 1
                                        - index);
    }
  else
    {
#if defined(_M_X64) || defined(_M_ARM64)
      _BitScanReverse64 (std::addressof (index),
                         static_cast<std::uint64_t> (value));
      return static_cast<std::uint8_t> (64 - 1 - index);
#else
      // 32-bit MSVC has no _BitScanReverse64. Split the value in half.
      std::uint32_t high = static_cast<std::uint32_t> (
          static_cast<std::uint64_t> (value) >> 32);
      if (high != 0)
        {
          _BitScanReverse (std::addressof (index), high);
          return static_cast<std::uint8_t> (32 - 1 - index);
        }
      else
        {
          _BitScanReverse (std::addressof (index),
                           static_cast<std::uint32_t> (value));
          return static_cast<std::uint8_t> (64 - 1 - index);
        }
#endif
    }

#elif defined(__GNUC__) || defined(__clang__)
  if (sizeof (NumericType) <= 4)
    {
      // __builtin_clz counts zeros of a 32-bit unsigned int, so a narrower
      // value is widened and its extra high bits are subtracted. The
      // arithmetic is done in int (the builtin returns int) and narrowed once.
      return static_cast<std::uint8_t> (
          __builtin_clz (static_cast<std::uint32_t> (value))
          - (32 - static_cast<int> (sizeof (NumericType) * 8)));
    }
  else
    {
      return static_cast<std::uint8_t> (
          __builtin_clzll (static_cast<std::uint64_t> (value)));
    }

#else
  std::uint8_t bits = static_cast<std::uint8_t> (sizeof (NumericType) * 8);
  std::uint8_t count = 0;

  // Halve the window: 32, then 16, 8, 4, 2, 1 for a 64-bit value.
  for (int shift = bits >> 1; shift > 0; shift >>= 1)
    {
      if ((value >> shift) != 0)
        {
          value = static_cast<NumericType> (value >> shift);
        }
      else
        {
          count = static_cast<std::uint8_t> (count + shift);
        }
    }
  return count;
#endif
}

// byte_swap needs C++20 concepts, std::bit_cast, std::ranges and
// std::is_constant_evaluated.
#if LUMEX_HAS_CONCEPTS && LUMEX_HAS_STD_BIT_CAST && LUMEX_HAS_STD_RANGES      \
    && LUMEX_HAS_STD_IS_CONSTANT_EVALUATED
/**
 * @brief Reverses the byte order of an integral value (byte swap / endianness
 * reverse).
 *
 * @details
 * Provides a portable `byteswap` for any integral type `T` (signed or
 * unsigned), preserving the bit pattern while reversing byte order:
 * - the low byte becomes the high byte,
 * - the high byte becomes the low byte.
 *
 * The implementation has two branches:
 * - Compile-time (`std::is_constant_evaluated() == true`): fully portable, via
 * `std::bit_cast` to an array of bytes followed by `std::ranges::reverse`.
 * - Run-time: uses compiler builtins/intrinsics where available
 * (`__builtin_bswap*` for Clang/GCC, `_byteswap_*` for MSVC), which usually
 * compile down to a single machine instruction (e.g. `bswap` on x86/x64). For
 * non-standard sizes (not 2/4/8) it falls back to the same portable path.
 *
 * The value is first converted to the corresponding unsigned type `U =
 * std::make_unsigned_t<T>`, the byte swap is performed on `U`, and the result
 * is cast back to `T`. This avoids any shift/overflow pitfalls with signed
 * types and keeps the operation defined purely in terms of object
 * representation.
 *
 * @tparam T Integral type (signed or unsigned).
 * @param[in] value Input value.
 * @return `value` with its byte order reversed.
 * @see https://en.cppreference.com/w/cpp/numeric/byteswap.html
 *
 * @note Exception-safety: nothrow (the function is `noexcept`).
 * @note Thread-safety: yes, no shared state is used.
 */
template <typename T>
  requires std::is_integral_v<T> && std::has_unique_object_representations_v<T>
LUMEX_CONSTEXPR T
byte_swap (T value) LUMEX_NOEXCEPT
{
  using U = std::make_unsigned_t<T>;
  U u = static_cast<U> (value);

  LUMEX_CONSTEXPR_IF (sizeof (T) == 1) { return value; }
  else
  {
    if (std::is_constant_evaluated ())
      {
        auto bytes = std::bit_cast<std::array<std::byte, sizeof (T)>> (u);
        std::ranges::reverse (bytes);
        U swapped = std::bit_cast<U> (bytes);
        return static_cast<T> (swapped);
      }

#if defined(__clang__) || defined(__GNUC__)
    LUMEX_CONSTEXPR_IF (sizeof (T) == 2)
    {
      u = static_cast<U> (__builtin_bswap16 (static_cast<std::uint16_t> (u)));
    }
    else LUMEX_CONSTEXPR_IF (sizeof (T) == 4)
    {
      u = static_cast<U> (__builtin_bswap32 (static_cast<std::uint32_t> (u)));
    }
    else LUMEX_CONSTEXPR_IF (sizeof (T) == 8)
    {
      u = static_cast<U> (__builtin_bswap64 (static_cast<std::uint64_t> (u)));
    }
    else
    {
      auto bytes = std::bit_cast<std::array<std::byte, sizeof (T)>> (u);
      std::ranges::reverse (bytes);
      u = std::bit_cast<U> (bytes);
    }
    return static_cast<T> (u);
#elif defined(_MSC_VER)
    LUMEX_CONSTEXPR_IF (sizeof (T) == 2)
    {
      u = static_cast<U> (_byteswap_ushort (static_cast<std::uint16_t> (u)));
    }
    else LUMEX_CONSTEXPR_IF (sizeof (T) == 4)
    {
      u = static_cast<U> (_byteswap_ulong (static_cast<std::uint32_t> (u)));
    }
    else LUMEX_CONSTEXPR_IF (sizeof (T) == 8)
    {
      u = static_cast<U> (_byteswap_uint64 (static_cast<std::uint64_t> (u)));
    }
    else
    {
      auto bytes = std::bit_cast<std::array<std::byte, sizeof (T)>> (u);
      std::ranges::reverse (bytes);
      u = std::bit_cast<U> (bytes);
    }
    return static_cast<T> (u);
#else
    // Portable fallback for compilers without a recognized bswap builtin.
    auto bytes = std::bit_cast<std::array<std::byte, sizeof (T)>> (u);
    std::ranges::reverse (bytes);
    u = std::bit_cast<U> (bytes);
    return static_cast<T> (u);
#endif
  }
}

#else // no C++20 byte_swap

namespace Detail
{
/// @brief One-byte value: nothing to reverse.
template <typename Unsigned>
LUMEX_CONSTEXPR Unsigned
swap_bytes (Unsigned value,
            std::integral_constant<std::size_t, 1> /* size */) LUMEX_NOEXCEPT
{
  return value;
}

/// @brief Reverses the two bytes of an unsigned value.
template <typename Unsigned>
LUMEX_CONSTEXPR Unsigned
swap_bytes (Unsigned value,
            std::integral_constant<std::size_t, 2> /* size */) LUMEX_NOEXCEPT
{
  return static_cast<Unsigned> (
      static_cast<Unsigned> (static_cast<Unsigned> (value & 0xFFU) << 8)
      | static_cast<Unsigned> (static_cast<Unsigned> (value >> 8) & 0xFFU));
}

/// @brief Reverses the four bytes of an unsigned value.
template <typename Unsigned>
LUMEX_CONSTEXPR Unsigned
swap_bytes (Unsigned value,
            std::integral_constant<std::size_t, 4> /* size */) LUMEX_NOEXCEPT
{
  return static_cast<Unsigned> (
      static_cast<Unsigned> (static_cast<Unsigned> (value & 0xFFU) << 24)
      | static_cast<Unsigned> (static_cast<Unsigned> (value & 0xFF00U) << 8)
      | static_cast<Unsigned> (static_cast<Unsigned> (value >> 8) & 0xFF00U)
      | static_cast<Unsigned> (value >> 24));
}

/// @brief Reverses the eight bytes of an unsigned value.
template <typename Unsigned>
LUMEX_CONSTEXPR Unsigned
swap_bytes (Unsigned value,
            std::integral_constant<std::size_t, 8> /* size */) LUMEX_NOEXCEPT
{
  return static_cast<Unsigned> (
      (static_cast<Unsigned> (value & 0xFFULL) << 56)
      | (static_cast<Unsigned> (value & 0xFF00ULL) << 40)
      | (static_cast<Unsigned> (value & 0xFF0000ULL) << 24)
      | (static_cast<Unsigned> (value & 0xFF000000ULL) << 8)
      | (static_cast<Unsigned> (value >> 8) & 0xFF000000ULL)
      | (static_cast<Unsigned> (value >> 24) & 0xFF0000ULL)
      | (static_cast<Unsigned> (value >> 40) & 0xFF00ULL)
      | static_cast<Unsigned> (value >> 56));
}
} // namespace Detail

/**
 * @brief Reverses the byte order of an integral value (byte swap / endianness
 * reverse), C++11 form.
 * @details Analogue of C++23 `std::byteswap`. The value is converted to the
 * unsigned type of the same size, the bytes are reversed with shifts and
 * masks, and the result is converted back, so signed values keep the bit
 * pattern of the swapped bytes. It is `constexpr` and `noexcept`. From C++20
 * the same function name is the `std::bit_cast` and builtin based template;
 * both give the same value.
 * @tparam T Integral type other than `bool`, of 1, 2, 4 or 8 bytes.
 * @param[in] value Input value.
 * @return `value` with its byte order reversed; a one-byte value is returned
 * unchanged.
 * @see https://en.cppreference.com/w/cpp/numeric/byteswap.html
 */
template <typename T>
LUMEX_CONSTEXPR
    typename std::enable_if<std::is_integral<T>::value
                                && !std::is_same<T, bool>::value
                                && (sizeof (T) == 1 || sizeof (T) == 2
                                    || sizeof (T) == 4 || sizeof (T) == 8),
                            T>::type
    byte_swap (T value) LUMEX_NOEXCEPT
{
  return static_cast<T> (Detail::swap_bytes (
      static_cast<typename std::make_unsigned<T>::type> (value),
      std::integral_constant<std::size_t, sizeof (T)> ()));
}

#endif // LUMEX_HAS_CONCEPTS && ...

/**
 * @brief Tells whether the machine stores the least significant byte of an
 * integer first (little endian).
 * @details Reads the first byte of the object representation of the 32-bit
 * value 1 through `std::memcpy`, so it has no undefined behavior. A middle
 * endian machine counts as not little endian.
 * @return `true` on a little endian machine (x86, x86-64 and, in practice,
 * ARM and every Windows target), `false` otherwise.
 */
inline bool
is_little_endian () LUMEX_NOEXCEPT
{
  std::uint32_t const probe = 1U;
  unsigned char first = 0;
  std::memcpy (&first, &probe, 1);
  return first == 1;
}

} // namespace bit
} // namespace utility
} // namespace core
} // namespace lumex

// NOLINTEND(readability-identifier-length,
// cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

#endif // !LUMEX_CORE_UTILITY_BIT_HPP
