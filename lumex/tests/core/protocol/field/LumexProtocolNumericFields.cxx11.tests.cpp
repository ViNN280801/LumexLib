// The numeric fields
// (lumex/core/protocol/field/LumexProtocolIntegerFields.hpp): every integer
// width and byte order with its boundary values, the 24-bit fields, the range
// checks of encode and of the arguments, scaled fields (rounding, raw value
// kept), floating fields and sign-magnitude fields, in the binary layout and
// the fixed-width text layouts.

#include <cmath>
#include <cstdint>
#include <limits>
#include <ratio>
#include <string>
#include <system_error>
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/core/protocol/LumexProtocol"
#include "lumex/tests/core/protocol/LumexProtocolFieldTables.hpp"

namespace
{
namespace field = lumex::core::protocol::field;
namespace layout = lumex::core::protocol::layout;
using lumex::core::protocol::error::protocol_errc;
using lumex_tests::field_tables::view_of;

using binary = layout::binary_layout<>;
using hex = layout::fixed_width_text_layout<16>;
using decimal = layout::fixed_width_text_layout<10>;

template <typename Layout, typename F>
layout::bytes_t
encoded (typename field::value_of<F>::type const &value)
{
  layout::bytes_t out;
  layout::encode_result_t const result
      = layout::encode<Layout, F> (out, value);
  EXPECT_TRUE (result.has_value ());
  return out;
}

template <typename Layout, typename F>
typename field::value_of<F>::type
decoded (layout::bytes_t const &bytes)
{
  layout::decode_result_t<typename field::value_of<F>::type> const result
      = layout::decode<Layout, F> (view_of (bytes));
  EXPECT_TRUE (result.has_value ());
  return result.has_value () ? *result : typename field::value_of<F>::type ();
}

template <typename Layout, typename F>
std::error_code
decode_error (layout::bytes_t const &bytes)
{
  layout::decode_result_t<typename field::value_of<F>::type> const result
      = layout::decode<Layout, F> (view_of (bytes));
  return result.has_value () ? std::error_code () : result.error ();
}

// The bytes of `bits` in `count` bytes, little or big endian.
layout::bytes_t
bytes_of_bits (std::uint64_t bits, std::size_t count, bool big)
{
  layout::bytes_t out (count);
  for (std::size_t i = 0; i < count; ++i)
    out[big ? count - 1 - i : i]
        = static_cast<std::uint8_t> ((bits >> (8 * i)) & 0xFF);
  return out;
}

template <typename F>
void
expect_round_trip (typename F::raw_type value)
{
  using raw = typename F::raw_type;
  std::size_t const size = F::size ();
  std::uint64_t const bits
      = static_cast<std::uint64_t> (static_cast<std::int64_t> (value))
        & (size >= 8 ? ~static_cast<std::uint64_t> (0)
                     : ((static_cast<std::uint64_t> (1) << (8 * size)) - 1));
  layout::bytes_t const bytes = encoded<binary, F> (value);
  EXPECT_EQ (bytes,
             bytes_of_bits (bits, size, F::order () == field::byte_order::big))
      << "value " << static_cast<std::int64_t> (value);
  EXPECT_EQ ((decoded<binary, F> (bytes)), value);
  EXPECT_EQ ((decoded<hex, F> (encoded<hex, F> (value))), value);
  EXPECT_EQ ((decoded<decimal, F> (encoded<decimal, F> (value))), value);
  (void)sizeof (raw);
}

template <typename F, typename Low, typename High>
void
expect_boundaries (Low low, High high)
{
  expect_round_trip<F> (static_cast<typename F::raw_type> (low));
  expect_round_trip<F> (static_cast<typename F::raw_type> (high));
  expect_round_trip<F> (static_cast<typename F::raw_type> (0));
}
} // namespace

