// LumexBit.tests.cpp
#include <cstdint>
#include <limits>
#include <type_traits>
#include <version>

#if defined(__cpp_lib_byteswap) && __cpp_lib_byteswap >= 202110L
#include <bit>
#endif

#include <gtest/gtest.h>

#include "lumex/core/utility/assert/LumexAssert.hpp"
#include "lumex/core/utility/bit/LumexBit.hpp"
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#endif

using namespace lumex::core::utility::bit;

using lumex::core::utility::bit::ByteSwap;

TEST (LumexBitTest, GivenUint16Value_WhenByteSwap_ThenBytesAreReversed)
{
  EXPECT_EQ (ByteSwap (static_cast<std::uint16_t> (0x1234)),
             static_cast<std::uint16_t> (0x3412));
}

TEST (LumexBitTest, GivenUint32Value_WhenByteSwap_ThenBytesAreReversed)
{
  EXPECT_EQ (ByteSwap (static_cast<std::uint32_t> (0x12345678)),
             static_cast<std::uint32_t> (0x78563412));
}

TEST (LumexBitTest, GivenUint64Value_WhenByteSwap_ThenBytesAreReversed)
{
  EXPECT_EQ (ByteSwap (static_cast<std::uint64_t> (0x0123456789ABCDEFULL)),
             static_cast<std::uint64_t> (0xEFCDAB8967452301ULL));
}

TEST (LumexBitTest, GivenSingleByteValue_WhenByteSwap_ThenValueIsUnchanged)
{
  EXPECT_EQ (ByteSwap (static_cast<std::uint8_t> (0xAB)),
             static_cast<std::uint8_t> (0xAB));
  EXPECT_EQ (ByteSwap (static_cast<std::int8_t> (-1)),
             static_cast<std::int8_t> (-1));
}

TEST (LumexBitTest,
      GivenSignedValue_WhenByteSwap_ThenBitPatternIsReversedConsistently)
{
  std::int32_t const original = -123456789;
  EXPECT_EQ (ByteSwap (ByteSwap (original)), original);
}

TEST (LumexBitTest, GivenZero_WhenByteSwap_ThenResultIsZero)
{
  EXPECT_EQ (ByteSwap (static_cast<std::uint32_t> (0)),
             static_cast<std::uint32_t> (0));
}

TEST (LumexBitTest,
      GivenConstexprContext_WhenByteSwap_ThenEvaluatesAtCompileTime)
{
  constexpr std::uint32_t swapped
      = ByteSwap (static_cast<std::uint32_t> (0x12345678));
  LUMEX_STATIC_ASSERT_MSG (swapped == 0x78563412U,
                           "ByteSwap must be usable in a constant expression");
  EXPECT_EQ (swapped, static_cast<std::uint32_t> (0x78563412));
}

TEST (LumexBitTest, GivenShortValue_WhenByteSwap_ThenBytesAreReversed)
{
  EXPECT_EQ (ByteSwap (static_cast<short> (0x1234)),
             static_cast<short> (0x3412));
  EXPECT_EQ (ByteSwap (static_cast<unsigned short> (0xABCD)),
             static_cast<unsigned short> (0xCDAB));
}

TEST (LumexBitTest, GivenDoubleSwap_WhenByteSwap_ThenRestoresOriginalValue)
{
  EXPECT_EQ (ByteSwap (ByteSwap (static_cast<std::uint16_t> (0xBEEF))),
             static_cast<std::uint16_t> (0xBEEF));
  EXPECT_EQ (
      ByteSwap (ByteSwap (static_cast<std::uint64_t> (0xDEADBEEFCAFEBABEULL))),
      static_cast<std::uint64_t> (0xDEADBEEFCAFEBABEULL));
}

