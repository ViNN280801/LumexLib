// Property tests of the layouts (lumex/core/protocol/layout): random values
// of the 40-field sink layout (every field kind) round trip in the binary,
// hexadecimal and tokenized layouts; random and damaged messages (bits
// flipped, bytes inserted or removed, cut at any length) never crash a
// decoder, never make it read past its input (the suite runs under ASan)
// and, when they decode, encode to a message that decodes and encodes to the
// same bytes again. The generator is seeded, so a failure repeats.

#include <cstddef>
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
using tables::view_of;

typedef field::record_of<tables::sink_t::fields_type>::type sink;
typedef layout::binary_layout<> binary;
typedef layout::fixed_width_text_layout<16> hex;
typedef layout::tokenized_text_layout<','> tokens;

// xorshift64*: deterministic and the same on every platform.
class random_source
{
public:
  explicit random_source (std::uint64_t seed) : m_state (seed | 1u) {}

  std::uint64_t
  next ()
  {
    m_state ^= m_state >> 12;
    m_state ^= m_state << 25;
    m_state ^= m_state >> 27;
    return m_state * 0x2545F4914F6CDD1Dull;
  }

  std::uint32_t
  below (std::uint32_t bound)
  {
    return static_cast<std::uint32_t> (next () % bound);
  }

private:
  std::uint64_t m_state;
};

enum class tail_form
{
  absent,
  short_form,
  long_form
};

template <typename T>
T
bits_as (random_source &random)
{
  return static_cast<T> (random.next ());
}

template <std::size_t N>
void
fill_text (random_source &random, field::fixed_string<N> &text)
{
  char letters[N] = {};
  std::size_t const size = random.below (static_cast<std::uint32_t> (N + 1));
  for (std::size_t i = 0; i < size; ++i)
    letters[i] = static_cast<char> ('a' + random.below (26));
  text.assign (field::text_view (letters, size));
}

template <typename Point>
void
fill_point (random_source &random, Point &point)
{
  field::get<0> (point) = bits_as<std::int16_t> (random);
  field::get<1> (point) = bits_as<std::uint8_t> (random);
}

