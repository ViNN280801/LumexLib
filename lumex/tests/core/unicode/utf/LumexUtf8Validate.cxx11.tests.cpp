// lumex/tests/core/unicode/utf/LumexUtf8Validate.cxx11.tests.cpp
//
// utf8_valid_prefix_length and is_valid_utf8 of utf/LumexUtf8Validate.hpp.
// They accept exactly the well-formed sequences of Table 3-7 of the Unicode
// standard, as utf8_decoder does, and the length of the prefix is the offset
// of the first byte that the decoder skips. The tests state that in three
// ways: the boundary values of every lead byte written out by hand, a
// reference written from the table (it shares no code with the functions
// under test), and the decoder itself (a buffer is valid exactly when the
// decoder skips nothing, and the prefix is the longest one that it decodes
// without skipping). The comparisons run over every string of up to four
// bytes made of the bytes at the edges of the ranges of the table, over an
// ill-formed sequence at every offset of runs of ASCII around the word size of
// the ASCII fast path, over every alignment of the buffer, and over
// pseudo-random text. Continuation bytes stand behind the buffers, outside
// the size that is passed, so a read past the end shows.
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/unicode/utf/LumexUtf.hpp"
#include "lumex/core/unicode/utf/LumexUtf8Validate.hpp"

using lumex::core::unicode::utf::is_valid_utf8;
using lumex::core::unicode::utf::utf8_counter;
using lumex::core::unicode::utf::utf8_decoder;
using lumex::core::unicode::utf::utf8_valid_prefix_length;

