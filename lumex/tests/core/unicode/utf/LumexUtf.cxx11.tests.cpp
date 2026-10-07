// lumex/tests/core/unicode/utf/LumexUtf.cxx11.tests.cpp
//
// The counters, writers and decoders of utf/LumexUtf.hpp. The expected bytes
// and units come from reference encoders in LumexUnicodeTestSupport.hpp, not
// from the policies under test. Behavior of the decoders on invalid input is
// pinned as the header documents it: they skip what they cannot decode and do
// not reject overlong forms or surrogates.
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/unicode/utf/LumexUtf.hpp"
#include "lumex/core/utility/bit/LumexBit.hpp"

#include "lumex/tests/core/unicode/LumexUnicodeTestSupport.hpp"

using namespace lumex::core::unicode::utf;
using lumex::core::utility::bit::byte_swap;
using unicode_test::append_utf16;
using unicode_test::append_utf8;
using unicode_test::decode_recording;
using unicode_test::encode_utf16;
using unicode_test::encode_utf8;
using unicode_test::recorder_t;
using unicode_test::sequence;

namespace
{
// The two passes of a conversion: the counter sizes the output, the writer
// fills it. A sentinel after the counted size shows a write past it.
template <typename Decoder, typename Counter, typename Writer, typename Out,
          typename In>
std::vector<Out>
convert (std::vector<In> const &input)
{
  In const *const data = input.empty () ? nullptr : &input[0];
  std::size_t const count
      = Decoder::process (data, input.size (), 0, Counter ());
  std::vector<Out> out (count + 1, static_cast<Out> (0xEE));
  Out *const begin = &out[0];
  Out *const end = Decoder::process (data, input.size (), begin, Writer ());
  EXPECT_EQ (static_cast<std::size_t> (end - begin), count);
  EXPECT_EQ (out[count], static_cast<Out> (0xEE));
  out.resize (count);
  return out;
}

std::vector<std::uint32_t>
swapped (std::vector<std::uint32_t> const &units)
{
  std::vector<std::uint32_t> out;
  for (std::size_t i = 0; i < units.size (); ++i)
    out.push_back (byte_swap (units[i]));
  return out;
}

std::vector<std::uint16_t>
swapped (std::vector<std::uint16_t> const &units)
{
  std::vector<std::uint16_t> out;
  for (std::size_t i = 0; i < units.size (); ++i)
    out.push_back (byte_swap (units[i]));
  return out;
}

std::vector<std::uint8_t>
bytes (std::initializer_list<int> values)
{
  std::vector<std::uint8_t> out;
  for (int value : values)
    out.push_back (static_cast<std::uint8_t> (value));
  return out;
}

std::vector<std::uint32_t>
points (std::initializer_list<std::uint32_t> values)
{
  return std::vector<std::uint32_t> (values);
}
} // namespace

// ---------------------------------------------------------------- counters

TEST (UtfCounterTest,
      GivenBoundaryCodePoints_WhenUtf8CounterLow_ThenAddsTheByteLength)
{
  EXPECT_EQ (utf8_counter::low (0, 0x00), 1U);
  EXPECT_EQ (utf8_counter::low (0, 0x7F), 1U);
  EXPECT_EQ (utf8_counter::low (0, 0x80), 2U);
  EXPECT_EQ (utf8_counter::low (0, 0x7FF), 2U);
  EXPECT_EQ (utf8_counter::low (0, 0x800), 3U);
  EXPECT_EQ (utf8_counter::low (0, 0xFFFF), 3U);
}

TEST (UtfCounterTest,
      GivenSupplementaryCodePoints_WhenUtf8CounterHigh_ThenAddsFour)
{
  EXPECT_EQ (utf8_counter::high (0, 0x10000), 4U);
  EXPECT_EQ (utf8_counter::high (0, 0x10FFFF), 4U);
}

TEST (UtfCounterTest, GivenARunningTotal_WhenCounted_ThenTheTotalIsThreaded)
{
  EXPECT_EQ (utf8_counter::low (10, 0x41), 11U);
  EXPECT_EQ (utf8_counter::low (10, 0x20AC), 13U);
  EXPECT_EQ (utf8_counter::high (10, 0x1F600), 14U);
  EXPECT_EQ (utf16_counter::low (10, 0x41), 11U);
  EXPECT_EQ (utf16_counter::high (10, 0x1F600), 12U);
  EXPECT_EQ (utf32_counter::low (10, 0x41), 11U);
  EXPECT_EQ (utf32_counter::high (10, 0x1F600), 11U);
}

TEST (UtfCounterTest, GivenTheCounters_WhenRead_ThenTheirValueTypeIsSizeT)
{
  static_assert (std::is_same<utf8_counter::value_type, std::size_t>::value,
                 "utf8_counter counts in std::size_t");
  static_assert (std::is_same<utf16_counter::value_type, std::size_t>::value,
                 "utf16_counter counts in std::size_t");
  static_assert (std::is_same<utf32_counter::value_type, std::size_t>::value,
                 "utf32_counter counts in std::size_t");
  SUCCEED ();
}

// ----------------------------------------------------------------- writers