// A valid sink value: the session gives the case of the context union, the
// layout which tail forms its length rule can tell apart; the payload of a
// nested message lives in `payload`.
sink
make_sink (random_source &random, std::int64_t kind, tail_form form,
           std::vector<std::uint8_t> &payload)
{
  sink value = sink ();
  field::get<0> (value) = bits_as<std::uint8_t> (random);
  field::get<1> (value) = bits_as<std::int8_t> (random);
  field::get<2> (value).raw = bits_as<std::uint16_t> (random);
  field::get<3> (value) = bits_as<std::uint16_t> (random);
  field::get<4> (value) = bits_as<std::int16_t> (random);
  field::get<5> (value).raw = bits_as<std::int16_t> (random);
  field::get<6> (value) = bits_as<std::uint32_t> (random) & 0xFFFFFFu;
  field::get<7> (value)
      = static_cast<std::int32_t> (random.below (0x1000000u)) - 0x800000;
  field::get<8> (value) = bits_as<std::uint32_t> (random);
  field::get<9> (value).raw = bits_as<std::int32_t> (random);
  field::get<10> (value) = random.next ();
  field::get<11> (value) = bits_as<std::int64_t> (random);
  field::get<12> (value)
      = static_cast<float> (random.below (2000001u)) / 8.0f - 125000.0f;
  field::get<13> (value)
      = static_cast<double> (bits_as<std::int32_t> (random)) / 1024.0;
  field::get<14> (value) = static_cast<std::int32_t> (
                               bits_as<std::uint32_t> (random) & 0x7FFFFFFFu)
                           * (random.below (2) == 0 ? 1 : -1);
  field::get<15> (value).raw = static_cast<std::int16_t> (
      static_cast<int> (random.below (65535u)) - 32767);
  field::get<16> (value) = bits_as<std::uint16_t> (random);
  field::get<17> (value).raw = bits_as<std::uint8_t> (random);
  field::get<18> (value).raw
      = static_cast<std::int64_t> (random.below (1099999999u)) - 99999999;
  field::get<19> (value) = static_cast<char> ('A' + random.below (26));
  fill_text (random, field::get<20> (value));
  field::get<21> (value) = static_cast<std::uint8_t> (random.below (16));
  field::get<22> (value) = static_cast<std::uint8_t> (random.below (16));
  std::uint16_t const word = bits_as<std::uint16_t> (random);
  field::get<23> (value).raw = word;
  field::get<0> (field::get<23> (value).parts)
      = static_cast<std::uint16_t> (word & 0xFFFu);
  field::get<1> (field::get<23> (value).parts)
      = static_cast<std::uint16_t> (word >> 12);
  tables::gauge_mode const modes[]
      = { tables::gauge_mode::idle, tables::gauge_mode::run,
          tables::gauge_mode::hold };
  field::get<24> (value) = modes[random.below (3)];
  field::get<25> (value) = random.below (2) == 0 ? tables::gauge_letter::start
                                                 : tables::gauge_letter::rest;
  for (std::size_t i = 0; i < 3; ++i)
    field::get<26> (value)[i] = bits_as<std::uint16_t> (random);
  for (std::size_t i = 0; i < 2; ++i)
    fill_point (random, field::get<27> (value)[i]);
  std::size_t const counted = random.below (9);
  for (std::size_t i = 0; i < counted; ++i)
    field::get<28> (value).push_back (bits_as<std::uint8_t> (random));
  std::size_t const computed = random.below (7);
  field::get<29> (value) = static_cast<std::uint8_t> (computed);
  for (std::size_t i = 0; i < computed; ++i)
    field::get<30> (value).push_back (bits_as<std::int16_t> (random));
  std::uint8_t const selector
      = static_cast<std::uint8_t> (1 + random.below (2));
  field::get<31> (value) = selector;
  field::get<32> (value).discriminant = selector;
  if (selector == 1)
    fill_point (random, field::get<32> (value).cases.emplace<0> ());
  else
    field::get<32> (value).cases.set<1> (bits_as<std::uint32_t> (random));
  if (random.below (2) == 0)
    {
      field::get<33> (value).discriminant = 7;
      fill_point (random, field::get<33> (value).cases.emplace<0> ());
    }
  else
    {
      field::get<33> (value).discriminant = 9;
      fill_text (random, field::get<33> (value).cases.emplace<1> ());
    }
  field::get<34> (value).discriminant = kind;
  if (kind == 1)
    field::get<34> (value).cases.set<0> (bits_as<std::uint8_t> (random));
  else
    field::get<34> (value).cases.set<1> (bits_as<std::uint16_t> (random));
  field::get<35> (value)
      = static_cast<std::int8_t> (static_cast<int> (random.below (32)) - 16);
  field::get<36> (value) = static_cast<std::uint8_t> (random.below (16));
  fill_point (random, field::get<37> (value));
  field::get<38> (value) = bits_as<std::uint8_t> (random);

  auto &tail = field::get<39> (value);
  tail.present = form != tail_form::absent;
  if (!tail.present)
    return value;
  field::get<0> (tail.fields) = bits_as<std::uint16_t> (random);
  auto &body = field::get<1> (tail.fields);
  if (form == tail_form::short_form)
    {
      auto &short_tail = body.emplace<0> ();
      field::get<0> (short_tail) = bits_as<std::uint8_t> (random);
      auto &item = field::get<1> (short_tail);
      if (random.below (2) == 0)
        {
          item.discriminant = 1;
          item.cases.set<0> (bits_as<std::uint16_t> (random));
        }
      else
        {
          item.discriminant = 2;
          item.cases.set<1> (bits_as<std::uint8_t> (random));
        }
      return value;
    }
  auto &long_tail = body.emplace<1> ();
  field::get<0> (long_tail) = bits_as<std::uint16_t> (random);
  // An inner LAMP message: lamp, level (3 bytes).
  payload.assign (3, 0);
  for (std::uint8_t &byte : payload)
    byte = bits_as<std::uint8_t> (random);
  field::get<1> (long_tail).key = 'L';
  field::get<1> (long_tail).payload
      = field::bytes_view_t (payload.data (), payload.size ());
  return value;
}

