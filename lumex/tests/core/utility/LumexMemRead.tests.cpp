// LumexMemRead.tests.cpp
#include <array>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/utility/mem/LumexMemRead.hpp"

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
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif

#if defined(__clang__)
#endif

using namespace lumex::core::utility::mem;

using lumex::core::utility::mem::As;

namespace
{
struct Point
{
  std::int32_t x;
  std::int32_t y;

  bool
  operator== (Point const &other) const
  {
    return x == other.x && y == other.y;
  }
};

struct FakeDataSource
{
  std::vector<unsigned char> bytes;

  void const *
  GetData () const
  {
    return bytes.data ();
  }

  int
  GetDataSize () const
  {
    return static_cast<int> (bytes.size ());
  }
};

struct NegativeSizeSource
{
  std::uint32_t value = 0xAABBCCDDu;

  void const *
  GetData () const
  {
    return &value;
  }

  int
  GetDataSize () const
  {
    return -1;
  }
};

struct NullDataSource
{
  void const *
  GetData () const
  {
    return nullptr;
  }

  int
  GetDataSize () const
  {
    return 16;
  }
};

struct EmptyLayout
{
};
} // namespace

TEST (LumexMemReadTest, GivenSufficientBuffer_WhenAs_ThenDecodesValue)
{
  std::uint32_t const value = 0xDEADBEEF;
  auto const result = As<std::uint32_t> (&value, sizeof (value));
  ASSERT_TRUE (result.has_value ());
  EXPECT_EQ (*result, value);
}

TEST (LumexMemReadTest, GivenTooSmallBuffer_WhenAs_ThenReturnsNullopt)
{
  std::uint16_t const value = 0x1234;
  auto const result = As<std::uint32_t> (&value, sizeof (value));
  EXPECT_FALSE (result.has_value ());
}

TEST (LumexMemReadTest, GivenNullptrBuffer_WhenAs_ThenReturnsNullopt)
{
  auto const result = As<std::uint32_t> (nullptr, 4);
  EXPECT_FALSE (result.has_value ());
}

TEST (LumexMemReadTest, GivenUnalignedBuffer_WhenAs_ThenDecodesValueCorrectly)
{
  std::array<unsigned char, sizeof (std::uint32_t) + 1> buffer{};
  std::uint32_t const value = 0x01020304;
  std::memcpy (buffer.data () + 1, &value, sizeof (value));

  auto const result = As<std::uint32_t> (buffer.data () + 1, sizeof (value));
  ASSERT_TRUE (result.has_value ());
  EXPECT_EQ (*result, value);
}

TEST (LumexMemReadTest, GivenStructType_WhenAs_ThenDecodesStructCorrectly)
{
  Point const point{ 42, -7 };
  auto const result = As<Point> (&point, sizeof (point));
  ASSERT_TRUE (result.has_value ());
  EXPECT_EQ (*result, point);
}

TEST (LumexMemReadTest, GivenDataSourceObject_WhenAs_ThenDecodesFromSource)
{
  FakeDataSource source;
  std::uint32_t const value = 0xCAFEBABE;
  source.bytes.resize (sizeof (value));
  std::memcpy (source.bytes.data (), &value, sizeof (value));

  auto const result = As<std::uint32_t> (source);
  ASSERT_TRUE (result.has_value ());
  EXPECT_EQ (*result, value);
}

TEST (LumexMemReadTest, GivenTooSmallDataSource_WhenAs_ThenReturnsNullopt)
{
  FakeDataSource source;
  source.bytes = { 0x01, 0x02 };

  auto const result = As<std::uint32_t> (source);
  EXPECT_FALSE (result.has_value ());
}

TEST (LumexMemReadTest, GivenByteSpan_WhenAs_ThenDecodesValue)
{
  std::uint32_t const value = 0x11223344;
  std::array<std::byte, sizeof (value)> buffer{};
  std::memcpy (buffer.data (), &value, sizeof (value));

  std::span<std::byte const> const span (buffer);
  auto const result = As<std::uint32_t> (span);
  ASSERT_TRUE (result.has_value ());
  EXPECT_EQ (*result, value);
}

TEST (LumexMemReadTest, GivenCharSpanTooSmall_WhenAs_ThenReturnsNullopt)
{
  std::array<char, 2> buffer{ 'a', 'b' };
  std::span<char const> const span (buffer);

  auto const result = As<std::uint32_t> (span);
  EXPECT_FALSE (result.has_value ());
}

TEST (LumexMemReadTest, GivenZeroSize_WhenAs_ThenReturnsNullopt)
{
  std::uint32_t const value = 1;
  EXPECT_FALSE (As<std::uint32_t> (&value, 0).has_value ());
}

TEST (LumexMemReadTest, GivenNullAndZeroSize_WhenAs_ThenReturnsNullopt)
{
  EXPECT_FALSE (As<std::uint32_t> (nullptr, 0).has_value ());
}

TEST (LumexMemReadTest, GivenSizeOneLessThanNeeded_WhenAs_ThenReturnsNullopt)
{
  std::array<unsigned char, sizeof (std::uint64_t)> buffer{};
  EXPECT_FALSE (As<std::uint64_t> (buffer.data (), sizeof (std::uint64_t) - 1)
                    .has_value ());
}

