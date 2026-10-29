// The sample value of a field
// (lumex/core/protocol/field/LumexProtocolSample.hpp): the smallest valid
// value of every field kind, which encodes: the first enumerator of a
// reflected enumeration without 0 (also in bits_ and split_ parts, and the
// first that fits the bits), the short form of by_length_, the first case of a
// union with its discriminant (leading, an earlier field, at the end, the
// session), no elements in counted_, the computed number of elements in
// computed_count_, an absent optional tail, a registry entry the command may
// access with a value inside its limits, and the 40-field layout with every
// field kind in the binary, hexadecimal and tokenized layouts.

#include <cstddef>
#include <cstdint>
#include <string>
#include <system_error>

#include <gtest/gtest.h>

#include "lumex/core/protocol/LumexProtocol"
#include "lumex/core/reflection/reflected_enum/LumexReflectedEnum.hpp"
#include "lumex/tests/core/protocol/LumexProtocolFieldTables.hpp"

namespace sample_tables
{
namespace dsl = lumex::core::protocol::dsl;
namespace field = lumex::core::protocol::field;
namespace layout = lumex::core::protocol::layout;
namespace registry = lumex::core::protocol::registry;

// No enumerator is 0.
LUMEX_DEFINE_REFLECTED_ENUM (stage, std::uint8_t, (warm, 3), (hot, 7))

// Negative enumerators only.
LUMEX_DEFINE_REFLECTED_ENUM (offset, std::int8_t, (back, -2), (ahead, 4))

// The enumerator nearest to 0 does not fit two signed bits, -2 does.
LUMEX_DEFINE_REFLECTED_ENUM (narrow, std::int8_t, (wide, 2), (fits, -2))

LUMEX_PROTOCOL_CONTEXT_KEY (sample_kind);

// Two elements whatever the earlier fields hold.
struct two_items
{
  template <typename Siblings>
  static std::size_t
  count (Siblings const &) LUMEX_NOEXCEPT
  {
    return 2;
  }
};

LUMEX_PROTOCOL_LAYOUT (stage_parts_t,
                       (bits_<u8_, 0, 4, stage>, bits_<u8_, 4, 2, narrow>),
                       ("stage", "narrow"));

LUMEX_PROTOCOL_LAYOUT (
    selected_t, (u8_, union_by_<field_<0>, case_<3, u8_>, case_<5, u16_le_>>),
    ("selector", "item"));

LUMEX_PROTOCOL_LAYOUT (computed_t,
                       (u8_, computed_count_<u8_, count_from_<0>, 4>,
                        computed_count_<u16_be_, two_items, 4>),
                       ("count", "first", "second"));

// The first entry is read-only and the second one allows nothing below 5.
// clang-format off
#define SAMPLE_PARAMS(P, P_AS) \
  P (LOCKED, (0x01), "Read only", (u8_), R, ()) \
  P (LEVEL, (0x02), "Writable", (i16_le_), RW, (limits (5, 10)))
// clang-format on
LUMEX_PROTOCOL_REGISTRY (sample_registry, SAMPLE_PARAMS);

struct codec
{
};

struct header
{
  using lumex_vocabulary = registry::table_vocabulary;
  static LUMEX_CONSTEXPR std::size_t key_width = 1;
};

// The reply reports an entry: a read is enough.
LUMEX_PROTOCOL_LAYOUT (report_t,
                       (registry::registry_key_<sample_registry, u8_>,
                        registry::registry_value_<sample_registry, 0, i16_le_,
                                                  registry::access_t::read>),
                       ("parameter", "value"));

// clang-format off
#define SAMPLE_ROWS(X, X_AS) \
  X (SET_PARAMETER, 0x01, "Write a parameter", \
     (registry_key_<sample_registry, u8_>, \
      registry_value_<sample_registry, 0, i16_le_>), \
     ("parameter", "value"), (report_t), ())
// clang-format on
LUMEX_PROTOCOL_DEFINE (sample_protocol, codec, header, SAMPLE_ROWS);
} // namespace sample_tables

