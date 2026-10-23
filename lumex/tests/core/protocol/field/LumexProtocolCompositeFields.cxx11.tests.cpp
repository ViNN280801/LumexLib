// The composite fields
// (lumex/core/protocol/field/LumexProtocolCompositeFields.hpp): arrays,
// counted and computed counts with their limits, optional tails, layouts
// chosen by length, unions by a leading, an earlier or a trailing discriminant
// and by a session value, nested messages of another protocol.

#include <array>
#include <cstdint>
#include <string>
#include <system_error>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/protocol/LumexProtocol"
#include "lumex/tests/core/protocol/LumexProtocolFieldTables.hpp"

namespace
{
namespace field = lumex::core::protocol::field;
namespace layout = lumex::core::protocol::layout;
namespace tables = lumex_tests::field_tables;
using lumex::core::protocol::error::protocol_errc;
using tables::view_of;

typedef layout::binary_layout<> binary;

template <typename F>
layout::bytes_t
encoded (typename field::value_of<F>::type const &value,
         field::decode_context_t const *context = nullptr)
{
  layout::bytes_t out;
  layout::encode_result_t const result
      = layout::encode<binary, F> (out, value, context);
  EXPECT_TRUE (result.has_value ());
  return out;
}

template <typename F>
layout::decode_result_t<typename field::value_of<F>::type>
decoded (layout::bytes_t const &bytes,
         field::decode_context_t const *context = nullptr)
{
  return layout::decode<binary, F> (view_of (bytes), context);
}

LUMEX_PROTOCOL_LAYOUT (counted_message_t, (u8_, counted_<u16_be_, u8_, 4>),
                       ("tag", "values"));

LUMEX_PROTOCOL_LAYOUT (computed_message_t,
                       (u8_, u8_, computed_count_<u8_, count_from_<1>, 5>),
                       ("tag", "count", "values"));

struct pairs_of_first
{
  template <typename Siblings>
  static std::size_t
  count (Siblings const &siblings)
  {
    return 2u * static_cast<std::size_t> (field::get<0> (siblings));
  }
};

LUMEX_PROTOCOL_LAYOUT (expression_message_t,
                       (u8_, computed_count_<u8_, pairs_of_first, 8>),
                       ("pairs", "values"));

LUMEX_PROTOCOL_LAYOUT (tail_message_t, (u8_, optional_tail_<u16_le_, u8_>),
                       ("base", "tail"));

LUMEX_PROTOCOL_LAYOUT (pair_t, (u8_, u8_), ("a", "b"));
LUMEX_PROTOCOL_LAYOUT (quad_t, (u8_, u8_, u8_, u8_), ("a", "b", "c", "d"));

LUMEX_PROTOCOL_LAYOUT (length_message_t, (u8_, by_length_<4, pair_t, quad_t>),
                       ("tag", "body"));

LUMEX_PROTOCOL_LAYOUT (lead_union_t,
                       (union_by_<u8_, case_<1, pair_t>, case_<2, u16_be_>>),
                       ("body"));

LUMEX_PROTOCOL_LAYOUT (
    field_union_t,
    (u8_, u8_, union_by_<field_<0>, case_<10, pair_t>, case_<20, quad_t>>),
    ("kind", "other", "body"));

LUMEX_PROTOCOL_LAYOUT (
    end_union_t,
    (u8_, union_by_<at_end_<u8_>, case_<3, pair_t>, case_<4, u16_le_>>),
    ("tag", "body"));

LUMEX_PROTOCOL_LAYOUT (
    context_union_t,
    (union_by_context_<tables::gauge_kind, case_<1, u8_>, case_<2, pair_t>>),
    ("body"));

LUMEX_PROTOCOL_LAYOUT (tunnel_t, (u8_, nested_<tables::inner_protocol, char_>),
                       ("channel", "inner"));
} // namespace

TEST (LumexProtocolCompositeFieldsTest,
      GivenAnArray_WhenRoundTripped_ThenEveryElementInOrder)
{
  typedef field::array_<field::u16_be_<>, 3> triple;
  std::array<std::uint16_t, 3> const value = { { 1, 0x0203, 0xFFFF } };
  EXPECT_EQ (encoded<triple> (value),
             (layout::bytes_t{ 0, 1, 2, 3, 0xFF, 0xFF }));
  EXPECT_TRUE (*decoded<triple> (encoded<triple> (value)) == value);
  EXPECT_EQ (triple::binary_size::value, 6u);
  EXPECT_TRUE (decoded<triple> (layout::bytes_t{ 0, 1, 2 }).error ()
               == protocol_errc::truncated);
  typedef field::impl_of<triple>::type impl;
  impl::value_type assigned = impl::value_type ();
  EXPECT_TRUE (impl::assign (assigned, std::vector<int>{ 7, 8, 9 }));
  EXPECT_EQ (assigned[2], 9u);
  EXPECT_FALSE (impl::assign (assigned, std::vector<int>{ 7, 8 }));
  EXPECT_FALSE (impl::assign (assigned, std::vector<int>{ 7, 8, 70000 }));
  EXPECT_EQ (triple::info ().count, 3u);
}