TEST (UtfWriterTest,
      GivenBoundaryCodePoints_WhenUtf8WriterLowAndHigh_ThenWritesTheEncoding)
{
  struct sample_t
  {
    std::uint32_t cp;
    std::vector<std::uint8_t> expected;
  };
  std::vector<sample_t> samples;
  samples.push_back ({ 0x00, bytes ({ 0x00 }) });
  samples.push_back ({ 0x7F, bytes ({ 0x7F }) });
  samples.push_back ({ 0x80, bytes ({ 0xC2, 0x80 }) });
  samples.push_back ({ 0x7FF, bytes ({ 0xDF, 0xBF }) });
  samples.push_back ({ 0x800, bytes ({ 0xE0, 0xA0, 0x80 }) });
  samples.push_back ({ 0xFFFF, bytes ({ 0xEF, 0xBF, 0xBF }) });
  samples.push_back ({ 0x10000, bytes ({ 0xF0, 0x90, 0x80, 0x80 }) });
  samples.push_back ({ 0x1F600, bytes ({ 0xF0, 0x9F, 0x98, 0x80 }) });
  samples.push_back ({ 0x10FFFF, bytes ({ 0xF4, 0x8F, 0xBF, 0xBF }) });

  for (std::size_t i = 0; i < samples.size (); ++i)
    {
      std::uint8_t buffer[8]
          = { 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE };
      std::uint8_t *const end
          = samples[i].cp < 0x10000
                ? utf8_writer::low (buffer, samples[i].cp)
                : utf8_writer::high (buffer, samples[i].cp);
      ASSERT_EQ (static_cast<std::size_t> (end - buffer),
                 samples[i].expected.size ())
          << std::hex << samples[i].cp;
      for (std::size_t k = 0; k < samples[i].expected.size (); ++k)
        EXPECT_EQ (buffer[k], samples[i].expected[k])
            << std::hex << samples[i].cp << " byte " << k;
      EXPECT_EQ (buffer[samples[i].expected.size ()], 0xEE)
          << "wrote past the sequence for " << std::hex << samples[i].cp;
    }
}

TEST (UtfWriterTest, GivenAnyCodePoint_WhenUtf8WriterAny_ThenPicksLowOrHigh)
{
  for (std::uint32_t cp : { 0x41U, 0x7FFU, 0xFFFFU, 0x10000U, 0x10FFFFU })
    {
      std::uint8_t via_any[4] = { 0, 0, 0, 0 };
      std::uint8_t via_pick[4] = { 0, 0, 0, 0 };
      std::uint8_t *const any_end = utf8_writer::any (via_any, cp);
      std::uint8_t *const pick_end = cp < 0x10000
                                         ? utf8_writer::low (via_pick, cp)
                                         : utf8_writer::high (via_pick, cp);
      EXPECT_EQ (any_end - via_any, pick_end - via_pick) << std::hex << cp;
      for (int k = 0; k < 4; ++k)
        EXPECT_EQ (via_any[k], via_pick[k]) << std::hex << cp;
    }
}

TEST (
    UtfWriterTest,
    GivenCodePoints_WhenUtf16WriterLowAndHigh_ThenWritesUnitsOrASurrogatePair)
{
  std::uint16_t buffer[3] = { 0xEEEE, 0xEEEE, 0xEEEE };

  std::uint16_t *end = utf16_writer::low (buffer, 0x20AC);
  EXPECT_EQ (end - buffer, 1);
  EXPECT_EQ (buffer[0], 0x20AC);
  EXPECT_EQ (buffer[1], 0xEEEE);

  end = utf16_writer::high (buffer, 0x10000);
  EXPECT_EQ (end - buffer, 2);
  EXPECT_EQ (buffer[0], 0xD800);
  EXPECT_EQ (buffer[1], 0xDC00);

  end = utf16_writer::high (buffer, 0x1F600);
  EXPECT_EQ (end - buffer, 2);
  EXPECT_EQ (buffer[0], 0xD83D);
  EXPECT_EQ (buffer[1], 0xDE00);

  end = utf16_writer::high (buffer, 0x10FFFF);
  EXPECT_EQ (end - buffer, 2);
  EXPECT_EQ (buffer[0], 0xDBFF);
  EXPECT_EQ (buffer[1], 0xDFFF);
  EXPECT_EQ (buffer[2], 0xEEEE);
}

TEST (UtfWriterTest, GivenAnyCodePoint_WhenUtf16WriterAny_ThenPicksLowOrHigh)
{
  std::uint16_t buffer[2] = { 0, 0 };
  EXPECT_EQ (utf16_writer::any (buffer, 0xFFFF) - buffer, 1);
  EXPECT_EQ (buffer[0], 0xFFFF);
  EXPECT_EQ (utf16_writer::any (buffer, 0x10000) - buffer, 2);
  EXPECT_EQ (buffer[0], 0xD800);
  EXPECT_EQ (buffer[1], 0xDC00);
}

