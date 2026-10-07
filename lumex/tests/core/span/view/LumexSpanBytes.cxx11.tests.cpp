// LumexSpanBytes.cxx11.tests.cpp
//
// as_bytes and as_writable_bytes: the result types and extents, the viewed
// memory, the writes through the writable view, and the element types they
// must not accept (const for the writable view, volatile for both).
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/span/LumexSpan"

using lumex::core::span::view::byte;
using lumex::core::span::view::dynamic_extent;
using lumex::core::span::view::span;
using lumex::core::span::view::to_integer;

namespace
{
template <typename Expected, typename Actual>
void
expect_type (Actual const &)
{
  static_assert (std::is_same<Expected, Actual>::value,
                 "the byte view has the wrong type");
}

template <typename T> struct make_void
{
  using type = void;
};

template <typename Span, typename = void> struct has_as_bytes : std::false_type
{
};

template <typename Span>
struct has_as_bytes<Span, typename make_void<decltype (as_bytes (
                              std::declval<Span> ()))>::type> : std::true_type
{
};

template <typename Span, typename = void>
struct has_as_writable_bytes : std::false_type
{
};

template <typename Span>
struct has_as_writable_bytes<Span,
                             typename make_void<decltype (as_writable_bytes (
                                 std::declval<Span> ()))>::type>
    : std::true_type
{
};
} // namespace

TEST (LumexSpanBytesTest, GivenStaticSpan_WhenAsBytes_ThenStaticByteExtent)
{
  std::int32_t values[3] = { 1, 2, 3 };
  span<std::int32_t, 3> const view (values);
  expect_type<span<byte const, 12>> (as_bytes (view));
  EXPECT_EQ (as_bytes (view).size (), 12u);
  EXPECT_EQ (as_bytes (view).size_bytes (), 12u);
  EXPECT_EQ (static_cast<void const *> (as_bytes (view).data ()),
             static_cast<void const *> (values));
}

TEST (LumexSpanBytesTest, GivenDynamicSpan_WhenAsBytes_ThenDynamicByteExtent)
{
  std::int64_t values[3] = { 1, 2, 3 };
  span<std::int64_t> const view (values);
  expect_type<span<byte const, dynamic_extent>> (as_bytes (view));
  EXPECT_EQ (as_bytes (view).size (), 24u);
}

TEST (LumexSpanBytesTest, GivenSpanOfConst_WhenAsBytes_ThenSameResult)
{
  std::int16_t const values[4] = { 1, 2, 3, 4 };
  span<std::int16_t const, 4> const fixed (values);
  span<std::int16_t const> const dynamic (values);
  expect_type<span<byte const, 8>> (as_bytes (fixed));
  expect_type<span<byte const, dynamic_extent>> (as_bytes (dynamic));
  EXPECT_EQ (as_bytes (dynamic).size (), 8u);
}

TEST (LumexSpanBytesTest, GivenSpan_WhenAsBytes_ThenViewsObjectRepresentation)
{
  std::uint32_t const value = 0x01020304u;
  span<std::uint32_t const, 1> const view (&value, 1);
  unsigned char expected[sizeof (value)];
  std::memcpy (expected, &value, sizeof (value));
  span<byte const, 4> const bytes = as_bytes (view);
  for (std::size_t index = 0; index < bytes.size (); ++index)
    {
      EXPECT_EQ (to_integer<unsigned> (bytes[index]), expected[index]);
    }
}

TEST (LumexSpanBytesTest, GivenSpanOfChar_WhenAsBytes_ThenOneByteEach)
{
  char text[] = { 'a', 'b', 'c' };
  span<char> const view (text);
  EXPECT_EQ (as_bytes (view).size (), 3u);
  EXPECT_EQ (to_integer<char> (as_bytes (view)[1]), 'b');
  expect_type<span<byte const, dynamic_extent>> (as_bytes (view));
  EXPECT_EQ ((as_bytes (span<char, 3> (text))).extent, 3u);
}

TEST (LumexSpanBytesTest, GivenEmptySpan_WhenAsBytes_ThenEmpty)
{
  EXPECT_TRUE (as_bytes (span<int> ()).empty ());
  EXPECT_EQ (as_bytes (span<int> ()).data (), nullptr);
  EXPECT_TRUE (as_bytes (span<int, 0> ()).empty ());
  expect_type<span<byte const, 0>> (as_bytes (span<int, 0> ()));
  EXPECT_TRUE (as_writable_bytes (span<int> ()).empty ());
}

