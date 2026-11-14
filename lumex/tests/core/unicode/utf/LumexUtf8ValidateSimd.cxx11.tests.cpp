// lumex/tests/core/unicode/utf/LumexUtf8ValidateSimd.cxx11.tests.cpp
//
// The vector routines behind utf8_valid_prefix_length
// (utf/LumexUtf8Validate.hpp): SSSE3 and AVX2 on x86 (chosen by CPUID), NEON
// on AArch64. Each routine is called directly, when this machine can run it
// (GTEST_SKIP otherwise), and must give exactly the result of the scalar
// routine, the length of the valid prefix, not only the valid / invalid
// answer. The comparisons run over
//  - every string of up to three bytes (four over a smaller alphabet) made of
//    the bytes at the edges of the ranges of Table 3-7, at every offset of a
//    window of 64, 100 and 128 bytes, so each position relative to the
//    blocks of 16 and 32 bytes and the groups of 64 is hit;
//  - every ill-formed pattern and every valid multi-byte character at every
//    offset of a window, over ASCII and over multi-byte filler;
//  - every cut of a text at every size, so a sequence left open by the end of
//    the buffer meets every tail length;
//  - long pseudo-random valid and corrupted texts of every kind at every
//    alignment of the start.
// A buffer is allocated with exactly its size, so a read past the end shows
// under AddressSanitizer. A routine also reports where it stops (the offset
// from which the scalar routine takes over): for a valid text that is the end
// of the last whole block, so a vector routine that reports an error for a
// well-formed text (which the result alone cannot show, since the scalar
// routine then finishes the job) is caught too. The dispatch is tested apart:
// which variant a CPU gets, and the macro that forces the scalar routine.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/unicode/utf/LumexUtf8Validate.hpp"

namespace
{
namespace detail = lumex::core::unicode::utf::detail;

using detail::utf8_variant;
using routine_function = std::size_t (*) (std::uint8_t const *, std::size_t);

// The window sizes and the bytes of the sweeps.
std::size_t const kBlockGroup = 64;
std::size_t const kOpenBytes = 3;

struct routine
{
  utf8_variant variant;
  char const *name;
  routine_function prefix; // null when this build has no such routine
  routine_function stop;   // where the vector part ends, null for scalar
  std::size_t block;       // the bytes of one block of the routine
  bool available;          // this machine can run it
};

routine
routine_of (utf8_variant variant)
{
  routine r = { variant, "scalar", &detail::utf8_valid_prefix_length_scalar,
                nullptr, 1,        true };
#if LUMEX_UNICODE_UTF8_VALIDATE_SIMD_X86
  if (variant == utf8_variant::ssse3)
    {
      r.name = "ssse3";
      r.prefix = &detail::utf8_valid_prefix_length_ssse3;
      r.stop = &detail::utf8_scan_ssse3;
      r.block = 16;
      r.available = detail::utf8_cpu_has_ssse3 ();
    }
  if (variant == utf8_variant::avx2)
    {
      r.name = "avx2";
      r.prefix = &detail::utf8_valid_prefix_length_avx2;
      r.stop = &detail::utf8_scan_avx2;
      r.block = 32;
      r.available = detail::utf8_cpu_has_avx2 ();
    }
#endif
#if LUMEX_UNICODE_UTF8_VALIDATE_SIMD_NEON
  if (variant == utf8_variant::neon)
    {
      r.name = "neon";
      r.prefix = &detail::utf8_valid_prefix_length_neon;
      r.stop = &detail::utf8_scan_neon;
      r.block = 16;
      r.available = true;
    }
#endif
  if (variant != utf8_variant::scalar
      && r.prefix == &detail::utf8_valid_prefix_length_scalar)
    {
      // not compiled into this build
      r.name = "absent";
      r.prefix = nullptr;
      r.available = false;
    }
  return r;
}

// A buffer of exactly `size` bytes on the heap (nothing behind it for the
// sanitizer to forgive), filled by `fill`.
class exact_buffer
{
public:
  explicit exact_buffer (std::size_t size)
      : size_ (size), bytes_ (new std::uint8_t[size == 0 ? 1 : size])
  {
  }

  std::uint8_t *
  data ()
  {
    return bytes_.get ();
  }