TEST (LumexProtocolNumericFieldsTest,
      GivenEveryIntegerName_WhenInspected_ThenWidthOrderAndSignednessMatch)
{
  EXPECT_EQ (field::i8_<>::size (), 1u);
  EXPECT_EQ (field::u24_le_<>::size (), 3u);
  EXPECT_EQ (field::i24_be_<>::size (), 3u);
  EXPECT_EQ (field::u32_be_<>::size (), 4u);
  EXPECT_EQ (field::i64_le_<>::size (), 8u);
  EXPECT_TRUE (field::i24_be_<>::order () == field::byte_order::big);
  EXPECT_TRUE (field::u24_le_<>::order () == field::byte_order::little);
  EXPECT_TRUE (
      (std::is_same<field::i24_le_<>::raw_type, std::int32_t>::value));
  EXPECT_TRUE (
      (std::is_same<field::u64_be_<>::raw_type, std::uint64_t>::value));
  EXPECT_TRUE ((std::is_same<field::f32_be_<>::value_type, float>::value));
  EXPECT_TRUE ((std::is_same<field::f64_le_<>::value_type, double>::value));
  field::field_info_t const info
      = field::i32_be_<field::scale<std::ratio<1, 1000>>,
                       field::unit::millivolt>::info ();
  EXPECT_TRUE (info.kind == field::field_kind::integer);
  EXPECT_EQ (info.binary_size, 4u);
  EXPECT_TRUE (info.order == field::byte_order::big);
  EXPECT_TRUE (info.is_signed);
  EXPECT_EQ (info.scale_num, 1);
  EXPECT_EQ (info.scale_den, 1000);
  EXPECT_EQ (std::string (info.unit), "mV");
  EXPECT_FALSE (field::u8_<>::info ().is_signed);
  EXPECT_EQ (std::string (field::u8_<>::info ().unit), "");
}

TEST (
    LumexProtocolNumericFieldsTest,
    GivenTheBoundaryValuesOfEveryWidth_WhenRoundTripped_ThenBytesAndValuesMatch)
{
  expect_boundaries<field::u8_<>> (0, 255);
  expect_boundaries<field::i8_<>> (-128, 127);
  expect_boundaries<field::u16_le_<>> (0, 65535);
  expect_boundaries<field::u16_be_<>> (1, 65534);
  expect_boundaries<field::i16_le_<>> (-32768, 32767);
  expect_boundaries<field::i16_be_<>> (-32768, 32767);
  expect_boundaries<field::u24_le_<>> (0, 0xFFFFFF);
  expect_boundaries<field::u24_be_<>> (1, 0xFFFFFE);
  expect_boundaries<field::i24_le_<>> (-8388608, 8388607);
  expect_boundaries<field::i24_be_<>> (-8388608, 8388607);
  expect_boundaries<field::u32_le_<>> (0u, 0xFFFFFFFFu);
  expect_boundaries<field::u32_be_<>> (1u, 0xFFFFFFFEu);
  expect_boundaries<field::i32_le_<>> (
      std::numeric_limits<std::int32_t>::min (),
      std::numeric_limits<std::int32_t>::max ());
  expect_boundaries<field::i32_be_<>> (
      std::numeric_limits<std::int32_t>::min (),
      std::numeric_limits<std::int32_t>::max ());
  expect_boundaries<field::u64_le_<>> (
      0u, std::numeric_limits<std::uint64_t>::max ());
  expect_boundaries<field::u64_be_<>> (
      1u, std::numeric_limits<std::uint64_t>::max () - 1u);
  expect_boundaries<field::i64_le_<>> (
      std::numeric_limits<std::int64_t>::min (),
      std::numeric_limits<std::int64_t>::max ());
  expect_boundaries<field::i64_be_<>> (
      std::numeric_limits<std::int64_t>::min (),
      std::numeric_limits<std::int64_t>::max ());
}

