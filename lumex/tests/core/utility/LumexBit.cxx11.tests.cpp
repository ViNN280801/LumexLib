// LumexBit.cxx11.tests.cpp
// count_leading_zeros of LumexBit.hpp, declared from C++11 for an unsigned
// integer of 1, 2, 4 or 8 bytes. byte_swap stays in LumexBit.cxx20.tests.cpp.
#include <cstdint>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

#include "lumex/core/utility/bit/LumexBit.hpp"

using lumex::core::utility::bit::count_leading_zeros;

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