TEST (UtfWriterTest,
      GivenCodePoints_WhenUtf32WriterLowHighAny_ThenStoresTheValue)
{
  std::uint32_t buffer[1] = { 0 };
  EXPECT_EQ (utf32_writer::low (buffer, 0x20AC) - buffer, 1);
  EXPECT_EQ (buffer[0], 0x20ACU);
  EXPECT_EQ (utf32_writer::high (buffer, 0x1F600) - buffer, 1);
  EXPECT_EQ (buffer[0], 0x1F600U);
  EXPECT_EQ (utf32_writer::any (buffer, 0x10FFFF) - buffer, 1);
  EXPECT_EQ (buffer[0], 0x10FFFFU);
}

TEST (UtfWriterTest,
      GivenCodePointsAboveLatin1_WhenLatin1Writer_ThenWritesAQuestionMark)
{
  std::uint8_t buffer[2] = { 0, 0 };
  EXPECT_EQ (latin1_writer::low (buffer, 0x41) - buffer, 1);
  EXPECT_EQ (buffer[0], 0x41);
  EXPECT_EQ (latin1_writer::low (buffer, 0xE9) - buffer, 1);
  EXPECT_EQ (buffer[0], 0xE9);
  EXPECT_EQ (latin1_writer::low (buffer, 0xFF) - buffer, 1);
  EXPECT_EQ (buffer[0], 0xFF);
  EXPECT_EQ (latin1_writer::low (buffer, 0x100) - buffer, 1);
  EXPECT_EQ (buffer[0], '?');
  EXPECT_EQ (latin1_writer::low (buffer, 0x20AC) - buffer, 1);
  EXPECT_EQ (buffer[0], '?');
  buffer[0] = 0;
  EXPECT_EQ (latin1_writer::high (buffer, 0x1F600) - buffer, 1);
  EXPECT_EQ (buffer[0], '?');
}

// ----------------------------------------------------------- UTF-8 decoder

TEST (UtfDecoderTest, GivenAscii_WhenUtf8Decode_ThenEveryByteIsALowCodePoint)
{
  std::vector<std::uint8_t> const input = bytes ({ 'H', 'e', 'l', 'l', 'o' });
  recorder_t const result = decode_recording<utf8_decoder> (input);
  EXPECT_EQ (result.all, points ({ 'H', 'e', 'l', 'l', 'o' }));
  EXPECT_EQ (result.high_calls.size (), 0U);
}

TEST (UtfDecoderTest,
      GivenEachSequenceLength_WhenUtf8Decode_ThenTheCodePointComesBack)
{
  EXPECT_EQ (decode_recording<utf8_decoder> (bytes ({ 0xC3, 0xA9 })).all,
             points ({ 0xE9 }));
  EXPECT_EQ (decode_recording<utf8_decoder> (bytes ({ 0xC2, 0x80 })).all,
             points ({ 0x80 }));
  EXPECT_EQ (decode_recording<utf8_decoder> (bytes ({ 0xDF, 0xBF })).all,
             points ({ 0x7FF }));
  EXPECT_EQ (decode_recording<utf8_decoder> (bytes ({ 0xE2, 0x82, 0xAC })).all,
             points ({ 0x20AC }));
  EXPECT_EQ (decode_recording<utf8_decoder> (bytes ({ 0xE0, 0xA0, 0x80 })).all,
             points ({ 0x800 }));
  EXPECT_EQ (decode_recording<utf8_decoder> (bytes ({ 0xEF, 0xBF, 0xBF })).all,
             points ({ 0xFFFF }));
  EXPECT_EQ (
      decode_recording<utf8_decoder> (bytes ({ 0xF0, 0x9F, 0x98, 0x80 })).all,
      points ({ 0x1F600 }));
  EXPECT_EQ (
      decode_recording<utf8_decoder> (bytes ({ 0xF0, 0x90, 0x80, 0x80 })).all,
      points ({ 0x10000 }));
  EXPECT_EQ (
      decode_recording<utf8_decoder> (bytes ({ 0xF4, 0x8F, 0xBF, 0xBF })).all,
      points ({ 0x10FFFF }));
}

TEST (UtfDecoderTest,
      GivenMixedLengths_WhenUtf8Decode_ThenLowAndHighAreSplitAtTenThousandHex)
{
  std::vector<std::uint8_t> const input = encode_utf8 (
      points ({ 0x41, 0xE9, 0x20AC, 0xFFFF, 0x10000, 0x1F600 }));
  recorder_t const result = decode_recording<utf8_decoder> (input);
  EXPECT_EQ (result.low_calls, points ({ 0x41, 0xE9, 0x20AC, 0xFFFF }));
  EXPECT_EQ (result.high_calls, points ({ 0x10000, 0x1F600 }));
  EXPECT_EQ (result.all,
             points ({ 0x41, 0xE9, 0x20AC, 0xFFFF, 0x10000, 0x1F600 }));
}

TEST (UtfDecoderTest, GivenNoInput_WhenUtf8Decode_ThenTheStartValueIsReturned)
{
  EXPECT_EQ (
      utf8_decoder::process (static_cast<std::uint8_t const *> (nullptr), 0, 7,
                             utf8_counter ()),
      7U);
  recorder_t recorder;
  EXPECT_EQ (
      utf8_decoder::process (static_cast<std::uint8_t const *> (nullptr), 0,
                             &recorder, unicode_test::recording_traits ()),
      &recorder);
  EXPECT_TRUE (recorder.all.empty ());
}

