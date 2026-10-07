// LumexBit.cxx11.tests.cpp
// LumexBit.hpp from C++11: count_leading_zeros for an unsigned integer of 1,
// 2, 4 or 8 bytes, byte_swap for an integral type of 1, 2, 4 or 8 bytes, and
// is_little_endian. The suites from C++20 compile this file too, where
// byte_swap is the std::bit_cast based template; both forms must give the
// same value, so these tests hold for either. LumexBit.cxx20.tests.cpp adds
// the cases that only that template has in mind.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

#include "lumex/core/utility/bit/LumexBit.hpp"

using lumex::core::utility::bit::byte_swap;
using lumex::core::utility::bit::count_leading_zeros;
using lumex::core::utility::bit::is_little_endian;

namespace
{

template <typename T, typename Enable = void>
struct IsCountLeadingZerosCallable : std::false_type
{
};

template <typename T>
struct IsCountLeadingZerosCallable<
    T, typename std::enable_if<
           std::is_same<decltype (count_leading_zeros (std::declval<T> ())),
                        std::uint8_t>::value>::type> : std::true_type
{
};

// The bytes of `value` in memory order, reversed by hand: the reference that
// byte_swap must agree with, written without shifts.
template <typename T>
T
reference_swap (T value)
{
  unsigned char bytes[sizeof (T)];
  std::memcpy (bytes, &value, sizeof (T));
  for (std::size_t low = 0, high = sizeof (T) - 1; low < high; ++low, --high)
    {
      unsigned char const kept = bytes[low];
      bytes[low] = bytes[high];
      bytes[high] = kept;
    }
  T result;
  std::memcpy (&result, bytes, sizeof (T));
  return result;
}

// A fixed pseudo-random sequence (xorshift), so a failure repeats.
std::uint64_t
next_sample (std::uint64_t &state)
{
  state ^= state << 13;
  state ^= state >> 7;
  state ^= state << 17;
  return state;
}

template <typename T>
void
expect_matches_reference ()
{
  std::uint64_t state = 0x9E3779B97F4A7C15ULL;
  for (int round = 0; round < 512; ++round)
    {
      T const value = static_cast<T> (next_sample (state));
      EXPECT_EQ (byte_swap (value), reference_swap (value));
      EXPECT_EQ (byte_swap (byte_swap (value)), value);
    }
}

template <typename T>
void
expect_width_samples ()
{
  std::uint8_t const width
      = static_cast<std::uint8_t> (sizeof (T) * static_cast<std::size_t> (8));
  EXPECT_EQ (count_leading_zeros (static_cast<T> (0)), width);
  EXPECT_EQ (count_leading_zeros (static_cast<T> (1)),
             static_cast<std::uint8_t> (width - 1));

  T const high_bit = static_cast<T> (1)
                     << (sizeof (T) * static_cast<std::size_t> (8) - 1);
  EXPECT_EQ (count_leading_zeros (high_bit), static_cast<std::uint8_t> (0));

  // Three leading zeros: the bit four positions below the top is set.
  T const below_top = static_cast<T> (1)
                      << (sizeof (T) * static_cast<std::size_t> (8) - 4);
  EXPECT_EQ (count_leading_zeros (below_top), static_cast<std::uint8_t> (3));
}

} // namespace

TEST (LumexBitTest, GivenUnsignedWidths_WhenCountLeadingZeros_ThenMatchesWidth)
{
  expect_width_samples<std::uint8_t> ();
  expect_width_samples<std::uint16_t> ();
  expect_width_samples<std::uint32_t> ();
  expect_width_samples<std::uint64_t> ();
}

TEST (LumexBitTest,
      GivenNarrowValue_WhenCountLeadingZeros_ThenAdjustsForThe32BitIntrinsic)
{
  // 0x0F in 8 bits is four leading zeros. A raw 32-bit count would be 28.
  EXPECT_EQ (count_leading_zeros (static_cast<std::uint8_t> (0x0F)),
             static_cast<std::uint8_t> (4));
  EXPECT_EQ (count_leading_zeros (static_cast<std::uint32_t> (0x00F00000U)),
             static_cast<std::uint8_t> (8));
}

TEST (LumexBitTest,
      GivenUint64HighHalf_WhenCountLeadingZeros_ThenScansPast32Bits)
{
  // Bit 32 set: 31 leading zeros. A scan of the low 32 bits alone sees zero.
  EXPECT_EQ (count_leading_zeros (static_cast<std::uint64_t> (1) << 32),
             static_cast<std::uint8_t> (31));
  // Low 32 bits full and the high half clear: 32 leading zeros.
  EXPECT_EQ (count_leading_zeros (static_cast<std::uint64_t> (0xFFFFFFFFULL)),
             static_cast<std::uint8_t> (32));
}