TEST (LumexProtocolNumericFieldsTest,
      GivenA24BitField_WhenEncoded_ThenThreeBytesInItsOrderAndTheSignExtended)
{
  EXPECT_EQ ((encoded<binary, field::u24_le_<>> (0x123456u)),
             (layout::bytes_t{ 0x56, 0x34, 0x12 }));
  EXPECT_EQ ((encoded<binary, field::u24_be_<>> (0x123456u)),
             (layout::bytes_t{ 0x12, 0x34, 0x56 }));
  EXPECT_EQ ((encoded<binary, field::i24_be_<>> (-2)),
             (layout::bytes_t{ 0xFF, 0xFF, 0xFE }));
  EXPECT_EQ ((decoded<binary, field::i24_le_<>> (
                 layout::bytes_t{ 0x00, 0x00, 0x80 })),
             -8388608);
  EXPECT_EQ ((decoded<binary, field::u24_le_<>> (
                 layout::bytes_t{ 0xFF, 0xFF, 0xFF })),
             0xFFFFFFu);
}

TEST (LumexProtocolNumericFieldsTest,
      GivenARawValueWiderThanTheWire_WhenEncoded_ThenInvalidValueAndNoBytes)
{
  layout::bytes_t out (2, 0xAA);
  layout::encode_result_t const wide
      = layout::encode<binary, field::u24_le_<>> (out, 0x1000000u);
  ASSERT_FALSE (wide.has_value ());
  EXPECT_TRUE (wide.error () == protocol_errc::invalid_value);
  EXPECT_EQ (out, (layout::bytes_t{ 0xAA, 0xAA }));
  layout::encode_result_t const low
      = layout::encode<binary, field::i24_be_<>> (out, -8388609);
  ASSERT_FALSE (low.has_value ());
  EXPECT_TRUE (low.error () == protocol_errc::invalid_value);
  EXPECT_EQ (out.size (), 2u);
}

TEST (LumexProtocolNumericFieldsTest,
      GivenIntegerArguments_WhenAssigned_ThenOnlyValuesOfTheWidthAreAccepted)
{
  using u8 = field::impl_of<field::u8_<>>::type;
  using i8 = field::impl_of<field::i8_<>>::type;
  using u24 = field::impl_of<field::u24_le_<>>::type;
  using i24 = field::impl_of<field::i24_be_<>>::type;
  using i64 = field::impl_of<field::i64_le_<>>::type;
  std::uint8_t a = 0;
  EXPECT_TRUE (u8::assign (a, 255));
  EXPECT_EQ (a, 255u);
  EXPECT_FALSE (u8::assign (a, 256));
  EXPECT_FALSE (u8::assign (a, -1));
  std::int8_t b = 0;
  EXPECT_TRUE (i8::assign (b, -128));
  EXPECT_FALSE (i8::assign (b, 128));
  EXPECT_FALSE (i8::assign (b, 200u));
  std::uint32_t c = 0;
  EXPECT_TRUE (u24::assign (c, 0xFFFFFFu));
  EXPECT_FALSE (u24::assign (c, 0x1000000u));
  std::int32_t d = 0;
  EXPECT_TRUE (i24::assign (d, -8388608));
  EXPECT_FALSE (i24::assign (d, -8388609));
  EXPECT_FALSE (i24::assign (d, 8388608));
  std::int64_t e = 0;
  EXPECT_TRUE (i64::assign (e, std::numeric_limits<std::int64_t>::min ()));
  EXPECT_FALSE (i64::assign (e, std::numeric_limits<std::uint64_t>::max ()));
}

TEST (
    LumexProtocolNumericFieldsTest,
    GivenAScaledField_WhenAssignedAnEngineeringValue_ThenTheRawIsRoundedAndKept)
{
  using hundredths = field::u16_be_<field::scale<std::ratio<1, 100>>>;
  using impl = field::impl_of<hundredths>::type;
  impl::value_type value = impl::value_type ();
  EXPECT_TRUE (impl::assign (value, 12.345));
  EXPECT_EQ (value.raw, 1235u);
  EXPECT_DOUBLE_EQ (value.value (), 12.35);
  EXPECT_TRUE (impl::assign (value, 655.35));
  EXPECT_EQ (value.raw, 65535u);
  EXPECT_FALSE (impl::assign (value, 655.355));
  EXPECT_FALSE (impl::assign (value, -0.01));
  EXPECT_FALSE (impl::assign (value, std::nan ("")));
  EXPECT_FALSE (impl::assign (value, HUGE_VAL));
  EXPECT_TRUE (impl::assign (value, 3));
  EXPECT_EQ (value.raw, 300u);

  using tenths
      = field::impl_of<field::i16_le_<field::scale<std::ratio<1, 10>>>>::type;
  tenths::value_type signed_value = tenths::value_type ();
  EXPECT_TRUE (tenths::assign (signed_value, -0.05));
  EXPECT_EQ (signed_value.raw, -1);
  EXPECT_TRUE (tenths::assign (signed_value, -3276.8));
  EXPECT_EQ (signed_value.raw, -32768);
  EXPECT_FALSE (tenths::assign (signed_value, -3276.9));

  layout::bytes_t const bytes = encoded<binary, hundredths> (value);
  EXPECT_EQ (bytes, (layout::bytes_t{ 0x01, 0x2C }));
  EXPECT_EQ ((decoded<binary, hundredths> (bytes)).raw, 300u);
}