TEST (UtfDecoderTest,
      GivenAStrayContinuationByte_WhenUtf8Decode_ThenItIsSkipped)
{
  EXPECT_EQ (decode_recording<utf8_decoder> (bytes ({ 'a', 0x80, 'b' })).all,
             points ({ 'a', 'b' }));
  EXPECT_EQ (decode_recording<utf8_decoder> (bytes ({ 0x80 })).all.size (),
             0U);
  EXPECT_EQ (
      decode_recording<utf8_decoder> (bytes ({ 0xBF, 0xBF })).all.size (), 0U);
}

TEST (UtfDecoderTest,
      GivenATruncatedSequence_WhenUtf8Decode_ThenItsBytesAreSkipped)
{
  EXPECT_EQ (
      decode_recording<utf8_decoder> (bytes ({ 0xE2, 0x82 })).all.size (), 0U);
  EXPECT_EQ (
      decode_recording<utf8_decoder> (bytes ({ 'a', 0xF0, 0x9F, 0x98 })).all,
      points ({ 'a' }));
  EXPECT_EQ (decode_recording<utf8_decoder> (bytes ({ 0xC3 })).all.size (),
             0U);
}

TEST (
    UtfDecoderTest,
    GivenALeadByteWithoutContinuation_WhenUtf8Decode_ThenOnlyTheLeadIsSkipped)
{
  EXPECT_EQ (decode_recording<utf8_decoder> (bytes ({ 0xE2, 'A', 'B' })).all,
             points ({ 'A', 'B' }));
  EXPECT_EQ (decode_recording<utf8_decoder> (bytes ({ 0xC3, 'A' })).all,
             points ({ 'A' }));
  EXPECT_EQ (
      decode_recording<utf8_decoder> (bytes ({ 0xF0, 0x9F, 'A', 'B' })).all,
      points ({ 'A', 'B' }));
}

TEST (UtfDecoderTest,
      GivenBytesThatCanNotLeadASequence_WhenUtf8Decode_ThenTheyAreSkipped)
{
  EXPECT_EQ (decode_recording<utf8_decoder> (
                 bytes ({ 0xF8, 0x88, 0x80, 0x80, 0x80, 'x' }))
                 .all,
             points ({ 'x' }));
  EXPECT_EQ (decode_recording<utf8_decoder> (bytes ({ 0xFF, 'y', 0xFE })).all,
             points ({ 'y' }));
}

TEST (
    UtfDecoderTest,
    GivenOverlongAndSurrogateForms_WhenUtf8Decode_ThenTheyAreAcceptedAsTheHeaderSays)
{
  // Documented leniency: no overlong or surrogate check.
  EXPECT_EQ (decode_recording<utf8_decoder> (bytes ({ 0xC0, 0x80 })).all,
             points ({ 0x00 }));
  EXPECT_EQ (decode_recording<utf8_decoder> (bytes ({ 0xE0, 0x80, 0x80 })).all,
             points ({ 0x00 }));
  EXPECT_EQ (decode_recording<utf8_decoder> (bytes ({ 0xED, 0xA0, 0x80 })).all,
             points ({ 0xD800 }));
  recorder_t const past_range
      = decode_recording<utf8_decoder> (bytes ({ 0xF7, 0xBF, 0xBF, 0xBF }));
  EXPECT_EQ (past_range.high_calls, points ({ 0x1FFFFF }));
}

TEST (
    UtfDecoderTest,
    GivenALengthThatCutsASequence_WhenUtf8Decode_ThenTheBytesBeyondItAreNotRead)
{
  // The bytes after the cut are valid continuation bytes. A decoder that read
  // past the given length would complete the character.
  std::uint8_t const four[] = { 0xF0, 0x9F, 0x98, 0x80 };
  EXPECT_EQ (decode_recording<utf8_decoder> (four, 3).all.size (), 0U);
  EXPECT_EQ (decode_recording<utf8_decoder> (four, 4).all,
             points ({ 0x1F600 }));

  std::uint8_t const three[] = { 0xE2, 0x82, 0xAC };
  EXPECT_EQ (decode_recording<utf8_decoder> (three, 2).all.size (), 0U);
  EXPECT_EQ (decode_recording<utf8_decoder> (three, 3).all,
             points ({ 0x20AC }));

  std::uint8_t const two[] = { 0xC3, 0xA9 };
  EXPECT_EQ (decode_recording<utf8_decoder> (two, 1).all.size (), 0U);
  EXPECT_EQ (decode_recording<utf8_decoder> (two, 2).all, points ({ 0xE9 }));
}