TEST (LumexSpanBytesTest,
      GivenSpanOfClassType_WhenAsBytes_ThenSizeofElementPerElement)
{
  struct padded
  {
    char tag;
    double value;
  };
  padded values[2] = {};
  span<padded> const view (values);
  EXPECT_EQ (as_bytes (view).size (), 2 * sizeof (padded));
  expect_type<span<byte const, 2 * sizeof (padded)>> (
      as_bytes (span<padded, 2> (values)));
}

TEST (LumexSpanBytesTest, GivenWritableBytes_WhenWritten_ThenObjectChanges)
{
  std::uint32_t value = 0;
  span<std::uint32_t, 1> const view (&value, 1);
  span<byte, 4> const bytes = as_writable_bytes (view);
  for (std::size_t index = 0; index < bytes.size (); ++index)
    {
      bytes[index] = static_cast<byte> (0xFF);
    }
  EXPECT_EQ (value, 0xFFFFFFFFu);
  bytes[0] = static_cast<byte> (0);
  unsigned char raw[sizeof (value)];
  std::memcpy (raw, &value, sizeof (value));
  EXPECT_EQ (raw[0], 0);
  EXPECT_EQ (raw[1], 0xFF);
}

TEST (LumexSpanBytesTest, GivenWritableBytes_WhenTypes_ThenMutableByteSpan)
{
  std::int32_t values[3] = { 1, 2, 3 };
  expect_type<span<byte, 12>> (
      as_writable_bytes (span<std::int32_t, 3> (values)));
  expect_type<span<byte, dynamic_extent>> (
      as_writable_bytes (span<std::int32_t> (values)));
  EXPECT_EQ (as_writable_bytes (span<std::int32_t> (values)).size (), 12u);
}

TEST (LumexSpanBytesTest,
      GivenWritableBytes_WhenCopyIntoObject_ThenValueRoundTrips)
{
  std::vector<std::uint16_t> target (2, 0);
  std::uint16_t const source[2] = { 0x1234, 0xABCD };
  span<byte> const destination
      = as_writable_bytes (span<std::uint16_t> (target));
  span<byte const> const origin
      = as_bytes (span<std::uint16_t const> (source));
  ASSERT_EQ (destination.size (), origin.size ());
  for (std::size_t index = 0; index < origin.size (); ++index)
    {
      destination[index] = origin[index];
    }
  EXPECT_EQ (target[0], 0x1234);
  EXPECT_EQ (target[1], 0xABCD);
}

TEST (LumexSpanBytesTest,
      GivenConstAndVolatileElements_WhenByteViews_ThenOnlyAllowedOnes)
{
  static_assert (has_as_bytes<span<int>>::value, "");
  static_assert (has_as_bytes<span<int const>>::value, "");
  static_assert (has_as_bytes<span<int, 3>>::value, "");
  static_assert (has_as_writable_bytes<span<int>>::value, "");
  static_assert (has_as_writable_bytes<span<int, 3>>::value, "");
  static_assert (!has_as_writable_bytes<span<int const>>::value,
                 "a const element has no writable byte view");
  static_assert (!has_as_writable_bytes<span<int const, 3>>::value, "");
  static_assert (!has_as_bytes<span<int volatile>>::value,
                 "a volatile element has no byte view");
  static_assert (!has_as_writable_bytes<span<int volatile>>::value, "");
  static_assert (!has_as_bytes<span<int const volatile>>::value, "");
  static_assert (!has_as_bytes<int *>::value, "only a span is accepted");
  static_assert (!has_as_bytes<std::vector<int> &>::value,
                 "a container is not a span");
  SUCCEED ();
}

TEST (LumexSpanBytesTest, GivenByteViews_WhenNoexcept_ThenNoexcept)
{
  span<int> view;
  static_assert (noexcept (as_bytes (view)), "");
  static_assert (noexcept (as_writable_bytes (view)), "");
  SUCCEED ();
}

TEST (LumexSpanBytesTest,
      GivenByteView_WhenFromStdArray_ThenUsableThroughConversion)
{
  std::array<std::uint8_t, 4> values = { { 1, 2, 3, 4 } };
  span<byte const> const bytes = as_bytes (span<std::uint8_t const> (values));
  EXPECT_EQ (bytes.size (), 4u);
  EXPECT_EQ (to_integer<int> (bytes.back ()), 4);
}