namespace
{
unsigned char
byte_at (std::string const &s, std::size_t i)
{
  return static_cast<unsigned char> (s[i]);
}

bool
byte_in (unsigned char b, unsigned int low, unsigned int high)
{
  return b >= low && b <= high;
}

// Table 3-7: the length of the well-formed sequence that starts at s[i], or 0
// when none does.
std::size_t
table_sequence_length (std::string const &s, std::size_t i)
{
  std::size_t const left = s.size () - i;
  unsigned char const b0 = byte_at (s, i);
  if (b0 <= 0x7F)
    return 1;
  if (byte_in (b0, 0xC2, 0xDF))
    return (left >= 2 && byte_in (byte_at (s, i + 1), 0x80, 0xBF)) ? 2 : 0;
  if (byte_in (b0, 0xE0, 0xEF))
    {
      unsigned int low = 0x80;
      unsigned int high = 0xBF;
      if (b0 == 0xE0)
        low = 0xA0;
      if (b0 == 0xED)
        high = 0x9F;
      return (left >= 3 && byte_in (byte_at (s, i + 1), low, high)
              && byte_in (byte_at (s, i + 2), 0x80, 0xBF))
                 ? 3
                 : 0;
    }
  if (byte_in (b0, 0xF0, 0xF4))
    {
      unsigned int low = 0x80;
      unsigned int high = 0xBF;
      if (b0 == 0xF0)
        low = 0x90;
      if (b0 == 0xF4)
        high = 0x8F;
      return (left >= 4 && byte_in (byte_at (s, i + 1), low, high)
              && byte_in (byte_at (s, i + 2), 0x80, 0xBF)
              && byte_in (byte_at (s, i + 3), 0x80, 0xBF))
                 ? 4
                 : 0;
    }
  return 0;
}

// The offset of the first byte that starts no well-formed sequence.
std::size_t
table_valid_prefix (std::string const &s)
{
  std::size_t i = 0;
  while (i < s.size ())
    {
      std::size_t const length = table_sequence_length (s, i);
      if (length == 0)
        return i;
      i += length;
    }
  return s.size ();
}

std::string
hex_of (std::string const &s)
{
  static char const digits[] = "0123456789ABCDEF";
  std::string out;
  for (std::size_t i = 0; i < s.size (); ++i)
    {
      out.push_back (digits[byte_at (s, i) >> 4]);
      out.push_back (digits[byte_at (s, i) & 15]);
      out.push_back (' ');
    }
  return out;
}

// What the decoder keeps of `s`: the number of bytes of the UTF-8 form of the
// code points it yields. It is the size of `s` when it skips nothing.
std::size_t
decoded_size (std::uint8_t const *data, std::size_t size)
{
  return utf8_decoder::process (data, size, 0, utf8_counter ());
}

// The longest prefix that the decoder passes without skipping a byte. A
// well-formed sequence decodes to as many bytes as it has, and the well-formed
// prefixes of a string are exactly the prefixes cut at the boundary of a
// sequence, so this is the largest length at which the decoder keeps all.
std::size_t
decoder_valid_prefix (std::vector<std::uint8_t> const &bytes, std::size_t size)
{
  std::size_t best = 0;
  for (std::size_t k = 0; k <= size; ++k)
    if (decoded_size (bytes.empty () ? nullptr : &bytes[0], k) == k)
      best = k;
  return best;
}

// Checks the functions on `text`, which stands in a buffer with three
// continuation bytes behind it, at `shift` bytes from the start of the buffer.
void
expect_agreement (std::string const &text, std::size_t shift = 0)
{
  std::vector<std::uint8_t> buffer (shift, 0x41);
  buffer.insert (buffer.end (), text.begin (), text.end ());
  buffer.push_back (0x80);
  buffer.push_back (0x80);
  buffer.push_back (0x80);
  std::uint8_t const *const data = &buffer[shift];
  char const *const chars = reinterpret_cast<char const *> (data);

  std::size_t const expected = table_valid_prefix (text);
  EXPECT_EQ (utf8_valid_prefix_length (data, text.size ()), expected)
      << hex_of (text) << "shift " << shift;
  EXPECT_EQ (utf8_valid_prefix_length (chars, text.size ()), expected)
      << hex_of (text) << "shift " << shift;
  EXPECT_EQ (is_valid_utf8 (data, text.size ()), expected == text.size ())
      << hex_of (text) << "shift " << shift;
  EXPECT_EQ (is_valid_utf8 (chars, text.size ()), expected == text.size ())
      << hex_of (text) << "shift " << shift;

  // the decoder agrees: it skips nothing exactly when the text is valid, and
  // the valid prefix is the longest one it keeps whole
  std::vector<std::uint8_t> bytes (text.begin (), text.end ());
  EXPECT_EQ (decoded_size (bytes.empty () ? nullptr : &bytes[0], bytes.size ())
                 == bytes.size (),
             expected == text.size ())
      << hex_of (text);
  // the exact prefix against the decoder costs a decoding per length: short
  // texts only; for the others, the prefix decodes whole and the rest does not
  if (bytes.size () <= 12)
    EXPECT_EQ (decoder_valid_prefix (bytes, bytes.size ()), expected)
        << hex_of (text);
  else
    {
      EXPECT_EQ (decoded_size (&bytes[0], expected), expected)
          << hex_of (text);
      if (expected < bytes.size ())
        EXPECT_LT (decoded_size (&bytes[expected], bytes.size () - expected),
                   bytes.size () - expected)
            << hex_of (text);
    }
}

// Ill-formed sequences of every kind the table rules out.
std::vector<std::string>
ill_formed_patterns ()
{
  std::vector<std::string> bad;
  bad.push_back (std::string ("\xC0\xBC"));
  bad.push_back (std::string ("\xC1\xBF"));
  bad.push_back (std::string ("\xE0\x80\xBC"));
  bad.push_back (std::string ("\xE0\x9F\xBF"));
  bad.push_back (std::string ("\xF0\x80\x80\xBC"));
  bad.push_back (std::string ("\xF0\x8F\xBF\xBF"));
  bad.push_back (std::string ("\xED\xA0\x80"));
  bad.push_back (std::string ("\xED\xBF\xBF"));
  bad.push_back (std::string ("\xF4\x90\x80\x80"));
  bad.push_back (std::string ("\xF5\x80\x80\x80"));
  bad.push_back (std::string ("\xF8\x88\x80\x80\x80"));
  bad.push_back (std::string ("\xFE"));
  bad.push_back (std::string ("\xFF"));
  bad.push_back (std::string ("\x80"));
  bad.push_back (std::string ("\xBF\x80"));
  bad.push_back (std::string ("\xC3"));
  bad.push_back (std::string ("\xE2\x82"));
  bad.push_back (std::string ("\xF0\x9F\x98"));
  return bad;
}
} // namespace