namespace
{
namespace tables = sample_tables;
namespace gauge = lumex_tests::field_tables;
namespace field = lumex::core::protocol::field;
namespace layout = lumex::core::protocol::layout;
namespace registry = lumex::core::protocol::registry;

using binary = layout::binary_layout<>;
using hex = layout::fixed_width_text_layout<16>;
using tokens = layout::tokenized_text_layout<','>;

layout::bytes_view_t
view_of (layout::bytes_t const &bytes)
{
  return layout::bytes_view_t (bytes.data (), bytes.size ());
}

// The sample of F encodes in Layout, decodes back and encodes to the same
// bytes again; the bytes are returned.
template <typename Layout, typename F>
layout::bytes_t
sample_bytes (field::decode_context_t const *context = nullptr)
{
  typename field::value_of<F>::type const value = field::sample_value<F> ();
  layout::bytes_t out;
  layout::encode_result_t const encoded
      = layout::encode<Layout, F> (out, value, context);
  EXPECT_TRUE (encoded.has_value ())
      << (encoded ? std::string () : encoded.error ().message ());
  layout::decode_result_t<typename field::value_of<F>::type> const back
      = layout::decode<Layout, F> (view_of (out), context);
  EXPECT_TRUE (back.has_value ())
      << (back ? std::string () : back.error ().message ());
  if (back.has_value ())
    {
      layout::bytes_t again;
      EXPECT_TRUE (
          (layout::encode<Layout, F> (again, *back, context).has_value ()));
      EXPECT_EQ (again, out);
    }
  return out;
}
} // namespace

TEST (
    LumexProtocolSampleTest,
    GivenAReflectedEnumerationWithoutZero_WhenSampled_ThenItIsTheEnumeratorNearestToZero)
{
  using stored = field::enum_<field::u8_<>, tables::stage>;
  EXPECT_TRUE (field::sample_value<stored> () == tables::stage::warm);
  EXPECT_EQ ((sample_bytes<binary, stored> ()), (layout::bytes_t{ 0x03 }));
  using negative = field::enum_<field::i8_<>, tables::offset>;
  EXPECT_TRUE (field::sample_value<negative> () == tables::offset::back);
  EXPECT_EQ ((sample_bytes<binary, negative> ()), (layout::bytes_t{ 0xFE }));
}

TEST (
    LumexProtocolSampleTest,
    GivenAReflectedEnumerationInBits_WhenSampled_ThenItIsTheFirstEnumeratorThatFits)
{
  using low = field::bits_<field::u8_<>, 0, 4, tables::stage>;
  EXPECT_TRUE (field::sample_value<low> () == tables::stage::warm);
  using two_bits = field::bits_<field::u8_<>, 4, 2, tables::narrow>;
  EXPECT_TRUE (field::sample_value<two_bits> () == tables::narrow::fits);
  EXPECT_EQ ((sample_bytes<binary, two_bits> ()), (layout::bytes_t{ 0x20 }));
  using parts = field::split_<field::u8_<>, tables::stage_parts_t>;
  field::value_of<parts>::type const value = field::sample_value<parts> ();
  EXPECT_TRUE (field::get<0> (value.parts) == tables::stage::warm);
  EXPECT_TRUE (field::get<1> (value.parts) == tables::narrow::fits);
  EXPECT_EQ ((sample_bytes<binary, parts> ()), (layout::bytes_t{ 0x23 }));
}

TEST (LumexProtocolSampleTest,
      GivenAPlainEnumerationAndNumbers_WhenSampled_ThenTheyAreValueInitialized)
{
  EXPECT_EQ (field::sample_value<field::u16_le_<>> (), 0u);
  EXPECT_EQ ((field::sample_value<
                 field::enum_<field::char_, gauge::gauge_letter>> ()),
             gauge::gauge_letter ());
  EXPECT_EQ ((sample_bytes<binary, field::u32_be_<>> ()),
             (layout::bytes_t{ 0x00, 0x00, 0x00, 0x00 }));
}

TEST (LumexProtocolSampleTest,
      GivenByLength_WhenSampled_ThenItIsTheShortFormAndEncodesBelowMin)
{
  using either = field::by_length_<3, field::u16_le_<>, field::u32_le_<>>;
  field::value_of<either>::type const value = field::sample_value<either> ();
  EXPECT_EQ (value.index (), 0u);
  EXPECT_EQ ((sample_bytes<binary, either> ()),
             (layout::bytes_t{ 0x00, 0x00 }));
}

TEST (LumexProtocolSampleTest,
      GivenUnions_WhenSampled_ThenTheFirstCaseAndItsDiscriminantAreSet)
{
  using leading
      = field::union_by_<field::u8_<>, field::case_<4, field::u16_le_<>>,
                         field::case_<9, field::u8_<>>>;
  field::value_of<leading>::type const value = field::sample_value<leading> ();
  EXPECT_EQ (value.cases.index (), 0u);
  EXPECT_EQ (value.discriminant, 4);
  EXPECT_EQ ((sample_bytes<binary, leading> ()),
             (layout::bytes_t{ 0x04, 0x00, 0x00 }));
  using trailing = field::union_by_<field::at_end_<field::u8_<>>,
                                    field::case_<2, field::u8_<>>>;
  EXPECT_EQ ((sample_bytes<binary, trailing> ()),
             (layout::bytes_t{ 0x00, 0x02 }));
  // The discriminant is an earlier field of the layout: the sample sets it.
  field::value_of<tables::selected_t>::type const selected
      = field::sample_value<tables::selected_t> ();
  EXPECT_EQ (field::get<0> (selected), 3u);
  EXPECT_EQ ((sample_bytes<binary, tables::selected_t> ()),
             (layout::bytes_t{ 0x03, 0x00 }));
}

