// LumexBit.cxx20.tests.cpp
// byte_swap of LumexBit.hpp, which needs C++20 concepts, std::bit_cast,
// std::ranges and std::is_constant_evaluated; where the toolchain lacks one
// of them a single test reports the skip. LumexBit.cxx23.tests.cpp compares
// byte_swap with std::byteswap (C++23).
#include <cstdint>
#include <limits>
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/core/utility/assert/LumexAssert.hpp"
#include "lumex/core/utility/bit/LumexBit.hpp"
#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#endif

#if LUMEX_HAS_CONCEPTS && LUMEX_HAS_STD_BIT_CAST && LUMEX_HAS_STD_RANGES      \
    && LUMEX_HAS_STD_IS_CONSTANT_EVALUATED

using namespace lumex::core::utility::bit;

using lumex::core::utility::bit::byte_swap;

TEST (LumexBitTest, GivenUint16Value_WhenByteSwap_ThenBytesAreReversed)
{
  EXPECT_EQ (byte_swap (static_cast<std::uint16_t> (0x1234)),
             static_cast<std::uint16_t> (0x3412));
}

TEST (LumexBitTest, GivenUint32Value_WhenByteSwap_ThenBytesAreReversed)
{
  EXPECT_EQ (byte_swap (static_cast<std::uint32_t> (0x12345678)),
             static_cast<std::uint32_t> (0x78563412));
}

TEST (LumexBitTest, GivenUint64Value_WhenByteSwap_ThenBytesAreReversed)
{
  EXPECT_EQ (byte_swap (static_cast<std::uint64_t> (0x0123456789ABCDEFULL)),
             static_cast<std::uint64_t> (0xEFCDAB8967452301ULL));
}

TEST (LumexBitTest, GivenSingleByteValue_WhenByteSwap_ThenValueIsUnchanged)
{
  EXPECT_EQ (byte_swap (static_cast<std::uint8_t> (0xAB)),
             static_cast<std::uint8_t> (0xAB));
  EXPECT_EQ (byte_swap (static_cast<std::int8_t> (-1)),
             static_cast<std::int8_t> (-1));
}

TEST (LumexBitTest,
      GivenSignedValue_WhenByteSwap_ThenBitPatternIsReversedConsistently)
{
  std::int32_t const original = -123456789;
  EXPECT_EQ (byte_swap (byte_swap (original)), original);
}

TEST (LumexBitTest, GivenZero_WhenByteSwap_ThenResultIsZero)
{
  EXPECT_EQ (byte_swap (static_cast<std::uint32_t> (0)),
             static_cast<std::uint32_t> (0));
}

TEST (LumexBitTest,
      GivenConstexprContext_WhenByteSwap_ThenEvaluatesAtCompileTime)
{
  constexpr std::uint32_t swapped
      = byte_swap (static_cast<std::uint32_t> (0x12345678));
  LUMEX_STATIC_ASSERT_MSG (
      swapped == 0x78563412U,
      "byte_swap must be usable in a constant expression");
  EXPECT_EQ (swapped, static_cast<std::uint32_t> (0x78563412));
}

TEST (LumexBitTest, GivenShortValue_WhenByteSwap_ThenBytesAreReversed)
{
  EXPECT_EQ (byte_swap (static_cast<short> (0x1234)),
             static_cast<short> (0x3412));
  EXPECT_EQ (byte_swap (static_cast<unsigned short> (0xABCD)),
             static_cast<unsigned short> (0xCDAB));
}

TEST (LumexBitTest, GivenDoubleSwap_WhenByteSwap_ThenRestoresOriginalValue)
{
  EXPECT_EQ (byte_swap (byte_swap (static_cast<std::uint16_t> (0xBEEF))),
             static_cast<std::uint16_t> (0xBEEF));
  EXPECT_EQ (byte_swap (byte_swap (
                 static_cast<std::uint64_t> (0xDEADBEEFCAFEBABEULL))),
             static_cast<std::uint64_t> (0xDEADBEEFCAFEBABEULL));
}

TEST (LumexBitTest, GivenAllOnes_WhenByteSwap_ThenStaysAllOnes)
{
  EXPECT_EQ (byte_swap (static_cast<std::uint16_t> (0xFFFF)),
             static_cast<std::uint16_t> (0xFFFF));
  EXPECT_EQ (byte_swap (static_cast<std::uint32_t> (0xFFFFFFFFu)),
             static_cast<std::uint32_t> (0xFFFFFFFFu));
  EXPECT_EQ (byte_swap (static_cast<std::uint64_t> (~0ULL)),
             static_cast<std::uint64_t> (~0ULL));
}

TEST (LumexBitTest, GivenAllZerosPerWidth_WhenByteSwap_ThenStaysZero)
{
  EXPECT_EQ (byte_swap (static_cast<std::uint8_t> (0)),
             static_cast<std::uint8_t> (0));
  EXPECT_EQ (byte_swap (static_cast<std::uint16_t> (0)),
             static_cast<std::uint16_t> (0));
  EXPECT_EQ (byte_swap (static_cast<std::uint64_t> (0)),
             static_cast<std::uint64_t> (0));
}

TEST (LumexBitTest, GivenSingleSetHighByte_WhenByteSwap_ThenMovesToLowByte)
{
  EXPECT_EQ (byte_swap (static_cast<std::uint32_t> (0xFF000000u)),
             static_cast<std::uint32_t> (0x000000FFu));
  EXPECT_EQ (byte_swap (static_cast<std::uint32_t> (0x000000FFu)),
             static_cast<std::uint32_t> (0xFF000000u));
}