TEST (Utf8ValidateTest, GivenAnEmptyBuffer_WhenChecked_ThenItIsValid)
{
  EXPECT_EQ (utf8_valid_prefix_length (
                 static_cast<std::uint8_t const *> (nullptr), 0),
             0U);
  EXPECT_EQ (utf8_valid_prefix_length (static_cast<char const *> (nullptr), 0),
             0U);
  EXPECT_TRUE (is_valid_utf8 (static_cast<std::uint8_t const *> (nullptr), 0));
  EXPECT_TRUE (is_valid_utf8 (static_cast<char const *> (nullptr), 0));
  EXPECT_TRUE (is_valid_utf8 ("", 0));
}

TEST (Utf8ValidateTest,
      GivenValidSequencesOfEveryLength_WhenChecked_ThenTheyAreValid)
{
  // The first and the last value of every length, and the edges of the
  // ranges that the table narrows (E0, ED, F0, F4).
  char const *const valid[] = { "A",
                                "\x7F",
                                "\xC2\x80",
                                "\xDF\xBF",
                                "\xE0\xA0\x80",
                                "\xE0\xBF\xBF",
                                "\xE1\x80\x80",
                                "\xEC\xBF\xBF",
                                "\xED\x80\x80",
                                "\xED\x9F\xBF",
                                "\xEE\x80\x80",
                                "\xEF\xBF\xBD",
                                "\xEF\xBF\xBF",
                                "\xF0\x90\x80\x80",
                                "\xF0\xBF\xBF\xBF",
                                "\xF1\x80\x80\x80",
                                "\xF3\xBF\xBF\xBF",
                                "\xF4\x80\x80\x80",
                                "\xF4\x8F\xBF\xBF" };
  for (std::size_t i = 0; i < sizeof (valid) / sizeof (valid[0]); ++i)
    {
      std::string const text (valid[i]);
      EXPECT_TRUE (is_valid_utf8 (text.data (), text.size ()))
          << hex_of (text);
      EXPECT_EQ (utf8_valid_prefix_length (text.data (), text.size ()),
                 text.size ())
          << hex_of (text);
    }
}

TEST (Utf8ValidateTest, GivenAByteZero_WhenChecked_ThenItIsValid)
{
  std::string const text ("a\0b", 3);
  EXPECT_TRUE (is_valid_utf8 (text.data (), text.size ()));
}

TEST (Utf8ValidateTest,
      GivenEachIllFormedSequence_WhenChecked_ThenThePrefixEndsBeforeIt)
{
  std::vector<std::string> const bad = ill_formed_patterns ();
  for (std::size_t i = 0; i < bad.size (); ++i)
    {
      std::string const text = "ab" + bad[i] + "cd";
      EXPECT_EQ (utf8_valid_prefix_length (text.data (), text.size ()), 2U)
          << hex_of (bad[i]);
      EXPECT_FALSE (is_valid_utf8 (text.data (), text.size ()))
          << hex_of (bad[i]);
      EXPECT_EQ (utf8_valid_prefix_length (bad[i].data (), bad[i].size ()), 0U)
          << hex_of (bad[i]);
    }
}

TEST (Utf8ValidateTest,
      GivenASequenceCutShortByTheSize_WhenChecked_ThenItIsNotCounted)
{
  // The bytes are valid as the start of a sequence, and the buffer ends: what
  // stands behind the size is not read.
  std::string const text ("ab\xE2\x82\xAC");
  EXPECT_EQ (utf8_valid_prefix_length (text.data (), 5), 5U);
  EXPECT_EQ (utf8_valid_prefix_length (text.data (), 4), 2U);
  EXPECT_EQ (utf8_valid_prefix_length (text.data (), 3), 2U);
  EXPECT_EQ (utf8_valid_prefix_length (text.data (), 2), 2U);
}