TEST (LumexProtocolCompositeFieldsTest,
      GivenACountedField_WhenRoundTripped_ThenTheCountComesFirst)
{
  typedef field::record_of<counted_message_t::fields_type>::type record_type;
  record_type value = record_type ();
  field::get<0> (value) = 9;
  ASSERT_TRUE (
      (field::impl_of<field::counted_<field::u16_be_<>, field::u8_<>, 4>>::
           type::assign (field::get<1> (value),
                         std::vector<unsigned>{ 0x0102, 0x0304 })));
  layout::bytes_t const bytes = encoded<counted_message_t> (value);
  EXPECT_EQ (bytes, (layout::bytes_t{ 9, 2, 1, 2, 3, 4 }));
  record_type const back = *decoded<counted_message_t> (bytes);
  EXPECT_EQ (field::get<1> (back).size (), 2u);
  EXPECT_EQ (field::get<1> (back)[1], 0x0304u);
  EXPECT_TRUE (back == value);
}

TEST (LumexProtocolCompositeFieldsTest,
      GivenACountAboveTheLimit_WhenDecoded_ThenLimitExceededWithoutReadingOn)
{
  // A count of 200 with a capacity of 4: refused before any element.
  EXPECT_TRUE (
      decoded<counted_message_t> (layout::bytes_t{ 9, 200, 1, 2 }).error ()
      == protocol_errc::limit_exceeded);
  EXPECT_TRUE (decoded<counted_message_t> (layout::bytes_t{ 9, 5 }).error ()
               == protocol_errc::limit_exceeded);
  EXPECT_TRUE (
      decoded<counted_message_t> (layout::bytes_t{ 9, 2, 1, 2, 3 }).error ()
      == protocol_errc::truncated);
  EXPECT_TRUE (decoded<counted_message_t> (layout::bytes_t{ 9, 0, 7 }).error ()
               == protocol_errc::trailing_bytes);
  layout::decode_result_t<
      field::record_of<counted_message_t::fields_type>::type> const empty
      = decoded<counted_message_t> (layout::bytes_t{ 9, 0 });
  ASSERT_TRUE (empty.has_value ());
  EXPECT_EQ (field::get<1> (*empty).size (), 0u);
  typedef field::impl_of<
      field::counted_<field::u16_be_<>, field::u8_<>, 4>>::type impl;
  impl::value_type value = impl::value_type ();
  EXPECT_FALSE (impl::assign (value, std::vector<int>{ 1, 2, 3, 4, 5 }));
}

TEST (LumexProtocolCompositeFieldsTest,
      GivenAComputedCount_WhenRoundTripped_ThenTheEarlierFieldGivesTheNumber)
{
  layout::decode_result_t<
      field::record_of<computed_message_t::fields_type>::type> const value
      = decoded<computed_message_t> (layout::bytes_t{ 1, 3, 7, 8, 9 });
  ASSERT_TRUE (value.has_value ());
  EXPECT_EQ (field::get<2> (*value).size (), 3u);
  EXPECT_EQ (field::get<2> (*value)[2], 9u);
  EXPECT_EQ (encoded<computed_message_t> (*value),
             (layout::bytes_t{ 1, 3, 7, 8, 9 }));
  EXPECT_TRUE (
      decoded<computed_message_t> (layout::bytes_t{ 1, 6, 0, 0, 0, 0, 0, 0 })
          .error ()
      == protocol_errc::limit_exceeded);
  // On encode the number of values must be the computed one.
  field::record_of<computed_message_t::fields_type>::type wrong = *value;
  field::get<1> (wrong) = 2;
  layout::bytes_t out;
  layout::encode_result_t const result
      = layout::encode<binary, computed_message_t> (out, wrong);
  ASSERT_FALSE (result.has_value ());
  EXPECT_TRUE (result.error () == protocol_errc::invalid_value);
  EXPECT_TRUE (out.empty ());
  layout::decode_result_t<
      field::record_of<expression_message_t::fields_type>::type> const pairs
      = decoded<expression_message_t> (layout::bytes_t{ 2, 1, 2, 3, 4 });
  ASSERT_TRUE (pairs.has_value ());
  EXPECT_EQ (field::get<1> (*pairs).size (), 4u);
}