  std::size_t
  size () const
  {
    return size_;
  }

private:
  std::size_t size_;
  std::unique_ptr<std::uint8_t[]> bytes_;
};

std::string
hex_of (std::uint8_t const *data, std::size_t size)
{
  static char const digits[] = "0123456789ABCDEF";
  std::string out;
  for (std::size_t i = 0; i < size && i < 200; ++i)
    {
      out.push_back (digits[data[i] >> 4]);
      out.push_back (digits[data[i] & 15]);
      out.push_back (' ');
    }
  return out;
}

// The routine's result is the scalar routine's, and it stops where it should.
// Returns false after the first failure, so that a sweep does not print
// millions of lines.
bool
agrees (routine const &r, std::uint8_t const *data, std::size_t size)
{
  std::size_t const expected
      = detail::utf8_valid_prefix_length_scalar (data, size);
  std::size_t const got = r.prefix (data, size);
  if (got != expected)
    {
      ADD_FAILURE () << r.name << ": size " << size << ", prefix " << got
                     << " instead of " << expected << ": "
                     << hex_of (data, size);
      return false;
    }
  if (r.stop != nullptr)
    {
      std::size_t const stop = r.stop (data, size);
      bool const stop_ok = expected == size
                               ? stop == size / r.block * r.block
                               : stop <= expected + kOpenBytes
                                     && expected < stop + kBlockGroup;
      if (!stop_ok)
        {
          ADD_FAILURE () << r.name << ": size " << size << ", prefix "
                         << expected << ", stops at " << stop << ": "
                         << hex_of (data, size);
          return false;
        }
    }
  return true;
}

std::vector<std::uint8_t>
bytes_of (char const *text)
{
  std::vector<std::uint8_t> out;
  for (; *text != '\0'; ++text)
    out.push_back (static_cast<std::uint8_t> (*text));
  return out;
}

// Ill-formed sequences of every kind that Table 3-7 rules out.
std::vector<std::vector<std::uint8_t>>
ill_formed ()
{
  char const *const texts[] = { "\xC0\xBC",
                                "\xC1\xBF",
                                "\xE0\x80\xBC",
                                "\xE0\x9F\xBF",
                                "\xF0\x80\x80\xBC",
                                "\xF0\x8F\xBF\xBF",
                                "\xED\xA0\x80",
                                "\xED\xBF\xBF",
                                "\xF4\x90\x80\x80",
                                "\xF5\x80\x80\x80",
                                "\xF8\x88\x80\x80\x80",
                                "\xFE",
                                "\xFF",
                                "\x80",
                                "\xBF\x80",
                                "\xC3",
                                "\xE2\x82",
                                "\xF0\x9F\x98",
                                "\xC3\xC3\xA9",
                                "\xE2\x82\x41",
                                "\xF0\x9F\x41\x80" };
  std::vector<std::vector<std::uint8_t>> out;
  for (std::size_t i = 0; i < sizeof (texts) / sizeof (texts[0]); ++i)
    out.push_back (bytes_of (texts[i]));
  return out;
}

// Valid characters of every length.
std::vector<std::vector<std::uint8_t>>
well_formed ()
{
  char const *const texts[] = { "a",
                                "\xC2\x80",
                                "\xC3\xA9",
                                "\xDF\xBF",
                                "\xE0\xA0\x80",
                                "\xE2\x82\xAC",
                                "\xED\x9F\xBF",
                                "\xEE\x80\x80",
                                "\xEF\xBF\xBD",
                                "\xF0\x90\x80\x80",
                                "\xF0\x9F\x98\x80",
                                "\xF4\x8F\xBF\xBF" };
  std::vector<std::vector<std::uint8_t>> out;
  for (std::size_t i = 0; i < sizeof (texts) / sizeof (texts[0]); ++i)
    out.push_back (bytes_of (texts[i]));
  return out;
}

// Fills `buffer` with `unit` repeated (cut at the end of the buffer).
void
fill_with (exact_buffer &buffer, std::vector<std::uint8_t> const &unit,
           std::size_t phase = 0)
{
  for (std::size_t i = 0; i < buffer.size (); ++i)
    buffer.data ()[i] = unit[(i + phase) % unit.size ()];
}

// Sweeps every string of 1..max_length bytes over `alphabet` through every
// offset of a window of `window` bytes whose other bytes are `filler`.
// Returns false at the first disagreement.
bool
sweep_strings (routine const &r, std::size_t window,
               std::vector<std::uint8_t> const &filler,
               unsigned char const *alphabet, std::size_t letters,
               std::size_t max_length)
{
  exact_buffer buffer (window);
  fill_with (buffer, filler);
  std::vector<std::uint8_t> original (buffer.data (), buffer.data () + window);
  for (std::size_t length = 1; length <= max_length; ++length)
    {
      std::size_t total = 1;
      for (std::size_t k = 0; k < length; ++k)
        total *= letters;
      for (std::size_t n = 0; n < total; ++n)
        for (std::size_t offset = 0; offset + length <= window; ++offset)
          {
            std::size_t rest = n;
            for (std::size_t k = 0; k < length; ++k)
              {
                buffer.data ()[offset + k] = alphabet[rest % letters];
                rest /= letters;
              }
            bool const ok = agrees (r, buffer.data (), window);
            for (std::size_t k = 0; k < length; ++k)
              buffer.data ()[offset + k] = original[offset + k];
            if (!ok)
              return false;
          }
    }
  return true;
}

// The bytes at the edges of every range of Table 3-7 and a few outside.
unsigned char const kEdges[]
    = { 0x00, 0x41, 0x7F, 0x80, 0x8F, 0x90, 0x9F, 0xA0, 0xBF,
        0xC0, 0xC1, 0xC2, 0xDF, 0xE0, 0xE1, 0xEC, 0xED, 0xEE,
        0xEF, 0xF0, 0xF1, 0xF3, 0xF4, 0xF5, 0xF8, 0xFF };
unsigned char const kThreeByteEdges[]
    = { 0x41, 0x80, 0x9F, 0xA0, 0xBF, 0xC2, 0xE0, 0xE1, 0xED, 0xF0, 0xF4 };
unsigned char const kFourByteEdges[]
    = { 0x41, 0x80, 0x8F, 0x90, 0xBF, 0xC3, 0xF0, 0xF4 };

// The pseudo-random generator of the tests.
struct lcg
{
  unsigned int state;

