// LumexSpanTraits.cxx11.tests.cpp
//
// dynamic_extent, the byte type and its operators, and the
// is_contiguous_iterator trait. The byte tests run at every standard: from
// C++17 `byte` is std::byte, before it is the module's own enumeration with
// the same operators.
#include <cstddef>
#include <deque>
#include <limits>
#include <list>
#include <string>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include "LumexSpanTestSupport.hpp"
#include "lumex/core/span/LumexSpan"

using lumex::core::span::view::byte;
using lumex::core::span::view::dynamic_extent;
using lumex::core::span::view::is_contiguous_iterator;
using lumex::core::span::view::span;
using lumex::core::span::view::to_integer;

namespace
{
template <std::size_t Value> struct constant_extent
{
  static const std::size_t value = Value;
};
} // namespace

TEST (LumexSpanTraitsTest, GivenDynamicExtent_WhenInspected_ThenLargestSizeT)
{
  static_assert (
      std::is_same<decltype (dynamic_extent), std::size_t const>::value,
      "dynamic_extent is a std::size_t constant");
  EXPECT_EQ (dynamic_extent, static_cast<std::size_t> (-1));
  EXPECT_EQ (dynamic_extent, std::numeric_limits<std::size_t>::max ());
}

TEST (LumexSpanTraitsTest, GivenDynamicExtent_WhenTemplateArgument_ThenUsable)
{
  static_assert (constant_extent<dynamic_extent>::value == dynamic_extent,
                 "usable as a non-type template argument");
  static_assert (span<int>::extent == dynamic_extent,
                 "the default extent is dynamic");
  static_assert (std::is_same<span<int>, span<int, dynamic_extent>>::value,
                 "the default extent is dynamic_extent");
  SUCCEED ();
}

TEST (LumexSpanTraitsTest, GivenByte_WhenInspected_ThenOneByteEnumeration)
{
  static_assert (std::is_enum<byte>::value, "byte is an enumeration");
  static_assert (sizeof (byte) == 1, "byte has the size of a char");
  static_assert (
      std::is_same<std::underlying_type<byte>::type, unsigned char>::value,
      "byte is unsigned char underneath");
  static_assert (std::is_trivially_copyable<byte>::value, "trivial");
  SUCCEED ();
}

TEST (LumexSpanTraitsTest, GivenByte_WhenToInteger_ThenValue)
{
  byte const zero = static_cast<byte> (0);
  byte const high = static_cast<byte> (0xA5);
  EXPECT_EQ (to_integer<int> (zero), 0);
  EXPECT_EQ (to_integer<int> (high), 0xA5);
  EXPECT_EQ (to_integer<unsigned char> (high), 0xA5u);
  EXPECT_EQ (to_integer<unsigned long long> (high), 0xA5ull);
  EXPECT_EQ (to_integer<char> (static_cast<byte> (65)), 'A');
}

TEST (LumexSpanTraitsTest, GivenBytes_WhenBitwiseOperators_ThenByteWise)
{
  byte const left = static_cast<byte> (0xF0);
  byte const right = static_cast<byte> (0x3C);
  EXPECT_EQ (to_integer<int> (left | right), 0xFC);
  EXPECT_EQ (to_integer<int> (left & right), 0x30);
  EXPECT_EQ (to_integer<int> (left ^ right), 0xCC);
  EXPECT_EQ (to_integer<int> (~left), 0x0F);
  EXPECT_EQ (to_integer<int> (~static_cast<byte> (0)), 0xFF);
}

TEST (LumexSpanTraitsTest, GivenByte_WhenShift_ThenBitsMoveAndAreCut)
{
  byte const value = static_cast<byte> (0x81);
  EXPECT_EQ (to_integer<int> (value << 1), 0x02);
  EXPECT_EQ (to_integer<int> (value >> 1), 0x40);
  EXPECT_EQ (to_integer<int> (value << 0), 0x81);
  EXPECT_EQ (to_integer<int> (value >> 7), 0x01);
  EXPECT_EQ (to_integer<int> (value << 7ul), 0x80);
  EXPECT_EQ (to_integer<int> (static_cast<byte> (1) << 8), 0x00);
}

TEST (LumexSpanTraitsTest, GivenByte_WhenCompoundOperators_ThenInPlace)
{
  byte value = static_cast<byte> (0x0F);
  value |= static_cast<byte> (0xF0);
  EXPECT_EQ (to_integer<int> (value), 0xFF);
  value &= static_cast<byte> (0x3C);
  EXPECT_EQ (to_integer<int> (value), 0x3C);
  value ^= static_cast<byte> (0xFF);
  EXPECT_EQ (to_integer<int> (value), 0xC3);
  value <<= 2;
  EXPECT_EQ (to_integer<int> (value), 0x0C);
  value >>= 2;
  EXPECT_EQ (to_integer<int> (value), 0x03);
}