TEST (LumexProtocolCompositeFieldsTest,
      GivenAnOptionalTail_WhenTheMessageEndsBeforeIt_ThenItIsAbsent)
{
  typedef field::record_of<tail_message_t::fields_type>::type record_type;
  record_type const without = *decoded<tail_message_t> (layout::bytes_t{ 4 });
  EXPECT_FALSE (field::get<1> (without).present);
  EXPECT_EQ (encoded<tail_message_t> (without), (layout::bytes_t{ 4 }));
  record_type const with
      = *decoded<tail_message_t> (layout::bytes_t{ 4, 0x34, 0x12, 9 });
  ASSERT_TRUE (field::get<1> (with).present);
  EXPECT_EQ (field::get<0> (field::get<1> (with).fields), 0x1234u);
  EXPECT_EQ (field::get<1> (field::get<1> (with).fields), 9u);
  EXPECT_EQ (encoded<tail_message_t> (with),
             (layout::bytes_t{ 4, 0x34, 0x12, 9 }));
  // A tail that starts and ends early is truncated, not absent.
  EXPECT_TRUE (decoded<tail_message_t> (layout::bytes_t{ 4, 0x34 }).error ()
               == protocol_errc::truncated);
  EXPECT_TRUE (
      decoded<tail_message_t> (layout::bytes_t{ 4, 1, 2, 3, 4 }).error ()
      == protocol_errc::trailing_bytes);
}

TEST (LumexProtocolCompositeFieldsTest,
      GivenALengthChosenLayout_WhenDecoded_ThenTheRemainingLengthChooses)
{
  typedef field::record_of<length_message_t::fields_type>::type record_type;
  record_type const short_form
      = *decoded<length_message_t> (layout::bytes_t{ 1, 5, 6 });
  ASSERT_EQ (field::get<1> (short_form).index (), 0u);
  EXPECT_EQ (field::get<1> (field::get<1> (short_form).get<0> ()), 6u);
  record_type const long_form
      = *decoded<length_message_t> (layout::bytes_t{ 1, 5, 6, 7, 8 });
  ASSERT_EQ (field::get<1> (long_form).index (), 1u);
  EXPECT_EQ (field::get<3> (field::get<1> (long_form).get<1> ()), 8u);
  EXPECT_EQ (encoded<length_message_t> (long_form),
             (layout::bytes_t{ 1, 5, 6, 7, 8 }));
  // Three bytes left choose the short form, which leaves one.
  EXPECT_TRUE (
      decoded<length_message_t> (layout::bytes_t{ 1, 5, 6, 7 }).error ()
      == protocol_errc::trailing_bytes);
  EXPECT_TRUE (decoded<length_message_t> (layout::bytes_t{ 1, 5 }).error ()
               == protocol_errc::truncated);
}

TEST (LumexProtocolCompositeFieldsTest,
      GivenALeadingDiscriminant_WhenDecoded_ThenItChoosesTheCase)
{
  typedef field::record_of<lead_union_t::fields_type>::type record_type;
  record_type const pair = *decoded<lead_union_t> (layout::bytes_t{ 1, 7, 8 });
  EXPECT_EQ (field::get<0> (pair).discriminant, 1);
  ASSERT_EQ (field::get<0> (pair).cases.index (), 0u);
  EXPECT_EQ (field::get<1> (field::get<0> (pair).cases.get<0> ()), 8u);
  record_type const word
      = *decoded<lead_union_t> (layout::bytes_t{ 2, 0x12, 0x34 });
  ASSERT_EQ (field::get<0> (word).cases.index (), 1u);
  EXPECT_EQ (field::get<0> (word).cases.get<1> (), 0x1234u);
  EXPECT_EQ (encoded<lead_union_t> (word), (layout::bytes_t{ 2, 0x12, 0x34 }));
  EXPECT_TRUE (decoded<lead_union_t> (layout::bytes_t{ 3, 0, 0 }).error ()
               == protocol_errc::unknown_case);
  EXPECT_TRUE (decoded<lead_union_t> (layout::bytes_t{}).error ()
               == protocol_errc::truncated);
  // The discriminant written is that of the case held.
  record_type changed = word;
  field::get<0> (changed).discriminant = 1;
  EXPECT_EQ (encoded<lead_union_t> (changed),
             (layout::bytes_t{ 2, 0x12, 0x34 }));
}

TEST (LumexProtocolCompositeFieldsTest,
      GivenAnEarlierFieldAsDiscriminant_WhenEncoded_ThenItMustAgree)
{
  typedef field::record_of<field_union_t::fields_type>::type record_type;
  record_type const quad
      = *decoded<field_union_t> (layout::bytes_t{ 20, 0, 1, 2, 3, 4 });
  ASSERT_EQ (field::get<2> (quad).cases.index (), 1u);
  EXPECT_EQ (field::get<2> (quad).discriminant, 20);
  EXPECT_EQ (encoded<field_union_t> (quad),
             (layout::bytes_t{ 20, 0, 1, 2, 3, 4 }));
  record_type wrong = quad;
  field::get<0> (wrong) = 10;
  layout::bytes_t out;
  layout::encode_result_t const result
      = layout::encode<binary, field_union_t> (out, wrong);
  ASSERT_FALSE (result.has_value ());
  EXPECT_TRUE (result.error () == protocol_errc::invalid_value);
  EXPECT_TRUE (decoded<field_union_t> (layout::bytes_t{ 30, 0, 1, 2 }).error ()
               == protocol_errc::unknown_case);
}