  unsigned int
  next ()
  {
    state = state * 1664525U + 1013904223U;
    return state >> 8;
  }
};

void
append_code_point (std::vector<std::uint8_t> &out, unsigned int cp)
{
  if (cp < 0x80)
    out.push_back (static_cast<std::uint8_t> (cp));
  else if (cp < 0x800)
    {
      out.push_back (static_cast<std::uint8_t> (0xC0 | (cp >> 6)));
      out.push_back (static_cast<std::uint8_t> (0x80 | (cp & 0x3F)));
    }
  else if (cp < 0x10000)
    {
      out.push_back (static_cast<std::uint8_t> (0xE0 | (cp >> 12)));
      out.push_back (static_cast<std::uint8_t> (0x80 | ((cp >> 6) & 0x3F)));
      out.push_back (static_cast<std::uint8_t> (0x80 | (cp & 0x3F)));
    }
  else
    {
      out.push_back (static_cast<std::uint8_t> (0xF0 | (cp >> 18)));
      out.push_back (static_cast<std::uint8_t> (0x80 | ((cp >> 12) & 0x3F)));
      out.push_back (static_cast<std::uint8_t> (0x80 | ((cp >> 6) & 0x3F)));
      out.push_back (static_cast<std::uint8_t> (0x80 | (cp & 0x3F)));
    }
}

// Valid text of one kind (0 ASCII, 1 mostly ASCII with accents, 2 Cyrillic,
// 3 CJK, 4 emoji, 5 a mix of every length) of at least `size` bytes.
std::vector<std::uint8_t>
make_text (lcg &random, int kind, std::size_t size)
{
  std::vector<std::uint8_t> out;
  while (out.size () < size)
    switch (kind)
      {
      case 0:
        out.push_back (static_cast<std::uint8_t> ('a' + random.next () % 26));
        break;
      case 1:
        append_code_point (out, random.next () % 8 == 0
                                    ? 0xE0 + random.next () % 0x20
                                    : 'a' + random.next () % 26);
        break;
      case 2:
        append_code_point (out, 0x410 + random.next () % 0x40);
        break;
      case 3:
        append_code_point (out, 0x4E00 + random.next () % 0x2000);
        break;
      case 4:
        append_code_point (out, 0x1F600 + random.next () % 0x50);
        break;
      default:
        {
          unsigned int const pick = random.next () % 4;
          unsigned int cp = 0x20 + random.next () % 0x5F;
          if (pick == 1)
            cp = 0x80 + random.next () % 0x780;
          else if (pick == 2)
            cp = 0x800 + random.next () % 0xD000; // below the surrogates
          else if (pick == 3)
            cp = 0x10000 + random.next () % 0x100000;
          append_code_point (out, cp);
        }
        break;
      }
  return out;
}

// The routines that the parameterized tests cover.
enum class simd_case : unsigned char
{
  ssse3,
  avx2,
  neon
};

utf8_variant
variant_of (simd_case which)
{
  if (which == simd_case::ssse3)
    return utf8_variant::ssse3;
  if (which == simd_case::avx2)
    return utf8_variant::avx2;
  return utf8_variant::neon;
}

char const *
case_name (simd_case which)
{
  if (which == simd_case::ssse3)
    return "ssse3";
  if (which == simd_case::avx2)
    return "avx2";
  return "neon";
}

void
PrintTo (simd_case which, std::ostream *out)
{
  *out << case_name (which);
}

class Utf8ValidateSimdTest : public ::testing::TestWithParam<simd_case>
{
protected:
  void
  SetUp () override
  {
    r = routine_of (variant_of (GetParam ()));
    if (!r.available)
      GTEST_SKIP () << "this build or this machine has no " << r.name
                    << " routine";
  }

