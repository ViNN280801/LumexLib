// The binary layout
// (lumex/core/protocol/layout/LumexProtocolBinaryLayout.hpp): fields one after
// another, every shorter message truncated, a longer one trailing, the buffer
// kept on a failed encode, and the alignment policy natural_alignment<Offset>
// (2-byte fields at even offsets, 4- and 8-byte fields at multiples of 4, up
// to the first field of variable size).

#include <cstdint>
#include <system_error>

#include <gtest/gtest.h>

#include "lumex/core/protocol/LumexProtocol"
#include "lumex/tests/core/protocol/LumexProtocolFieldTables.hpp"

namespace
{
namespace dsl = lumex::core::protocol::dsl;
namespace field = lumex::core::protocol::field;
namespace layout = lumex::core::protocol::layout;
namespace tables = lumex_tests::field_tables;
using lumex::core::protocol::error::protocol_errc;
using tables::view_of;

using binary = layout::binary_layout<>;

template <std::size_t Offset> struct aligned_at
{
  template <typename F>
  struct check : layout::natural_alignment<Offset>::template check<
                     typename field::impl_of<F>::type>
  {
  };
};

LUMEX_PROTOCOL_LAYOUT (words_t, (u16_le_, u16_le_, u32_le_), ("a", "b", "c"));
LUMEX_PROTOCOL_LAYOUT (odd_t, (u8_, u16_le_), ("a", "b"));
LUMEX_PROTOCOL_LAYOUT (odd_array_t, (u8_, array_<u16_le_, 2>), ("a", "b"));
LUMEX_PROTOCOL_LAYOUT (nested_odd_t, (u16_le_, tables::point_t, u32_le_),
                       ("a", "point", "c"));
LUMEX_PROTOCOL_LAYOUT (after_variable_t, (u8_, counted_<u8_, u8_, 4>, u32_le_),
                       ("a", "b", "c"));
LUMEX_PROTOCOL_LAYOUT (bytes_and_24_t, (u8_, u24_le_, u8_, text_<3>),
                       ("a", "b", "c", "d"));
LUMEX_PROTOCOL_LAYOUT (wide_t, (u32_le_, u64_be_, f64_le_, u16_be_, u16_be_),
                       ("a", "b", "c", "d", "e"));
LUMEX_PROTOCOL_LAYOUT (three_t, (u8_, u16_be_, u32_be_), ("a", "b", "c"));
// Aligned after a 2-byte header: offsets 2, 4, 8.
LUMEX_PROTOCOL_LAYOUT (after_header_t, (u16_le_, u32_le_, u16_le_),
                       ("a", "b", "c"));

struct aligned_header
{
  using layout_type = layout::binary_layout<layout::natural_alignment<2>>;
};

struct codec
{
};

// clang-format off
#define ALIGNED_ROWS(X, X_AS) \
  X (GET, 0x10, "Get", (u16_le_, u32_le_), ("a", "b"), (after_header_t), ()) \
  X (SET, 0x11, "Set", (u16_be_), ("a"), (), ())
#define MISALIGNED_ROWS(X, X_AS) \
  X (BAD, 0x12, "Bad", (u8_), ("a"), (odd_t), ())
// clang-format on
LUMEX_PROTOCOL_DEFINE (aligned_protocol, codec, aligned_header, ALIGNED_ROWS);
LUMEX_PROTOCOL_EXTEND (misaligned_protocol, aligned_protocol, MISALIGNED_ROWS);
} // namespace

