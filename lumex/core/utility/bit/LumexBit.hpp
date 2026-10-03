/**
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
 * @brief `ByteSwap()`, which reverses the byte order of any integral value and
 * is usable in constant expressions; an analogue of C++23 `std::byteswap`.
 * @details At run time it uses the compiler's byte swap builtin for 2, 4 and 8
 * bytes (`__builtin_bswap*` with GCC and Clang, `_byteswap_*` with MSVC). In a
 * constant expression, and for other sizes, it reverses the bytes of a
 * `std::bit_cast` copy.
 * @warning Requires C++20 (concepts, `std::bit_cast`, `std::ranges` and
 * `std::is_constant_evaluated`); with an older standard the header declares
 * nothing.
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
#include <type_traits>

#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

// Needs C++20 concepts, std::bit_cast, std::ranges and
// std::is_constant_evaluated; without them the header declares nothing.
#if LUMEX_HAS_CONCEPTS && LUMEX_HAS_STD_BIT_CAST && LUMEX_HAS_STD_RANGES      \
    && LUMEX_HAS_STD_IS_CONSTANT_EVALUATED

namespace lumex
{
namespace core
{
namespace utility
{
namespace bit
{
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
ByteSwap (T value) LUMEX_NOEXCEPT
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
} // namespace bit
} // namespace utility
} // namespace core
} // namespace lumex

#endif // LUMEX_HAS_CONCEPTS && ...

// NOLINTEND(readability-identifier-length,
// cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

#endif // !LUMEX_CORE_UTILITY_BIT_HPP