  routine r = routine_of (utf8_variant::scalar);
};

} // namespace

INSTANTIATE_TEST_SUITE_P (Variants, Utf8ValidateSimdTest,
                          ::testing::Values (simd_case::ssse3, simd_case::avx2,
                                             simd_case::neon),
                          [] (::testing::TestParamInfo<simd_case> const &info)
                            { return std::string (case_name (info.param)); });

TEST_P (
    Utf8ValidateSimdTest,
    GivenEveryShortStringAtEveryOffsetOfAWindow_WhenChecked_ThenItIsTheScalarResult)
{
  std::vector<std::uint8_t> const ascii (1, 'a');
  std::vector<std::uint8_t> const accent = bytes_of ("\xC3\xA9");
  std::size_t const letters = sizeof (kEdges);

  // two bytes over the whole alphabet, in windows of 64, 100 and 128 bytes
  // (the block boundaries of 16 and 32 and the group of 64 fall on every
  // offset), over ASCII and over text of two-byte characters
  std::size_t const windows[] = { 64, 100, 128 };
  for (std::size_t w = 0; w < sizeof (windows) / sizeof (windows[0]); ++w)
    {
      ASSERT_TRUE (sweep_strings (r, windows[w], ascii, kEdges, letters, 2))
          << windows[w];
      ASSERT_TRUE (sweep_strings (r, windows[w], accent, kEdges, letters, 2))
          << windows[w];
    }

  // three and four bytes over the smaller alphabets
  ASSERT_TRUE (sweep_strings (r, 128, ascii, kThreeByteEdges,
                              sizeof (kThreeByteEdges), 3));
  ASSERT_TRUE (sweep_strings (r, 128, accent, kThreeByteEdges,
                              sizeof (kThreeByteEdges), 3));
  ASSERT_TRUE (sweep_strings (r, 128, ascii, kFourByteEdges,
                              sizeof (kFourByteEdges), 4));
}