template <typename Layout>
layout::bytes_t
encode_sink (sink const &value, field::decode_context_t const &context)
{
  layout::bytes_t out;
  layout::encode_result_t const result
      = layout::encode<Layout, tables::sink_t> (out, value, &context);
  EXPECT_TRUE (result.has_value ())
      << (result.has_value () ? std::string () : result.error ().message ());
  return out;
}

template <typename Layout>
void
expect_random_round_trips (std::uint64_t seed, tail_form const *forms,
                           std::size_t form_count)
{
  random_source random (seed);
  for (int round = 0; round < 300; ++round)
    {
      std::int64_t const kind = 1 + random.below (2);
      tables::test_session const session = { kind, true };
      field::decode_context_t const context = session.context ();
      std::vector<std::uint8_t> payload;
      sink const value = make_sink (
          random, kind,
          forms[random.below (static_cast<std::uint32_t> (form_count))],
          payload);
      layout::bytes_t const bytes = encode_sink<Layout> (value, context);
      layout::decode_result_t<sink> const back
          = layout::decode<Layout, tables::sink_t> (view_of (bytes), &context);
      ASSERT_TRUE (back.has_value ())
          << "round " << round << ": " << back.error ().message ();
      EXPECT_TRUE (*back == value) << "round " << round;
    }
}

// Decodes `bytes`; when it decodes, its encoding must decode and encode to
// the same bytes.
template <typename Layout>
void
expect_stable (layout::bytes_t const &bytes,
               field::decode_context_t const &context, std::size_t &decoded)
{
  // A copy of exactly the message: a read past it is an error ASan reports.
  layout::bytes_t const exact (bytes);
  layout::decode_result_t<sink> const first
      = layout::decode<Layout, tables::sink_t> (view_of (exact), &context);
  if (!first.has_value ())
    {
      EXPECT_STREQ (first.error ().category ().name (), "lumex.protocol");
      return;
    }
  ++decoded;
  layout::bytes_t once;
  ASSERT_TRUE ((layout::encode<Layout, tables::sink_t> (once, *first, &context)
                    .has_value ()));
  layout::decode_result_t<sink> const second
      = layout::decode<Layout, tables::sink_t> (view_of (once), &context);
  ASSERT_TRUE (second.has_value ());
  layout::bytes_t twice;
  ASSERT_TRUE (
      (layout::encode<Layout, tables::sink_t> (twice, *second, &context)
           .has_value ()));
  EXPECT_EQ (once, twice);
}

template <typename Layout>
std::size_t
damage_and_decode (std::uint64_t seed, tail_form const *forms,
                   std::size_t form_count)
{
  random_source random (seed);
  std::size_t decoded = 0;
  for (int round = 0; round < 400; ++round)
    {
      std::int64_t const kind = 1 + random.below (2);
      tables::test_session const session = { kind, true };
      field::decode_context_t const context = session.context ();
      std::vector<std::uint8_t> payload;
      sink const value = make_sink (
          random, kind,
          forms[random.below (static_cast<std::uint32_t> (form_count))],
          payload);
      layout::bytes_t const bytes = encode_sink<Layout> (value, context);
      // Every cut.
      for (std::size_t size = 0; size < bytes.size ();
           size += 1 + random.below (5))
        expect_stable<Layout> (
            layout::bytes_t (bytes.begin (),
                             bytes.begin ()
                                 + static_cast<std::ptrdiff_t> (size)),
            context, decoded);
      // Flipped bits, changed, inserted and removed bytes.
      for (int damage = 0; damage < 8; ++damage)
        {
          layout::bytes_t broken = bytes;
          std::size_t const at
              = random.below (static_cast<std::uint32_t> (broken.size ()));
          switch (random.below (4))
            {
            case 0:
              broken[at] ^= static_cast<std::uint8_t> (1u << random.below (8));
              break;
            case 1:
              broken[at] = bits_as<std::uint8_t> (random);
              break;
            case 2:
              broken.insert (broken.begin ()
                                 + static_cast<std::ptrdiff_t> (at),
                             bits_as<std::uint8_t> (random));
              break;
            default:
              broken.erase (broken.begin ()
                            + static_cast<std::ptrdiff_t> (at));
              break;
            }
          expect_stable<Layout> (broken, context, decoded);
        }
      // Pure noise of the same length.
      layout::bytes_t noise (bytes.size ());
      for (std::uint8_t &byte : noise)
        byte = bits_as<std::uint8_t> (random);
      expect_stable<Layout> (noise, context, decoded);
    }
  return decoded;
}