TEST (
    UtfDecoderTest,
    GivenAStrayByteInsideAsciiRuns_WhenUtf8Decode_ThenItIsSkippedAtEveryAlignment)
{
  // Any of the four bytes of an aligned block can be the one that is not
  // ASCII; the block must then go through the byte-wise path.
  int const strays[] = { 0x80, 0xBF, 0xC3, 0xE2, 0xF0, 0xFF };
  for (std::size_t offset = 0; offset < 4; ++offset)
    for (std::size_t position = 0; position < 24; ++position)
      for (std::size_t kind = 0; kind < sizeof (strays) / sizeof (strays[0]);
           ++kind)
        {
          alignas (8) std::uint8_t raw[64];
          std::vector<std::uint32_t> expected;
          for (std::size_t i = 0; i < 24; ++i)
            {
              if (i == position)
                raw[offset + i] = static_cast<std::uint8_t> (strays[kind]);
              else
                {
                  raw[offset + i] = static_cast<std::uint8_t> ('a' + i % 26);
                  expected.push_back ('a' + i % 26);
                }
            }
          recorder_t const result = decode_recording<utf8_decoder> (
              static_cast<std::uint8_t const *> (raw + offset), 24);
          ASSERT_EQ (result.all, expected)
              << "offset " << offset << " position " << position << " byte "
              << std::hex << strays[kind];
        }
}

TEST (
    UtfDecoderTest,
    GivenLongAsciiRunsAtEveryAlignment_WhenUtf8Decode_ThenTheFastPathAgreesWithTheReference)
{
  // The decoder reads aligned ASCII four bytes at a time. Place the same text
  // at four start offsets and put a two-byte character at every position of
  // the run, so the block loop ends at every possible spot.
  for (std::size_t offset = 0; offset < 4; ++offset)
    for (std::size_t position = 0; position < 40; ++position)
      {
        std::vector<std::uint32_t> text;
        for (std::size_t i = 0; i < 40; ++i)
          text.push_back (i == position ? 0xE9U : 'a' + i % 26);
        std::vector<std::uint8_t> const encoded = encode_utf8 (text);

        alignas (8) std::uint8_t raw[128];
        for (std::size_t i = 0; i < sizeof (raw); ++i)
          raw[i] = 0xEE;
        for (std::size_t i = 0; i < encoded.size (); ++i)
          raw[offset + i] = encoded[i];

        recorder_t const result = decode_recording<utf8_decoder> (
            static_cast<std::uint8_t const *> (raw + offset), encoded.size ());
        ASSERT_EQ (result.all, text)
            << "offset " << offset << " position " << position;
      }
}

TEST (UtfDecoderTest,
      GivenAPlainAsciiRunOfEveryLength_WhenUtf8Decode_ThenEveryByteIsReported)
{
  for (std::size_t length = 0; length < 48; ++length)
    {
      alignas (8) std::uint8_t raw[64];
      for (std::size_t i = 0; i < length; ++i)
        raw[i] = static_cast<std::uint8_t> ('A' + i % 26);
      recorder_t const result = decode_recording<utf8_decoder> (
          static_cast<std::uint8_t const *> (raw), length);
      ASSERT_EQ (result.all.size (), length);
      for (std::size_t i = 0; i < length; ++i)
        ASSERT_EQ (result.all[i], static_cast<std::uint32_t> ('A' + i % 26));
    }
}

// ---------------------------------------------------------- UTF-16 decoder

TEST (UtfDecoderTest, GivenBmpUnits_WhenUtf16Decode_ThenEachIsALowCodePoint)
{
  std::vector<std::uint16_t> units;
  units.push_back (0x0041);
  units.push_back (0x20AC);
  units.push_back (0xD7FF);
  units.push_back (0xE000);
  units.push_back (0xFFFF);
  recorder_t const result = decode_recording<utf16_decoder<false>> (units);
  EXPECT_EQ (result.low_calls,
             points ({ 0x41, 0x20AC, 0xD7FF, 0xE000, 0xFFFF }));
  EXPECT_EQ (result.high_calls.size (), 0U);
}

TEST (UtfDecoderTest,
      GivenSurrogatePairs_WhenUtf16Decode_ThenTheyAreOneHighCodePoint)
{
  std::vector<std::uint16_t> units;
  units.push_back (0xD800);
  units.push_back (0xDC00);
  units.push_back (0xD83D);
  units.push_back (0xDE00);
  units.push_back (0xDBFF);
  units.push_back (0xDFFF);
  recorder_t const result = decode_recording<utf16_decoder<false>> (units);
  EXPECT_EQ (result.high_calls, points ({ 0x10000, 0x1F600, 0x10FFFF }));
  EXPECT_EQ (result.low_calls.size (), 0U);
}

TEST (UtfDecoderTest,
      GivenUnpairedSurrogates_WhenUtf16Decode_ThenTheyAreSkipped)
{
  std::vector<std::uint16_t> lone_high_then_bmp;
  lone_high_then_bmp.push_back (0xD800);
  lone_high_then_bmp.push_back (0x0041);
  EXPECT_EQ (decode_recording<utf16_decoder<false>> (lone_high_then_bmp).all,
             points ({ 0x41 }));

  std::vector<std::uint16_t> lone_high_at_end;
  lone_high_at_end.push_back (0x0041);
  lone_high_at_end.push_back (0xD83D);
  EXPECT_EQ (decode_recording<utf16_decoder<false>> (lone_high_at_end).all,
             points ({ 0x41 }));

  std::vector<std::uint16_t> lone_low;
  lone_low.push_back (0xDC00);
  lone_low.push_back (0x0042);
  lone_low.push_back (0xDFFF);
  EXPECT_EQ (decode_recording<utf16_decoder<false>> (lone_low).all,
             points ({ 0x42 }));

  std::vector<std::uint16_t> high_high_low;
  high_high_low.push_back (0xD800);
  high_high_low.push_back (0xD800);
  high_high_low.push_back (0xDC00);
  EXPECT_EQ (decode_recording<utf16_decoder<false>> (high_high_low).all,
             points ({ 0x10000 }));
}