TEST (LumexBitTest, CountLeadingZeros_OverloadSetAndNoexcept)
{
  static_assert (noexcept (count_leading_zeros (std::uint32_t ())),
                 "count_leading_zeros is noexcept");
  static_assert (IsCountLeadingZerosCallable<std::uint32_t>::value,
                 "uint32_t is in the overload set");
  static_assert (IsCountLeadingZerosCallable<std::uint64_t>::value,
                 "uint64_t is in the overload set");
  static_assert (!IsCountLeadingZerosCallable<bool>::value,
                 "bool is not in the overload set");
  static_assert (!IsCountLeadingZerosCallable<int>::value,
                 "a signed integer is not in the overload set");
#if defined(__SIZEOF_INT128__)
  static_assert (!IsCountLeadingZerosCallable<unsigned __int128>::value,
                 "unsigned __int128 is not in the overload set");
#endif
}

TEST (LumexBitTest, GivenUnsignedWidths_WhenByteSwap_ThenBytesAreReversed)
{
  EXPECT_EQ (byte_swap (static_cast<std::uint16_t> (0x1234)),
             static_cast<std::uint16_t> (0x3412));
  EXPECT_EQ (byte_swap (static_cast<std::uint32_t> (0x12345678U)),
             static_cast<std::uint32_t> (0x78563412U));
  EXPECT_EQ (byte_swap (static_cast<std::uint64_t> (0x0123456789ABCDEFULL)),
             static_cast<std::uint64_t> (0xEFCDAB8967452301ULL));
}

TEST (LumexBitTest, GivenOneByteValues_WhenByteSwap_ThenValueIsUnchanged)
{
  EXPECT_EQ (byte_swap (static_cast<std::uint8_t> (0xAB)),
             static_cast<std::uint8_t> (0xAB));
  EXPECT_EQ (byte_swap (static_cast<std::int8_t> (-1)),
             static_cast<std::int8_t> (-1));
  EXPECT_EQ (byte_swap (static_cast<char> (0x7F)), static_cast<char> (0x7F));
}

TEST (LumexBitTest, GivenSignedValues_WhenByteSwap_ThenBytesAreReversed)
{
  // 0x00FF swaps to 0xFF00: a negative int16_t.
  EXPECT_EQ (static_cast<std::uint16_t> (
                 byte_swap (static_cast<std::int16_t> (0x00FF))),
             static_cast<std::uint16_t> (0xFF00));
  EXPECT_EQ (byte_swap (static_cast<std::int32_t> (0x01020304)),
             static_cast<std::int32_t> (0x04030201));
  EXPECT_EQ (byte_swap (static_cast<std::int64_t> (0x0102030405060708LL)),
             static_cast<std::int64_t> (0x0807060504030201LL));
  EXPECT_EQ (byte_swap (static_cast<std::int32_t> (-1)),
             static_cast<std::int32_t> (-1));
  EXPECT_EQ (byte_swap (byte_swap (static_cast<std::int64_t> (-123456789))),
             static_cast<std::int64_t> (-123456789));
}

TEST (LumexBitTest, GivenOtherIntegralTypes_WhenByteSwap_ThenBytesAreReversed)
{
  EXPECT_EQ (byte_swap (static_cast<char16_t> (0x1234)),
             static_cast<char16_t> (0x3412));
  EXPECT_EQ (byte_swap (static_cast<char32_t> (0x12345678U)),
             static_cast<char32_t> (0x78563412U));
  EXPECT_EQ (byte_swap (static_cast<unsigned short> (0xABCD)),
             static_cast<unsigned short> (0xCDAB));
  expect_matches_reference<long> ();
  expect_matches_reference<unsigned long> ();
  expect_matches_reference<long long> ();
  expect_matches_reference<std::size_t> ();
  expect_matches_reference<wchar_t> ();
}

TEST (LumexBitTest, GivenEachBytePosition_WhenByteSwap_ThenItMovesToTheMirror)
{
  for (unsigned int index = 0; index < 4; ++index)
    {
      std::uint32_t const value = static_cast<std::uint32_t> (0xA5U)
                                  << (8 * index);
      EXPECT_EQ (byte_swap (value), static_cast<std::uint32_t> (0xA5U)
                                        << (8 * (3 - index)))
          << "byte " << index;
    }
  for (unsigned int index = 0; index < 8; ++index)
    {
      std::uint64_t const value = static_cast<std::uint64_t> (0xA5U)
                                  << (8 * index);
      EXPECT_EQ (byte_swap (value), static_cast<std::uint64_t> (0xA5U)
                                        << (8 * (7 - index)))
          << "byte " << index;
    }
  for (unsigned int index = 0; index < 2; ++index)
    {
      std::uint16_t const value = static_cast<std::uint16_t> (
          static_cast<unsigned int> (0xA5U) << (8 * index));
      EXPECT_EQ (byte_swap (value),
                 static_cast<std::uint16_t> (static_cast<unsigned int> (0xA5U)
                                             << (8 * (1 - index))))
          << "byte " << index;
    }
}