TEST_P (
    Utf8ValidateSimdTest,
    GivenEveryThreeByteStringOverTheWholeAlphabet_WhenChecked_ThenItIsTheScalarResult)
{
  // every string of three bytes over the 26 edge bytes, at the offsets around
  // the first three block boundaries and the end of a window of 70 bytes
  std::vector<std::uint8_t> const ascii (1, 'a');
  std::size_t const letters = sizeof (kEdges);
  exact_buffer buffer (70);
  fill_with (buffer, ascii);
  std::size_t const offsets[]
      = { 0,  1,  13, 14, 15, 16, 17, 29, 30, 31, 32, 33,
          45, 46, 47, 48, 49, 61, 62, 63, 64, 65, 66, 67 };
  for (std::size_t n = 0; n < letters * letters * letters; ++n)
    for (std::size_t o = 0; o < sizeof (offsets) / sizeof (offsets[0]); ++o)
      {
        std::size_t const offset = offsets[o];
        std::size_t rest = n;
        for (std::size_t k = 0; k < 3; ++k)
          {
            buffer.data ()[offset + k] = kEdges[rest % letters];
            rest /= letters;
          }
        bool const ok = agrees (r, buffer.data (), buffer.size ());
        for (std::size_t k = 0; k < 3; ++k)
          buffer.data ()[offset + k] = 'a';
        ASSERT_TRUE (ok);
      }
}

TEST_P (
    Utf8ValidateSimdTest,
    GivenAnIllFormedOrValidSequenceAtEveryOffset_WhenChecked_ThenItIsTheScalarResult)
{
  std::vector<std::vector<std::uint8_t>> const bad = ill_formed ();
  std::vector<std::vector<std::uint8_t>> const good = well_formed ();
  std::vector<std::vector<std::uint8_t>> fillers;
  fillers.push_back (std::vector<std::uint8_t> (1, 'a'));
  fillers.push_back (bytes_of ("\xC3\xA9"));
  fillers.push_back (bytes_of ("\xE2\x82\xAC"));
  fillers.push_back (bytes_of ("\xF0\x9F\x98\x80"));
  fillers.push_back (bytes_of ("a\xC3\xA9"));

  std::size_t const sizes[] = { 64, 66, 97, 130 };
  for (std::size_t f = 0; f < fillers.size (); ++f)
    for (std::size_t s = 0; s < sizeof (sizes) / sizeof (sizes[0]); ++s)
      for (std::size_t phase = 0; phase < fillers[f].size (); ++phase)
        {
          exact_buffer buffer (sizes[s]);
          fill_with (buffer, fillers[f], phase);
          std::vector<std::uint8_t> const original (
              buffer.data (), buffer.data () + buffer.size ());
          for (std::size_t kind = 0; kind < 2; ++kind)
            {
              std::vector<std::vector<std::uint8_t>> const &patterns
                  = kind == 0 ? bad : good;
              for (std::size_t p = 0; p < patterns.size (); ++p)
                for (std::size_t offset = 0;
                     offset + patterns[p].size () <= buffer.size (); ++offset)
                  {
                    for (std::size_t k = 0; k < patterns[p].size (); ++k)
                      buffer.data ()[offset + k] = patterns[p][k];
                    bool const ok = agrees (r, buffer.data (), buffer.size ());
                    for (std::size_t k = 0; k < patterns[p].size (); ++k)
                      buffer.data ()[offset + k] = original[offset + k];
                    ASSERT_TRUE (ok) << "filler " << f << " phase " << phase;
                  }
            }
        }
}

TEST_P (
    Utf8ValidateSimdTest,
    GivenATextCutAtEverySize_WhenChecked_ThenTheOpenSequenceAtTheEndIsTheScalarResult)
{
  // The sequence that the end of the buffer cuts short meets every tail
  // length and every block phase: every prefix of 260 bytes of text made of
  // characters of every length, and of ASCII with a lead byte at the end.
  lcg random = { 7 };
  for (int kind = 1; kind <= 5; ++kind)
    {
      std::vector<std::uint8_t> const text = make_text (random, kind, 300);
      for (std::size_t size = 0; size <= 260; ++size)
        {
          exact_buffer buffer (size);
          for (std::size_t i = 0; i < size; ++i)
            buffer.data ()[i] = text[i];
          ASSERT_TRUE (agrees (r, buffer.data (), size))
              << "kind " << kind << " size " << size;
        }
    }

  std::uint8_t const leads[] = { 0xC3, 0xE2, 0xED, 0xF0, 0xF4, 0xE0 };
  for (std::size_t l = 0; l < sizeof (leads); ++l)
    for (std::size_t size = 1; size <= 200; ++size)
      for (std::size_t have = 0; have <= 3 && have < size; ++have)
        {
          exact_buffer buffer (size);
          for (std::size_t i = 0; i < size; ++i)
            buffer.data ()[i] = 'a';
          buffer.data ()[size - 1 - have] = leads[l];
          for (std::size_t k = 0; k < have; ++k)
            buffer.data ()[size - have + k] = 0x8F;
          ASSERT_TRUE (agrees (r, buffer.data (), size))
              << "lead " << static_cast<unsigned int> (leads[l]) << " size "
              << size << " have " << have;
        }
}