TEST (LumexProtocolBinaryLayoutTest,
      GivenFieldsAtTheirOffsets_WhenChecked_ThenTheNaturalAlignmentDecides)
{
  EXPECT_TRUE ((aligned_at<0>::check<words_t>::value));
  EXPECT_FALSE ((aligned_at<1>::check<words_t>::value));
  EXPECT_FALSE ((aligned_at<2>::check<words_t>::value));
  EXPECT_TRUE ((aligned_at<4>::check<words_t>::value));
  EXPECT_FALSE ((aligned_at<0>::check<odd_t>::value));
  EXPECT_TRUE ((aligned_at<1>::check<odd_t>::value));
  EXPECT_FALSE ((aligned_at<0>::check<odd_array_t>::value));
  EXPECT_TRUE ((aligned_at<1>::check<odd_array_t>::value));
  // point_t is i16 + u8 at offset 2: the u32 after it lands on 5.
  EXPECT_FALSE ((aligned_at<0>::check<nested_odd_t>::value));
  // Fields after one of variable size are not checked.
  EXPECT_TRUE ((aligned_at<0>::check<after_variable_t>::value));
  // Bytes, 24-bit integers and texts need no alignment.
  EXPECT_TRUE ((aligned_at<0>::check<bytes_and_24_t>::value));
  EXPECT_TRUE ((aligned_at<1>::check<bytes_and_24_t>::value));
  // 8-byte fields need a multiple of 4.
  EXPECT_TRUE ((aligned_at<0>::check<wide_t>::value));
  EXPECT_TRUE ((aligned_at<4>::check<wide_t>::value));
  EXPECT_FALSE ((aligned_at<2>::check<wide_t>::value));
  EXPECT_FALSE ((aligned_at<0>::check<three_t>::value));
  EXPECT_TRUE ((aligned_at<1>::check<three_t>::value));
  EXPECT_TRUE (
      (layout::unaligned::check<field::impl_of<three_t>::type>::value));
}

TEST (
    LumexProtocolBinaryLayoutTest,
    GivenAProtocolWithAnAlignedLayout_WhenItsRowsAreChecked_ThenALayerCanBreakIt)
{
  EXPECT_TRUE (layout::is_aligned<aligned_protocol>::value);
  EXPECT_FALSE (layout::is_aligned<misaligned_protocol>::value);
  EXPECT_TRUE (layout::is_aligned<tables::gauge>::value);
  layout::bytes_t out;
  ASSERT_TRUE (
      (layout::encode_request<aligned_protocol, aligned_protocol::GET_t> (out,
                                                                          1, 2)
           .has_value ()));
  EXPECT_EQ (out, (layout::bytes_t{ 1, 0, 2, 0, 0, 0 }));
}

TEST (LumexProtocolBinaryLayoutTest,
      GivenEveryShorterMessage_WhenDecoded_ThenTruncatedAndNeverPastTheEnd)
{
  layout::bytes_t out;
  ASSERT_TRUE (
      (layout::encode_request<tables::gauge, tables::gauge::SET_LEVEL_t> (
           out, 12.5, 3, -2)
           .has_value ()));
  EXPECT_EQ (out, (layout::bytes_t{ 0x04, 0xE2, 3, 0xFE, 0xFF, 0xFF, 0xFF }));
  for (std::size_t size = 0; size < out.size (); ++size)
    {
      // A copy of exactly `size` bytes: a read past it is an error ASan
      // reports.
      layout::bytes_t const part (
          out.begin (), out.begin () + static_cast<std::ptrdiff_t> (size));
      layout::decode_result_t<
          layout::request_of<tables::gauge::SET_LEVEL_t>::type> const result
          = layout::decode_request<tables::gauge, tables::gauge::SET_LEVEL_t> (
              view_of (part));
      ASSERT_FALSE (result.has_value ()) << size;
      EXPECT_TRUE (result.error () == protocol_errc::truncated) << size;
    }
  out.push_back (0);
  EXPECT_TRUE (
      (layout::decode_request<tables::gauge, tables::gauge::SET_LEVEL_t> (
           view_of (out)))
          .error ()
      == protocol_errc::trailing_bytes);
}

TEST (LumexProtocolBinaryLayoutTest,
      GivenAFailingField_WhenEncoded_ThenTheBufferKeepsItsBytes)
{
  layout::bytes_t out{ 0xAB, 0xCD };
  layout::encode_result_t const result
      = layout::encode_request<tables::gauge, tables::gauge::SET_LEVEL_t> (
          out, 12.5, 300, -2);
  ASSERT_FALSE (result.has_value ());
  EXPECT_TRUE (result.error () == protocol_errc::invalid_value);
  EXPECT_EQ (out, (layout::bytes_t{ 0xAB, 0xCD }));
  layout::request_of<tables::gauge::SET_LEVEL_t>::type record
      = *layout::make_request<tables::gauge::SET_LEVEL_t> (1.0, 1, 1);
  field::get<0> (record).raw = 7;
  ASSERT_TRUE (
      (layout::encode_request_record<tables::gauge,
                                     tables::gauge::SET_LEVEL_t> (out, record)
           .has_value ()));
  EXPECT_EQ (out.size (), 9u);
  EXPECT_EQ (out[2], 0);
  EXPECT_EQ (out[3], 7);
}