TEST (
    UtfDecoderTest,
    GivenAHighSurrogateBeforeANonLowSurrogate_WhenUtf16Decode_ThenNoPairIsFormed)
{
  struct case_t
  {
    std::uint16_t first;
    std::uint16_t second;
    std::vector<std::uint32_t> expected;
  };
  std::vector<case_t> cases;
  cases.push_back ({ 0xD800, 0xE000, points ({ 0xE000 }) });
  cases.push_back ({ 0xD800, 0xE3FF, points ({ 0xE3FF }) });
  cases.push_back ({ 0xD800, 0xFFFF, points ({ 0xFFFF }) });
  cases.push_back ({ 0xD800, 0xD7FF, points ({ 0xD7FF }) });
  cases.push_back ({ 0xD800, 0xDBFF, points ({}) });
  cases.push_back ({ 0xD800, 0xDFFF, points ({ 0x103FF }) });
  cases.push_back ({ 0xDBFF, 0xDC00, points ({ 0x10FC00 }) });
  for (std::size_t i = 0; i < cases.size (); ++i)
    {
      std::vector<std::uint16_t> units;
      units.push_back (cases[i].first);
      units.push_back (cases[i].second);
      EXPECT_EQ (decode_recording<utf16_decoder<false>> (units).all,
                 cases[i].expected)
          << std::hex << cases[i].first << " " << cases[i].second;
    }
}

TEST (UtfDecoderTest,
      GivenSwappedUnits_WhenUtf16DecodeWithSwapBytes_ThenTheResultIsTheSame)
{
  sequence generator (0x1234567);
  std::vector<std::uint32_t> const text = generator.code_points (200);
  std::vector<std::uint16_t> const units = encode_utf16 (text);
  recorder_t const plain = decode_recording<utf16_decoder<false>> (units);
  recorder_t const reversed
      = decode_recording<utf16_decoder<true>> (swapped (units));
  EXPECT_EQ (plain.all, text);
  EXPECT_EQ (reversed.all, text);
  EXPECT_EQ (reversed.high_calls, plain.high_calls);
}

TEST (UtfDecoderTest, GivenNoInput_WhenUtf16Decode_ThenTheStartValueIsReturned)
{
  EXPECT_EQ (
      utf16_decoder<false>::process (
          static_cast<std::uint16_t const *> (nullptr), 0, 3, utf8_counter ()),
      3U);
  EXPECT_EQ (
      utf16_decoder<true>::process (
          static_cast<std::uint16_t const *> (nullptr), 0, 3, utf8_counter ()),
      3U);
}

// ---------------------------------------------------------- UTF-32 decoder

TEST (UtfDecoderTest,
      GivenUnits_WhenUtf32Decode_ThenTheSplitIsAtTenThousandHex)
{
  std::vector<std::uint32_t> units;
  units.push_back (0x41);
  units.push_back (0xFFFF);
  units.push_back (0x10000);
  units.push_back (0x10FFFF);
  recorder_t const result = decode_recording<utf32_decoder<false>> (units);
  EXPECT_EQ (result.low_calls, points ({ 0x41, 0xFFFF }));
  EXPECT_EQ (result.high_calls, points ({ 0x10000, 0x10FFFF }));
}

TEST (UtfDecoderTest,
      GivenAValueAboveTheUnicodeRange_WhenUtf32Decode_ThenItIsReportedAsIs)
{
  // Documented: the UTF-32 decoder does not validate.
  std::vector<std::uint32_t> units;
  units.push_back (0x110000);
  units.push_back (0xD800);
  recorder_t const result = decode_recording<utf32_decoder<false>> (units);
  EXPECT_EQ (result.high_calls, points ({ 0x110000 }));
  EXPECT_EQ (result.low_calls, points ({ 0xD800 }));
}

TEST (UtfDecoderTest,
      GivenSwappedUnits_WhenUtf32DecodeWithSwapBytes_ThenTheResultIsTheSame)
{
  sequence generator (0x7654321);
  std::vector<std::uint32_t> const text = generator.code_points (200);
  recorder_t const reversed
      = decode_recording<utf32_decoder<true>> (swapped (text));
  EXPECT_EQ (reversed.all, text);
}

// ---------------------------------------------------------- Latin-1 decoder

TEST (UtfDecoderTest,
      GivenEveryByte_WhenLatin1Decode_ThenTheByteIsTheCodePoint)
{
  std::vector<std::uint8_t> input;
  std::vector<std::uint32_t> expected;
  for (unsigned int value = 0; value < 256; ++value)
    {
      input.push_back (static_cast<std::uint8_t> (value));
      expected.push_back (value);
    }
  recorder_t const result = decode_recording<latin1_decoder> (input);
  EXPECT_EQ (result.low_calls, expected);
  EXPECT_EQ (result.high_calls.size (), 0U);
}

