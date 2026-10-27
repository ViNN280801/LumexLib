// The layouts with the C++20 standard types (lumex/core/protocol/layout): a
// std::span of bytes is a message to decode, a decoded token is a
// std::string_view of the message, and the descriptors of the fields are
// constants in a consteval context.

#include <cstdint>
#include <ratio>
#include <string_view>
#include <vector>
#if defined(__has_include)
#if __has_include(<span>)
#include <span>
#endif
#endif

#include <gtest/gtest.h>

#include "lumex/core/protocol/LumexProtocol"
#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"
#include "lumex/tests/core/protocol/LumexProtocolFieldTables.hpp"

namespace
{
namespace field = lumex::core::protocol::field;
namespace layout = lumex::core::protocol::layout;
namespace tables = lumex_tests::field_tables;

LUMEX_PROTOCOL_LAYOUT (named_t, (u8_, token_), ("id", "name"));

#if LUMEX_HAS_CONSTEVAL
consteval std::size_t
sink_fields ()
{
  return field::impl_of<tables::sink_t>::type::info ().count;
}
#endif
} // namespace

TEST (LumexProtocolLayoutStdTest,
      GivenAStdSpanOfBytes_WhenDecoded_ThenTheLayoutReadsIt)
{
#if LUMEX_HAS_STD_SPAN
  std::vector<std::uint8_t> const bytes{
    0x04, 0xE2, 3, 0xFE, 0xFF, 0xFF, 0xFF
  };
  std::span<std::uint8_t const> const message (bytes);
  auto const request
      = layout::decode_request<tables::gauge, tables::gauge::SET_LEVEL_t> (
          message);
  ASSERT_TRUE (request.has_value ());
  EXPECT_DOUBLE_EQ (field::get<0> (*request).value (), 12.5);
  EXPECT_EQ (field::get<2> (*request), -2);
#else
  GTEST_SKIP () << "the standard library has no std::span";
#endif
}

TEST (LumexProtocolLayoutStdTest,
      GivenADecodedToken_WhenMadeAStdStringView_ThenItViewsTheMessage)
{
#if LUMEX_HAS_STD_SPAN
  std::vector<std::uint8_t> const bytes{ 7, 'p', 'u', 'm', 'p' };
  auto const value = layout::decode<layout::binary_layout<>, named_t> (
      std::span<std::uint8_t const> (bytes));
  ASSERT_TRUE (value.has_value ());
  field::text_view const name = field::get<1> (*value);
  std::string_view const view (name.data (), name.size ());
  EXPECT_EQ (view, "pump");
  EXPECT_EQ (static_cast<void const *> (view.data ()),
             static_cast<void const *> (bytes.data () + 1));
#else
  GTEST_SKIP () << "the standard library has no std::span";
#endif
}

TEST (
    LumexProtocolLayoutStdTest,
    GivenTheDescriptorOfALayout_WhenReadInAConstevalFunction_ThenItIsAConstant)
{
#if LUMEX_HAS_CONSTEVAL
  static_assert (sink_fields () == 40, "the sink layout has 40 fields");
  constexpr field::field_info_t info
      = field::u24_be_<field::scale<std::ratio<1, 8>>>::info ();
  static_assert (info.binary_size == 3 && info.scale_den == 8,
                 "a descriptor is a constant");
  SUCCEED ();
#else
  GTEST_SKIP () << "the compiler has no consteval";
#endif
}