TEST (
    LumexProtocolSampleTest,
    GivenAUnionChosenByTheSession_WhenSampled_ThenTheFirstCaseDecodesWithASessionThatKnowsIt)
{
  using by_session
      = field::union_by_context_<gauge::gauge_kind,
                                 field::case_<1, field::u8_<>>,
                                 field::case_<2, field::u16_be_<>>>;
  gauge::test_session const session = { 1, true };
  field::decode_context_t const context = session.context ();
  EXPECT_EQ ((sample_bytes<binary, by_session> (&context)),
             (layout::bytes_t{ 0x00 }));
}

TEST (LumexProtocolSampleTest,
      GivenCountedAndComputedArrays_WhenSampled_ThenTheCountsAreConsistent)
{
  using counted = field::counted_<field::u8_<>, field::u8_<>, 8>;
  EXPECT_EQ (field::sample_value<counted> ().size (), 0u);
  EXPECT_EQ ((sample_bytes<binary, counted> ()), (layout::bytes_t{ 0x00 }));
  field::value_of<tables::computed_t>::type const value
      = field::sample_value<tables::computed_t> ();
  EXPECT_EQ (field::get<1> (value).size (), 0u);
  EXPECT_EQ (field::get<2> (value).size (), 2u);
  EXPECT_EQ ((sample_bytes<binary, tables::computed_t> ()),
             (layout::bytes_t{ 0x00, 0x00, 0x00, 0x00, 0x00 }));
}

TEST (
    LumexProtocolSampleTest,
    GivenATailAndArrays_WhenSampled_ThenTheTailIsAbsentAndEveryElementIsSampled)
{
  using tail = field::optional_tail_<field::u8_<>, field::u16_le_<>>;
  EXPECT_FALSE (field::sample_value<tail> ().present);
  EXPECT_TRUE ((sample_bytes<binary, tail> ()).empty ());
  using stages = field::array_<field::enum_<field::u8_<>, tables::stage>, 3>;
  field::value_of<stages>::type const value = field::sample_value<stages> ();
  for (std::size_t i = 0; i < value.size (); ++i)
    EXPECT_TRUE (value[i] == tables::stage::warm);
  EXPECT_EQ ((sample_bytes<binary, stages> ()),
             (layout::bytes_t{ 0x03, 0x03, 0x03 }));
}

TEST (
    LumexProtocolSampleTest,
    GivenARegistryValue_WhenSampled_ThenTheEntryAllowsTheAccessAndTheValueIsInsideItsLimits)
{
  using row = tables::sample_protocol::SET_PARAMETER_t;
  layout::request_of<row>::type const request
      = field::sample_value<row::fields_type> ();
  EXPECT_TRUE (field::get<0> (request) == tables::sample_registry::LEVEL);
  EXPECT_EQ (field::get<1> (request).raw, 5);
  layout::bytes_t out;
  ASSERT_TRUE ((layout::encode_request_record<tables::sample_protocol, row> (
                    out, request)
                    .has_value ()));
  EXPECT_EQ (out, (layout::bytes_t{ 0x02, 0x05, 0x00 }));
  // The reply reports a value: the read-only entry is the first one.
  layout::reply_value_of<row>::type const reply
      = field::sample_value<row::reply_type> ();
  EXPECT_TRUE (field::get<0> (reply) == tables::sample_registry::LOCKED);
  layout::bytes_t answer;
  ASSERT_TRUE (
      (layout::encode_reply<tables::sample_protocol, row> (answer, reply)
           .has_value ()));
  EXPECT_EQ (answer, (layout::bytes_t{ 0x01, 0x00, 0x00 }));
}

TEST (
    LumexProtocolSampleTest,
    GivenTheLayoutWithEveryFieldKind_WhenSampled_ThenItEncodesAndDecodesInEveryLayout)
{
  gauge::test_session const session = { 1, true };
  field::decode_context_t const context = session.context ();
  EXPECT_FALSE ((sample_bytes<binary, gauge::sink_t> (&context)).empty ());
  EXPECT_FALSE ((sample_bytes<hex, gauge::sink_t> (&context)).empty ());
  EXPECT_FALSE ((sample_bytes<tokens, gauge::sink_t> (&context)).empty ());
  field::value_of<gauge::sink_t>::type const value
      = field::sample_value<gauge::sink_t> ();
  // The union by field 31 has its discriminant in field 31.
  EXPECT_EQ (field::get<31> (value), 1u);
  EXPECT_TRUE (field::get<24> (value) == gauge::gauge_mode::idle);
}