// ------------------------------------------------- counter and writer pairs

TEST (UtfConversionTest,
      GivenRandomText_WhenUtf8ToUtf16_ThenTheCounterSizesTheWriterOutput)
{
  sequence generator (0xA11CE);
  for (int round = 0; round < 50; ++round)
    {
      std::vector<std::uint32_t> const text = generator.code_points (80);
      std::vector<std::uint16_t> const out
          = convert<utf8_decoder, utf16_counter, utf16_writer, std::uint16_t> (
              encode_utf8 (text));
      EXPECT_EQ (out, encode_utf16 (text)) << "round " << round;
    }
}

TEST (UtfConversionTest,
      GivenRandomText_WhenUtf16ToUtf8_ThenTheBytesAreTheReferenceEncoding)
{
  sequence generator (0xB0B);
  for (int round = 0; round < 50; ++round)
    {
      std::vector<std::uint32_t> const text = generator.code_points (80);
      std::vector<std::uint8_t> const out
          = convert<utf16_decoder<false>, utf8_counter, utf8_writer,
                    std::uint8_t> (encode_utf16 (text));
      EXPECT_EQ (out, encode_utf8 (text)) << "round " << round;
    }
}

TEST (UtfConversionTest,
      GivenRandomText_WhenUtf8ToUtf32AndBack_ThenTheTextSurvives)
{
  sequence generator (0xC0FFEE);
  for (int round = 0; round < 50; ++round)
    {
      std::vector<std::uint32_t> const text = generator.code_points (80);
      std::vector<std::uint32_t> const wide
          = convert<utf8_decoder, utf32_counter, utf32_writer, std::uint32_t> (
              encode_utf8 (text));
      EXPECT_EQ (wide, text) << "round " << round;
      std::vector<std::uint8_t> const back
          = convert<utf32_decoder<false>, utf8_counter, utf8_writer,
                    std::uint8_t> (wide);
      EXPECT_EQ (back, encode_utf8 (text)) << "round " << round;
    }
}

TEST (UtfConversionTest,
      GivenRandomText_WhenUtf16ToUtf32AndBack_ThenTheTextSurvives)
{
  sequence generator (0xD00D);
  for (int round = 0; round < 50; ++round)
    {
      std::vector<std::uint32_t> const text = generator.code_points (80);
      std::vector<std::uint16_t> const units = encode_utf16 (text);
      std::vector<std::uint32_t> const wide
          = convert<utf16_decoder<false>, utf32_counter, utf32_writer,
                    std::uint32_t> (units);
      EXPECT_EQ (wide, text) << "round " << round;
      std::vector<std::uint16_t> const back
          = convert<utf32_decoder<false>, utf16_counter, utf16_writer,
                    std::uint16_t> (wide);
      EXPECT_EQ (back, units) << "round " << round;
    }
}

TEST (
    UtfConversionTest,
    GivenUtf16OfTheOtherByteOrder_WhenConvertedWithSwapBytes_ThenTheBytesAreTheReferenceEncoding)
{
  sequence generator (0xE4E4);
  std::vector<std::uint32_t> const text = generator.code_points (300);
  std::vector<std::uint8_t> const out
      = convert<utf16_decoder<true>, utf8_counter, utf8_writer, std::uint8_t> (
          swapped (encode_utf16 (text)));
  EXPECT_EQ (out, encode_utf8 (text));
}

TEST (
    UtfConversionTest,
    GivenUtf8Text_WhenConvertedToLatin1_ThenLatin1CharactersSurviveAndOthersBecomeQuestionMarks)
{
  std::vector<std::uint32_t> const text
      = points ({ 'c', 'a', 'f', 0xE9, 0x20AC, 0x1F600, 0xFF, 0x100, 'z' });
  // Latin-1 has one byte per code point, which utf32_counter counts.
  std::vector<std::uint8_t> const out
      = convert<utf8_decoder, utf32_counter, latin1_writer, std::uint8_t> (
          encode_utf8 (text));
  EXPECT_EQ (out, bytes ({ 'c', 'a', 'f', 0xE9, '?', '?', 0xFF, '?', 'z' }));
}

TEST (UtfConversionTest,
      GivenLatin1Bytes_WhenConvertedToUtf8_ThenHighBytesTakeTwoBytes)
{
  std::vector<std::uint8_t> const latin1 = bytes ({ 'c', 0xE9, 0x80, 0xFF });
  std::vector<std::uint8_t> const out
      = convert<latin1_decoder, utf8_counter, utf8_writer, std::uint8_t> (
          latin1);
  EXPECT_EQ (out, bytes ({ 'c', 0xC3, 0xA9, 0xC2, 0x80, 0xC3, 0xBF }));
}

// ------------------------------------------------------------------ wchar_t