TEST (LumexSpanTraitsTest,
      GivenByteOperators_WhenConstantExpression_ThenUsable)
{
  static_assert (
      to_integer<int> (static_cast<byte> (3) | static_cast<byte> (4)) == 7,
      "or in a constant expression");
  static_assert (
      to_integer<int> (static_cast<byte> (6) & static_cast<byte> (3)) == 2,
      "and in a constant expression");
  static_assert (to_integer<int> (static_cast<byte> (1) << 4) == 16,
                 "shift in a constant expression");
  SUCCEED ();
}

#if LUMEX_HAS_STD_BYTE
TEST (LumexSpanTraitsTest, GivenStdByteLibrary_WhenByte_ThenStdByte)
{
  static_assert (std::is_same<byte, std::byte>::value,
                 "from C++17 byte is std::byte");
  SUCCEED ();
}
#else
TEST (LumexSpanTraitsTest, GivenNoStdByte_WhenByte_ThenOwnEnumeration)
{
  static_assert (std::is_enum<byte>::value
                     && !std::is_convertible<byte, int>::value,
                 "the own byte is a scoped enumeration");
  SUCCEED ();
}
#endif

TEST (LumexSpanTraitsTest, GivenObjectPointers_WhenContiguousIterator_ThenTrue)
{
  EXPECT_TRUE (is_contiguous_iterator<int *>::value);
  EXPECT_TRUE (is_contiguous_iterator<int const *>::value);
  EXPECT_TRUE (is_contiguous_iterator<int volatile *>::value);
  EXPECT_TRUE (is_contiguous_iterator<int const volatile *>::value);
  EXPECT_TRUE (is_contiguous_iterator<std::string *>::value);
  EXPECT_TRUE (is_contiguous_iterator<int **>::value);
}

TEST (LumexSpanTraitsTest,
      GivenVoidAndFunctionPointers_WhenContiguous_ThenFalse)
{
  EXPECT_FALSE (is_contiguous_iterator<void *>::value);
  EXPECT_FALSE (is_contiguous_iterator<void const *>::value);
  EXPECT_FALSE (is_contiguous_iterator<void (*) ()>::value);
}

TEST (LumexSpanTraitsTest, GivenNonIterators_WhenContiguous_ThenFalse)
{
  EXPECT_FALSE (is_contiguous_iterator<int>::value);
  EXPECT_FALSE (is_contiguous_iterator<std::size_t>::value);
  EXPECT_FALSE (is_contiguous_iterator<std::string>::value);
  EXPECT_FALSE (is_contiguous_iterator<std::nullptr_t>::value);
}

TEST (LumexSpanTraitsTest,
      GivenNodeAndSegmentedContainers_WhenContiguous_ThenFalse)
{
  EXPECT_FALSE (is_contiguous_iterator<std::list<int>::iterator>::value);
  EXPECT_FALSE (is_contiguous_iterator<std::deque<int>::iterator>::value);
  EXPECT_FALSE (is_contiguous_iterator<std::vector<bool>::iterator>::value);
}

TEST (LumexSpanTraitsTest,
      GivenUnregisteredIteratorClass_WhenContiguous_ThenFalse)
{
  EXPECT_FALSE (is_contiguous_iterator<
                lumex_span_test::unregistered_iterator<int>>::value);
}

TEST (LumexSpanTraitsTest,
      GivenRegisteredIteratorClass_WhenContiguous_ThenTrue)
{
  EXPECT_TRUE (
      is_contiguous_iterator<lumex_span_test::pointer_iterator<int>>::value);
  EXPECT_TRUE (is_contiguous_iterator<
               lumex_span_test::pointer_iterator<int const>>::value);
}

#if LUMEX_SPAN_HAS_CONCEPTS
TEST (LumexSpanTraitsTest,
      GivenStandardContiguousContainers_WhenConcepts_ThenTrue)
{
  EXPECT_TRUE (is_contiguous_iterator<std::vector<int>::iterator>::value);
  EXPECT_TRUE (
      is_contiguous_iterator<std::vector<int>::const_iterator>::value);
  EXPECT_TRUE (is_contiguous_iterator<std::string::iterator>::value);
  EXPECT_FALSE (is_contiguous_iterator<std::string::reverse_iterator>::value);
}
#else
TEST (LumexSpanTraitsTest,
      GivenStandardIteratorClasses_WhenNoConcepts_ThenOptInOnly)
{
  EXPECT_FALSE (is_contiguous_iterator<std::vector<int>::iterator>::value);
  EXPECT_FALSE (is_contiguous_iterator<std::string::iterator>::value);
}
#endif