TEST (LumexProtocolNumericFieldsTest,
      GivenFloatingFields_WhenRoundTripped_ThenTheIeeeBitsAreKept)
{
  EXPECT_EQ ((encoded<binary, field::f32_le_<>> (1.5f)),
             (layout::bytes_t{ 0x00, 0x00, 0xC0, 0x3F }));
  EXPECT_EQ ((encoded<binary, field::f64_be_<>> (1.5)),
             (layout::bytes_t{ 0x3F, 0xF8, 0, 0, 0, 0, 0, 0 }));
  EXPECT_EQ ((decoded<binary, field::f32_be_<>> (
                 layout::bytes_t{ 0xC0, 0x20, 0x00, 0x00 })),
             -2.5f);
  EXPECT_EQ ((decoded<hex, field::f64_le_<>> (
                 encoded<hex, field::f64_le_<>> (-1e300))),
             -1e300);
  EXPECT_EQ (lumex_tests::field_tables::text_of (
                 encoded<hex, field::f32_le_<>> (1.5f)),
             "3FC00000");
  float const nan = std::numeric_limits<float>::quiet_NaN ();
  float const back = decoded<binary, field::f32_le_<>> (
      encoded<binary, field::f32_le_<>> (nan));
  EXPECT_TRUE (back != back);
  EXPECT_TRUE (field::f32_le_<field::unit::nanometer>::info ().kind
               == field::field_kind::floating);
  EXPECT_EQ (
      std::string (field::f32_le_<field::unit::nanometer>::info ().unit),
      "nm");
}

TEST (LumexProtocolNumericFieldsTest,
      GivenASignMagnitudeField_WhenEncodedInBinary_ThenTheTopBitIsTheSign)
{
  using sm = field::sign_magnitude_<field::i32_be_<>>;
  EXPECT_EQ ((encoded<binary, sm> (-5)),
             (layout::bytes_t{ 0x80, 0x00, 0x00, 0x05 }));
  EXPECT_EQ ((encoded<binary, sm> (5)),
             (layout::bytes_t{ 0x00, 0x00, 0x00, 0x05 }));
  EXPECT_EQ ((decoded<binary, sm> (layout::bytes_t{ 0x80, 0, 0, 0 })), 0);
  EXPECT_EQ ((decoded<binary, sm> (layout::bytes_t{ 0xFF, 0xFF, 0xFF, 0xFF })),
             -2147483647);
  EXPECT_EQ ((decoded<binary, sm> (layout::bytes_t{ 0x7F, 0xFF, 0xFF, 0xFF })),
             2147483647);
  using impl = field::impl_of<sm>::type;
  std::int32_t value = 0;
  EXPECT_FALSE (
      impl::assign (value, std::numeric_limits<std::int32_t>::min ()));
  EXPECT_TRUE (impl::assign (value, -2147483647));
  layout::bytes_t out;
  layout::encode_result_t const result = layout::encode<binary, sm> (
      out, std::numeric_limits<std::int32_t>::min ());
  ASSERT_FALSE (result.has_value ());
  EXPECT_TRUE (result.error () == protocol_errc::invalid_value);
  EXPECT_TRUE (out.empty ());
  EXPECT_TRUE (impl::info ().kind == field::field_kind::sign_magnitude);
}

