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

#ifndef LUMEX_CORE_CRC_PARAMETRIC_HPP
#define LUMEX_CORE_CRC_PARAMETRIC_HPP

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-libc-call"
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wswitch-enum"
#pragma clang diagnostic ignored "-Wnrvo"
#pragma clang diagnostic ignored "-Wheader-hygiene"
#pragma clang diagnostic ignored "-Wused-but-marked-unused"
#pragma clang diagnostic ignored "-Wundefined-var-template"
#pragma clang diagnostic ignored "-Wdeprecated-redundant-constexpr-static-def"
#pragma clang diagnostic ignored "-Wvariadic-macro-arguments-omitted"
#pragma clang diagnostic ignored "-Wunused-result"
#pragma clang diagnostic ignored "-Wextra-semi-stmt"
#pragma clang diagnostic ignored "-Wexpansion-to-defined"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wundefined-func-template"
#pragma clang diagnostic ignored "-Wfloat-equal"
#endif

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#if __cplusplus >= 202002L
#include <span>
#endif

#include "lumex/core/utility/assert/LumexAssert.hpp"
#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/core/utility/macros/LumexConstantMacros.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

namespace lumex
{
namespace core
{
namespace crc
{
namespace parametric
{
/**
 * @brief Parametric CRC engines following the CRC RevEng catalogue (Greg
 * Cook).
 * @details Algorithm parameters: `width`, `poly`, `init`, `refin`, `refout`,
 * `xorout` as defined at https://reveng.sourceforge.io/crc-catalogue/all.htm .
 *          Catalogue `check` is the CRC of the ASCII bytes `123456789`.
 *
 * @note Widths 1..64 and all four RefIn/RefOut combinations are supported.
 *       Widths `kWidth >= 8` use a 256-entry table; narrower widths use the
 *       bit engine (a per-byte table is incorrect for a register narrower than
 * 8).
 *
 * @note Also see Ross Williams, "A Painless Guide to CRC Error Detection
 * Algorithms", and Philip Koopman, CRC Polynomial Zoo (CMU).
 */
namespace Detail
{
// if constexpr is C++17+; C++14 has relaxed constexpr but not if constexpr.
#if __cplusplus >= 201703L
#define LUMEX_CRC_DETAIL_CONSTEXPR LUMEX_CONSTEXPR
#elif __cplusplus >= 201402L
#define LUMEX_CRC_DETAIL_CONSTEXPR LUMEX_CONSTEXPR
#else
#define LUMEX_CRC_DETAIL_CONSTEXPR inline
#endif

// std::array::at() is not constexpr until C++17; constexpr lookup tables need
// operator[] on C++11/14.
#if __cplusplus >= 201703L
#define LUMEX_CRC_ARRAY_REF(table, index)                                     \
  (table).at (static_cast<std::size_t> (index))
#else
#define LUMEX_CRC_ARRAY_REF(table, index)                                     \
  (table)[static_cast<std::size_t> (index)]
#endif

LUMEX_CONST_NUM int kMaxCrcBitWidth = 64;
LUMEX_CONST_NUM int kBitsPerByte = 8;
LUMEX_CONST_NUM std::size_t kTableByteCount = 256U;
LUMEX_CONST_NUM std::uint8_t kByteMaskU8 = 0xFFU;

#if __cplusplus >= 201402L
template <typename Spec>
LUMEX_CONSTEXPR_FUNCTION void
ValidateSpec () LUMEX_NOEXCEPT
{
  LUMEX_STATIC_ASSERT_MSG (Spec::kWidth > 0 && Spec::kWidth <= kMaxCrcBitWidth,
                           "Spec::kWidth must be 1..64");
  LUMEX_STATIC_ASSERT_MSG (
      std::numeric_limits<typename Spec::ValueType>::is_integer,
      "Spec::ValueType must be an integer type");
  LUMEX_STATIC_ASSERT_MSG (
      std::numeric_limits<typename Spec::ValueType>::digits >= Spec::kWidth,
      "Spec::ValueType must hold at least kWidth bits");
}
#else
template <typename Spec>
inline void
ValidateSpec () LUMEX_NOEXCEPT
{
  struct validator_t
  {
    LUMEX_STATIC_ASSERT_MSG (Spec::kWidth > 0
                                 && Spec::kWidth <= kMaxCrcBitWidth,
                             "Spec::kWidth must be 1..64");
    LUMEX_STATIC_ASSERT_MSG (
        std::numeric_limits<typename Spec::ValueType>::is_integer,
        "Spec::ValueType must be an integer type");
    LUMEX_STATIC_ASSERT_MSG (
        std::numeric_limits<typename Spec::ValueType>::digits >= Spec::kWidth,
        "Spec::ValueType must hold at least kWidth bits");
  };
  (void)sizeof (validator_t);
}
#endif

// Partial specialization: avoid (1u64 << Width) when Width == digits(uint64_t)
// (UB / -Wshift).
template <int Width, typename Enable = void> struct mask_impl_t
{
  static LUMEX_CRC_DETAIL_CONSTEXPR std::uint64_t
  Value () LUMEX_NOEXCEPT
  {
    return (std::uint64_t{ 1 } << static_cast<unsigned> (Width))
           - std::uint64_t{ 1 };
  }
};

template <int Width>
struct mask_impl_t<
    Width,
    typename std::enable_if<
        Width == std::numeric_limits<std::uint64_t>::digits, void>::type>
{
  static LUMEX_CRC_DETAIL_CONSTEXPR std::uint64_t
  Value () LUMEX_NOEXCEPT
  {
    return ~std::uint64_t{};
  }
};

template <int Width>
LUMEX_CRC_DETAIL_CONSTEXPR std::uint64_t
Mask () LUMEX_NOEXCEPT
{
  LUMEX_STATIC_ASSERT_MSG (Width > 0 && Width <= kMaxCrcBitWidth,
                           "CRC width must be in 1..64");
  return mask_impl_t<Width>::Value ();
}

// NOLINTBEGIN(bugprone-easily-swappable-parameters) - signatures match RevEng
// notation (data, width / index, poly)
LUMEX_CRC_DETAIL_CONSTEXPR std::uint64_t
Reflect (std::uint64_t data, int width) LUMEX_NOEXCEPT
{
  std::uint64_t reflected = 0;
  for (int bitIndex = 0; bitIndex < width; ++bitIndex)
    if (((data >> bitIndex) & 1U) != 0U)
      reflected |= (std::uint64_t{ 1 } << (width - 1 - bitIndex));
  return reflected;
}

template <int Width>
LUMEX_CRC_DETAIL_CONSTEXPR std::uint64_t
MsbTableByte (std::uint64_t tableIndex, std::uint64_t poly) LUMEX_NOEXCEPT
{
  LUMEX_STATIC_ASSERT_MSG (Width >= kBitsPerByte,
                           "MSB table entries assume width >= 8");
  std::uint64_t const mask = Mask<Width> ();
  std::uint64_t const highBitMask = std::uint64_t{ 1 } << (Width - 1);
  std::uint64_t registerValue = (tableIndex << (Width - kBitsPerByte)) & mask;
  for (int step = 0; step < kBitsPerByte; ++step)
    if ((registerValue & highBitMask) != 0U)
      registerValue = ((registerValue << 1) & mask) ^ poly;
    else
      registerValue = (registerValue << 1) & mask;
  return registerValue & mask;
}

template <int Width>
LUMEX_CRC_DETAIL_CONSTEXPR std::uint64_t
LsbTableByte (std::uint64_t tableIndex, std::uint64_t poly) LUMEX_NOEXCEPT
{
  std::uint64_t const mask = Mask<Width> ();
  std::uint64_t const reflectedPoly = Reflect (poly, Width) & mask;
  std::uint64_t registerValue = tableIndex;
  for (int step = 0; step < kBitsPerByte; ++step)
    if ((registerValue & 1U) != 0U)
      registerValue = (registerValue >> 1) ^ reflectedPoly;
    else
      registerValue >>= 1;
  return registerValue & mask;
}

// NOLINTEND(bugprone-easily-swappable-parameters)

// C++14: non-const std::array::operator[]/at are not constexpr; build the
// table via index_sequence
//        and aggregate initialization instead of a mutating loop inside
//        constexpr MakeLookupTable.
#if __cplusplus >= 201402L
template <typename Spec, std::size_t Index>
LUMEX_CONSTEXPR_FUNCTION std::uint64_t
LookupTableEntry () LUMEX_NOEXCEPT
{
  return Spec::kRefIn
             ? LsbTableByte<Spec::kWidth> (static_cast<std::uint64_t> (Index),
                                           Spec::kPoly)
             : MsbTableByte<Spec::kWidth> (static_cast<std::uint64_t> (Index),
                                           Spec::kPoly);
}

template <typename Spec, std::size_t... I>
LUMEX_CONSTEXPR_FUNCTION std::array<std::uint64_t, kTableByteCount>
MakeLookupTableImpl (std::index_sequence<I...>) LUMEX_NOEXCEPT
{
  return std::array<std::uint64_t, kTableByteCount>{
    { LookupTableEntry<Spec, I> ()... }
  };
}
#endif

template <typename Spec>
LUMEX_CRC_DETAIL_CONSTEXPR std::array<std::uint64_t, kTableByteCount>
MakeLookupTable () LUMEX_NOEXCEPT
{
  ValidateSpec<Spec> ();
#if __cplusplus >= 201402L
  return MakeLookupTableImpl<Spec> (
      std::make_index_sequence<kTableByteCount>{});
#else
  std::array<std::uint64_t, kTableByteCount> lookupTable{};
  for (int tableIndex = 0; tableIndex < static_cast<int> (kTableByteCount);
       ++tableIndex)
    {
      std::size_t const entry = static_cast<std::size_t> (tableIndex);
      LUMEX_CONSTEXPR_IF (Spec::kRefIn)
      LUMEX_CRC_ARRAY_REF (lookupTable, entry) = LsbTableByte<Spec::kWidth> (
          static_cast<std::uint64_t> (tableIndex), Spec::kPoly);
      else LUMEX_CRC_ARRAY_REF (lookupTable, entry)
          = MsbTableByte<Spec::kWidth> (
              static_cast<std::uint64_t> (tableIndex), Spec::kPoly);
    }
  return lookupTable;
#endif
}

template <typename Spec>
LUMEX_CRC_DETAIL_CONSTEXPR typename Spec::ValueType
ComputeBitwise (std::uint8_t const *data, std::size_t size) LUMEX_NOEXCEPT
{
  ValidateSpec<Spec> ();
  std::uint64_t const mask = Mask<Spec::kWidth> ();
  std::uint64_t const polyMasked = Spec::kPoly & mask;
  std::uint64_t const polyReflected
      = Reflect (polyMasked, Spec::kWidth) & mask;
  std::uint64_t crcRegister
      = (Spec::kRefIn ? Reflect (Spec::kInit, Spec::kWidth) : Spec::kInit)
        & mask;

  if (data == nullptr || size == 0UL)
    return static_cast<typename Spec::ValueType> (0);

  // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  for (std::size_t offset = 0; offset < size; ++offset)
    {
      std::uint8_t const messageByte = data[offset];
      LUMEX_CONSTEXPR_IF (Spec::kRefIn)
      {
        for (int bitIndex = 0; bitIndex < kBitsPerByte; ++bitIndex)
          {
            std::uint8_t const dataBit = static_cast<std::uint8_t> (
                (static_cast<unsigned> (messageByte) >> bitIndex) & 1U);
            crcRegister ^= static_cast<std::uint64_t> (dataBit);
            if ((crcRegister & 1U) != 0U)
              crcRegister = (crcRegister >> 1) ^ polyReflected;
            else
              crcRegister >>= 1;
            crcRegister &= mask;
          }
      }
      else
      {
        int const msbShiftIndex = Spec::kWidth - 1;
        for (int bitIndex = 0; bitIndex < kBitsPerByte; ++bitIndex)
          {
            std::uint8_t const dataBit = static_cast<std::uint8_t> (
                (static_cast<unsigned> (messageByte)
                 >> (kBitsPerByte - 1 - bitIndex))
                & 1U);
            std::uint64_t const topBit = (crcRegister >> msbShiftIndex) & 1U;
            crcRegister = (crcRegister << 1) & mask;
            if ((topBit ^ static_cast<std::uint64_t> (dataBit)) != 0U)
              crcRegister ^= polyMasked;
          }
      }
    }
  // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)

  // RefIn=true: same as zlib / RevEng tables - no final Reflect before xorout.
  LUMEX_CONSTEXPR_IF (!Spec::kRefIn && Spec::kRefOut)
  crcRegister = Reflect (crcRegister, Spec::kWidth) & mask;
  crcRegister = (crcRegister ^ Spec::kXorOut) & mask;
  return static_cast<typename Spec::ValueType> (crcRegister);
}

template <typename Spec>
LUMEX_CRC_DETAIL_CONSTEXPR typename Spec::ValueType
ComputeTableDriven (std::uint8_t const *data, std::size_t size) LUMEX_NOEXCEPT
{
  ValidateSpec<Spec> ();
#if __cplusplus >= 201402L
  LUMEX_CONSTEXPR std::array<std::uint64_t, kTableByteCount> kByteLookupTable
      = MakeLookupTable<Spec> ();
#else
  static std::array<std::uint64_t, kTableByteCount> const kByteLookupTable
      = MakeLookupTable<Spec> ();
#endif
  std::uint64_t const mask = Mask<Spec::kWidth> ();
  std::uint64_t crcRegister
      = (Spec::kRefIn ? Reflect (Spec::kInit, Spec::kWidth) : Spec::kInit)
        & mask;

  if (data == nullptr || size == 0UL)
    return static_cast<typename Spec::ValueType> (0);

  // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  LUMEX_CONSTEXPR_IF (Spec::kRefIn)
  {
    for (std::size_t offset = 0; offset < size; ++offset)
      {
        std::uint8_t const messageByte = data[offset];
        crcRegister
            = (crcRegister >> kBitsPerByte)
              ^ LUMEX_CRC_ARRAY_REF (
                  kByteLookupTable,
                  (crcRegister ^ static_cast<std::uint64_t> (messageByte))
                      & kByteMaskU8);
        crcRegister &= mask;
      }
  }
  else
  {
    for (std::size_t offset = 0; offset < size; ++offset)
      {
        std::uint8_t const messageByte = data[offset];
        crcRegister = ((crcRegister << kBitsPerByte) & mask)
                      ^ LUMEX_CRC_ARRAY_REF (
                          kByteLookupTable,
                          ((crcRegister >> (Spec::kWidth - kBitsPerByte))
                           ^ static_cast<std::uint64_t> (messageByte))
                              & kByteMaskU8);
        crcRegister &= mask;
      }
    LUMEX_CONSTEXPR_IF (Spec::kRefOut)
    crcRegister = Reflect (crcRegister, Spec::kWidth) & mask;
  }
  // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)

  crcRegister = (crcRegister ^ Spec::kXorOut) & mask;
  return static_cast<typename Spec::ValueType> (crcRegister);
}

template <typename Spec, typename Enable = void> struct crc_dispatch_t;

template <typename Spec>
struct crc_dispatch_t<
    Spec, typename std::enable_if<(Spec::kWidth < kBitsPerByte), void>::type>
{
  static LUMEX_CRC_DETAIL_CONSTEXPR typename Spec::ValueType
  Run (std::uint8_t const *data, std::size_t size) LUMEX_NOEXCEPT
  {
    return ComputeBitwise<Spec> (data, size);
  }
};

template <typename Spec>
struct crc_dispatch_t<
    Spec, typename std::enable_if<(Spec::kWidth >= kBitsPerByte), void>::type>
{
  static LUMEX_CRC_DETAIL_CONSTEXPR typename Spec::ValueType
  Run (std::uint8_t const *data, std::size_t size) LUMEX_NOEXCEPT
  {
    return ComputeTableDriven<Spec> (data, size);
  }
};

template <typename Spec>
LUMEX_CRC_DETAIL_CONSTEXPR typename Spec::ValueType
Compute (std::uint8_t const *data, std::size_t size) LUMEX_NOEXCEPT
{
  return crc_dispatch_t<Spec>::Run (data, size);
}

#undef LUMEX_CRC_DETAIL_CONSTEXPR
#undef LUMEX_CRC_ARRAY_REF
} // namespace Detail

// NOLINTBEGIN(readability-identifier-naming) - Spec suffix matches the RevEng
// catalogue

// ---- Catalogue specifications (names and parameters as in CRC RevEng) ----

/**
 * @brief CRC-3/GSM: width=3 poly=0x3 init=0 refin=false refout=false
 * xorout=0x7.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-3/GSM
 */
struct crc3_gsm_spec_t
{
  using ValueType = std::uint8_t;
  LUMEX_CONST_NUM int kWidth = 3;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x3U;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x0U;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x7U;
  LUMEX_CONST_NUM std::uint8_t kCatalogCheck = 0x4U;
};

/**
 * @brief CRC-5/USB: width=5 poly=0x05 init=0x1f refin=true refout=true
 * xorout=0x1f.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-5/USB
 */
struct crc5_usb_spec_t
{
  using ValueType = std::uint8_t;
  LUMEX_CONST_NUM int kWidth = 5;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x05U;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x1FU;
  LUMEX_CONST_NUM bool kRefIn = true;
  LUMEX_CONST_NUM bool kRefOut = true;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x1FU;
  LUMEX_CONST_NUM std::uint8_t kCatalogCheck = 0x19U;
};

/**
 * @brief CRC-8/MAXIM-DOW (Dallas): width=8 poly=0x31 init=0 refin=true
 * refout=true xorout=0.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-8/MAXIM-DOW
 */
struct crc8_maxim_dow_spec_t
{
  using ValueType = std::uint8_t;
  LUMEX_CONST_NUM int kWidth = 8;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x31U;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x0U;
  LUMEX_CONST_NUM bool kRefIn = true;
  LUMEX_CONST_NUM bool kRefOut = true;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x0U;
  LUMEX_CONST_NUM std::uint8_t kCatalogCheck = 0xA1U;
};

/**
 * @brief CRC-6/CDMA2000-A (3GPP2 C.S0002).
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-6/CDMA2000-A
 */
struct crc6_cdma2000_a_spec_t
{
  using ValueType = std::uint8_t;
  LUMEX_CONST_NUM int kWidth = 6;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x27U;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x3FU;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x0U;
  LUMEX_CONST_NUM std::uint8_t kCatalogCheck = 0x0DU;
};

/**
 * @brief CRC-6/CDMA2000-B (3GPP2 C.S0002).
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-6/CDMA2000-B
 */
struct crc6_cdma2000_b_spec_t
{
  using ValueType = std::uint8_t;
  LUMEX_CONST_NUM int kWidth = 6;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x07U;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x3FU;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x0U;
  LUMEX_CONST_NUM std::uint8_t kCatalogCheck = 0x3BU;
};

/**
 * @brief CRC-8/CDMA2000 (3GPP2 / WCDMA family).
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-8/CDMA2000
 */
struct crc8_cdma2000_spec_t
{
  using ValueType = std::uint8_t;
  LUMEX_CONST_NUM int kWidth = 8;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x9BU;
  LUMEX_CONST_NUM std::uint64_t kInit = 0xFFU;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x0U;
  LUMEX_CONST_NUM std::uint8_t kCatalogCheck = 0xDAU;
};

/**
 * @brief CRC-10/CDMA2000.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-10/CDMA2000
 */
struct crc10_cdma2000_spec_t
{
  using ValueType = std::uint16_t;
  LUMEX_CONST_NUM int kWidth = 10;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x3D9U;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x3FFU;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x000U;
  LUMEX_CONST_NUM std::uint16_t kCatalogCheck = 0x0233U;
};

/**
 * @brief CRC-12/CDMA2000.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-12/CDMA2000
 */
struct crc12_cdma2000_spec_t
{
  using ValueType = std::uint16_t;
  LUMEX_CONST_NUM int kWidth = 12;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0xF13U;
  LUMEX_CONST_NUM std::uint64_t kInit = 0xFFFU;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x000U;
  LUMEX_CONST_NUM std::uint16_t kCatalogCheck = 0xD4DU;
};

/**
 * @brief CRC-16/CDMA2000.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-16/CDMA2000
 */
struct crc16_cdma2000_spec_t
{
  using ValueType = std::uint16_t;
  LUMEX_CONST_NUM int kWidth = 16;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0xC867U;
  LUMEX_CONST_NUM std::uint64_t kInit = 0xFFFFU;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x0000U;
  LUMEX_CONST_NUM std::uint16_t kCatalogCheck = 0x4C06U;
};

/**
 * @brief CRC-30/CDMA (3GPP2).
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-30/CDMA
 */
struct crc30_cdma_spec_t
{
  using ValueType = std::uint32_t;
  LUMEX_CONST_NUM int kWidth = 30;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x2030B9C7UL;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x3FFFFFFFUL;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x3FFFFFFFUL;
  LUMEX_CONST_NUM std::uint32_t kCatalogCheck = 0x04C34ABFUL;
};

/**
 * @brief CRC-16/IBM-3740: width=16 poly=0x1021 init=0xffff refin=false
 * refout=false xorout=0.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-16/IBM-3740
 */
struct crc16_ibm3740_spec_t
{
  using ValueType = std::uint16_t;
  LUMEX_CONST_NUM int kWidth = 16;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x1021U;
  LUMEX_CONST_NUM std::uint64_t kInit = 0xFFFFU;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x0000U;
  LUMEX_CONST_NUM std::uint16_t kCatalogCheck = 0x29B1U;
};

/**
 * @brief CRC-16/KERMIT: width=16 poly=0x1021 init=0 refin=true refout=true
 * xorout=0.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-16/KERMIT
 */
struct crc16_kermit_spec_t
{
  using ValueType = std::uint16_t;
  LUMEX_CONST_NUM int kWidth = 16;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x1021U;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x0000U;
  LUMEX_CONST_NUM bool kRefIn = true;
  LUMEX_CONST_NUM bool kRefOut = true;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x0000U;
  LUMEX_CONST_NUM std::uint16_t kCatalogCheck = 0x2189U;
};

/**
 * @brief CRC-16/MODBUS: width=16 poly=0x8005 init=0xffff refin=true
 * refout=true xorout=0.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-16/MODBUS
 */
struct crc16_modbus_spec_t
{
  using ValueType = std::uint16_t;
  LUMEX_CONST_NUM int kWidth = 16;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x8005U;
  LUMEX_CONST_NUM std::uint64_t kInit = 0xFFFFU;
  LUMEX_CONST_NUM bool kRefIn = true;
  LUMEX_CONST_NUM bool kRefOut = true;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x0000U;
  LUMEX_CONST_NUM std::uint16_t kCatalogCheck = 0x4B37U;
};

/**
 * @brief CRC-24/OPENPGP: width=24 poly=0x864cfb init=0xb704ce refin=false
 * refout=false xorout=0.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-24/OPENPGP
 */
struct crc24_open_pgp_spec_t
{
  using ValueType = std::uint32_t;
  LUMEX_CONST_NUM int kWidth = 24;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x864CFBU;
  LUMEX_CONST_NUM std::uint64_t kInit = 0xB704CEU;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x000000U;
  LUMEX_CONST_NUM std::uint32_t kCatalogCheck = 0x21CF02U;
};

/**
 * @brief CRC-32/ISO-HDLC (IEEE 802.3 FCS, PKZIP, zlib crc32).
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-32/ISO-HDLC
 */
struct crc32_iso_hdlc_spec_t
{
  using ValueType = std::uint32_t;
  LUMEX_CONST_NUM int kWidth = 32;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x04C11DB7UL;
  LUMEX_CONST_NUM std::uint64_t kInit = 0xFFFFFFFFUL;
  LUMEX_CONST_NUM bool kRefIn = true;
  LUMEX_CONST_NUM bool kRefOut = true;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0xFFFFFFFFUL;
  LUMEX_CONST_NUM std::uint32_t kCatalogCheck = 0xCBF43926UL;
};

/**
 * @brief CRC-32/ISCSI (Castagnoli, CRC-32C).
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-32/ISCSI
 */
struct crc32_iscsi_spec_t
{
  using ValueType = std::uint32_t;
  LUMEX_CONST_NUM int kWidth = 32;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x1EDC6F41UL;
  LUMEX_CONST_NUM std::uint64_t kInit = 0xFFFFFFFFUL;
  LUMEX_CONST_NUM bool kRefIn = true;
  LUMEX_CONST_NUM bool kRefOut = true;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0xFFFFFFFFUL;
  LUMEX_CONST_NUM std::uint32_t kCatalogCheck = 0xE3069283UL;
};

/**
 * @brief CRC-64/ECMA-182 (XZ, WE).
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-64/ECMA-182
 */
struct crc64_ecma182_spec_t
{
  using ValueType = std::uint64_t;
  LUMEX_CONST_NUM int kWidth = 64;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x42F0E1EBA9EA3693ULL;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x0ULL;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x0ULL;
  LUMEX_CONST_NUM std::uint64_t kCatalogCheck = 0x6C40DF5F0B497347ULL;
};

// ---- Additional specs from CRC RevEng catalogue ----

/**
 * @brief CRC-3/ROHC: width=3 poly=0x3 init=0x7 refin=true refout=true
 * xorout=0x0.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-3/ROHC
 */
struct crc3_rohc_spec_t
{
  using ValueType = std::uint8_t;
  LUMEX_CONST_NUM int kWidth = 3;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x3U;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x7U;
  LUMEX_CONST_NUM bool kRefIn = true;
  LUMEX_CONST_NUM bool kRefOut = true;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x0U;
  LUMEX_CONST_NUM std::uint8_t kCatalogCheck = 0x6U;
};

/**
 * @brief CRC-4/G-704: width=4 poly=0x3 init=0x0 refin=true refout=true
 * xorout=0x0.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-4/G-704
 */
struct crc4_g704_spec_t
{
  using ValueType = std::uint8_t;
  LUMEX_CONST_NUM int kWidth = 4;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x3U;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x0U;
  LUMEX_CONST_NUM bool kRefIn = true;
  LUMEX_CONST_NUM bool kRefOut = true;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x0U;
  LUMEX_CONST_NUM std::uint8_t kCatalogCheck = 0x7U;
};

/**
 * @brief CRC-4/INTERLAKEN: width=4 poly=0x3 init=0xf refin=false refout=false
 * xorout=0xf.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-4/INTERLAKEN
 */
struct crc4_interlaken_spec_t
{
  using ValueType = std::uint8_t;
  LUMEX_CONST_NUM int kWidth = 4;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x3U;
  LUMEX_CONST_NUM std::uint64_t kInit = 0xFU;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0xFU;
  LUMEX_CONST_NUM std::uint8_t kCatalogCheck = 0xBU;
};

/**
 * @brief CRC-5/EPC-C1G2: width=5 poly=0x09 init=0x09 refin=false refout=false
 * xorout=0x00.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-5/EPC-C1G2
 */
struct crc5_epc_c1_g2_spec_t
{
  using ValueType = std::uint8_t;
  LUMEX_CONST_NUM int kWidth = 5;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x09U;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x09U;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x00U;
  LUMEX_CONST_NUM std::uint8_t kCatalogCheck = 0x00U;
};

/**
 * @brief CRC-5/G-704: width=5 poly=0x15 init=0x00 refin=true refout=true
 * xorout=0x00.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-5/G-704
 */
struct crc5_g704_spec_t
{
  using ValueType = std::uint8_t;
  LUMEX_CONST_NUM int kWidth = 5;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x15U;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x00U;
  LUMEX_CONST_NUM bool kRefIn = true;
  LUMEX_CONST_NUM bool kRefOut = true;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x00U;
  LUMEX_CONST_NUM std::uint8_t kCatalogCheck = 0x07U;
};

/**
 * @brief CRC-6/DARC: width=6 poly=0x19 init=0x00 refin=true refout=true
 * xorout=0x00.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-6/DARC
 */
struct crc6_darc_spec_t
{
  using ValueType = std::uint8_t;
  LUMEX_CONST_NUM int kWidth = 6;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x19U;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x00U;
  LUMEX_CONST_NUM bool kRefIn = true;
  LUMEX_CONST_NUM bool kRefOut = true;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x00U;
  LUMEX_CONST_NUM std::uint8_t kCatalogCheck = 0x26U;
};

/**
 * @brief CRC-6/G-704: width=6 poly=0x03 init=0x00 refin=true refout=true
 * xorout=0x00.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-6/G-704
 */
struct crc6_g704_spec_t
{
  using ValueType = std::uint8_t;
  LUMEX_CONST_NUM int kWidth = 6;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x03U;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x00U;
  LUMEX_CONST_NUM bool kRefIn = true;
  LUMEX_CONST_NUM bool kRefOut = true;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x00U;
  LUMEX_CONST_NUM std::uint8_t kCatalogCheck = 0x06U;
};

/**
 * @brief CRC-6/GSM: width=6 poly=0x2f init=0x00 refin=false refout=false
 * xorout=0x3f.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-6/GSM
 */
struct crc6_gsm_spec_t
{
  using ValueType = std::uint8_t;
  LUMEX_CONST_NUM int kWidth = 6;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x2FU;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x00U;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x3FU;
  LUMEX_CONST_NUM std::uint8_t kCatalogCheck = 0x13U;
};

/**
 * @brief CRC-7/MMC: width=7 poly=0x09 init=0x00 refin=false refout=false
 * xorout=0x00.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-7/MMC
 */
struct crc7_mmc_spec_t
{
  using ValueType = std::uint8_t;
  LUMEX_CONST_NUM int kWidth = 7;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x09U;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x00U;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x00U;
  LUMEX_CONST_NUM std::uint8_t kCatalogCheck = 0x75U;
};

/**
 * @brief CRC-7/ROHC: width=7 poly=0x4f init=0x7f refin=true refout=true
 * xorout=0x00.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-7/ROHC
 */
struct crc7_rohc_spec_t
{
  using ValueType = std::uint8_t;
  LUMEX_CONST_NUM int kWidth = 7;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x4FU;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x7FU;
  LUMEX_CONST_NUM bool kRefIn = true;
  LUMEX_CONST_NUM bool kRefOut = true;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x00U;
  LUMEX_CONST_NUM std::uint8_t kCatalogCheck = 0x53U;
};

/**
 * @brief CRC-7/UMTS: width=7 poly=0x45 init=0x00 refin=false refout=false
 * xorout=0x00.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-7/UMTS
 */
struct crc7_umts_spec_t
{
  using ValueType = std::uint8_t;
  LUMEX_CONST_NUM int kWidth = 7;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x45U;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x00U;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x00U;
  LUMEX_CONST_NUM std::uint8_t kCatalogCheck = 0x61U;
};

/**
 * @brief CRC-8/AUTOSAR: width=8 poly=0x2f init=0xff refin=false refout=false
 * xorout=0xff.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-8/AUTOSAR
 */
struct crc8_autosar_spec_t
{
  using ValueType = std::uint8_t;
  LUMEX_CONST_NUM int kWidth = 8;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x2FU;
  LUMEX_CONST_NUM std::uint64_t kInit = 0xFFU;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0xFFU;
  LUMEX_CONST_NUM std::uint8_t kCatalogCheck = 0xDFU;
};

/**
 * @brief CRC-8/BLUETOOTH: width=8 poly=0xa7 init=0x00 refin=true refout=true
 * xorout=0x00.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-8/BLUETOOTH
 */
struct crc8_bluetooth_spec_t
{
  using ValueType = std::uint8_t;
  LUMEX_CONST_NUM int kWidth = 8;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0xA7U;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x00U;
  LUMEX_CONST_NUM bool kRefIn = true;
  LUMEX_CONST_NUM bool kRefOut = true;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x00U;
  LUMEX_CONST_NUM std::uint8_t kCatalogCheck = 0x26U;
};

/**
 * @brief CRC-8/DARC: width=8 poly=0x39 init=0x00 refin=true refout=true
 * xorout=0x00.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-8/DARC
 */
struct crc8_darc_spec_t
{
  using ValueType = std::uint8_t;
  LUMEX_CONST_NUM int kWidth = 8;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x39U;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x00U;
  LUMEX_CONST_NUM bool kRefIn = true;
  LUMEX_CONST_NUM bool kRefOut = true;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x00U;
  LUMEX_CONST_NUM std::uint8_t kCatalogCheck = 0x15U;
};

/**
 * @brief CRC-8/DVB-S2: width=8 poly=0xd5 init=0x00 refin=false refout=false
 * xorout=0x00.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-8/DVB-S2
 */
struct crc8_dvb_s2_spec_t
{
  using ValueType = std::uint8_t;
  LUMEX_CONST_NUM int kWidth = 8;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0xD5U;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x00U;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x00U;
  LUMEX_CONST_NUM std::uint8_t kCatalogCheck = 0xBCU;
};

/**
 * @brief CRC-8/GSM-A: width=8 poly=0x1d init=0x00 refin=false refout=false
 * xorout=0x00.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-8/GSM-A
 */
struct crc8_gsm_a_spec_t
{
  using ValueType = std::uint8_t;
  LUMEX_CONST_NUM int kWidth = 8;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x1DU;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x00U;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x00U;
  LUMEX_CONST_NUM std::uint8_t kCatalogCheck = 0x37U;
};

/**
 * @brief CRC-8/GSM-B: width=8 poly=0x49 init=0x00 refin=false refout=false
 * xorout=0xff.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-8/GSM-B
 */
struct crc8_gsm_b_spec_t
{
  using ValueType = std::uint8_t;
  LUMEX_CONST_NUM int kWidth = 8;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x49U;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x00U;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0xFFU;
  LUMEX_CONST_NUM std::uint8_t kCatalogCheck = 0x94U;
};

/**
 * @brief CRC-8/HITAG: width=8 poly=0x1d init=0xff refin=false refout=false
 * xorout=0x00.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-8/HITAG
 */
struct crc8_hitag_spec_t
{
  using ValueType = std::uint8_t;
  LUMEX_CONST_NUM int kWidth = 8;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x1DU;
  LUMEX_CONST_NUM std::uint64_t kInit = 0xFFU;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x00U;
  LUMEX_CONST_NUM std::uint8_t kCatalogCheck = 0xB4U;
};

/**
 * @brief CRC-8/I-432-1: width=8 poly=0x07 init=0x00 refin=false refout=false
 * xorout=0x55.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-8/I-432-1
 */
struct crc8_i4321_spec_t
{
  using ValueType = std::uint8_t;
  LUMEX_CONST_NUM int kWidth = 8;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x07U;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x00U;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x55U;
  LUMEX_CONST_NUM std::uint8_t kCatalogCheck = 0xA1U;
};

/**
 * @brief CRC-8/I-CODE: width=8 poly=0x1d init=0xfd refin=false refout=false
 * xorout=0x00.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-8/I-CODE
 */
struct crc8_i_code_spec_t
{
  using ValueType = std::uint8_t;
  LUMEX_CONST_NUM int kWidth = 8;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x1DU;
  LUMEX_CONST_NUM std::uint64_t kInit = 0xFDU;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x00U;
  LUMEX_CONST_NUM std::uint8_t kCatalogCheck = 0x7EU;
};

/**
 * @brief CRC-8/LTE: width=8 poly=0x9b init=0x00 refin=false refout=false
 * xorout=0x00.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-8/LTE
 */
struct crc8_lte_spec_t
{
  using ValueType = std::uint8_t;
  LUMEX_CONST_NUM int kWidth = 8;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x9BU;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x00U;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x00U;
  LUMEX_CONST_NUM std::uint8_t kCatalogCheck = 0xEAU;
};

/**
 * @brief CRC-8/MIFARE-MAD: width=8 poly=0x1d init=0xc7 refin=false
 * refout=false xorout=0x00.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-8/MIFARE-MAD
 */
struct crc8_mifare_mad_spec_t
{
  using ValueType = std::uint8_t;
  LUMEX_CONST_NUM int kWidth = 8;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x1DU;
  LUMEX_CONST_NUM std::uint64_t kInit = 0xC7U;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x00U;
  LUMEX_CONST_NUM std::uint8_t kCatalogCheck = 0x99U;
};

/**
 * @brief CRC-8/NRSC-5: width=8 poly=0x31 init=0xff refin=false refout=false
 * xorout=0x00.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-8/NRSC-5
 */
struct crc8_nrsc5_spec_t
{
  using ValueType = std::uint8_t;
  LUMEX_CONST_NUM int kWidth = 8;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x31U;
  LUMEX_CONST_NUM std::uint64_t kInit = 0xFFU;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x00U;
  LUMEX_CONST_NUM std::uint8_t kCatalogCheck = 0xF7U;
};

/**
 * @brief CRC-8/OPENSAFETY: width=8 poly=0x2f init=0x00 refin=false
 * refout=false xorout=0x00.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-8/OPENSAFETY
 */
struct crc8_opensafety_spec_t
{
  using ValueType = std::uint8_t;
  LUMEX_CONST_NUM int kWidth = 8;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x2FU;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x00U;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x00U;
  LUMEX_CONST_NUM std::uint8_t kCatalogCheck = 0x3EU;
};

/**
 * @brief CRC-8/ROHC: width=8 poly=0x07 init=0xff refin=true refout=true
 * xorout=0x00.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-8/ROHC
 */
struct crc8_rohc_spec_t
{
  using ValueType = std::uint8_t;
  LUMEX_CONST_NUM int kWidth = 8;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x07U;
  LUMEX_CONST_NUM std::uint64_t kInit = 0xFFU;
  LUMEX_CONST_NUM bool kRefIn = true;
  LUMEX_CONST_NUM bool kRefOut = true;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x00U;
  LUMEX_CONST_NUM std::uint8_t kCatalogCheck = 0xD0U;
};

/**
 * @brief CRC-8/SAE-J1850: width=8 poly=0x1d init=0xff refin=false refout=false
 * xorout=0xff.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-8/SAE-J1850
 */
struct crc8_sae_j1850_spec_t
{
  using ValueType = std::uint8_t;
  LUMEX_CONST_NUM int kWidth = 8;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x1DU;
  LUMEX_CONST_NUM std::uint64_t kInit = 0xFFU;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0xFFU;
  LUMEX_CONST_NUM std::uint8_t kCatalogCheck = 0x4BU;
};

/**
 * @brief CRC-8/SMBUS: width=8 poly=0x07 init=0x00 refin=false refout=false
 * xorout=0x00.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-8/SMBUS
 */
struct crc8_smbus_spec_t
{
  using ValueType = std::uint8_t;
  LUMEX_CONST_NUM int kWidth = 8;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x07U;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x00U;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x00U;
  LUMEX_CONST_NUM std::uint8_t kCatalogCheck = 0xF4U;
};

/**
 * @brief CRC-8/TECH-3250: width=8 poly=0x1d init=0xff refin=true refout=true
 * xorout=0x00.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-8/TECH-3250
 */
struct crc8_tech3250_spec_t
{
  using ValueType = std::uint8_t;
  LUMEX_CONST_NUM int kWidth = 8;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x1DU;
  LUMEX_CONST_NUM std::uint64_t kInit = 0xFFU;
  LUMEX_CONST_NUM bool kRefIn = true;
  LUMEX_CONST_NUM bool kRefOut = true;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x00U;
  LUMEX_CONST_NUM std::uint8_t kCatalogCheck = 0x97U;
};

/**
 * @brief CRC-8/WCDMA: width=8 poly=0x9b init=0x00 refin=true refout=true
 * xorout=0x00.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-8/WCDMA
 */
struct crc8_wcdma_spec_t
{
  using ValueType = std::uint8_t;
  LUMEX_CONST_NUM int kWidth = 8;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x9BU;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x00U;
  LUMEX_CONST_NUM bool kRefIn = true;
  LUMEX_CONST_NUM bool kRefOut = true;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x00U;
  LUMEX_CONST_NUM std::uint8_t kCatalogCheck = 0x25U;
};

/**
 * @brief CRC-10/ATM: width=10 poly=0x233 init=0x000 refin=false refout=false
 * xorout=0x000.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-10/ATM
 */
struct crc10_atm_spec_t
{
  using ValueType = std::uint16_t;
  LUMEX_CONST_NUM int kWidth = 10;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x233U;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x000U;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x000U;
  LUMEX_CONST_NUM std::uint16_t kCatalogCheck = 0x199U;
};

/**
 * @brief CRC-10/GSM: width=10 poly=0x175 init=0x000 refin=false refout=false
 * xorout=0x3ff.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-10/GSM
 */
struct crc10_gsm_spec_t
{
  using ValueType = std::uint16_t;
  LUMEX_CONST_NUM int kWidth = 10;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x175U;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x000U;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x3FFU;
  LUMEX_CONST_NUM std::uint16_t kCatalogCheck = 0x12AU;
};

/**
 * @brief CRC-11/FLEXRAY: width=11 poly=0x385 init=0x01a refin=false
 * refout=false xorout=0x000.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-11/FLEXRAY
 */
struct crc11_flexray_spec_t
{
  using ValueType = std::uint16_t;
  LUMEX_CONST_NUM int kWidth = 11;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x385U;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x01AU;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x000U;
  LUMEX_CONST_NUM std::uint16_t kCatalogCheck = 0x5A3U;
};

/**
 * @brief CRC-11/UMTS: width=11 poly=0x307 init=0x000 refin=false refout=false
 * xorout=0x000.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-11/UMTS
 */
struct crc11_umts_spec_t
{
  using ValueType = std::uint16_t;
  LUMEX_CONST_NUM int kWidth = 11;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x307U;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x000U;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x000U;
  LUMEX_CONST_NUM std::uint16_t kCatalogCheck = 0x061U;
};

/**
 * @brief CRC-12/DECT: width=12 poly=0x80f init=0x000 refin=false refout=false
 * xorout=0x000.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-12/DECT
 */
struct crc12_dect_spec_t
{
  using ValueType = std::uint16_t;
  LUMEX_CONST_NUM int kWidth = 12;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x80FU;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x000U;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x000U;
  LUMEX_CONST_NUM std::uint16_t kCatalogCheck = 0xF5BU;
};

/**
 * @brief CRC-12/GSM: width=12 poly=0xd31 init=0x000 refin=false refout=false
 * xorout=0xfff.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-12/GSM
 */
struct crc12_gsm_spec_t
{
  using ValueType = std::uint16_t;
  LUMEX_CONST_NUM int kWidth = 12;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0xD31U;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x000U;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0xFFFU;
  LUMEX_CONST_NUM std::uint16_t kCatalogCheck = 0xB34U;
};

/**
 * @brief CRC-12/UMTS: width=12 poly=0x80f init=0x000 refin=false refout=true
 * xorout=0x000.
 * @note Unusual combination: refin=false, refout=true.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-12/UMTS
 */
struct crc12_umts_spec_t
{
  using ValueType = std::uint16_t;
  LUMEX_CONST_NUM int kWidth = 12;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x80FU;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x000U;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = true;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x000U;
  LUMEX_CONST_NUM std::uint16_t kCatalogCheck = 0xDAFU;
};

/**
 * @brief CRC-13/BBC: width=13 poly=0x1cf5 init=0x0000 refin=false refout=false
 * xorout=0x0000.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-13/BBC
 */
struct crc13_bbc_spec_t
{
  using ValueType = std::uint16_t;
  LUMEX_CONST_NUM int kWidth = 13;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x1CF5U;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x0000U;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x0000U;
  LUMEX_CONST_NUM std::uint16_t kCatalogCheck = 0x04FAU;
};

/**
 * @brief CRC-14/DARC: width=14 poly=0x0805 init=0x0000 refin=true refout=true
 * xorout=0x0000.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-14/DARC
 */
struct crc14_darc_spec_t
{
  using ValueType = std::uint16_t;
  LUMEX_CONST_NUM int kWidth = 14;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x0805U;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x0000U;
  LUMEX_CONST_NUM bool kRefIn = true;
  LUMEX_CONST_NUM bool kRefOut = true;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x0000U;
  LUMEX_CONST_NUM std::uint16_t kCatalogCheck = 0x082DU;
};

/**
 * @brief CRC-14/GSM: width=14 poly=0x202d init=0x0000 refin=false refout=false
 * xorout=0x3fff.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-14/GSM
 */
struct crc14_gsm_spec_t
{
  using ValueType = std::uint16_t;
  LUMEX_CONST_NUM int kWidth = 14;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x202DU;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x0000U;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x3FFFU;
  LUMEX_CONST_NUM std::uint16_t kCatalogCheck = 0x30AEU;
};

/**
 * @brief CRC-15/CAN: width=15 poly=0x4599 init=0x0000 refin=false refout=false
 * xorout=0x0000.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-15/CAN
 */
struct crc15_can_spec_t
{
  using ValueType = std::uint16_t;
  LUMEX_CONST_NUM int kWidth = 15;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x4599U;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x0000U;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x0000U;
  LUMEX_CONST_NUM std::uint16_t kCatalogCheck = 0x059EU;
};

/**
 * @brief CRC-15/MPT1327: width=15 poly=0x6815 init=0x0000 refin=false
 * refout=false xorout=0x0001.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-15/MPT1327
 */
struct crc15_mpt1327_spec_t
{
  using ValueType = std::uint16_t;
  LUMEX_CONST_NUM int kWidth = 15;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x6815U;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x0000U;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x0001U;
  LUMEX_CONST_NUM std::uint16_t kCatalogCheck = 0x2566U;
};

/**
 * @brief CRC-16/ARC: width=16 poly=0x8005 init=0x0000 refin=true refout=true
 * xorout=0x0000.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-16/ARC
 */
struct crc16_arc_spec_t
{
  using ValueType = std::uint16_t;
  LUMEX_CONST_NUM int kWidth = 16;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x8005U;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x0000U;
  LUMEX_CONST_NUM bool kRefIn = true;
  LUMEX_CONST_NUM bool kRefOut = true;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x0000U;
  LUMEX_CONST_NUM std::uint16_t kCatalogCheck = 0xBB3DU;
};

/**
 * @brief CRC-16/CMS: width=16 poly=0x8005 init=0xffff refin=false refout=false
 * xorout=0x0000.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-16/CMS
 */
struct crc16_cms_spec_t
{
  using ValueType = std::uint16_t;
  LUMEX_CONST_NUM int kWidth = 16;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x8005U;
  LUMEX_CONST_NUM std::uint64_t kInit = 0xFFFFU;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x0000U;
  LUMEX_CONST_NUM std::uint16_t kCatalogCheck = 0xAEE7U;
};

/**
 * @brief CRC-16/DDS-110: width=16 poly=0x8005 init=0x800d refin=false
 * refout=false xorout=0x0000.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-16/DDS-110
 */
struct crc16_dds110_spec_t
{
  using ValueType = std::uint16_t;
  LUMEX_CONST_NUM int kWidth = 16;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x8005U;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x800DU;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x0000U;
  LUMEX_CONST_NUM std::uint16_t kCatalogCheck = 0x9ECFU;
};

/**
 * @brief CRC-16/DECT-R: width=16 poly=0x0589 init=0x0000 refin=false
 * refout=false xorout=0x0001.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-16/DECT-R
 */
struct crc16_dect_r_spec_t
{
  using ValueType = std::uint16_t;
  LUMEX_CONST_NUM int kWidth = 16;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x0589U;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x0000U;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x0001U;
  LUMEX_CONST_NUM std::uint16_t kCatalogCheck = 0x007EU;
};

/**
 * @brief CRC-16/DECT-X: width=16 poly=0x0589 init=0x0000 refin=false
 * refout=false xorout=0x0000.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-16/DECT-X
 */
struct crc16_dect_x_spec_t
{
  using ValueType = std::uint16_t;
  LUMEX_CONST_NUM int kWidth = 16;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x0589U;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x0000U;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x0000U;
  LUMEX_CONST_NUM std::uint16_t kCatalogCheck = 0x007FU;
};

/**
 * @brief CRC-16/DNP: width=16 poly=0x3d65 init=0x0000 refin=true refout=true
 * xorout=0xffff.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-16/DNP
 */
struct crc16_dnp_spec_t
{
  using ValueType = std::uint16_t;
  LUMEX_CONST_NUM int kWidth = 16;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x3D65U;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x0000U;
  LUMEX_CONST_NUM bool kRefIn = true;
  LUMEX_CONST_NUM bool kRefOut = true;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0xFFFFU;
  LUMEX_CONST_NUM std::uint16_t kCatalogCheck = 0xEA82U;
};

/**
 * @brief CRC-16/EN-13757: width=16 poly=0x3d65 init=0x0000 refin=false
 * refout=false xorout=0xffff.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-16/EN-13757
 */
struct crc16_en13757_spec_t
{
  using ValueType = std::uint16_t;
  LUMEX_CONST_NUM int kWidth = 16;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x3D65U;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x0000U;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0xFFFFU;
  LUMEX_CONST_NUM std::uint16_t kCatalogCheck = 0xC2B7U;
};

/**
 * @brief CRC-16/GENIBUS: width=16 poly=0x1021 init=0xffff refin=false
 * refout=false xorout=0xffff.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-16/GENIBUS
 */
struct crc16_genibus_spec_t
{
  using ValueType = std::uint16_t;
  LUMEX_CONST_NUM int kWidth = 16;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x1021U;
  LUMEX_CONST_NUM std::uint64_t kInit = 0xFFFFU;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0xFFFFU;
  LUMEX_CONST_NUM std::uint16_t kCatalogCheck = 0xD64EU;
};

/**
 * @brief CRC-16/GSM: width=16 poly=0x1021 init=0x0000 refin=false refout=false
 * xorout=0xffff.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-16/GSM
 */
struct crc16_gsm_spec_t
{
  using ValueType = std::uint16_t;
  LUMEX_CONST_NUM int kWidth = 16;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x1021U;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x0000U;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0xFFFFU;
  LUMEX_CONST_NUM std::uint16_t kCatalogCheck = 0xCE3CU;
};

/**
 * @brief CRC-16/IBM-SDLC: width=16 poly=0x1021 init=0xffff refin=true
 * refout=true xorout=0xffff.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-16/IBM-SDLC
 */
struct crc16_ibm_sdlc_spec_t
{
  using ValueType = std::uint16_t;
  LUMEX_CONST_NUM int kWidth = 16;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x1021U;
  LUMEX_CONST_NUM std::uint64_t kInit = 0xFFFFU;
  LUMEX_CONST_NUM bool kRefIn = true;
  LUMEX_CONST_NUM bool kRefOut = true;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0xFFFFU;
  LUMEX_CONST_NUM std::uint16_t kCatalogCheck = 0x906EU;
};

/**
 * @brief CRC-16/ISO-IEC-14443-3-A: width=16 poly=0x1021 init=0xc6c6 refin=true
 * refout=true xorout=0x0000.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm -
 * CRC-16/ISO-IEC-14443-3-A
 */
struct crc16_iso_iec14443_3_a_spec_t
{
  using ValueType = std::uint16_t;
  LUMEX_CONST_NUM int kWidth = 16;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x1021U;
  LUMEX_CONST_NUM std::uint64_t kInit = 0xC6C6U;
  LUMEX_CONST_NUM bool kRefIn = true;
  LUMEX_CONST_NUM bool kRefOut = true;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x0000U;
  LUMEX_CONST_NUM std::uint16_t kCatalogCheck = 0xBF05U;
};

/**
 * @brief CRC-16/LJ1200: width=16 poly=0x6f63 init=0x0000 refin=false
 * refout=false xorout=0x0000.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-16/LJ1200
 */
struct crc16_lj1200_spec_t
{
  using ValueType = std::uint16_t;
  LUMEX_CONST_NUM int kWidth = 16;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x6F63U;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x0000U;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x0000U;
  LUMEX_CONST_NUM std::uint16_t kCatalogCheck = 0xBDF4U;
};

/**
 * @brief CRC-16/M17: width=16 poly=0x5935 init=0xffff refin=false refout=false
 * xorout=0x0000.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-16/M17
 */
struct crc16_m17_spec_t
{
  using ValueType = std::uint16_t;
  LUMEX_CONST_NUM int kWidth = 16;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x5935U;
  LUMEX_CONST_NUM std::uint64_t kInit = 0xFFFFU;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x0000U;
  LUMEX_CONST_NUM std::uint16_t kCatalogCheck = 0x772BU;
};

/**
 * @brief CRC-16/MAXIM-DOW: width=16 poly=0x8005 init=0x0000 refin=true
 * refout=true xorout=0xffff.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-16/MAXIM-DOW
 */
struct crc16_maxim_dow_spec_t
{
  using ValueType = std::uint16_t;
  LUMEX_CONST_NUM int kWidth = 16;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x8005U;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x0000U;
  LUMEX_CONST_NUM bool kRefIn = true;
  LUMEX_CONST_NUM bool kRefOut = true;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0xFFFFU;
  LUMEX_CONST_NUM std::uint16_t kCatalogCheck = 0x44C2U;
};

/**
 * @brief CRC-16/MCRF4XX: width=16 poly=0x1021 init=0xffff refin=true
 * refout=true xorout=0x0000.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-16/MCRF4XX
 */
struct crc16_mcrf4xx_spec_t
{
  using ValueType = std::uint16_t;
  LUMEX_CONST_NUM int kWidth = 16;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x1021U;
  LUMEX_CONST_NUM std::uint64_t kInit = 0xFFFFU;
  LUMEX_CONST_NUM bool kRefIn = true;
  LUMEX_CONST_NUM bool kRefOut = true;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x0000U;
  LUMEX_CONST_NUM std::uint16_t kCatalogCheck = 0x6F91U;
};

/**
 * @brief CRC-16/NRSC-5: width=16 poly=0x080b init=0xffff refin=true
 * refout=true xorout=0x0000.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-16/NRSC-5
 */
struct crc16_nrsc5_spec_t
{
  using ValueType = std::uint16_t;
  LUMEX_CONST_NUM int kWidth = 16;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x080BU;
  LUMEX_CONST_NUM std::uint64_t kInit = 0xFFFFU;
  LUMEX_CONST_NUM bool kRefIn = true;
  LUMEX_CONST_NUM bool kRefOut = true;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x0000U;
  LUMEX_CONST_NUM std::uint16_t kCatalogCheck = 0xA066U;
};

/**
 * @brief CRC-16/OPENSAFETY-A: width=16 poly=0x5935 init=0x0000 refin=false
 * refout=false xorout=0x0000.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm -
 * CRC-16/OPENSAFETY-A
 */
struct crc16_opensafety_a_spec_t
{
  using ValueType = std::uint16_t;
  LUMEX_CONST_NUM int kWidth = 16;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x5935U;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x0000U;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x0000U;
  LUMEX_CONST_NUM std::uint16_t kCatalogCheck = 0x5D38U;
};

/**
 * @brief CRC-16/OPENSAFETY-B: width=16 poly=0x755b init=0x0000 refin=false
 * refout=false xorout=0x0000.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm -
 * CRC-16/OPENSAFETY-B
 */
struct crc16_opensafety_b_spec_t
{
  using ValueType = std::uint16_t;
  LUMEX_CONST_NUM int kWidth = 16;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x755BU;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x0000U;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x0000U;
  LUMEX_CONST_NUM std::uint16_t kCatalogCheck = 0x20FEU;
};

/**
 * @brief CRC-16/PROFIBUS: width=16 poly=0x1dcf init=0xffff refin=false
 * refout=false xorout=0xffff.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-16/PROFIBUS
 */
struct crc16_profibus_spec_t
{
  using ValueType = std::uint16_t;
  LUMEX_CONST_NUM int kWidth = 16;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x1DCFU;
  LUMEX_CONST_NUM std::uint64_t kInit = 0xFFFFU;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0xFFFFU;
  LUMEX_CONST_NUM std::uint16_t kCatalogCheck = 0xA819U;
};

/**
 * @brief CRC-16/RIELLO: width=16 poly=0x1021 init=0xb2aa refin=true
 * refout=true xorout=0x0000.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-16/RIELLO
 */
struct crc16_riello_spec_t
{
  using ValueType = std::uint16_t;
  LUMEX_CONST_NUM int kWidth = 16;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x1021U;
  LUMEX_CONST_NUM std::uint64_t kInit = 0xB2AAU;
  LUMEX_CONST_NUM bool kRefIn = true;
  LUMEX_CONST_NUM bool kRefOut = true;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x0000U;
  LUMEX_CONST_NUM std::uint16_t kCatalogCheck = 0x63D0U;
};

/**
 * @brief CRC-16/SPI-FUJITSU: width=16 poly=0x1021 init=0x1d0f refin=false
 * refout=false xorout=0x0000.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm -
 * CRC-16/SPI-FUJITSU
 */
struct crc16_spi_fujitsu_spec_t
{
  using ValueType = std::uint16_t;
  LUMEX_CONST_NUM int kWidth = 16;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x1021U;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x1D0FU;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x0000U;
  LUMEX_CONST_NUM std::uint16_t kCatalogCheck = 0xE5CCU;
};

/**
 * @brief CRC-16/T10-DIF: width=16 poly=0x8bb7 init=0x0000 refin=false
 * refout=false xorout=0x0000.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-16/T10-DIF
 */
struct crc16_t10_dif_spec_t
{
  using ValueType = std::uint16_t;
  LUMEX_CONST_NUM int kWidth = 16;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x8BB7U;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x0000U;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x0000U;
  LUMEX_CONST_NUM std::uint16_t kCatalogCheck = 0xD0DBU;
};

/**
 * @brief CRC-16/TELEDISK: width=16 poly=0xa097 init=0x0000 refin=false
 * refout=false xorout=0x0000.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-16/TELEDISK
 */
struct crc16_teledisk_spec_t
{
  using ValueType = std::uint16_t;
  LUMEX_CONST_NUM int kWidth = 16;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0xA097U;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x0000U;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x0000U;
  LUMEX_CONST_NUM std::uint16_t kCatalogCheck = 0x0FB3U;
};

/**
 * @brief CRC-16/TMS37157: width=16 poly=0x1021 init=0x89ec refin=true
 * refout=true xorout=0x0000.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-16/TMS37157
 */
struct crc16_tms37157_spec_t
{
  using ValueType = std::uint16_t;
  LUMEX_CONST_NUM int kWidth = 16;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x1021U;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x89ECU;
  LUMEX_CONST_NUM bool kRefIn = true;
  LUMEX_CONST_NUM bool kRefOut = true;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x0000U;
  LUMEX_CONST_NUM std::uint16_t kCatalogCheck = 0x26B1U;
};

/**
 * @brief CRC-16/UMTS: width=16 poly=0x8005 init=0x0000 refin=false
 * refout=false xorout=0x0000.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-16/UMTS
 */
struct crc16_umts_spec_t
{
  using ValueType = std::uint16_t;
  LUMEX_CONST_NUM int kWidth = 16;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x8005U;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x0000U;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x0000U;
  LUMEX_CONST_NUM std::uint16_t kCatalogCheck = 0xFEE8U;
};

/**
 * @brief CRC-16/USB: width=16 poly=0x8005 init=0xffff refin=true refout=true
 * xorout=0xffff.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-16/USB
 */
struct crc16_usb_spec_t
{
  using ValueType = std::uint16_t;
  LUMEX_CONST_NUM int kWidth = 16;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x8005U;
  LUMEX_CONST_NUM std::uint64_t kInit = 0xFFFFU;
  LUMEX_CONST_NUM bool kRefIn = true;
  LUMEX_CONST_NUM bool kRefOut = true;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0xFFFFU;
  LUMEX_CONST_NUM std::uint16_t kCatalogCheck = 0xB4C8U;
};

/**
 * @brief CRC-16/XMODEM: width=16 poly=0x1021 init=0x0000 refin=false
 * refout=false xorout=0x0000.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-16/XMODEM
 */
struct crc16_xmodem_spec_t
{
  using ValueType = std::uint16_t;
  LUMEX_CONST_NUM int kWidth = 16;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x1021U;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x0000U;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x0000U;
  LUMEX_CONST_NUM std::uint16_t kCatalogCheck = 0x31C3U;
};

/**
 * @brief CRC-17/CAN-FD: width=17 poly=0x1685b init=0x00000 refin=false
 * refout=false xorout=0x00000.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-17/CAN-FD
 */
struct crc17_can_fd_spec_t
{
  using ValueType = std::uint32_t;
  LUMEX_CONST_NUM int kWidth = 17;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x1685BUL;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x00000UL;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x00000UL;
  LUMEX_CONST_NUM std::uint32_t kCatalogCheck = 0x04F03UL;
};

/**
 * @brief CRC-21/CAN-FD: width=21 poly=0x102899 init=0x000000 refin=false
 * refout=false xorout=0x000000.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-21/CAN-FD
 */
struct crc21_can_fd_spec_t
{
  using ValueType = std::uint32_t;
  LUMEX_CONST_NUM int kWidth = 21;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x102899UL;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x000000UL;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x000000UL;
  LUMEX_CONST_NUM std::uint32_t kCatalogCheck = 0x0ED841UL;
};

/**
 * @brief CRC-24/BLE: width=24 poly=0x00065b init=0x555555 refin=true
 * refout=true xorout=0x000000.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-24/BLE
 */
struct crc24_ble_spec_t
{
  using ValueType = std::uint32_t;
  LUMEX_CONST_NUM int kWidth = 24;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x00065BUL;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x555555UL;
  LUMEX_CONST_NUM bool kRefIn = true;
  LUMEX_CONST_NUM bool kRefOut = true;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x000000UL;
  LUMEX_CONST_NUM std::uint32_t kCatalogCheck = 0xC25A56UL;
};

/**
 * @brief CRC-24/FLEXRAY-A: width=24 poly=0x5d6dcb init=0xfedcba refin=false
 * refout=false xorout=0x000000.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-24/FLEXRAY-A
 */
struct crc24_flexray_a_spec_t
{
  using ValueType = std::uint32_t;
  LUMEX_CONST_NUM int kWidth = 24;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x5D6DCBUL;
  LUMEX_CONST_NUM std::uint64_t kInit = 0xFEDCBAUL;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x000000UL;
  LUMEX_CONST_NUM std::uint32_t kCatalogCheck = 0x7979BDUL;
};

/**
 * @brief CRC-24/FLEXRAY-B: width=24 poly=0x5d6dcb init=0xabcdef refin=false
 * refout=false xorout=0x000000.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-24/FLEXRAY-B
 */
struct crc24_flexray_b_spec_t
{
  using ValueType = std::uint32_t;
  LUMEX_CONST_NUM int kWidth = 24;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x5D6DCBUL;
  LUMEX_CONST_NUM std::uint64_t kInit = 0xABCDEFUL;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x000000UL;
  LUMEX_CONST_NUM std::uint32_t kCatalogCheck = 0x1F23B8UL;
};

/**
 * @brief CRC-24/INTERLAKEN: width=24 poly=0x328b63 init=0xffffff refin=false
 * refout=false xorout=0xffffff.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-24/INTERLAKEN
 */
struct crc24_interlaken_spec_t
{
  using ValueType = std::uint32_t;
  LUMEX_CONST_NUM int kWidth = 24;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x328B63UL;
  LUMEX_CONST_NUM std::uint64_t kInit = 0xFFFFFFUL;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0xFFFFFFUL;
  LUMEX_CONST_NUM std::uint32_t kCatalogCheck = 0xB4F3E6UL;
};

/**
 * @brief CRC-24/LTE-A: width=24 poly=0x864cfb init=0x000000 refin=false
 * refout=false xorout=0x000000.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-24/LTE-A
 */
struct crc24_lte_a_spec_t
{
  using ValueType = std::uint32_t;
  LUMEX_CONST_NUM int kWidth = 24;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x864CFBUL;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x000000UL;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x000000UL;
  LUMEX_CONST_NUM std::uint32_t kCatalogCheck = 0xCDE703UL;
};

/**
 * @brief CRC-24/LTE-B: width=24 poly=0x800063 init=0x000000 refin=false
 * refout=false xorout=0x000000.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-24/LTE-B
 */
struct crc24_lte_b_spec_t
{
  using ValueType = std::uint32_t;
  LUMEX_CONST_NUM int kWidth = 24;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x800063UL;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x000000UL;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x000000UL;
  LUMEX_CONST_NUM std::uint32_t kCatalogCheck = 0x23EF52UL;
};

/**
 * @brief CRC-24/OS-9: width=24 poly=0x800063 init=0xffffff refin=false
 * refout=false xorout=0xffffff.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-24/OS-9
 */
struct crc24_os9_spec_t
{
  using ValueType = std::uint32_t;
  LUMEX_CONST_NUM int kWidth = 24;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x800063UL;
  LUMEX_CONST_NUM std::uint64_t kInit = 0xFFFFFFUL;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0xFFFFFFUL;
  LUMEX_CONST_NUM std::uint32_t kCatalogCheck = 0x200FA5UL;
};

/**
 * @brief CRC-31/PHILIPS: width=31 poly=0x04c11db7 init=0x7fffffff refin=false
 * refout=false xorout=0x7fffffff.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-31/PHILIPS
 */
struct crc31_philips_spec_t
{
  using ValueType = std::uint32_t;
  LUMEX_CONST_NUM int kWidth = 31;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x04C11DB7UL;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x7FFFFFFFUL;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x7FFFFFFFUL;
  LUMEX_CONST_NUM std::uint32_t kCatalogCheck = 0x0CE9E46CUL;
};

/**
 * @brief CRC-32/AIXM: width=32 poly=0x814141ab init=0x00000000 refin=false
 * refout=false xorout=0x00000000.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-32/AIXM
 */
struct crc32_aixm_spec_t
{
  using ValueType = std::uint32_t;
  LUMEX_CONST_NUM int kWidth = 32;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x814141ABUL;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x00000000UL;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x00000000UL;
  LUMEX_CONST_NUM std::uint32_t kCatalogCheck = 0x3010BF7FUL;
};

/**
 * @brief CRC-32/AUTOSAR: width=32 poly=0xf4acfb13 init=0xffffffff refin=true
 * refout=true xorout=0xffffffff.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-32/AUTOSAR
 */
struct crc32_autosar_spec_t
{
  using ValueType = std::uint32_t;
  LUMEX_CONST_NUM int kWidth = 32;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0xF4ACFB13UL;
  LUMEX_CONST_NUM std::uint64_t kInit = 0xFFFFFFFFUL;
  LUMEX_CONST_NUM bool kRefIn = true;
  LUMEX_CONST_NUM bool kRefOut = true;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0xFFFFFFFFUL;
  LUMEX_CONST_NUM std::uint32_t kCatalogCheck = 0x1697D06AUL;
};

/**
 * @brief CRC-32/BASE91-D: width=32 poly=0xa833982b init=0xffffffff refin=true
 * refout=true xorout=0xffffffff.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-32/BASE91-D
 */
struct crc32_base91_d_spec_t
{
  using ValueType = std::uint32_t;
  LUMEX_CONST_NUM int kWidth = 32;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0xA833982BUL;
  LUMEX_CONST_NUM std::uint64_t kInit = 0xFFFFFFFFUL;
  LUMEX_CONST_NUM bool kRefIn = true;
  LUMEX_CONST_NUM bool kRefOut = true;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0xFFFFFFFFUL;
  LUMEX_CONST_NUM std::uint32_t kCatalogCheck = 0x87315576UL;
};

/**
 * @brief CRC-32/BZIP2: width=32 poly=0x04c11db7 init=0xffffffff refin=false
 * refout=false xorout=0xffffffff.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-32/BZIP2
 */
struct crc32_bzip2_spec_t
{
  using ValueType = std::uint32_t;
  LUMEX_CONST_NUM int kWidth = 32;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x04C11DB7UL;
  LUMEX_CONST_NUM std::uint64_t kInit = 0xFFFFFFFFUL;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0xFFFFFFFFUL;
  LUMEX_CONST_NUM std::uint32_t kCatalogCheck = 0xFC891918UL;
};

/**
 * @brief CRC-32/CD-ROM-EDC: width=32 poly=0x8001801b init=0x00000000
 * refin=true refout=true xorout=0x00000000.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-32/CD-ROM-EDC
 */
struct crc32_cd_rom_edc_spec_t
{
  using ValueType = std::uint32_t;
  LUMEX_CONST_NUM int kWidth = 32;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x8001801BUL;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x00000000UL;
  LUMEX_CONST_NUM bool kRefIn = true;
  LUMEX_CONST_NUM bool kRefOut = true;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x00000000UL;
  LUMEX_CONST_NUM std::uint32_t kCatalogCheck = 0x6EC2EDC4UL;
};

/**
 * @brief CRC-32/CKSUM: width=32 poly=0x04c11db7 init=0x00000000 refin=false
 * refout=false xorout=0xffffffff.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-32/CKSUM
 */
struct crc32_cksum_spec_t
{
  using ValueType = std::uint32_t;
  LUMEX_CONST_NUM int kWidth = 32;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x04C11DB7UL;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x00000000UL;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0xFFFFFFFFUL;
  LUMEX_CONST_NUM std::uint32_t kCatalogCheck = 0x765E7680UL;
};

/**
 * @brief CRC-32/JAMCRC: width=32 poly=0x04c11db7 init=0xffffffff refin=true
 * refout=true xorout=0x00000000.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-32/JAMCRC
 */
struct crc32_jamcrc_spec_t
{
  using ValueType = std::uint32_t;
  LUMEX_CONST_NUM int kWidth = 32;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x04C11DB7UL;
  LUMEX_CONST_NUM std::uint64_t kInit = 0xFFFFFFFFUL;
  LUMEX_CONST_NUM bool kRefIn = true;
  LUMEX_CONST_NUM bool kRefOut = true;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x00000000UL;
  LUMEX_CONST_NUM std::uint32_t kCatalogCheck = 0x340BC6D9UL;
};

/**
 * @brief CRC-32/MEF: width=32 poly=0x741b8cd7 init=0xffffffff refin=true
 * refout=true xorout=0x00000000.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-32/MEF
 */
struct crc32_mef_spec_t
{
  using ValueType = std::uint32_t;
  LUMEX_CONST_NUM int kWidth = 32;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x741B8CD7UL;
  LUMEX_CONST_NUM std::uint64_t kInit = 0xFFFFFFFFUL;
  LUMEX_CONST_NUM bool kRefIn = true;
  LUMEX_CONST_NUM bool kRefOut = true;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x00000000UL;
  LUMEX_CONST_NUM std::uint32_t kCatalogCheck = 0xD2C22F51UL;
};

/**
 * @brief CRC-32/MPEG-2: width=32 poly=0x04c11db7 init=0xffffffff refin=false
 * refout=false xorout=0x00000000.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-32/MPEG-2
 */
struct crc32_mpeg2_spec_t
{
  using ValueType = std::uint32_t;
  LUMEX_CONST_NUM int kWidth = 32;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x04C11DB7UL;
  LUMEX_CONST_NUM std::uint64_t kInit = 0xFFFFFFFFUL;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x00000000UL;
  LUMEX_CONST_NUM std::uint32_t kCatalogCheck = 0x0376E6E7UL;
};

/**
 * @brief CRC-32/XFER: width=32 poly=0x000000af init=0x00000000 refin=false
 * refout=false xorout=0x00000000.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-32/XFER
 */
struct crc32_xfer_spec_t
{
  using ValueType = std::uint32_t;
  LUMEX_CONST_NUM int kWidth = 32;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x000000AFUL;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x00000000UL;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x00000000UL;
  LUMEX_CONST_NUM std::uint32_t kCatalogCheck = 0xBD0BE338UL;
};

/**
 * @brief CRC-40/GSM: width=40 poly=0x0004820009 init=0x0000000000 refin=false
 * refout=false xorout=0xffffffffff.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-40/GSM
 */
struct crc40_gsm_spec_t
{
  using ValueType = std::uint64_t;
  LUMEX_CONST_NUM int kWidth = 40;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x0004820009ULL;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x0000000000ULL;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0xFFFFFFFFFFULL;
  LUMEX_CONST_NUM std::uint64_t kCatalogCheck = 0xD4164FC646ULL;
};

/**
 * @brief CRC-64/GO-ISO: width=64 poly=0x000000000000001b
 * init=0xffffffffffffffff refin=true refout=true xorout=0xffffffffffffffff.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-64/GO-ISO
 */
struct crc64_go_iso_spec_t
{
  using ValueType = std::uint64_t;
  LUMEX_CONST_NUM int kWidth = 64;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x000000000000001BULL;
  LUMEX_CONST_NUM std::uint64_t kInit = 0xFFFFFFFFFFFFFFFFULL;
  LUMEX_CONST_NUM bool kRefIn = true;
  LUMEX_CONST_NUM bool kRefOut = true;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0xFFFFFFFFFFFFFFFFULL;
  LUMEX_CONST_NUM std::uint64_t kCatalogCheck = 0xB90956C775A41001ULL;
};

/**
 * @brief CRC-64/MS: width=64 poly=0x259c84cba6426349 init=0xffffffffffffffff
 * refin=true refout=true xorout=0x0000000000000000.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-64/MS
 */
struct crc64_ms_spec_t
{
  using ValueType = std::uint64_t;
  LUMEX_CONST_NUM int kWidth = 64;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x259C84CBA6426349ULL;
  LUMEX_CONST_NUM std::uint64_t kInit = 0xFFFFFFFFFFFFFFFFULL;
  LUMEX_CONST_NUM bool kRefIn = true;
  LUMEX_CONST_NUM bool kRefOut = true;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x0000000000000000ULL;
  LUMEX_CONST_NUM std::uint64_t kCatalogCheck = 0x75D4B74F024ECEEAULL;
};

/**
 * @brief CRC-64/NVME: width=64 poly=0xad93d23594c93659 init=0xffffffffffffffff
 * refin=true refout=true xorout=0xffffffffffffffff.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-64/NVME
 */
struct crc64_nvme_spec_t
{
  using ValueType = std::uint64_t;
  LUMEX_CONST_NUM int kWidth = 64;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0xAD93D23594C93659ULL;
  LUMEX_CONST_NUM std::uint64_t kInit = 0xFFFFFFFFFFFFFFFFULL;
  LUMEX_CONST_NUM bool kRefIn = true;
  LUMEX_CONST_NUM bool kRefOut = true;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0xFFFFFFFFFFFFFFFFULL;
  LUMEX_CONST_NUM std::uint64_t kCatalogCheck = 0xAE8B14860A799888ULL;
};

/**
 * @brief CRC-64/REDIS: width=64 poly=0xad93d23594c935a9
 * init=0x0000000000000000 refin=true refout=true xorout=0x0000000000000000.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-64/REDIS
 */
struct crc64_redis_spec_t
{
  using ValueType = std::uint64_t;
  LUMEX_CONST_NUM int kWidth = 64;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0xAD93D23594C935A9ULL;
  LUMEX_CONST_NUM std::uint64_t kInit = 0x0000000000000000ULL;
  LUMEX_CONST_NUM bool kRefIn = true;
  LUMEX_CONST_NUM bool kRefOut = true;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0x0000000000000000ULL;
  LUMEX_CONST_NUM std::uint64_t kCatalogCheck = 0xE9C6D914C4B8D9CAULL;
};

/**
 * @brief CRC-64/WE: width=64 poly=0x42f0e1eba9ea3693 init=0xffffffffffffffff
 * refin=false refout=false xorout=0xffffffffffffffff.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-64/WE
 */
struct crc64_we_spec_t
{
  using ValueType = std::uint64_t;
  LUMEX_CONST_NUM int kWidth = 64;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x42F0E1EBA9EA3693ULL;
  LUMEX_CONST_NUM std::uint64_t kInit = 0xFFFFFFFFFFFFFFFFULL;
  LUMEX_CONST_NUM bool kRefIn = false;
  LUMEX_CONST_NUM bool kRefOut = false;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0xFFFFFFFFFFFFFFFFULL;
  LUMEX_CONST_NUM std::uint64_t kCatalogCheck = 0x62EC59E3F1A4F00AULL;
};

/**
 * @brief CRC-64/XZ: width=64 poly=0x42f0e1eba9ea3693 init=0xffffffffffffffff
 * refin=true refout=true xorout=0xffffffffffffffff.
 * @see https://reveng.sourceforge.io/crc-catalogue/all.htm - CRC-64/XZ
 */
struct crc64_xz_spec_t
{
  using ValueType = std::uint64_t;
  LUMEX_CONST_NUM int kWidth = 64;
  LUMEX_CONST_NUM std::uint64_t kPoly = 0x42F0E1EBA9EA3693ULL;
  LUMEX_CONST_NUM std::uint64_t kInit = 0xFFFFFFFFFFFFFFFFULL;
  LUMEX_CONST_NUM bool kRefIn = true;
  LUMEX_CONST_NUM bool kRefOut = true;
  LUMEX_CONST_NUM std::uint64_t kXorOut = 0xFFFFFFFFFFFFFFFFULL;
  LUMEX_CONST_NUM std::uint64_t kCatalogCheck = 0x995DC9BBDF1939FAULL;
};

// CRC-82/DARC (width=82) is not in this catalogue: its polynomial does not
// fit in std::uint64_t, and kMaxCrcBitWidth is 64. Every other RevEng entry
// is a specification here and a named engine after CrcParametric.

/**
 * @brief Every RevEng catalogue specification of width 3..64 (112 entries).
 *        Order is the runtime catalogue index. CRC-82/DARC is the only
 *        catalogue algorithm omitted.
 */
using all_crc_specs_t = std::tuple<
    // Width 3
    crc3_gsm_spec_t, crc3_rohc_spec_t,
    // Width 4
    crc4_g704_spec_t, crc4_interlaken_spec_t,
    // Width 5
    crc5_usb_spec_t, crc5_epc_c1_g2_spec_t, crc5_g704_spec_t,
    // Width 6
    crc6_cdma2000_a_spec_t, crc6_cdma2000_b_spec_t, crc6_darc_spec_t,
    crc6_g704_spec_t, crc6_gsm_spec_t,
    // Width 7
    crc7_mmc_spec_t, crc7_rohc_spec_t, crc7_umts_spec_t,
    // Width 8
    crc8_maxim_dow_spec_t, crc8_cdma2000_spec_t, crc8_autosar_spec_t,
    crc8_bluetooth_spec_t, crc8_darc_spec_t, crc8_dvb_s2_spec_t,
    crc8_gsm_a_spec_t, crc8_gsm_b_spec_t, crc8_hitag_spec_t, crc8_i4321_spec_t,
    crc8_i_code_spec_t, crc8_lte_spec_t, crc8_mifare_mad_spec_t,
    crc8_nrsc5_spec_t, crc8_opensafety_spec_t, crc8_rohc_spec_t,
    crc8_sae_j1850_spec_t, crc8_smbus_spec_t, crc8_tech3250_spec_t,
    crc8_wcdma_spec_t,
    // Width 10
    crc10_cdma2000_spec_t, crc10_atm_spec_t, crc10_gsm_spec_t,
    // Width 11
    crc11_flexray_spec_t, crc11_umts_spec_t,
    // Width 12
    crc12_cdma2000_spec_t, crc12_dect_spec_t, crc12_gsm_spec_t,
    crc12_umts_spec_t,
    // Width 13
    crc13_bbc_spec_t,
    // Width 14
    crc14_darc_spec_t, crc14_gsm_spec_t,
    // Width 15
    crc15_can_spec_t, crc15_mpt1327_spec_t,
    // Width 16
    crc16_cdma2000_spec_t, crc16_ibm3740_spec_t, crc16_kermit_spec_t,
    crc16_modbus_spec_t, crc16_arc_spec_t, crc16_cms_spec_t,
    crc16_dds110_spec_t, crc16_dect_r_spec_t, crc16_dect_x_spec_t,
    crc16_dnp_spec_t, crc16_en13757_spec_t, crc16_genibus_spec_t,
    crc16_gsm_spec_t, crc16_ibm_sdlc_spec_t, crc16_iso_iec14443_3_a_spec_t,
    crc16_lj1200_spec_t, crc16_m17_spec_t, crc16_maxim_dow_spec_t,
    crc16_mcrf4xx_spec_t, crc16_nrsc5_spec_t, crc16_opensafety_a_spec_t,
    crc16_opensafety_b_spec_t, crc16_profibus_spec_t, crc16_riello_spec_t,
    crc16_spi_fujitsu_spec_t, crc16_t10_dif_spec_t, crc16_teledisk_spec_t,
    crc16_tms37157_spec_t, crc16_umts_spec_t, crc16_usb_spec_t,
    crc16_xmodem_spec_t,
    // Width 17
    crc17_can_fd_spec_t,
    // Width 21
    crc21_can_fd_spec_t,
    // Width 24
    crc24_open_pgp_spec_t, crc24_ble_spec_t, crc24_flexray_a_spec_t,
    crc24_flexray_b_spec_t, crc24_interlaken_spec_t, crc24_lte_a_spec_t,
    crc24_lte_b_spec_t, crc24_os9_spec_t,
    // Width 30
    crc30_cdma_spec_t,
    // Width 31
    crc31_philips_spec_t,
    // Width 32
    crc32_iso_hdlc_spec_t, crc32_iscsi_spec_t, crc32_aixm_spec_t,
    crc32_autosar_spec_t, crc32_base91_d_spec_t, crc32_bzip2_spec_t,
    crc32_cd_rom_edc_spec_t, crc32_cksum_spec_t, crc32_jamcrc_spec_t,
    crc32_mef_spec_t, crc32_mpeg2_spec_t, crc32_xfer_spec_t,
    // Width 40
    crc40_gsm_spec_t,
    // Width 64
    crc64_ecma182_spec_t, crc64_go_iso_spec_t, crc64_ms_spec_t,
    crc64_nvme_spec_t, crc64_redis_spec_t, crc64_we_spec_t, crc64_xz_spec_t>;

// NOLINTEND(readability-identifier-naming)

/**
 * @brief CRC computation parameterized by user specification `Spec`.
 * @tparam Spec Structure with `ValueType`, `kWidth`, `kPoly`, `kInit`,
 * `kRefIn`, `kRefOut`, `kXorOut`.
 */
template <typename Spec> class CrcParametric
{
public:
  using ValueType = typename Spec::ValueType;

  CrcParametric () = delete;
  CrcParametric (CrcParametric const &) = delete;
  CrcParametric (CrcParametric &&) = delete;
  CrcParametric &operator= (CrcParametric const &) = delete;
  CrcParametric &operator= (CrcParametric &&) = delete;
  ~CrcParametric () = default;

  /**
   * @brief Computes the CRC of a buffer.
   * @return 0 for `nullptr` or a zero size.
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "CRC result is required for integrity verification.")
  static ValueType
  calculate (std::uint8_t const *data, std::size_t size) LUMEX_NOEXCEPT
  {
    return Detail::Compute<Spec> (data, size);
  }

  LUMEX_ATTRIBUTE_NODISCARD (
      "CRC result is required for integrity verification.")
  static ValueType
  calculate (std::vector<std::uint8_t> const &data) LUMEX_NOEXCEPT
  {
    if (data.empty ())
      return Detail::Compute<Spec> (nullptr, 0);
    return Detail::Compute<Spec> (data.data (), data.size ());
  }

#if __cplusplus >= 202002L
  LUMEX_ATTRIBUTE_NODISCARD (
      "CRC result is required for integrity verification.")
  static ValueType
  calculate (std::span<std::uint8_t const> data) LUMEX_NOEXCEPT
  {
    return Detail::Compute<Spec> (data.data (), data.size ());
  }
#endif
};

/** @brief CRC-3/GSM. */
using Crc3Gsm = CrcParametric<crc3_gsm_spec_t>;

/** @brief CRC-3/ROHC. */
using Crc3Rohc = CrcParametric<crc3_rohc_spec_t>;

/** @brief CRC-4/G-704. */
using Crc4G704 = CrcParametric<crc4_g704_spec_t>;

/** @brief CRC-4/INTERLAKEN. */
using Crc4Interlaken = CrcParametric<crc4_interlaken_spec_t>;

/** @brief CRC-5/USB. */
using Crc5Usb = CrcParametric<crc5_usb_spec_t>;

/** @brief CRC-5/EPC-C1G2. */
using Crc5EpcC1G2 = CrcParametric<crc5_epc_c1_g2_spec_t>;

/** @brief CRC-5/G-704. */
using Crc5G704 = CrcParametric<crc5_g704_spec_t>;

/** @brief CRC-6/CDMA2000-A. */
using Crc6Cdma2000A = CrcParametric<crc6_cdma2000_a_spec_t>;

/** @brief CRC-6/CDMA2000-B. */
using Crc6Cdma2000B = CrcParametric<crc6_cdma2000_b_spec_t>;

/** @brief CRC-6/DARC. */
using Crc6Darc = CrcParametric<crc6_darc_spec_t>;

/** @brief CRC-6/G-704. */
using Crc6G704 = CrcParametric<crc6_g704_spec_t>;

/** @brief CRC-6/GSM. */
using Crc6Gsm = CrcParametric<crc6_gsm_spec_t>;

/** @brief CRC-7/MMC. */
using Crc7Mmc = CrcParametric<crc7_mmc_spec_t>;

/** @brief CRC-7/ROHC. */
using Crc7Rohc = CrcParametric<crc7_rohc_spec_t>;

/** @brief CRC-7/UMTS. */
using Crc7Umts = CrcParametric<crc7_umts_spec_t>;

/** @brief CRC-8/MAXIM-DOW. */
using Crc8MaximDow = CrcParametric<crc8_maxim_dow_spec_t>;

/** @brief CRC-8/CDMA2000. */
using Crc8Cdma2000 = CrcParametric<crc8_cdma2000_spec_t>;

/** @brief CRC-8/AUTOSAR. */
using Crc8Autosar = CrcParametric<crc8_autosar_spec_t>;

/** @brief CRC-8/BLUETOOTH. */
using Crc8Bluetooth = CrcParametric<crc8_bluetooth_spec_t>;

/** @brief CRC-8/DARC. */
using Crc8Darc = CrcParametric<crc8_darc_spec_t>;

/** @brief CRC-8/DVB-S2. */
using Crc8DvbS2 = CrcParametric<crc8_dvb_s2_spec_t>;

/** @brief CRC-8/GSM-A. */
using Crc8GsmA = CrcParametric<crc8_gsm_a_spec_t>;

/** @brief CRC-8/GSM-B. */
using Crc8GsmB = CrcParametric<crc8_gsm_b_spec_t>;

/** @brief CRC-8/HITAG. */
using Crc8Hitag = CrcParametric<crc8_hitag_spec_t>;

/** @brief CRC-8/I-432-1. */
using Crc8I4321 = CrcParametric<crc8_i4321_spec_t>;

/** @brief CRC-8/I-CODE. */
using Crc8ICode = CrcParametric<crc8_i_code_spec_t>;

/** @brief CRC-8/LTE. */
using Crc8Lte = CrcParametric<crc8_lte_spec_t>;

/** @brief CRC-8/MIFARE-MAD. */
using Crc8MifareMad = CrcParametric<crc8_mifare_mad_spec_t>;

/** @brief CRC-8/NRSC-5. */
using Crc8Nrsc5 = CrcParametric<crc8_nrsc5_spec_t>;

/** @brief CRC-8/OPENSAFETY. */
using Crc8Opensafety = CrcParametric<crc8_opensafety_spec_t>;

/** @brief CRC-8/ROHC. */
using Crc8Rohc = CrcParametric<crc8_rohc_spec_t>;

/** @brief CRC-8/SAE-J1850. */
using Crc8SaeJ1850 = CrcParametric<crc8_sae_j1850_spec_t>;

/** @brief CRC-8/SMBUS. */
using Crc8Smbus = CrcParametric<crc8_smbus_spec_t>;

/** @brief CRC-8/TECH-3250. */
using Crc8Tech3250 = CrcParametric<crc8_tech3250_spec_t>;

/** @brief CRC-8/WCDMA. */
using Crc8Wcdma = CrcParametric<crc8_wcdma_spec_t>;

/** @brief CRC-10/CDMA2000. */
using Crc10Cdma2000 = CrcParametric<crc10_cdma2000_spec_t>;

/** @brief CRC-10/ATM. */
using Crc10Atm = CrcParametric<crc10_atm_spec_t>;

/** @brief CRC-10/GSM. */
using Crc10Gsm = CrcParametric<crc10_gsm_spec_t>;

/** @brief CRC-11/FLEXRAY. */
using Crc11Flexray = CrcParametric<crc11_flexray_spec_t>;

/** @brief CRC-11/UMTS. */
using Crc11Umts = CrcParametric<crc11_umts_spec_t>;

/** @brief CRC-12/CDMA2000. */
using Crc12Cdma2000 = CrcParametric<crc12_cdma2000_spec_t>;

/** @brief CRC-12/DECT. */
using Crc12Dect = CrcParametric<crc12_dect_spec_t>;

/** @brief CRC-12/GSM. */
using Crc12Gsm = CrcParametric<crc12_gsm_spec_t>;

/** @brief CRC-12/UMTS. */
using Crc12Umts = CrcParametric<crc12_umts_spec_t>;

/** @brief CRC-13/BBC. */
using Crc13Bbc = CrcParametric<crc13_bbc_spec_t>;

/** @brief CRC-14/DARC. */
using Crc14Darc = CrcParametric<crc14_darc_spec_t>;

/** @brief CRC-14/GSM. */
using Crc14Gsm = CrcParametric<crc14_gsm_spec_t>;

/** @brief CRC-15/CAN. */
using Crc15Can = CrcParametric<crc15_can_spec_t>;

/** @brief CRC-15/MPT1327. */
using Crc15Mpt1327 = CrcParametric<crc15_mpt1327_spec_t>;

/** @brief CRC-16/CDMA2000. */
using Crc16Cdma2000 = CrcParametric<crc16_cdma2000_spec_t>;

/** @brief CRC-16/IBM-3740. */
using Crc16Ibm3740 = CrcParametric<crc16_ibm3740_spec_t>;

/** @brief CRC-16/KERMIT. */
using Crc16Kermit = CrcParametric<crc16_kermit_spec_t>;

/** @brief CRC-16/MODBUS. */
using Crc16Modbus = CrcParametric<crc16_modbus_spec_t>;

/** @brief CRC-16/ARC. */
using Crc16Arc = CrcParametric<crc16_arc_spec_t>;

/** @brief CRC-16/CMS. */
using Crc16Cms = CrcParametric<crc16_cms_spec_t>;

/** @brief CRC-16/DDS-110. */
using Crc16Dds110 = CrcParametric<crc16_dds110_spec_t>;

/** @brief CRC-16/DECT-R. */
using Crc16DectR = CrcParametric<crc16_dect_r_spec_t>;

/** @brief CRC-16/DECT-X. */
using Crc16DectX = CrcParametric<crc16_dect_x_spec_t>;

/** @brief CRC-16/DNP. */
using Crc16Dnp = CrcParametric<crc16_dnp_spec_t>;

/** @brief CRC-16/EN-13757. */
using Crc16En13757 = CrcParametric<crc16_en13757_spec_t>;

/** @brief CRC-16/GENIBUS. */
using Crc16Genibus = CrcParametric<crc16_genibus_spec_t>;

/** @brief CRC-16/GSM. */
using Crc16Gsm = CrcParametric<crc16_gsm_spec_t>;

/** @brief CRC-16/IBM-SDLC. */
using Crc16IbmSdlc = CrcParametric<crc16_ibm_sdlc_spec_t>;

/** @brief CRC-16/ISO-IEC-14443-3-A. */
using Crc16IsoIec144433A = CrcParametric<crc16_iso_iec14443_3_a_spec_t>;

/** @brief CRC-16/LJ1200. */
using Crc16Lj1200 = CrcParametric<crc16_lj1200_spec_t>;

/** @brief CRC-16/M17. */
using Crc16M17 = CrcParametric<crc16_m17_spec_t>;

/** @brief CRC-16/MAXIM-DOW. */
using Crc16MaximDow = CrcParametric<crc16_maxim_dow_spec_t>;

/** @brief CRC-16/MCRF4XX. */
using Crc16Mcrf4xx = CrcParametric<crc16_mcrf4xx_spec_t>;

/** @brief CRC-16/NRSC-5. */
using Crc16Nrsc5 = CrcParametric<crc16_nrsc5_spec_t>;

/** @brief CRC-16/OPENSAFETY-A. */
using Crc16OpensafetyA = CrcParametric<crc16_opensafety_a_spec_t>;

/** @brief CRC-16/OPENSAFETY-B. */
using Crc16OpensafetyB = CrcParametric<crc16_opensafety_b_spec_t>;

/** @brief CRC-16/PROFIBUS. */
using Crc16Profibus = CrcParametric<crc16_profibus_spec_t>;

/** @brief CRC-16/RIELLO. */
using Crc16Riello = CrcParametric<crc16_riello_spec_t>;

/** @brief CRC-16/SPI-FUJITSU. */
using Crc16SpiFujitsu = CrcParametric<crc16_spi_fujitsu_spec_t>;

/** @brief CRC-16/T10-DIF. */
using Crc16T10Dif = CrcParametric<crc16_t10_dif_spec_t>;

/** @brief CRC-16/TELEDISK. */
using Crc16Teledisk = CrcParametric<crc16_teledisk_spec_t>;

/** @brief CRC-16/TMS37157. */
using Crc16Tms37157 = CrcParametric<crc16_tms37157_spec_t>;

/** @brief CRC-16/UMTS. */
using Crc16Umts = CrcParametric<crc16_umts_spec_t>;

/** @brief CRC-16/USB. */
using Crc16Usb = CrcParametric<crc16_usb_spec_t>;

/** @brief CRC-16/XMODEM. */
using Crc16Xmodem = CrcParametric<crc16_xmodem_spec_t>;

/** @brief CRC-17/CAN-FD. */
using Crc17CanFd = CrcParametric<crc17_can_fd_spec_t>;

/** @brief CRC-21/CAN-FD. */
using Crc21CanFd = CrcParametric<crc21_can_fd_spec_t>;

/** @brief CRC-24/OPENPGP. */
using Crc24OpenPgp = CrcParametric<crc24_open_pgp_spec_t>;

/** @brief CRC-24/BLE. */
using Crc24Ble = CrcParametric<crc24_ble_spec_t>;

/** @brief CRC-24/FLEXRAY-A. */
using Crc24FlexrayA = CrcParametric<crc24_flexray_a_spec_t>;

/** @brief CRC-24/FLEXRAY-B. */
using Crc24FlexrayB = CrcParametric<crc24_flexray_b_spec_t>;

/** @brief CRC-24/INTERLAKEN. */
using Crc24Interlaken = CrcParametric<crc24_interlaken_spec_t>;

/** @brief CRC-24/LTE-A. */
using Crc24LteA = CrcParametric<crc24_lte_a_spec_t>;

/** @brief CRC-24/LTE-B. */
using Crc24LteB = CrcParametric<crc24_lte_b_spec_t>;

/** @brief CRC-24/OS-9. */
using Crc24Os9 = CrcParametric<crc24_os9_spec_t>;

/** @brief CRC-30/CDMA. */
using Crc30Cdma = CrcParametric<crc30_cdma_spec_t>;

/** @brief CRC-31/PHILIPS. */
using Crc31Philips = CrcParametric<crc31_philips_spec_t>;

/** @brief CRC-32/ISO-HDLC. */
using Crc32IsoHdlc = CrcParametric<crc32_iso_hdlc_spec_t>;

/** @brief CRC-32/ISCSI. */
using Crc32Iscsi = CrcParametric<crc32_iscsi_spec_t>;

/** @brief CRC-32/AIXM. */
using Crc32Aixm = CrcParametric<crc32_aixm_spec_t>;

/** @brief CRC-32/AUTOSAR. */
using Crc32Autosar = CrcParametric<crc32_autosar_spec_t>;

/** @brief CRC-32/BASE91-D. */
using Crc32Base91D = CrcParametric<crc32_base91_d_spec_t>;

/** @brief CRC-32/BZIP2. */
using Crc32Bzip2 = CrcParametric<crc32_bzip2_spec_t>;

/** @brief CRC-32/CD-ROM-EDC. */
using Crc32CdRomEdc = CrcParametric<crc32_cd_rom_edc_spec_t>;

/** @brief CRC-32/CKSUM. */
using Crc32Cksum = CrcParametric<crc32_cksum_spec_t>;

/** @brief CRC-32/JAMCRC. */
using Crc32Jamcrc = CrcParametric<crc32_jamcrc_spec_t>;

/** @brief CRC-32/MEF. */
using Crc32Mef = CrcParametric<crc32_mef_spec_t>;

/** @brief CRC-32/MPEG-2. */
using Crc32Mpeg2 = CrcParametric<crc32_mpeg2_spec_t>;

/** @brief CRC-32/XFER. */
using Crc32Xfer = CrcParametric<crc32_xfer_spec_t>;

/** @brief CRC-40/GSM. */
using Crc40Gsm = CrcParametric<crc40_gsm_spec_t>;

/** @brief CRC-64/ECMA-182. */
using Crc64Ecma182 = CrcParametric<crc64_ecma182_spec_t>;

/** @brief CRC-64/GO-ISO. */
using Crc64GoIso = CrcParametric<crc64_go_iso_spec_t>;

/** @brief CRC-64/MS. */
using Crc64Ms = CrcParametric<crc64_ms_spec_t>;

/** @brief CRC-64/NVME. */
using Crc64Nvme = CrcParametric<crc64_nvme_spec_t>;

/** @brief CRC-64/REDIS. */
using Crc64Redis = CrcParametric<crc64_redis_spec_t>;

/** @brief CRC-64/WE. */
using Crc64We = CrcParametric<crc64_we_spec_t>;

/** @brief CRC-64/XZ. */
using Crc64Xz = CrcParametric<crc64_xz_spec_t>;

/**
 * @brief Named CRC engines in the same order as @ref all_crc_specs_t.
 *        Each name is @c CrcParametric of the matching specification.
 */
using all_crc_algorithms_t = std::tuple<
    // Width 3
    Crc3Gsm, Crc3Rohc,
    // Width 4
    Crc4G704, Crc4Interlaken,
    // Width 5
    Crc5Usb, Crc5EpcC1G2, Crc5G704,
    // Width 6
    Crc6Cdma2000A, Crc6Cdma2000B, Crc6Darc, Crc6G704, Crc6Gsm,
    // Width 7
    Crc7Mmc, Crc7Rohc, Crc7Umts,
    // Width 8
    Crc8MaximDow, Crc8Cdma2000, Crc8Autosar, Crc8Bluetooth, Crc8Darc,
    Crc8DvbS2, Crc8GsmA, Crc8GsmB, Crc8Hitag, Crc8I4321, Crc8ICode, Crc8Lte,
    Crc8MifareMad, Crc8Nrsc5, Crc8Opensafety, Crc8Rohc, Crc8SaeJ1850,
    Crc8Smbus, Crc8Tech3250, Crc8Wcdma,
    // Width 10
    Crc10Cdma2000, Crc10Atm, Crc10Gsm,
    // Width 11
    Crc11Flexray, Crc11Umts,
    // Width 12
    Crc12Cdma2000, Crc12Dect, Crc12Gsm, Crc12Umts,
    // Width 13
    Crc13Bbc,
    // Width 14
    Crc14Darc, Crc14Gsm,
    // Width 15
    Crc15Can, Crc15Mpt1327,
    // Width 16
    Crc16Cdma2000, Crc16Ibm3740, Crc16Kermit, Crc16Modbus, Crc16Arc, Crc16Cms,
    Crc16Dds110, Crc16DectR, Crc16DectX, Crc16Dnp, Crc16En13757, Crc16Genibus,
    Crc16Gsm, Crc16IbmSdlc, Crc16IsoIec144433A, Crc16Lj1200, Crc16M17,
    Crc16MaximDow, Crc16Mcrf4xx, Crc16Nrsc5, Crc16OpensafetyA,
    Crc16OpensafetyB, Crc16Profibus, Crc16Riello, Crc16SpiFujitsu, Crc16T10Dif,
    Crc16Teledisk, Crc16Tms37157, Crc16Umts, Crc16Usb, Crc16Xmodem,
    // Width 17
    Crc17CanFd,
    // Width 21
    Crc21CanFd,
    // Width 24
    Crc24OpenPgp, Crc24Ble, Crc24FlexrayA, Crc24FlexrayB, Crc24Interlaken,
    Crc24LteA, Crc24LteB, Crc24Os9,
    // Width 30
    Crc30Cdma,
    // Width 31
    Crc31Philips,
    // Width 32
    Crc32IsoHdlc, Crc32Iscsi, Crc32Aixm, Crc32Autosar, Crc32Base91D,
    Crc32Bzip2, Crc32CdRomEdc, Crc32Cksum, Crc32Jamcrc, Crc32Mef, Crc32Mpeg2,
    Crc32Xfer,
    // Width 40
    Crc40Gsm,
    // Width 64
    Crc64Ecma182, Crc64GoIso, Crc64Ms, Crc64Nvme, Crc64Redis, Crc64We,
    Crc64Xz>;

static_assert (std::tuple_size<all_crc_algorithms_t>::value
                   == std::tuple_size<all_crc_specs_t>::value,
               "one named CRC engine per catalogue spec");
} // namespace parametric
} // namespace crc
} // namespace core
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_CRC_PARAMETRIC_HPP