tail_form const kBinaryForms[]
    = { tail_form::absent, tail_form::short_form, tail_form::long_form };
tail_form const kHexForms[] = { tail_form::absent, tail_form::long_form };
tail_form const kTokenForms[] = { tail_form::absent, tail_form::short_form };
} // namespace

TEST (LumexProtocolLayoutPropertyTest,
      GivenRandomSinkValues_WhenRoundTrippedInBinary_ThenEveryValueComesBack)
{
  expect_random_round_trips<binary> (0x5EED0001u, kBinaryForms, 3);
}

TEST (LumexProtocolLayoutPropertyTest,
      GivenRandomSinkValues_WhenRoundTrippedAsHexText_ThenEveryValueComesBack)
{
  expect_random_round_trips<hex> (0x5EED0002u, kHexForms, 2);
}

TEST (LumexProtocolLayoutPropertyTest,
      GivenRandomSinkValues_WhenRoundTrippedAsTokens_ThenEveryValueComesBack)
{
  expect_random_round_trips<tokens> (0x5EED0003u, kTokenForms, 2);
}

TEST (LumexProtocolLayoutPropertyTest,
      GivenDamagedBinaryMessages_WhenDecoded_ThenNoCrashAndStableEncodings)
{
  std::size_t const decoded
      = damage_and_decode<binary> (0xBAD0001u, kBinaryForms, 3);
  // Cuts at the optional tail and some damages still decode.
  EXPECT_GT (decoded, 0u);
}

TEST (LumexProtocolLayoutPropertyTest,
      GivenDamagedHexMessages_WhenDecoded_ThenNoCrashAndStableEncodings)
{
  std::size_t const decoded
      = damage_and_decode<hex> (0xBAD0002u, kHexForms, 2);
  EXPECT_GT (decoded, 0u);
}

TEST (LumexProtocolLayoutPropertyTest,
      GivenDamagedTokenMessages_WhenDecoded_ThenNoCrashAndStableEncodings)
{
  std::size_t const decoded
      = damage_and_decode<tokens> (0xBAD0003u, kTokenForms, 2);
  EXPECT_GT (decoded, 0u);
}

TEST (LumexProtocolLayoutPropertyTest,
      GivenRandomBytesOfEveryLength_WhenDecodedAsRowsOfEveryLayout_ThenNoCrash)
{
  random_source random (0xF00Du);
  tables::test_session const session = { 2, true };
  field::decode_context_t const context = session.context ();
  for (int round = 0; round < 3000; ++round)
    {
      layout::bytes_t bytes (random.below (96));
      for (std::uint8_t &byte : bytes)
        byte = random.below (4) == 0 ? static_cast<std::uint8_t> (',')
                                     : bits_as<std::uint8_t> (random);
      layout::bytes_t const exact (bytes);
      (void)layout::decode_reply<tables::gauge, tables::gauge::READ_SINK_t> (
          view_of (exact), &context);
      (void)
          layout::decode_reply<tables::gauge_hex, tables::gauge::READ_SINK_t> (
              view_of (exact), &context);
      (void)layout::decode_reply<tables::gauge_text,
                                 tables::gauge::READ_SINK_t> (view_of (exact),
                                                              &context);
      (void)layout::decode_request<tables::gauge_decimal,
                                   tables::gauge::SET_LEVEL_t> (
          view_of (exact));
      (void)layout::decode_request<tables::gauge_text,
                                   tables::gauge::SET_NAME_t> (
          view_of (exact));
    }
  SUCCEED ();
}