TEST (UtfWcharTest, GivenTheSelectors_WhenRead_ThenTheyMatchTheUnitWidth)
{
  static_assert (std::is_same<wchar_selector<2>::type, std::uint16_t>::value,
                 "2-byte wchar_t is UTF-16");
  static_assert (std::is_same<wchar_selector<4>::type, std::uint32_t>::value,
                 "4-byte wchar_t is UTF-32");
  static_assert (
      std::is_same<wchar_selector<2>::counter, utf16_counter>::value,
      "2-byte counter");
  static_assert (
      std::is_same<wchar_selector<4>::counter, utf32_counter>::value,
      "4-byte counter");
  static_assert (std::is_same<wchar_selector<2>::writer, utf16_writer>::value,
                 "2-byte writer");
  static_assert (std::is_same<wchar_selector<4>::writer, utf32_writer>::value,
                 "4-byte writer");
  static_assert (
      std::is_same<wchar_selector<2>::decoder, utf16_decoder<false>>::value,
      "2-byte decoder");
  static_assert (
      std::is_same<wchar_selector<4>::decoder, utf32_decoder<false>>::value,
      "4-byte decoder");
  SUCCEED ();
}

TEST (UtfWcharTest,
      GivenThePlatformWchar_WhenAliasesRead_ThenTheyFollowTheSelector)
{
  static_assert (
      std::is_same<wchar_counter,
                   wchar_selector<sizeof (wchar_t)>::counter>::value,
      "wchar_counter follows sizeof (wchar_t)");
  static_assert (std::is_same<wchar_writer,
                              wchar_selector<sizeof (wchar_t)>::writer>::value,
                 "wchar_writer follows sizeof (wchar_t)");
  static_assert (std::is_same<wchar_decoder::type, wchar_t>::value,
                 "wchar_decoder reads wchar_t");
  SUCCEED ();
}

TEST (UtfWcharTest,
      GivenWcharTextOfThePlatform_WhenDecoded_ThenTheCodePointsComeBack)
{
  std::vector<std::uint32_t> const text
      = points ({ 'A', 0xE9, 0x20AC, 0x1F600, 0x10FFFF });
  std::vector<wchar_t> input;
  if (sizeof (wchar_t) == 2)
    {
      std::vector<std::uint16_t> const units = encode_utf16 (text);
      for (std::size_t i = 0; i < units.size (); ++i)
        input.push_back (static_cast<wchar_t> (units[i]));
    }
  else
    {
      for (std::size_t i = 0; i < text.size (); ++i)
        input.push_back (static_cast<wchar_t> (text[i]));
    }
  EXPECT_EQ (decode_recording<wchar_decoder> (input).all, text);
}

TEST (
    UtfWcharTest,
    GivenWcharTextOfThePlatform_WhenConvertedToUtf8_ThenTheBytesAreTheReferenceEncoding)
{
  std::vector<std::uint32_t> const text
      = points ({ 'A', 0xE9, 0x20AC, 0x1F600, 0x10FFFF });
  std::vector<wchar_t> input;
  if (sizeof (wchar_t) == 2)
    {
      std::vector<std::uint16_t> const units = encode_utf16 (text);
      for (std::size_t i = 0; i < units.size (); ++i)
        input.push_back (static_cast<wchar_t> (units[i]));
    }
  else
    {
      for (std::size_t i = 0; i < text.size (); ++i)
        input.push_back (static_cast<wchar_t> (text[i]));
    }
  std::vector<std::uint8_t> const out
      = convert<wchar_decoder, utf8_counter, utf8_writer, std::uint8_t> (
          input);
  EXPECT_EQ (out, encode_utf8 (text));
}

TEST (UtfWcharTest,
      GivenUtf8Text_WhenConvertedToWchar_ThenTheUnitsFollowTheWidth)
{
  typedef wchar_selector<sizeof (wchar_t)>::type unit_t;
  std::vector<std::uint32_t> const text
      = points ({ 'A', 0xE9, 0x20AC, 0x1F600 });
  std::vector<unit_t> const out
      = convert<utf8_decoder, wchar_counter, wchar_writer, unit_t> (
          encode_utf8 (text));
  std::vector<unit_t> expected;
  if (sizeof (wchar_t) == 2)
    {
      std::vector<std::uint16_t> const units = encode_utf16 (text);
      for (std::size_t i = 0; i < units.size (); ++i)
        expected.push_back (static_cast<unit_t> (units[i]));
    }
  else
    {
      for (std::size_t i = 0; i < text.size (); ++i)
        expected.push_back (static_cast<unit_t> (text[i]));
    }
  EXPECT_EQ (out, expected);
}

TEST (UtfWcharTest,
      GivenBothSelectorsOnAnyHost_WhenDecoding_ThenEachWidthReadsItsOwnUnits)
{
  // Windows has 2-byte wchar_t and Linux 4-byte; both decoders are exercised
  // here on explicit unit types, whatever this host's wchar_t is.
  std::vector<std::uint32_t> const text = points ({ 'A', 0x20AC, 0x1F600 });
  std::vector<std::uint16_t> const units16 = encode_utf16 (text);
  EXPECT_EQ (decode_recording<wchar_selector<2>::decoder> (units16).all, text);
  EXPECT_EQ (decode_recording<wchar_selector<4>::decoder> (text).all, text);
  EXPECT_EQ (units16.size (), 4U);
}