TEST_P (Utf8ValidateSimdTest,
        GivenLongValidText_WhenChecked_ThenTheVectorRoutineReportsNoError)
{
  // A well-formed text of every kind, at every alignment: the routine stops
  // only at the end of its last whole block, never at a false error.
  lcg random = { 99 };
  for (int kind = 0; kind <= 5; ++kind)
    {
      std::vector<std::uint8_t> text = make_text (random, kind, 20000);
      for (std::size_t shift = 0; shift < 64; shift += 5)
        {
          std::size_t const size = text.size ();
          exact_buffer buffer (shift + size);
          for (std::size_t i = 0; i < shift; ++i)
            buffer.data ()[i] = 'p';
          for (std::size_t i = 0; i < size; ++i)
            buffer.data ()[shift + i] = text[i];
          std::uint8_t const *const data = buffer.data () + shift;
          ASSERT_EQ (r.prefix (data, size), size)
              << "kind " << kind << " shift " << shift;
          ASSERT_TRUE (agrees (r, data, size))
              << "kind " << kind << " shift " << shift;
        }
    }
}

TEST_P (
    Utf8ValidateSimdTest,
    GivenRandomCorruptedText_WhenChecked_ThenItIsTheScalarResultAtEveryAlignment)
{
  lcg random = { 2024 };
  for (std::size_t round = 0; round < 6000; ++round)
    {
      int const kind = static_cast<int> (random.next () % 6);
      std::size_t const size = random.next () % 700;
      std::size_t const shift = random.next () % 64;
      std::vector<std::uint8_t> text
          = make_text (random, kind, shift + size + 8);
      exact_buffer buffer (shift + size);
      for (std::size_t i = 0; i < buffer.size (); ++i)
        buffer.data ()[i] = text[i];
      // corrupt none, one or a few bytes
      std::size_t const flips = round % 4;
      for (std::size_t f = 0; f < flips && buffer.size () > 0; ++f)
        buffer.data ()[random.next () % buffer.size ()]
            = static_cast<std::uint8_t> (random.next ());
      ASSERT_TRUE (agrees (r, buffer.data () + shift, size))
          << "round " << round << " kind " << kind << " size " << size
          << " shift " << shift;
    }
}

TEST_P (
    Utf8ValidateSimdTest,
    GivenTheBytesOfADifferentSizeEachTime_WhenChecked_ThenNothingBehindTheBufferIsRead)
{
  // Valid text that fills a buffer of every size from 0 to 200 exactly, with
  // a lead byte or a full character at the end: a read past the end is a
  // sanitizer error, and the result is the scalar one.
  for (std::size_t size = 0; size <= 200; ++size)
    {
      exact_buffer buffer (size);
      for (std::size_t i = 0; i < size; ++i)
        buffer.data ()[i] = (i % 5 == 4) ? 0xA9 : (i % 5 == 3 ? 0xC3 : 'x');
      ASSERT_TRUE (agrees (r, buffer.data (), size)) << size;
    }
}

TEST (Utf8ValidateDispatchTest,
      GivenAnyCpu_WhenTheX86VariantIsChosen_ThenItIsTheBestOneTheCpuHas)
{
#if LUMEX_UNICODE_UTF8_VALIDATE_SIMD_X86
  EXPECT_EQ (detail::utf8_x86_variant_for (false, false),
             utf8_variant::scalar);
  EXPECT_EQ (detail::utf8_x86_variant_for (true, false), utf8_variant::ssse3);
  EXPECT_EQ (detail::utf8_x86_variant_for (true, true), utf8_variant::avx2);
  // AVX2 without SSSE3 does not exist; the choice stays the safe one
  EXPECT_NE (detail::utf8_x86_variant_for (false, true), utf8_variant::avx2);
#else
  GTEST_SKIP () << "this build has no x86 routines";
#endif
}