TEST (LumexMemReadTest, GivenExtraTrailingBytes_WhenAs_ThenDecodesOnlyPrefix)
{
  std::array<unsigned char, 8> buffer{ 0x11, 0x22, 0x33, 0x44,
                                       0x55, 0x66, 0x77, 0x88 };
  auto const result = As<std::uint32_t> (buffer.data (), buffer.size ());
  ASSERT_TRUE (result.has_value ());
  std::uint32_t expected = 0;
  std::memcpy (&expected, buffer.data (), sizeof (expected));
  EXPECT_EQ (*result, expected);
}

TEST (LumexMemReadTest, GivenUint8AndUint64_WhenAs_ThenDecodesEachWidth)
{
  std::uint8_t const u8 = 0xAB;
  std::uint64_t const u64 = 0x0102030405060708ULL;
  auto const r8 = As<std::uint8_t> (&u8, sizeof (u8));
  auto const r64 = As<std::uint64_t> (&u64, sizeof (u64));
  ASSERT_TRUE (r8.has_value ());
  ASSERT_TRUE (r64.has_value ());
  EXPECT_EQ (*r8, u8);
  EXPECT_EQ (*r64, u64);
}

TEST (LumexMemReadTest, GivenFloatAndDouble_WhenAs_ThenPreservesIeeeBits)
{
  float const f = 3.14159f;
  double const d = -2.5;
  auto const rf = As<float> (&f, sizeof (f));
  auto const rd = As<double> (&d, sizeof (d));
  ASSERT_TRUE (rf.has_value ());
  ASSERT_TRUE (rd.has_value ());
  EXPECT_FLOAT_EQ (*rf, f);
  EXPECT_DOUBLE_EQ (*rd, d);
}

TEST (LumexMemReadTest, GivenBoolTrueAndFalse_WhenAs_ThenDecodesBool)
{
  bool const yes = true;
  bool const no = false;
  auto const ry = As<bool> (&yes, sizeof (yes));
  auto const rn = As<bool> (&no, sizeof (no));
  ASSERT_TRUE (ry.has_value ());
  ASSERT_TRUE (rn.has_value ());
  EXPECT_EQ (*ry, true);
  EXPECT_EQ (*rn, false);
}

TEST (LumexMemReadTest, GivenEmptyStandardLayoutType_WhenAs_ThenSucceeds)
{
  EmptyLayout empty{};
  auto const result = As<EmptyLayout> (&empty, sizeof (empty));
  EXPECT_TRUE (result.has_value ());
}

TEST (LumexMemReadTest,
      GivenNegativeGetDataSize_WhenAsFromSource_ThenReturnsNullopt)
{
  NegativeSizeSource source;
  EXPECT_FALSE (As<std::uint32_t> (source).has_value ());
}

TEST (LumexMemReadTest, GivenNullGetData_WhenAsFromSource_ThenReturnsNullopt)
{
  NullDataSource source;
  EXPECT_FALSE (As<std::uint32_t> (source).has_value ());
}

TEST (LumexMemReadTest, GivenEmptyDataSource_WhenAs_ThenReturnsNullopt)
{
  FakeDataSource source;
  EXPECT_FALSE (As<std::uint32_t> (source).has_value ());
}

TEST (LumexMemReadTest, GivenUnsignedCharSpan_WhenAs_ThenDecodesValue)
{
  std::uint16_t const value = 0xBEEF;
  std::array<unsigned char, sizeof (value)> buffer{};
  std::memcpy (buffer.data (), &value, sizeof (value));
  std::span<unsigned char const> const span (buffer);
  auto const result = As<std::uint16_t> (span);
  ASSERT_TRUE (result.has_value ());
  EXPECT_EQ (*result, value);
}

TEST (LumexMemReadTest, GivenCharSpanExactSize_WhenAs_ThenDecodesValue)
{
  std::uint16_t const value = 0x3344;
  std::array<char, sizeof (value)> buffer{};
  std::memcpy (buffer.data (), &value, sizeof (value));
  std::span<char const> const span (buffer);
  auto const result = As<std::uint16_t> (span);
  ASSERT_TRUE (result.has_value ());
  EXPECT_EQ (*result, value);
}

TEST (LumexMemReadTest, GivenEmptyByteSpan_WhenAs_ThenReturnsNullopt)
{
  std::span<std::byte const> const span;
  EXPECT_FALSE (As<std::uint32_t> (span).has_value ());
}

TEST (LumexMemReadTest,
      GivenOffsetThreeUnaligned_WhenAs_ThenStillDecodesCorrectly)
{
  std::array<unsigned char, sizeof (std::uint32_t) + 3> buffer{};
  std::uint32_t const value = 0xA1B2C3D4u;
  std::memcpy (buffer.data () + 3, &value, sizeof (value));
  auto const result = As<std::uint32_t> (buffer.data () + 3, sizeof (value));
  ASSERT_TRUE (result.has_value ());
  EXPECT_EQ (*result, value);
}

TEST (LumexMemReadTest, GivenSignedInteger_WhenAs_ThenPreservesTwoComplement)
{
  std::int32_t const value = -123456;
  auto const result = As<std::int32_t> (&value, sizeof (value));
  ASSERT_TRUE (result.has_value ());
  EXPECT_EQ (*result, value);
}