TEST (LumexBitTest, GivenAllOnes_WhenByteSwap_ThenStaysAllOnes)
{
  EXPECT_EQ (ByteSwap (static_cast<std::uint16_t> (0xFFFF)),
             static_cast<std::uint16_t> (0xFFFF));
  EXPECT_EQ (ByteSwap (static_cast<std::uint32_t> (0xFFFFFFFFu)),
             static_cast<std::uint32_t> (0xFFFFFFFFu));
  EXPECT_EQ (ByteSwap (static_cast<std::uint64_t> (~0ULL)),
             static_cast<std::uint64_t> (~0ULL));
}

TEST (LumexBitTest, GivenAllZerosPerWidth_WhenByteSwap_ThenStaysZero)
{
  EXPECT_EQ (ByteSwap (static_cast<std::uint8_t> (0)),
             static_cast<std::uint8_t> (0));
  EXPECT_EQ (ByteSwap (static_cast<std::uint16_t> (0)),
             static_cast<std::uint16_t> (0));
  EXPECT_EQ (ByteSwap (static_cast<std::uint64_t> (0)),
             static_cast<std::uint64_t> (0));
}

TEST (LumexBitTest, GivenSingleSetHighByte_WhenByteSwap_ThenMovesToLowByte)
{
  EXPECT_EQ (ByteSwap (static_cast<std::uint32_t> (0xFF000000u)),
             static_cast<std::uint32_t> (0x000000FFu));
  EXPECT_EQ (ByteSwap (static_cast<std::uint32_t> (0x000000FFu)),
             static_cast<std::uint32_t> (0xFF000000u));
}

TEST (LumexBitTest, GivenAlternatingBytes_WhenByteSwap_ThenReversesPattern)
{
  EXPECT_EQ (ByteSwap (static_cast<std::uint32_t> (0xA0B0C0D0u)),
             static_cast<std::uint32_t> (0xD0C0B0A0u));
  EXPECT_EQ (ByteSwap (static_cast<std::uint64_t> (0x0102030405060708ULL)),
             static_cast<std::uint64_t> (0x0807060504030201ULL));
}

TEST (LumexBitTest, GivenSignedMinMax_WhenByteSwapTwice_ThenRestoresOriginal)
{
  EXPECT_EQ (ByteSwap (ByteSwap (std::numeric_limits<std::int16_t>::min ())),
             std::numeric_limits<std::int16_t>::min ());
  EXPECT_EQ (ByteSwap (ByteSwap (std::numeric_limits<std::int16_t>::max ())),
             std::numeric_limits<std::int16_t>::max ());
  EXPECT_EQ (ByteSwap (ByteSwap (std::numeric_limits<std::int32_t>::min ())),
             std::numeric_limits<std::int32_t>::min ());
  EXPECT_EQ (ByteSwap (ByteSwap (std::numeric_limits<std::int32_t>::max ())),
             std::numeric_limits<std::int32_t>::max ());
  EXPECT_EQ (ByteSwap (ByteSwap (std::numeric_limits<std::int64_t>::min ())),
             std::numeric_limits<std::int64_t>::min ());
  EXPECT_EQ (ByteSwap (ByteSwap (std::numeric_limits<std::int64_t>::max ())),
             std::numeric_limits<std::int64_t>::max ());
}

TEST (LumexBitTest, GivenNegativeOne_WhenByteSwap_ThenStaysNegativeOne)
{
  EXPECT_EQ (ByteSwap (static_cast<std::int16_t> (-1)),
             static_cast<std::int16_t> (-1));
  EXPECT_EQ (ByteSwap (static_cast<std::int32_t> (-1)),
             static_cast<std::int32_t> (-1));
  EXPECT_EQ (ByteSwap (static_cast<std::int64_t> (-1)),
             static_cast<std::int64_t> (-1));
}

TEST (LumexBitTest, GivenCharTypes_WhenByteSwap_ThenIdentityForOneByte)
{
  EXPECT_EQ (ByteSwap (static_cast<char> (0x7F)), static_cast<char> (0x7F));
  EXPECT_EQ (ByteSwap (static_cast<unsigned char> (0x80)),
             static_cast<unsigned char> (0x80));
  EXPECT_EQ (ByteSwap (static_cast<signed char> (-128)),
             static_cast<signed char> (-128));
}