TEST (Utf8ValidateTest,
      GivenEveryShortByteString_WhenChecked_ThenTheTableAndTheDecoderAgree)
{
  // The bytes at the edges of every range of Table 3-7 and a few outside of
  // it, in every string up to four bytes: 475255 strings.
  unsigned char const alphabet[]
      = { 0x00, 0x41, 0x7F, 0x80, 0x8F, 0x90, 0x9F, 0xA0, 0xBF,
          0xC0, 0xC1, 0xC2, 0xDF, 0xE0, 0xE1, 0xEC, 0xED, 0xEE,
          0xEF, 0xF0, 0xF1, 0xF3, 0xF4, 0xF5, 0xF8, 0xFF };
  std::size_t const letters = sizeof (alphabet) / sizeof (alphabet[0]);
  std::size_t checked = 0;
  for (std::size_t length = 0; length <= 4; ++length)
    {
      std::size_t total = 1;
      for (std::size_t k = 0; k < length; ++k)
        total *= letters;
      for (std::size_t n = 0; n < total; ++n)
        {
          std::string text;
          std::size_t rest = n;
          for (std::size_t k = 0; k < length; ++k)
            {
              text.push_back (static_cast<char> (alphabet[rest % letters]));
              rest /= letters;
            }
          expect_agreement (text);
          ++checked;
        }
    }
  EXPECT_EQ (checked, 475255U);
}

TEST (
    Utf8ValidateTest,
    GivenAnIllFormedSequenceAtEveryPlaceOfAnAsciiRun_WhenChecked_ThenTheTableAndTheDecoderAgree)
{
  // The ASCII fast path reads eight bytes at a time: put each pattern at
  // every offset of runs of every length around the word size, in the middle
  // and at the end, and a valid character instead of it.
  std::vector<std::string> const bad = ill_formed_patterns ();
  for (std::size_t i = 0; i < bad.size (); ++i)
    for (std::size_t before = 0; before <= 20; ++before)
      for (std::size_t after = 0; after <= 20; ++after)
        {
          expect_agreement (std::string (before, 'a') + bad[i]
                            + std::string (after, 'b'));
          expect_agreement (std::string (before, 'a') + "\xE2\x82\xAC"
                            + std::string (after, 'b'));
        }
}

TEST (Utf8ValidateTest,
      GivenAnyAlignmentOfTheBuffer_WhenChecked_ThenTheResultIsTheSame)
{
  std::vector<std::string> texts = ill_formed_patterns ();
  texts.push_back (std::string ("plain ascii text of some length, no more"));
  texts.push_back (
      std::string ("caf\xC3\xA9 \xE2\x82\xAC \xF0\x9F\x98\x80 end"));
  for (std::size_t i = 0; i < texts.size (); ++i)
    for (std::size_t shift = 0; shift < 8; ++shift)
      {
        expect_agreement (texts[i], shift);
        expect_agreement (
            std::string (13, 'x') + texts[i] + std::string (9, 'y'), shift);
      }
}

TEST (Utf8ValidateTest,
      GivenPseudoRandomText_WhenChecked_ThenTheTableAndTheDecoderAgree)
{
  // Mostly ASCII and valid sequences, an invalid piece now and then.
  unsigned int seed = 12345;
  std::string const valid[] = { "a",
                                "b",
                                " ",
                                "\xC3\xA9",
                                "\xDF\xBF",
                                "\xE2\x82\xAC",
                                "\xED\x9F\xBF",
                                "\xEE\x80\x80",
                                "\xEF\xBF\xBD",
                                "\xF0\x90\x80\x80",
                                "\xF4\x8F\xBF\xBF" };
  std::vector<std::string> const invalid = ill_formed_patterns ();
  for (std::size_t round = 0; round < 3000; ++round)
    {
      std::string text;
      std::size_t const pieces = 1 + round % 90;
      for (std::size_t k = 0; k < pieces; ++k)
        {
          seed = seed * 1664525U + 1013904223U;
          unsigned int const pick = (seed >> 8) % 100;
          if (pick < 3)
            text += invalid[(seed >> 16) % invalid.size ()];
          else
            text += valid[(seed >> 16) % (sizeof (valid) / sizeof (valid[0]))];
        }
      expect_agreement (text);
    }
}