TEST (LumexBitTest, GivenAlternatingBytes_WhenByteSwap_ThenReversesPattern)
{
  EXPECT_EQ (byte_swap (static_cast<std::uint32_t> (0xA0B0C0D0u)),
             static_cast<std::uint32_t> (0xD0C0B0A0u));
  EXPECT_EQ (byte_swap (static_cast<std::uint64_t> (0x0102030405060708ULL)),
             static_cast<std::uint64_t> (0x0807060504030201ULL));
}

TEST (LumexBitTest, GivenSignedMinMax_WhenByteSwapTwice_ThenRestoresOriginal)
{
  EXPECT_EQ (byte_swap (byte_swap (std::numeric_limits<std::int16_t>::min ())),
             std::numeric_limits<std::int16_t>::min ());
  EXPECT_EQ (byte_swap (byte_swap (std::numeric_limits<std::int16_t>::max ())),
             std::numeric_limits<std::int16_t>::max ());
  EXPECT_EQ (byte_swap (byte_swap (std::numeric_limits<std::int32_t>::min ())),
             std::numeric_limits<std::int32_t>::min ());
  EXPECT_EQ (byte_swap (byte_swap (std::numeric_limits<std::int32_t>::max ())),
             std::numeric_limits<std::int32_t>::max ());
  EXPECT_EQ (byte_swap (byte_swap (std::numeric_limits<std::int64_t>::min ())),
             std::numeric_limits<std::int64_t>::min ());
  EXPECT_EQ (byte_swap (byte_swap (std::numeric_limits<std::int64_t>::max ())),
             std::numeric_limits<std::int64_t>::max ());
}

TEST (LumexBitTest, GivenNegativeOne_WhenByteSwap_ThenStaysNegativeOne)
{
  EXPECT_EQ (byte_swap (static_cast<std::int16_t> (-1)),
             static_cast<std::int16_t> (-1));
  EXPECT_EQ (byte_swap (static_cast<std::int32_t> (-1)),
             static_cast<std::int32_t> (-1));
  EXPECT_EQ (byte_swap (static_cast<std::int64_t> (-1)),
             static_cast<std::int64_t> (-1));
}

TEST (LumexBitTest, GivenCharTypes_WhenByteSwap_ThenIdentityForOneByte)
{
  EXPECT_EQ (byte_swap (static_cast<char> (0x7F)), static_cast<char> (0x7F));
  EXPECT_EQ (byte_swap (static_cast<unsigned char> (0x80)),
             static_cast<unsigned char> (0x80));
  EXPECT_EQ (byte_swap (static_cast<signed char> (-128)),
             static_cast<signed char> (-128));
}

TEST (LumexBitTest, GivenLongAndSizeT_WhenByteSwapTwice_ThenRestoresOriginal)
{
  long const signed_long = -0x1234567L;
  unsigned long const unsigned_long = 0x89ABCDEUL;
  std::size_t const size = static_cast<std::size_t> (0x1122334455667788ULL);

  EXPECT_EQ (byte_swap (byte_swap (signed_long)), signed_long);
  EXPECT_EQ (byte_swap (byte_swap (unsigned_long)), unsigned_long);
  EXPECT_EQ (byte_swap (byte_swap (size)), size);
}

TEST (LumexBitTest,
      GivenConstexprSixteenAndSixtyFour_WhenByteSwap_ThenMatchesKnownValues)
{
  constexpr auto swapped16 = byte_swap (static_cast<std::uint16_t> (0xAABB));
  constexpr auto swapped64
      = byte_swap (static_cast<std::uint64_t> (0x1122334455667788ULL));
  LUMEX_STATIC_ASSERT_MSG (swapped16 == 0xBBAA, "constexpr uint16 byte_swap");
  LUMEX_STATIC_ASSERT_MSG (swapped64 == 0x8877665544332211ULL,
                           "constexpr uint64 byte_swap");
  EXPECT_EQ (swapped16, static_cast<std::uint16_t> (0xBBAA));
  EXPECT_EQ (swapped64, static_cast<std::uint64_t> (0x8877665544332211ULL));
}

TEST (LumexBitTest, GivenByteSwap_WhenCalled_ThenIsNoexcept)
{
  LUMEX_STATIC_ASSERT_MSG (
      noexcept (byte_swap (static_cast<std::uint32_t> (1))),
      "byte_swap is noexcept");
  SUCCEED ();
}

TEST (LumexBitTest, GivenPowerOfTwoBoundaries_WhenByteSwap_ThenMovesTheSetBit)
{
  EXPECT_EQ (byte_swap (static_cast<std::uint16_t> (0x0100)),
             static_cast<std::uint16_t> (0x0001));
  EXPECT_EQ (byte_swap (static_cast<std::uint32_t> (0x00010000u)),
             static_cast<std::uint32_t> (0x00000100u));
}

class LumexBitRoundtrip32Test : public ::testing::TestWithParam<std::uint32_t>
{
};

TEST_P (LumexBitRoundtrip32Test,
        GivenArbitraryPattern_WhenByteSwapTwice_ThenRestoresOriginal)
{
  std::uint32_t const value = GetParam ();
  EXPECT_EQ (byte_swap (byte_swap (value)), value);
}

INSTANTIATE_TEST_SUITE_P (KnownPatterns, LumexBitRoundtrip32Test,
                          ::testing::Values (0u, 1u, 0xFFu, 0xFF00u, 0xFF0000u,
                                             0x80000000u, 0x7FFFFFFFu,
                                             0xA5A5A5A5u, 0x5A5A5A5Au));

#else // the toolchain lacks the features of the module

TEST (LumexBitTest, UnavailableOnThisToolchain)
{
  GTEST_SKIP () << "LumexBit.hpp needs C++20 concepts, std::bit_cast, "
                   "std::ranges and std::is_constant_evaluated";
}

#endif