TEST (LumexBitTest,
      GivenPseudoRandomValues_WhenByteSwap_ThenMatchesTheReference)
{
  expect_matches_reference<std::uint16_t> ();
  expect_matches_reference<std::uint32_t> ();
  expect_matches_reference<std::uint64_t> ();
  expect_matches_reference<std::int16_t> ();
  expect_matches_reference<std::int32_t> ();
  expect_matches_reference<std::int64_t> ();
}

TEST (LumexBitTest, GivenZeroAndAllOnes_WhenByteSwap_ThenStayTheSame)
{
  EXPECT_EQ (byte_swap (static_cast<std::uint32_t> (0)),
             static_cast<std::uint32_t> (0));
  EXPECT_EQ (byte_swap (static_cast<std::uint16_t> (0xFFFFU)),
             static_cast<std::uint16_t> (0xFFFFU));
  EXPECT_EQ (byte_swap (static_cast<std::uint64_t> (~0ULL)),
             static_cast<std::uint64_t> (~0ULL));
}

TEST (LumexBitTest,
      GivenConstantExpression_WhenByteSwap_ThenEvaluatesAtCompileTime)
{
  constexpr std::uint16_t swapped16
      = byte_swap (static_cast<std::uint16_t> (0xAABB));
  constexpr std::uint32_t swapped32
      = byte_swap (static_cast<std::uint32_t> (0x12345678U));
  constexpr std::uint64_t swapped64
      = byte_swap (static_cast<std::uint64_t> (0x1122334455667788ULL));
  static_assert (swapped16 == 0xBBAA, "constexpr uint16 byte_swap");
  static_assert (swapped32 == 0x78563412U, "constexpr uint32 byte_swap");
  static_assert (swapped64 == 0x8877665544332211ULL,
                 "constexpr uint64 byte_swap");
  EXPECT_EQ (swapped16, static_cast<std::uint16_t> (0xBBAA));
  EXPECT_EQ (swapped32, static_cast<std::uint32_t> (0x78563412U));
  EXPECT_EQ (swapped64, static_cast<std::uint64_t> (0x8877665544332211ULL));
}

TEST (LumexBitTest, ByteSwap_ResultTypeAndNoexcept)
{
  static_assert (noexcept (byte_swap (std::uint32_t ())),
                 "byte_swap is noexcept");
  static_assert (std::is_same<decltype (byte_swap (std::int16_t ())),
                              std::int16_t>::value,
                 "the result has the type of the argument");
  static_assert (std::is_same<decltype (byte_swap (std::uint64_t ())),
                              std::uint64_t>::value,
                 "the result has the type of the argument");
}

TEST (LumexBitTest, GivenTheMachine_WhenIsLittleEndian_ThenMatchesTheFirstByte)
{
  std::uint16_t const probe = 0x0102;
  unsigned char first = 0;
  std::memcpy (&first, &probe, 1);
  EXPECT_EQ (is_little_endian (), first == 0x02);
  EXPECT_NE (is_little_endian (), first == 0x01);
}

TEST (LumexBitTest,
      GivenTheCompilerByteOrderMacro_WhenIsLittleEndian_ThenAgrees)
{
#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__)
  EXPECT_EQ (is_little_endian (), __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__);
#else
  GTEST_SKIP () << "the compiler does not define __BYTE_ORDER__";
#endif
}

TEST (LumexBitTest,
      GivenByteSwap_WhenComparedWithTheByteOrder_ThenSwapsMemoryOrder)
{
  // 0x01020304 in memory: 04 03 02 01 on a little endian machine. The swap
  // reverses that, so its first byte is 01 there and 04 on a big endian one.
  std::uint32_t const swapped
      = byte_swap (static_cast<std::uint32_t> (0x01020304U));
  unsigned char first = 0;
  std::memcpy (&first, &swapped, 1);
  EXPECT_EQ (first, is_little_endian () ? 0x01 : 0x04);
}

TEST (LumexBitTest, IsLittleEndian_Noexcept)
{
  static_assert (noexcept (is_little_endian ()),
                 "is_little_endian is noexcept");
  SUCCEED ();
}