TEST (
    Utf8ValidateDispatchTest,
    GivenThisMachine_WhenAVariantIsSelected_ThenItIsTheBestOneAndItsRoutineIsCalled)
{
  utf8_variant expected = utf8_variant::scalar;
#if LUMEX_UNICODE_UTF8_VALIDATE_SIMD_X86
  if (detail::utf8_cpu_has_avx2 ())
    expected = utf8_variant::avx2;
  else if (detail::utf8_cpu_has_ssse3 ())
    expected = utf8_variant::ssse3;
#elif LUMEX_UNICODE_UTF8_VALIDATE_SIMD_NEON
  expected = utf8_variant::neon;
#endif
  EXPECT_EQ (detail::utf8_selected_variant (), expected);

  // the routine of a variant is that variant's own, whether this CPU can run
  // it or not
  EXPECT_EQ (detail::utf8_function_of (utf8_variant::scalar),
             &detail::utf8_valid_prefix_length_scalar);
  utf8_variant const compiled[]
      = { utf8_variant::ssse3, utf8_variant::avx2, utf8_variant::neon };
  for (std::size_t i = 0; i < sizeof (compiled) / sizeof (compiled[0]); ++i)
    {
      routine const known = routine_of (compiled[i]);
      EXPECT_EQ (detail::utf8_function_of (compiled[i]),
                 known.prefix != nullptr
                     ? known.prefix
                     : &detail::utf8_valid_prefix_length_scalar)
          << known.name;
    }
  EXPECT_EQ (detail::utf8_function_of (expected)
                 == &detail::utf8_valid_prefix_length_scalar,
             expected == utf8_variant::scalar);

  // the public functions give the scalar result through that routine
  exact_buffer buffer (300);
  for (std::size_t i = 0; i < buffer.size (); ++i)
    buffer.data ()[i] = (i % 7 == 6) ? 0xA9 : (i % 7 == 5 ? 0xC3 : 'q');
  buffer.data ()[200] = 0xFF;
  for (std::size_t size = 0; size <= buffer.size (); ++size)
    EXPECT_EQ (lumex::core::unicode::utf::utf8_valid_prefix_length (
                   buffer.data (), size),
               detail::utf8_valid_prefix_length_scalar (buffer.data (), size))
        << size;
  EXPECT_FALSE (
      lumex::core::unicode::utf::is_valid_utf8 (buffer.data (), 300));
  EXPECT_EQ (lumex::core::unicode::utf::utf8_valid_prefix_length (
                 buffer.data (), 300),
             200U);

#if LUMEX_UNICODE_UTF8_VALIDATE_SIMD_X86                                      \
    || LUMEX_UNICODE_UTF8_VALIDATE_SIMD_NEON
  // the calls above have resolved the routine: the slot holds the one of the
  // selected variant
  EXPECT_EQ (detail::utf8_prefix_slot ().load (),
             detail::utf8_function_of (expected));
#endif
}

