// The integer fields the DSL core needs (lumex/core/protocol/field): size,
// byte order, scale, and the field and modifier traits.

#include <cstdint>
#include <ratio>
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/core/protocol/LumexProtocol"

namespace
{
namespace field = lumex::core::protocol::field;

struct not_a_field
{
};
} // namespace

TEST (LumexProtocolIntegerFieldsTest,
      GivenTheIntegerNames_WhenInspected_ThenSizeOrderAndRawTypeMatch)
{
  EXPECT_EQ (field::u8_<>::size (), 1u);
  EXPECT_EQ (field::u16_le_<>::size (), 2u);
  EXPECT_EQ (field::u16_be_<>::size (), 2u);
  EXPECT_TRUE (field::u16_le_<>::order () == field::byte_order::little);
  EXPECT_TRUE (field::u16_be_<>::order () == field::byte_order::big);
  EXPECT_TRUE ((std::is_same<field::u8_<>::raw_type, std::uint8_t>::value));
  EXPECT_TRUE (
      (std::is_same<field::u16_be_<>::raw_type, std::uint16_t>::value));
}

TEST (LumexProtocolIntegerFieldsTest,
      GivenNoScale_WhenTheScaleIsRead_ThenItIsOne)
{
  EXPECT_TRUE (
      (std::is_same<field::u16_le_<>::scale_type, std::ratio<1>>::value));
}

TEST (LumexProtocolIntegerFieldsTest,
      GivenAScaleWithACommaInItsRatio_WhenRead_ThenItIsTheReducedRatio)
{
  typedef field::u16_le_<field::scale<std::ratio<1, 100>>> hundredths;
  typedef field::u16_be_<field::scale<std::ratio<2, 200>>> also_hundredths;
  EXPECT_TRUE (
      (std::is_same<hundredths::scale_type, std::ratio<1, 100>>::value));
  EXPECT_TRUE (
      (std::is_same<also_hundredths::scale_type, std::ratio<1, 100>>::value));
}

TEST (LumexProtocolIntegerFieldsTest,
      GivenTypes_WhenAskedIfFields_ThenOnlyFieldTypesAre)
{
  EXPECT_TRUE (field::is_field<field::u8_<>>::value);
  EXPECT_TRUE ((field::is_field<
                field::u16_le_<field::scale<std::ratio<1, 10>>>>::value));
  EXPECT_FALSE (field::is_field<int>::value);
  EXPECT_FALSE (field::is_field<not_a_field>::value);
  EXPECT_FALSE ((field::is_field<field::scale<std::ratio<1, 10>>>::value));
}

TEST (LumexProtocolIntegerFieldsTest,
      GivenTypes_WhenAskedIfModifiers_ThenOnlyScaleIs)
{
  EXPECT_TRUE ((field::is_modifier<field::scale<std::ratio<1, 10>>>::value));
  EXPECT_FALSE (field::is_modifier<field::u8_<>>::value);
  EXPECT_FALSE (field::is_modifier<int>::value);
}