TEST (LumexBitTest, GivenLongAndSizeT_WhenByteSwapTwice_ThenRestoresOriginal)
{
  long const signed_long = -0x1234567L;
  unsigned long const unsigned_long = 0x89ABCDEUL;
  std::size_t const size = static_cast<std::size_t> (0x1122334455667788ULL);

  EXPECT_EQ (ByteSwap (ByteSwap (signed_long)), signed_long);
  EXPECT_EQ (ByteSwap (ByteSwap (unsigned_long)), unsigned_long);
  EXPECT_EQ (ByteSwap (ByteSwap (size)), size);
}

TEST (LumexBitTest,
      GivenConstexprSixteenAndSixtyFour_WhenByteSwap_ThenMatchesKnownValues)
{
  constexpr auto swapped16 = ByteSwap (static_cast<std::uint16_t> (0xAABB));
  constexpr auto swapped64
      = ByteSwap (static_cast<std::uint64_t> (0x1122334455667788ULL));
  LUMEX_STATIC_ASSERT_MSG (swapped16 == 0xBBAA, "constexpr uint16 ByteSwap");
  LUMEX_STATIC_ASSERT_MSG (swapped64 == 0x8877665544332211ULL,
                           "constexpr uint64 ByteSwap");
  EXPECT_EQ (swapped16, static_cast<std::uint16_t> (0xBBAA));
  EXPECT_EQ (swapped64, static_cast<std::uint64_t> (0x8877665544332211ULL));
}

TEST (LumexBitTest, GivenByteSwap_WhenCalled_ThenIsNoexcept)
{
  LUMEX_STATIC_ASSERT_MSG (
      noexcept (ByteSwap (static_cast<std::uint32_t> (1))),
      "ByteSwap is noexcept");
  SUCCEED ();
}

TEST (LumexBitTest, GivenPowerOfTwoBoundaries_WhenByteSwap_ThenMovesTheSetBit)
{
  EXPECT_EQ (ByteSwap (static_cast<std::uint16_t> (0x0100)),
             static_cast<std::uint16_t> (0x0001));
  EXPECT_EQ (ByteSwap (static_cast<std::uint32_t> (0x00010000u)),
             static_cast<std::uint32_t> (0x00000100u));
}

#if defined(__cpp_lib_byteswap) && __cpp_lib_byteswap >= 202110L
TEST (LumexBitTest,
      GivenCpp23Byteswap_WhenCompared_ThenMatchesStandardByteswap)
{
  std::uint16_t const u16 = 0xBEEF;
  std::uint32_t const u32 = 0xCAFEBABEu;
  std::uint64_t const u64 = 0x0123456789ABCDEFULL;
  EXPECT_EQ (ByteSwap (u16), std::byteswap (u16));
  EXPECT_EQ (ByteSwap (u32), std::byteswap (u32));
  EXPECT_EQ (ByteSwap (u64), std::byteswap (u64));
  EXPECT_EQ (ByteSwap (static_cast<std::int32_t> (-99)),
             std::byteswap (static_cast<std::int32_t> (-99)));
}
#endif

class LumexBitRoundtrip32Test : public ::testing::TestWithParam<std::uint32_t>
{
};

TEST_P (LumexBitRoundtrip32Test,
        GivenArbitraryPattern_WhenByteSwapTwice_ThenRestoresOriginal)
{
  std::uint32_t const value = GetParam ();
  EXPECT_EQ (ByteSwap (ByteSwap (value)), value);
}

INSTANTIATE_TEST_SUITE_P (KnownPatterns, LumexBitRoundtrip32Test,
                          ::testing::Values (0u, 1u, 0xFFu, 0xFF00u, 0xFF0000u,
                                             0x80000000u, 0x7FFFFFFFu,
                                             0xA5A5A5A5u, 0x5A5A5A5Au));