TEST (
    LumexProtocolNumericFieldsTest,
    GivenASignMagnitudeField_WhenEncodedAsText_ThenHexKeepsTheBitsAndDecimalTheNumber)
{
  using sm = field::sign_magnitude_<
      field::i16_le_<field::scale<std::ratio<1, 100>>>>;
  using impl = field::impl_of<sm>::type;
  impl::value_type value = impl::value_type ();
  ASSERT_TRUE (impl::assign (value, -0.05));
  EXPECT_EQ (value.raw, -5);
  EXPECT_EQ (lumex_tests::field_tables::text_of (encoded<hex, sm> (value)),
             "8005");
  EXPECT_EQ (lumex_tests::field_tables::text_of (encoded<decimal, sm> (value)),
             "-00005");
  EXPECT_EQ (
      (decoded<hex, sm> (lumex_tests::field_tables::bytes_of ("8005"))).raw,
      -5);
  EXPECT_EQ (
      (decoded<decimal, sm> (lumex_tests::field_tables::bytes_of ("-00005")))
          .raw,
      -5);
  // The two's complement minimum is no sign-magnitude value.
  EXPECT_TRUE ((decode_error<decimal, sm> (
                   lumex_tests::field_tables::bytes_of ("-32768")))
               == protocol_errc::invalid_value);
}

TEST (LumexProtocolNumericFieldsTest,
      GivenEveryShorterInput_WhenDecoded_ThenTruncated)
{
  layout::bytes_t const full{ 1, 2, 3, 4 };
  for (std::size_t size = 0; size < full.size (); ++size)
    {
      layout::bytes_t const part (full.begin (),
                                  full.begin () + static_cast<long> (size));
      EXPECT_TRUE ((decode_error<binary, field::u32_le_<>> (part))
                   == protocol_errc::truncated)
          << size;
      EXPECT_TRUE ((decode_error<binary, field::f32_le_<>> (part))
                   == protocol_errc::truncated)
          << size;
    }
  EXPECT_TRUE ((decode_error<binary, field::u16_le_<>> (full))
               == protocol_errc::trailing_bytes);
}

TEST (LumexProtocolNumericFieldsTest,
      GivenTextDigits_WhenTheyAreNoNumberOrOutOfRange_ThenInvalidValue)
{
  using lumex_tests::field_tables::bytes_of;
  EXPECT_TRUE ((decode_error<hex, field::u16_be_<>> (bytes_of ("12G4")))
               == protocol_errc::invalid_value);
  EXPECT_TRUE ((decode_error<decimal, field::u8_<>> (bytes_of ("256")))
               == protocol_errc::invalid_value);
  EXPECT_TRUE ((decode_error<decimal, field::u8_<>> (bytes_of ("-01")))
               == protocol_errc::invalid_value);
  EXPECT_TRUE ((decode_error<decimal, field::i8_<>> (bytes_of ("-129")))
               == protocol_errc::invalid_value);
  EXPECT_EQ ((decoded<decimal, field::i8_<>> (bytes_of ("-128"))), -128);
  // One above the signed maximum is no value of the field (mutation M22).
  EXPECT_TRUE ((decode_error<decimal, field::i8_<>> (bytes_of ("+128")))
               == protocol_errc::invalid_value);
  EXPECT_EQ ((decoded<decimal, field::i8_<>> (bytes_of ("+127"))), 127);
  EXPECT_EQ ((decoded<decimal, field::u16_le_<>> (bytes_of ("00042"))), 42u);
  EXPECT_EQ (lumex_tests::field_tables::text_of (
                 encoded<decimal, field::i16_be_<>> (-42)),
             "-00042");
  EXPECT_EQ (
      lumex_tests::field_tables::text_of (encoded<hex, field::i16_be_<>> (-1)),
      "FFFF");
  EXPECT_EQ ((decoded<hex, field::u16_be_<>> (bytes_of ("abcd"))), 0xABCDu);
}