TEST (LumexProtocolCompositeFieldsTest,
      GivenATrailingDiscriminant_WhenDecoded_ThenItIsReadFromTheEndFirst)
{
  typedef field::record_of<end_union_t::fields_type>::type record_type;
  record_type const pair
      = *decoded<end_union_t> (layout::bytes_t{ 9, 5, 6, 3 });
  ASSERT_EQ (field::get<1> (pair).cases.index (), 0u);
  EXPECT_EQ (field::get<0> (field::get<1> (pair).cases.get<0> ()), 5u);
  EXPECT_EQ (encoded<end_union_t> (pair), (layout::bytes_t{ 9, 5, 6, 3 }));
  record_type const word
      = *decoded<end_union_t> (layout::bytes_t{ 9, 0x22, 0x11, 4 });
  EXPECT_EQ (field::get<1> (word).cases.get<1> (), 0x1122u);
  EXPECT_TRUE (decoded<end_union_t> (layout::bytes_t{ 9, 5, 6, 7 }).error ()
               == protocol_errc::unknown_case);
  EXPECT_TRUE (decoded<end_union_t> (layout::bytes_t{ 9, 5, 6, 7, 3 }).error ()
               == protocol_errc::trailing_bytes);
  EXPECT_TRUE (decoded<end_union_t> (layout::bytes_t{ 9 }).error ()
               == protocol_errc::truncated);
}

TEST (LumexProtocolCompositeFieldsTest,
      GivenASessionValue_WhenDecoded_ThenItChoosesTheCase)
{
  typedef field::record_of<context_union_t::fields_type>::type record_type;
  tables::test_session const first = { 1, true };
  tables::test_session const second = { 2, true };
  tables::test_session const none = { 0, false };
  field::decode_context_t const first_context = first.context ();
  field::decode_context_t const second_context = second.context ();
  field::decode_context_t const none_context = none.context ();
  record_type const byte
      = *decoded<context_union_t> (layout::bytes_t{ 7 }, &first_context);
  EXPECT_EQ (field::get<0> (byte).cases.get<0> (), 7u);
  record_type const pair
      = *decoded<context_union_t> (layout::bytes_t{ 7, 8 }, &second_context);
  ASSERT_EQ (field::get<0> (pair).cases.index (), 1u);
  EXPECT_EQ (field::get<0> (pair).discriminant, 2);
  EXPECT_TRUE (decoded<context_union_t> (layout::bytes_t{ 7 }).error ()
               == protocol_errc::unknown_case);
  EXPECT_TRUE (
      decoded<context_union_t> (layout::bytes_t{ 7 }, &none_context).error ()
      == protocol_errc::unknown_case);
  EXPECT_EQ (encoded<context_union_t> (pair, &second_context),
             (layout::bytes_t{ 7, 8 }));
  layout::bytes_t out;
  EXPECT_FALSE (
      (layout::encode<binary, context_union_t> (out, pair, &first_context)
           .has_value ()));
  EXPECT_EQ (encoded<context_union_t> (pair), (layout::bytes_t{ 7, 8 }));
}

TEST (LumexProtocolCompositeFieldsTest,
      GivenATunnel_WhenDecoded_ThenTheInnerRowDecodesThePayload)
{
  typedef field::record_of<tunnel_t::fields_type>::type record_type;
  layout::bytes_t const wire{ 3, 'L', 2, 0x01, 0xF4 };
  record_type const value = *decoded<tunnel_t> (wire);
  EXPECT_EQ (field::get<1> (value).key, 'L');
  EXPECT_EQ (field::get<1> (value).payload.size (), 3u);
  layout::decode_result_t<
      layout::request_of<tables::inner_protocol::LAMP_t>::type> const lamp
      = layout::decode_nested<tables::inner_protocol,
                              tables::inner_protocol::LAMP_t> (
          field::get<1> (value));
  ASSERT_TRUE (lamp.has_value ());
  EXPECT_EQ (field::get<0> (*lamp), 2u);
  EXPECT_EQ (field::get<1> (*lamp), 500u);
  EXPECT_TRUE ((layout::decode_nested<tables::inner_protocol,
                                      tables::inner_protocol::VALVE_t> (
                    field::get<1> (value)))
                   .error ()
               == protocol_errc::unknown_frame);
  EXPECT_EQ (encoded<tunnel_t> (value), wire);
}