TEST (
    Utf8ValidateDispatchTest,
    GivenTheAnswersOfACpu_WhenTheFeaturesAreDecided_ThenAvx2NeedsTheOperatingSystemToo)
{
#if LUMEX_UNICODE_UTF8_VALIDATE_SIMD_X86
  unsigned int const ssse3 = 1U << 9;
  unsigned int const osxsave = 1U << 27;
  unsigned int const avx = 1U << 28;
  unsigned int const avx2 = 1U << 5;
  unsigned int const ymm_state = 6U;
  unsigned int const os_and_avx = osxsave | avx;

  // no leaf 1: nothing
  detail::utf8_x86_features none = detail::utf8_x86_features_from (
      0, ssse3 | os_and_avx, avx2, ymm_state);
  EXPECT_FALSE (none.ssse3);
  EXPECT_FALSE (none.avx2);

  // SSSE3 alone
  detail::utf8_x86_features only_ssse3
      = detail::utf8_x86_features_from (1, ssse3, 0, 0);
  EXPECT_TRUE (only_ssse3.ssse3);
  EXPECT_FALSE (only_ssse3.avx2);

  // everything
  detail::utf8_x86_features both = detail::utf8_x86_features_from (
      7, ssse3 | os_and_avx, avx2, ymm_state);
  EXPECT_TRUE (both.ssse3);
  EXPECT_TRUE (both.avx2);

  // AVX2 in the CPU, and one thing missing: the bit of leaf 7, the leaf, the
  // AVX bit, OSXSAVE, the XMM or the YMM state in XCR0, SSSE3
  EXPECT_FALSE (
      detail::utf8_x86_features_from (7, ssse3 | os_and_avx, 0, ymm_state)
          .avx2);
  EXPECT_FALSE (
      detail::utf8_x86_features_from (6, ssse3 | os_and_avx, avx2, ymm_state)
          .avx2);
  EXPECT_FALSE (
      detail::utf8_x86_features_from (7, ssse3 | osxsave, avx2, ymm_state)
          .avx2);
  EXPECT_FALSE (
      detail::utf8_x86_features_from (7, ssse3 | avx, avx2, ymm_state).avx2);
  EXPECT_FALSE (
      detail::utf8_x86_features_from (7, ssse3 | os_and_avx, avx2, 2U).avx2);
  EXPECT_FALSE (
      detail::utf8_x86_features_from (7, ssse3 | os_and_avx, avx2, 4U).avx2);
  EXPECT_FALSE (
      detail::utf8_x86_features_from (7, os_and_avx, avx2, ymm_state).avx2);

  // this machine: the cached answer is what a new reading says
  EXPECT_EQ (detail::utf8_cpu_has_ssse3 (),
             detail::utf8_detect_x86_features ().ssse3);
  EXPECT_EQ (detail::utf8_cpu_has_avx2 (),
             detail::utf8_detect_x86_features ().avx2);
  EXPECT_TRUE (!detail::utf8_cpu_has_avx2 () || detail::utf8_cpu_has_ssse3 ());
#else
  GTEST_SKIP () << "this build has no x86 routines";
#endif
}

TEST (
    Utf8ValidateDispatchTest,
    GivenAnEmulatedCpu_WhenSelected_ThenTheVariantNamedByTheEnvironmentIsChosen)
{
  // The tests unicode.utf.emulated_cpu.* of lumex/tests/CMakeLists.txt run
  // this binary under qemu-x86_64 -cpu <model> (qemu-i386 for a 32-bit build)
  // with the variant that the model must get (a CPU of this machine cannot
  // show that an AVX2 flag reads the right bit of CPUID).
#if defined(_MSC_VER)
  GTEST_SKIP () << "the emulated CPUs are run on Linux";
#else
  char const *const wanted = std::getenv ("LUMEX_TEST_EXPECT_UTF8_VARIANT");
  if (wanted == nullptr)
    GTEST_SKIP () << "LUMEX_TEST_EXPECT_UTF8_VARIANT is not set";

  std::string const name (wanted);
  utf8_variant const chosen = detail::utf8_selected_variant ();
  std::string chosen_name = "scalar";
  if (chosen == utf8_variant::ssse3)
    chosen_name = "ssse3";
  else if (chosen == utf8_variant::avx2)
    chosen_name = "avx2";
  else if (chosen == utf8_variant::neon)
    chosen_name = "neon";
  EXPECT_EQ (chosen_name, name);
#if LUMEX_UNICODE_UTF8_VALIDATE_SIMD_X86
  EXPECT_EQ (detail::utf8_cpu_has_ssse3 (), name == "ssse3" || name == "avx2");
  EXPECT_EQ (detail::utf8_cpu_has_avx2 (), name == "avx2");
#endif
#endif
}

TEST (Utf8ValidateDispatchTest,
      GivenTheForceScalarMacro_WhenBuilt_ThenOnlyTheScalarRoutineExists)
{
#if defined(LUMEX_UNICODE_UTF8_VALIDATE_NO_SIMD)
  EXPECT_EQ (LUMEX_UNICODE_UTF8_VALIDATE_SIMD_X86, 0);
  EXPECT_EQ (LUMEX_UNICODE_UTF8_VALIDATE_SIMD_NEON, 0);
  EXPECT_EQ (detail::utf8_selected_variant (), utf8_variant::scalar);
  EXPECT_EQ (detail::utf8_function_of (utf8_variant::avx2),
             &detail::utf8_valid_prefix_length_scalar);
#else
  GTEST_SKIP () << "built without LUMEX_UNICODE_UTF8_VALIDATE_NO_SIMD";
#endif
}
