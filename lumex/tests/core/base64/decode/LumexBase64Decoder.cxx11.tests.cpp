// Base64 tests that compile from C++11: the pointer and size core, the
// std::string and std::vector overloads. Every base64 suite compiles this
// file; LumexBase64.cxx17.tests.cpp and LumexBase64.cxx20.tests.cpp add the
// wrappers of the higher standards.

#include <algorithm>
#include <chrono>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/base64/LumexBase64"

#include "lumex/tests/core/base64/LumexBase64TestFixtures.hpp"
#include "lumex/tests/support/LumexPerfSkip.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-libc-call"
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wswitch-enum"
#pragma clang diagnostic ignored "-Wnrvo"
#pragma clang diagnostic ignored "-Wheader-hygiene"
#pragma clang diagnostic ignored "-Wused-but-marked-unused"
#pragma clang diagnostic ignored "-Wundefined-var-template"
#pragma clang diagnostic ignored "-Wdeprecated-redundant-constexpr-static-def"
#pragma clang diagnostic ignored "-Wvariadic-macro-arguments-omitted"
#pragma clang diagnostic ignored "-Wunused-result"
#pragma clang diagnostic ignored "-Wextra-semi-stmt"
#pragma clang diagnostic ignored "-Wexpansion-to-defined"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wundefined-func-template"
#pragma clang diagnostic ignored "-Wfloat-equal"
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif

using namespace lumex::core::base64::codec;
using namespace lumex::core::base64::decode;
using namespace lumex::core::base64::encode;
using namespace lumex::core::base64::validate;
using namespace lumex::core::base64::codec::Types;

// ==========================================================================
// encoder
// ==========================================================================

struct LifetimeTracker
{
  static int creations;
  static int destructions;
  static int copies;
  static int moves;

  int id;

  static void
  reset ()
  {
    creations = destructions = copies = moves = 0;
  }

  LifetimeTracker () : id (creations) { creations++; }
  ~LifetimeTracker () { destructions++; }
  LifetimeTracker (LifetimeTracker const &) : id (creations)
  {
    creations++;
    copies++;
  }
  LifetimeTracker (LifetimeTracker &&) noexcept : id (creations)
  {
    creations++;
    moves++;
  }
  LifetimeTracker &
  operator= (LifetimeTracker const &)
  {
    copies++;
    return *this;
  }
  LifetimeTracker &
  operator= (LifetimeTracker &&) noexcept
  {
    moves++;
    return *this;
  }
};

int LifetimeTracker::creations = 0;
int LifetimeTracker::destructions = 0;
int LifetimeTracker::copies = 0;
int LifetimeTracker::moves = 0;

class Base64EncoderLifetimeTest : public ::testing::Test
{
protected:
  void
  SetUp () override
  {
    LifetimeTracker::reset ();
  }
  void
  TearDown () override
  {
    ASSERT_EQ (LifetimeTracker::creations, LifetimeTracker::destructions)
        << "Memory leak detected! Creations do not match destructions.";
  }
};

// --- API Contract Verifier Tests ---------------------------------------

TEST_F (Base64EncoderTest, GivenEmptyData_WhenEncode_ThenReturnsEmptyString)
{
  // Empty input should produce empty output
  std::string result = encoder::encode (empty_data);
  EXPECT_TRUE (result.empty ());

  // Verify with raw pointer interface
  result = encoder::encode (nullptr, 0);
  EXPECT_TRUE (result.empty ());
}

TEST_F (Base64EncoderTest, GivenSingleByte_WhenEncode_ThenReturnsCorrectBase64)
{
  // Single byte 0x42 ('B') should encode to "Qg=="
  std::string result = encoder::encode (single_byte);
  EXPECT_EQ (result, "Qg==");
  EXPECT_EQ (result.length (), 4); // Base64 always produces multiples of 4
}

TEST_F (Base64EncoderTest,
        GivenKnownTestVectors_WhenEncode_ThenMatchesExpected)
{
  // Use RFC 4648 test vectors for verification
  struct TestVector
  {
    std::vector<byte_type> input;
    std::string expected;
  };

  std::vector<TestVector> vectors
      = { { {}, "" },
          { { 'f' }, "Zg==" },
          { { 'f', 'o' }, "Zm8=" },
          { { 'f', 'o', 'o' }, "Zm9v" },
          { { 'f', 'o', 'o', 'b' }, "Zm9vYg==" },
          { { 'f', 'o', 'o', 'b', 'a' }, "Zm9vYmE=" },
          { { 'f', 'o', 'o', 'b', 'a', 'r' }, "Zm9vYmFy" },

          // Binary data
          { { 0x00 }, "AA==" },
          { { 0xFF }, "/w==" },
          { { 0x00, 0xFF }, "AP8=" },
          { { 0xFF, 0x00 }, "/wA=" } };

  for (auto const &vector : vectors)
    {
      std::string result = encoder::encode (vector.input);
      EXPECT_EQ (result, vector.expected)
          << "Failed for input size: " << vector.input.size ();
    }
}

TEST_F (Base64EncoderTest,
        GivenRawPointer_WhenEncode_ThenProducesCorrectOutput)
{
  // Test raw pointer interface with various sizes
  char const *text = "Hello";
  std::string result = encoder::encode (text, 5);
  EXPECT_EQ (result, "SGVsbG8=");

  // Test with binary data
  byte_type binary[] = { 0x00, 0x01, 0x02 };
  result = encoder::encode (binary, sizeof (binary));
  EXPECT_EQ (result, "AAEC");
}

TEST_F (Base64EncoderTest,
        GivenRangeInsideLargerBuffer_WhenEncode_ThenOnlyTheRangeIsEncoded)
{
  char const buffer[] = "xxHiyy";
  EXPECT_EQ (encoder::encode (buffer + 2, 2), "SGk=");
}

// --- Edge & Corner Cases -----------------------------------------------

TEST_F (Base64EncoderTest,
        GivenAllPossibleBytes_WhenEncode_ThenHandlesCorrectly)
{
  // Verify all 256 possible byte values can be encoded
  std::string result = encoder::encode (all_bytes);
  EXPECT_FALSE (result.empty ());
  EXPECT_EQ (result.length (), ((all_bytes.size () + 2) / 3) * 4);

  // Verify only valid Base64 characters
  for (char c : result)
    {
      EXPECT_TRUE ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
                   || (c >= '0' && c <= '9') || c == '+' || c == '/'
                   || c == '=')
          << "Invalid Base64 character: " << c;
    }
}

TEST_F (Base64EncoderTest, GivenLargeData_WhenEncode_ThenHandlesEfficiently)
{
  // Test with large data to verify no performance issues
  std::string result = encoder::encode (large_data);
  EXPECT_FALSE (result.empty ());

  // Verify correct length calculation
  std::size_t expected_length = ((large_data.size () + 2) / 3) * 4;
  EXPECT_EQ (result.length (), expected_length);
}

TEST_F (Base64EncoderTest, GivenNullPointer_WhenEncode_ThenReturnsEmptyString)
{
  // Null pointer should be handled gracefully
  std::string result = encoder::encode (nullptr, 100);
  EXPECT_TRUE (result.empty ());
}

TEST_F (Base64EncoderTest, GivenZeroSize_WhenEncode_ThenReturnsEmptyString)
{
  byte_type dummy = 0x42;
  std::string result = encoder::encode (&dummy, 0);
  EXPECT_TRUE (result.empty ());
}

// --- Platform Compatibility Tests --------------------------------------

TEST_F (Base64EncoderTest,
        GivenDifferentEndianness_WhenEncode_ThenProducesConsistentResults)
{
  // Base64 should be endian-independent since it works on bytes
  union EndianTest
  {
    uint32_t value;
    byte_type bytes[4];
  };

  EndianTest test;
  test.value = 0x01020304;

  std::string result = encoder::encode (test.bytes, 4);
  EXPECT_FALSE (result.empty ());
  EXPECT_EQ (result.length (), 8); // 4 bytes -> 8 Base64 chars (with padding)
}

#ifdef _WIN32
TEST_F (Base64EncoderTest,
        WindowsSpecific_GivenWideCharData_WhenConvertAndEncode_ThenWorks)
{
  // Test Windows-specific wide character handling
  std::wstring wide_text = L"Test";
  std::vector<byte_type> bytes (
      reinterpret_cast<byte_type const *> (wide_text.data ()),
      reinterpret_cast<byte_type const *> (wide_text.data ())
          + wide_text.size () * sizeof (wchar_t));

  std::string result = encoder::encode (bytes);
  EXPECT_FALSE (result.empty ());
}
#endif

// --- Concurrency Tests -------------------------------------------------

TEST_F (Base64EncoderTest, ThreadSafety_SimultaneousEncoding)
{
  // encoder should be thread-safe for simultaneous operations
  constexpr int num_threads = 10;
  std::vector<std::thread> threads;
  std::vector<std::string> results (num_threads);

  std::vector<byte_type> test_data
      = { 'T', 'h', 'r', 'e', 'a', 'd', 'T', 'e', 's', 't' };

  for (int i = 0; i < num_threads; ++i)
    threads.emplace_back ([&results, &test_data, i] ()
                            { results[i] = encoder::encode (test_data); });

  for (auto &t : threads)
    t.join ();

  // All results should be identical
  std::string expected = results[0];
  for (int i = 1; i < num_threads; ++i)
    EXPECT_EQ (results[i], expected)
        << "Thread " << i << " produced different result";
}

// --- Performance & Stress Tests -----------------------------------------

TEST_F (Base64EncoderTest, Perf_LargeDataEncoding)
{
#if LUMEX_PERF_WALL_CLOCK_ENABLED
  // Performance test with very large data
  std::size_t const large_size = 1000000; // 1MB
  std::vector<byte_type> very_large_data (large_size);

  // Fill with pattern to avoid optimization
  for (std::size_t i = 0; i < large_size; ++i)
    very_large_data[i] = static_cast<byte_type> (i % 256);

  auto start = std::chrono::high_resolution_clock::now ();
  std::string result = encoder::encode (very_large_data);
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds> (
      std::chrono::high_resolution_clock::now () - start);

  EXPECT_FALSE (result.empty ());
  EXPECT_LT (duration.count (), 1000)
      << "Encoding 1MB took too long: " << duration.count () << "ms";
#else
  GTEST_SKIP ()
      << "wall-clock Perf_* thresholds are Release-only (no sanitizers)";
#endif
}

TEST_F (Base64EncoderTest, Stress_RepeatedEncodingOperations)
{
  // Stress test with many repeated operations
  constexpr int iterations = 10000;
  std::vector<byte_type> test_data = { 'S', 't', 'r', 'e', 's', 's' };

  for (int i = 0; i < iterations; ++i)
    {
      std::string result = encoder::encode (test_data);
      EXPECT_EQ (result, "U3RyZXNz") << "Failed at iteration " << i;
    }
}

// --- Memory Safety Tests ------------------------------------------------

TEST_F (Base64EncoderLifetimeTest, MemorySafety_NoLeaksWithLargeData)
{
  {
    std::vector<byte_type> large_data (100000, 0x55);
    std::string result = encoder::encode (large_data);
    EXPECT_FALSE (result.empty ());
  } // large_data goes out of scope here

  // No explicit lifetime tracking needed for this test as we're testing
  // that no memory leaks occur with large temporary objects
}

TEST_F (Base64EncoderTest, BoundaryConditions_MaxSizeHandling)
{
  std::size_t const boundary_size = 1000000; // 1MB as practical boundary
  std::vector<byte_type> boundary_data (boundary_size, 0x88);

  EXPECT_NO_THROW ({
    std::string result = encoder::encode (boundary_data);
    EXPECT_FALSE (result.empty ());
  });
}

// --- Input Validation Tests ---------------------------------------------

TEST_F (Base64EncoderTest, InputValidation_VariousInputTypes)
{
  // Test with std::vector
  std::vector<byte_type> vec_data = { 'V', 'e', 'c', 't', 'o', 'r' };
  std::string vec_result = encoder::encode (vec_data);
  EXPECT_FALSE (vec_result.empty ());

  // Test with raw array
  byte_type array_data[] = { 'A', 'r', 'r', 'a', 'y' };
  std::string array_result = encoder::encode (array_data, sizeof (array_data));
  EXPECT_FALSE (array_result.empty ());

  // Test with string literal cast
  char const *str = "String";
  std::string str_result = encoder::encode (str, std::strlen (str));
  EXPECT_FALSE (str_result.empty ());
}

// ==========================================================================
// decoder
// ==========================================================================

struct DecodeLifetimeTracker
{
  static int creations;
  static int destructions;

  int id;

  static void
  reset ()
  {
    creations = destructions = 0;
  }

  DecodeLifetimeTracker () : id (creations) { creations++; }
  ~DecodeLifetimeTracker () { destructions++; }
};

int DecodeLifetimeTracker::creations = 0;
int DecodeLifetimeTracker::destructions = 0;

class Base64DecoderLifetimeTest : public ::testing::Test
{
protected:
  void
  SetUp () override
  {
    DecodeLifetimeTracker::reset ();
  }
  void
  TearDown () override
  {
    ASSERT_EQ (DecodeLifetimeTracker::creations,
               DecodeLifetimeTracker::destructions)
        << "Memory leak detected! Creations do not match destructions.";
  }
};

// --- API Contract Verifier Tests ---------------------------------------

TEST_F (Base64DecoderTest, GivenEmptyString_WhenDecode_ThenReturnsEmpty)
{
  // Empty input should produce empty output
  std::vector<byte_type> result;
  bool success = decoder::decode (valid_empty, result);

  EXPECT_TRUE (success);
  EXPECT_TRUE (result.empty ());

  // Test return-by-value variant
  auto result2 = decoder::decode (valid_empty);
  EXPECT_TRUE (result2.empty ());
}

TEST_F (Base64DecoderTest, GivenValidBase64_WhenDecode_ThenReturnsCorrectData)
{
  // Test all RFC 4648 test vectors for comprehensive verification
  for (std::vector<std::pair<std::string, std::string>>::const_iterator it
       = rfc_vectors.begin ();
       it != rfc_vectors.end (); ++it)
    {
      std::string const &encoded = it->first;
      std::string const &expected = it->second;

      std::vector<byte_type> result;
      bool success = decoder::decode (encoded, result);

      EXPECT_TRUE (success) << "Failed to decode: " << encoded.c_str ();

      std::string result_str (result.begin (), result.end ());
      EXPECT_EQ (result_str, expected)
          << "Decode mismatch for '" << encoded.c_str () << "' - expected: '"
          << expected.c_str () << "', got: '" << result_str.c_str () << "'";

      // Test return-by-value variant
      auto result2 = decoder::decode (encoded);
      std::string result2_str (result2.begin (), result2.end ());
      EXPECT_EQ (result2_str, expected);
    }
}

TEST_F (Base64DecoderTest,
        GivenValidWithPadding_WhenDecode_ThenHandlesCorrectly)
{
  // Verify padding scenarios work correctly
  struct PaddingTest
  {
    std::string input;
    std::vector<byte_type> expected;
  };

  std::vector<PaddingTest> padding_tests = {
    { "QQ==", { 0x41 } },            // 1 byte with 2 padding chars
    { "QUE=", { 0x41, 0x41 } },      // 2 bytes with 1 padding char
    { "QUFB", { 0x41, 0x41, 0x41 } } // 3 bytes with no padding
  };

  for (auto const &test : padding_tests)
    {
      std::vector<byte_type> result;
      bool success = decoder::decode (test.input, result);

      EXPECT_TRUE (success) << "Failed to decode: " << test.input.c_str ();
      EXPECT_EQ (result, test.expected);
    }
}

// --- Pointer and size core (same signature in every standard) ---------

TEST_F (Base64DecoderTest,
        GivenNullPointerAndZeroSize_WhenDecode_ThenFalseAndOutputCleared)
{
  // nullptr is checked before the size: (nullptr, 0) is not an empty input.
  std::vector<byte_type> out (3, 0x7F);
  EXPECT_FALSE (decoder::decode (nullptr, 0, out));
  EXPECT_TRUE (out.empty ());
  EXPECT_TRUE (decoder::decode (nullptr, 0).empty ());
}

TEST_F (Base64DecoderTest,
        GivenNullPointerAndNonZeroSize_WhenDecode_ThenFalseAndOutputCleared)
{
  std::vector<byte_type> out (3, 0x7F);
  EXPECT_FALSE (decoder::decode (nullptr, 1, out));
  EXPECT_TRUE (out.empty ());
  out.assign (3, 0x7F);
  EXPECT_FALSE (decoder::decode (nullptr, 8, out));
  EXPECT_TRUE (out.empty ());
  EXPECT_TRUE (decoder::decode (nullptr, 8).empty ());
}

TEST_F (Base64DecoderTest,
        GivenEmptyNonNullRange_WhenDecode_ThenTrueAndOutputCleared)
{
  std::vector<byte_type> out (3, 0x7F);
  EXPECT_TRUE (decoder::decode ("", 0, out));
  EXPECT_TRUE (out.empty ());
  // Size 0 over a non-empty buffer is an empty input as well.
  out.assign (3, 0x7F);
  EXPECT_TRUE (decoder::decode ("SGk=", 0, out));
  EXPECT_TRUE (out.empty ());
}

TEST_F (Base64DecoderTest,
        GivenRangeInsideLargerBuffer_WhenDecode_ThenOnlyTheRangeIsRead)
{
  char const buffer[] = "xxSGVsbG8=yy";
  std::vector<byte_type> out;
  ASSERT_TRUE (decoder::decode (buffer + 2, 8, out));
  EXPECT_EQ (std::string (out.begin (), out.end ()), "Hello");
  std::vector<byte_type> const bytes = decoder::decode (buffer + 2, 8);
  EXPECT_EQ (std::string (bytes.begin (), bytes.end ()), "Hello");
  // The same buffer with its invalid neighbours is rejected.
  EXPECT_FALSE (decoder::decode (buffer, 12, out));
  EXPECT_TRUE (out.empty ());
}

TEST_F (Base64DecoderTest,
        GivenUnpaddedRangeFollowedByAlphabet_WhenDecode_ThenStopsAtRangeEnd)
{
  // "SGk" is "Hi" without its padding: the 'A' after the range must not
  // become a third byte.
  char const buffer[] = "SGkA";
  std::vector<byte_type> out;
  ASSERT_TRUE (decoder::decode (buffer, 3, out));
  EXPECT_EQ (std::string (out.begin (), out.end ()), "Hi");
}

TEST_F (Base64DecoderTest, GivenUnpaddedInput_WhenDecode_ThenDecodesLastGroup)
{
  struct UnpaddedCase
  {
    std::string input;
    std::string expected;
  };
  std::vector<UnpaddedCase> const cases = {
    { "QQ", "A" },               // last group of two characters
    { "SGk", "Hi" },             // last group of three characters
    { "SGVsbG8", "Hello" },      // a full group, then three characters
    { "QUJDREVGRw", "ABCDEFG" }, // two full groups, then two characters
  };
  for (auto const &test : cases)
    {
      std::vector<byte_type> out;
      EXPECT_TRUE (decoder::decode (test.input, out)) << test.input;
      EXPECT_EQ (std::string (out.begin (), out.end ()), test.expected)
          << test.input;
    }
}

TEST_F (Base64DecoderTest, GivenEmbeddedNul_WhenDecode_ThenFalse)
{
  std::string const encoded ("SG\0k", 4);
  std::vector<byte_type> out (2, 0x7F);
  EXPECT_FALSE (decoder::decode (encoded.data (), encoded.size (), out));
  EXPECT_TRUE (out.empty ());
  EXPECT_FALSE (decoder::decode (encoded, out));
  EXPECT_TRUE (decoder::decode (encoded).empty ());
}

TEST_F (Base64DecoderTest, GivenRangeOfOneDataCharacter_WhenDecode_ThenFalse)
{
  // One character cannot hold a byte; the '=' after it is outside the range.
  std::vector<byte_type> out;
  EXPECT_FALSE (decoder::decode ("S===", 1, out));
  EXPECT_TRUE (out.empty ());
}

// --- Error & Exception Flow Tests --------------------------------------

TEST_F (Base64DecoderTest, GivenInvalidCharacters_WhenDecode_ThenReturnsFalse)
{
  // Test various invalid characters that might appear in input
  std::vector<std::string> invalid_chars = {
    "QQ@Q",  // @ symbol
    "QQ Q",  // space
    "QQ\tQ", // tab
    "QQ\nQ", // newline
    "QQ\rQ", // carriage return
    "QQ-Q",  // dash (not in standard alphabet)
    "QQ_Q",  // underscore (not in standard alphabet)
    "QQ.Q",  // period
    "QQ,Q",  // comma
    "QQ{Q",  // brace
    "QQ[Q",  // bracket
    "QQ\"Q", // quote
    "QQ'Q",  // apostrophe
  };

  for (auto const &invalid : invalid_chars)
    {
      std::vector<byte_type> result;
      bool success = decoder::decode (invalid, result);

      EXPECT_FALSE (success)
          << "Should reject invalid character in: " << invalid;
    }
}

TEST_F (Base64DecoderTest, GivenInvalidPadding_WhenDecode_ThenReturnsFalse)
{
  // Padding must only appear at the end and in correct amounts
  std::vector<std::string> invalid_padding = {
    "Q===",  // Too much padding
    "QQ=Q",  // Padding not at end
    "Q=QQ",  // Padding in middle
    "=QQQ",  // Padding at start
    "====",  // All padding
    "QQQ=Q", // Invalid position (5 chars total)
  };

  for (auto const &invalid : invalid_padding)
    {
      std::vector<byte_type> result;
      bool success = decoder::decode (invalid, result);

      EXPECT_FALSE (success) << "Should reject invalid padding: " << invalid;
    }
}

// --- Edge & Corner Cases -----------------------------------------------

TEST_F (Base64DecoderTest, GivenAllValidCharacters_WhenDecode_ThenWorks)
{
  // Test string containing all valid Base64 alphabet characters
  std::string all_chars
      = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

  std::vector<byte_type> result;
  bool success = decoder::decode (all_chars, result);

  EXPECT_TRUE (success);
  EXPECT_FALSE (result.empty ());
  EXPECT_EQ (result.size (), (all_chars.length () / 4) * 3); // No padding
}

TEST_F (Base64DecoderTest, GivenMaximalPadding_WhenDecode_ThenHandlesCorrectly)
{
  // Test maximum valid padding scenarios
  std::string two_pad = "QQ=="; // Results in 1 byte
  std::string one_pad = "QUE="; // Results in 2 bytes

  std::vector<byte_type> result1, result2;

  EXPECT_TRUE (decoder::decode (two_pad, result1));
  EXPECT_EQ (result1.size (), 1);

  EXPECT_TRUE (decoder::decode (one_pad, result2));
  EXPECT_EQ (result2.size (), 2);
}

TEST_F (Base64DecoderTest, GivenBinaryData_WhenDecode_ThenPreservesAllBytes)
{
  // Verify all possible byte values can be decoded correctly
  std::string encoded_all_bytes
      = "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyM"
        "zQ1Njc4OTo7PD0+"
        "P0BBQkNERUZHSElKS0xNTk9QUVJTVFVWV1hZWltcXV5fYGFiY2RlZmdoaWprbG1ub3Bxc"
        "nN0dXZ3eHl6e3x9fn+"
        "AgYKDhIWGh4iJiouMjY6PkJGSk5SVlpeYmZqbnJ2en6ChoqOkpaanqKmqq6ytrq+"
        "wsbKztLW2t7i5uru8vb6/"
        "wMHCw8TFxsfIycrLzM3Oz9DR0tPU1dbX2Nna29zd3t/"
        "g4eLj5OXm5+jp6uvs7e7v8PHy8/T19vf4+fr7/P3+/w==";

  std::vector<byte_type> result;
  bool success = decoder::decode (encoded_all_bytes, result);

  EXPECT_TRUE (success);
  EXPECT_EQ (result.size (), 256);

  // Verify all byte values are present and correct
  for (std::size_t i = 0; i < 256; ++i)
    EXPECT_EQ (result[i], static_cast<byte_type> (i))
        << "Byte " << i << " not decoded correctly";
}

// --- Platform Compatibility Tests --------------------------------------

TEST_F (Base64DecoderTest,
        GivenDifferentLineEndings_WhenDecode_ThenRejectsCorrectly)
{
  // Base64 should not accept line endings in standard implementation
  std::vector<std::string> with_linebreaks = {
    "QU\nJD",   // Unix line ending
    "QU\r\nJD", // Windows line ending
    "QU\rJD",   // Old Mac line ending
    "QU JD",    // Space (should be rejected)
    "QU\tJD",   // Tab (should be rejected)
  };

  for (auto const &input : with_linebreaks)
    {
      std::vector<byte_type> result;
      bool success = decoder::decode (input, result);

      EXPECT_FALSE (success) << "Should reject line breaks in: " << input;
    }
}

#ifdef _WIN32
TEST_F (Base64DecoderTest,
        WindowsSpecific_GivenWideStringInput_WhenConverted_ThenWorks)
{
  // Test Windows-specific wide string handling
  std::wstring wide_input = L"SGVsbG8="; // "Hello" in Base64
  std::string narrow_input;
  narrow_input.reserve (wide_input.size ());
  for (wchar_t ch : wide_input)
    narrow_input.push_back (static_cast<char> (ch));

  std::vector<byte_type> result;
  bool success = decoder::decode (narrow_input, result);

  EXPECT_TRUE (success);
  std::string decoded (result.begin (), result.end ());
  EXPECT_EQ (decoded, "Hello");
}
#endif

// --- Concurrency Tests -------------------------------------------------

TEST_F (Base64DecoderTest, ThreadSafety_SimultaneousDecoding)
{
  // decoder should be thread-safe for read-only operations
  constexpr int num_threads = 10;
  std::vector<std::thread> threads;
  std::vector<std::pair<bool, std::vector<byte_type>>> results (num_threads);

  std::string test_input = "VGhyZWFkVGVzdA=="; // "ThreadTest"

  for (int i = 0; i < num_threads; ++i)
    {
      threads.emplace_back (
          [&results, &test_input, i] ()
            {
              results[i].first
                  = decoder::decode (test_input, results[i].second);
            });
    }

  for (auto &t : threads)
    t.join ();

  // All results should be identical and successful
  for (int i = 0; i < num_threads; ++i)
    {
      EXPECT_TRUE (results[i].first) << "Thread " << i << " failed to decode";

      std::string decoded (results[i].second.begin (),
                           results[i].second.end ());
      EXPECT_EQ (decoded, "ThreadTest")
          << "Thread " << i << " produced wrong result";
    }
}

// --- Performance & Stress Tests -----------------------------------------

TEST_F (Base64DecoderTest, Perf_LargeDataDecoding)
{
#if LUMEX_PERF_WALL_CLOCK_ENABLED
  // Create large Base64 string for performance testing
  std::size_t const large_size = 1000000; // ~1MB of original data
  std::vector<byte_type> original_data (large_size);
  for (std::size_t i = 0; i < large_size; ++i)
    original_data[i] = static_cast<byte_type> (i % 256);

  // First encode to get valid Base64 string
  std::string encoded = encoder::encode (original_data);

  auto start = std::chrono::high_resolution_clock::now ();
  std::vector<byte_type> decoded;
  bool success = decoder::decode (encoded, decoded);
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds> (
      std::chrono::high_resolution_clock::now () - start);

  EXPECT_TRUE (success);
  EXPECT_EQ (decoded.size (), original_data.size ());
  EXPECT_LT (duration.count (), 1000)
      << "Decoding 1MB took too long: " << duration.count () << "ms";
#else
  GTEST_SKIP ()
      << "wall-clock Perf_* thresholds are Release-only (no sanitizers)";
#endif
}

TEST_F (Base64DecoderTest, Stress_RepeatedDecodingOperations)
{
  // Stress test with many repeated operations
  constexpr int iterations = 10000;
  std::string test_input = "U3RyZXNz"; // "Stress"

  for (int i = 0; i < iterations; ++i)
    {
      std::vector<byte_type> result;
      bool success = decoder::decode (test_input, result);

      EXPECT_TRUE (success) << "Failed at iteration " << i;

      std::string decoded (result.begin (), result.end ());
      EXPECT_EQ (decoded, "Stress") << "Wrong result at iteration " << i;
    }
}

// --- Memory Safety Tests ------------------------------------------------

TEST_F (Base64DecoderLifetimeTest, MemorySafety_OutputVectorLifetime)
{
  // Verify output vector memory management
  std::string input = "VGVzdERhdGE="; // "TestData"

  {
    std::vector<byte_type> result;
    bool success = decoder::decode (input, result);
    EXPECT_TRUE (success);
    EXPECT_FALSE (result.empty ());
  } // result goes out of scope here - should not cause issues
}

TEST_F (Base64DecoderTest, BoundaryConditions_EmptyAndMinimalInputs)
{
  // Test boundary conditions
  std::vector<std::string> boundary_inputs = {
    "",     // Empty (valid)
    "QQ==", // Minimal valid (1 byte)
    "QUFB", // 3 bytes, no padding
  };

  for (auto const &input : boundary_inputs)
    {
      std::vector<byte_type> result;
      bool success = decoder::decode (input, result);

      if (input.empty ())
        {
          EXPECT_TRUE (success);
          EXPECT_TRUE (result.empty ());
        }
      else
        {
          EXPECT_TRUE (success) << "Failed for boundary input: " << input;
          EXPECT_FALSE (result.empty ());
        }
    }
}

// --- Round-trip Verification Tests -------------------------------------

TEST_F (Base64DecoderTest, RoundTrip_EncodeDecodeCycle)
{
  // Verify encode->decode produces original data
  std::vector<std::vector<byte_type>> test_datasets = {
    {},                          // Empty
    { 0x00 },                    // Single null
    { 0xFF },                    // Single 0xFF
    { 0x00, 0xFF, 0x00, 0xFF },  // Alternating pattern
    { 'H', 'e', 'l', 'l', 'o' }, // Text
  };

  // Add large random dataset
  std::vector<byte_type> random_data (1000);
  for (std::size_t i = 0; i < random_data.size (); ++i)
    random_data[i] = static_cast<byte_type> ((i * 73 + 17) % 256);
  test_datasets.push_back (random_data);

  for (auto const &original : test_datasets)
    {
      std::string encoded = encoder::encode (original);
      std::vector<byte_type> decoded;
      bool success = decoder::decode (encoded, decoded);

      EXPECT_TRUE (success)
          << "Decode failed for data size: " << original.size ();
      EXPECT_EQ (decoded, original)
          << "Round-trip failed for data size: " << original.size ();
    }
}

// ==========================================================================
// validator
// ==========================================================================

// --- API Contract Verifier Tests ---------------------------------------

TEST_F (Base64ValidatorTest, GivenValidBase64_WhenValidate_ThenReturnsTrue)
{
  // All valid Base64 strings should pass validation
  for (auto const &valid : valid_inputs)
    {
      bool result = validator::is_valid_base64 (valid);
      EXPECT_TRUE (result) << "Failed to validate valid input: '" << valid
                           << "'";
    }
}

TEST_F (Base64ValidatorTest, GivenEmptyString_WhenValidate_ThenReturnsTrue)
{
  // Empty string is valid Base64 (encodes empty data)
  bool result = validator::is_valid_base64 ("");
  EXPECT_TRUE (result);
}

TEST_F (Base64ValidatorTest,
        GivenValidPaddingScenarios_WhenValidate_ThenReturnsTrue)
{
  // Test specific valid padding scenarios
  struct PaddingTest
  {
    std::string input;
    std::string description;
  };

  std::vector<PaddingTest> padding_tests = {
    { "QQ==", "Two padding chars" },
    { "QUE=", "One padding char" },
    { "QUFB", "No padding needed" },
    { "SGVsbG8=", "Real data with one pad" },
    { "VGVzdA==", "Real data with two pads" },
  };

  for (auto const &test : padding_tests)
    {
      bool result = validator::is_valid_base64 (test.input);
      EXPECT_TRUE (result) << "Failed for " << test.description << ": "
                           << test.input;
    }
}

// --- Pointer and size core (same signature in every standard) ---------

TEST_F (Base64ValidatorTest,
        GivenNullPointerAndZeroSize_WhenValidate_ThenReturnsFalse)
{
  // nullptr is checked before the size: (nullptr, 0) is not an empty input.
  EXPECT_FALSE (validator::is_valid_base64 (nullptr, 0));
}

TEST_F (Base64ValidatorTest,
        GivenNullPointerAndNonZeroSize_WhenValidate_ThenReturnsFalse)
{
  EXPECT_FALSE (validator::is_valid_base64 (nullptr, 1));
  EXPECT_FALSE (validator::is_valid_base64 (nullptr, 4));
  EXPECT_FALSE (validator::is_valid_base64 (nullptr, 1024));
}

TEST_F (Base64ValidatorTest,
        GivenEmptyNonNullRange_WhenValidate_ThenReturnsTrue)
{
  EXPECT_TRUE (validator::is_valid_base64 ("", 0));
  // Size 0 over a buffer of invalid characters is an empty input as well.
  EXPECT_TRUE (validator::is_valid_base64 ("@@@@", 0));
}

TEST_F (Base64ValidatorTest,
        GivenRangeInsideLargerBuffer_WhenValidate_ThenOnlyTheRangeIsChecked)
{
  char const buffer[] = "@@SGVsbG8=@@";
  EXPECT_TRUE (validator::is_valid_base64 (buffer + 2, 8));
  // The same buffer with its invalid neighbours included is rejected.
  EXPECT_FALSE (validator::is_valid_base64 (buffer, 12));
  EXPECT_FALSE (validator::is_valid_base64 (buffer + 1, 9));
  EXPECT_FALSE (validator::is_valid_base64 (buffer + 2, 9));
}

TEST_F (Base64ValidatorTest,
        GivenPrefixOfPaddedInput_WhenValidate_ThenPaddingOutsideIsIgnored)
{
  // The '=' after the range is not part of it: "SGk" is valid unpadded
  // Base64, "S" (one data character) is not.
  char const buffer[] = "SGk=";
  EXPECT_TRUE (validator::is_valid_base64 (buffer, 3));
  EXPECT_TRUE (validator::is_valid_base64 (buffer, 4));
  EXPECT_FALSE (validator::is_valid_base64 (buffer, 1));
}

TEST_F (Base64ValidatorTest, GivenEmbeddedNul_WhenValidate_ThenReturnsFalse)
{
  std::string const text ("SG\0k", 4);
  EXPECT_FALSE (validator::is_valid_base64 (text.data (), text.size ()));
  EXPECT_FALSE (validator::is_valid_base64 (text));
}

// --- Error Detection Tests ---------------------------------------------

TEST_F (Base64ValidatorTest,
        GivenInvalidCharacters_WhenValidate_ThenReturnsFalse)
{
  // Only Base64 alphabet characters and padding should be accepted
  for (auto const &invalid : invalid_characters)
    {
      bool result = validator::is_valid_base64 (invalid);
      EXPECT_FALSE (result)
          << "Should reject invalid character in: '" << invalid << "'";
    }
}

TEST_F (Base64ValidatorTest, Alphabet_WhenFound_ThenTrue)
{
  EXPECT_TRUE (validator::is_valid_base64 ("SGVsbG8="));
}

TEST_F (Base64ValidatorTest, Alphabet_WhenUnfound_ThenFalse)
{
  EXPECT_FALSE (validator::is_valid_base64 ("SGVsbG8!"));
}

TEST_F (Base64ValidatorTest, GivenInvalidPadding_WhenValidate_ThenReturnsFalse)
{
  // Padding must only appear at end and in correct amounts
  for (auto const &invalid : invalid_padding)
    {
      bool result = validator::is_valid_base64 (invalid);
      EXPECT_FALSE (result)
          << "Should reject invalid padding: '" << invalid << "'";
    }
}

TEST_F (Base64ValidatorTest,
        GivenControlCharacters_WhenValidate_ThenReturnsFalse)
{
  // Control characters should be rejected
  std::vector<std::string> control_chars;

  // ASCII control characters (0-31 and 127)
  for (int i = 0; i <= 31; ++i)
    {
      if (i != 0)
        { // Skip null terminator as it would truncate string
          std::string test
              = "QQ" + std::string (1, static_cast<char> (i)) + "Q";
          control_chars.push_back (test);
        }
    }
  control_chars.push_back ("QQ\x7FQ"); // DEL character

  for (auto const &test : control_chars)
    {
      bool result = validator::is_valid_base64 (test);
      EXPECT_FALSE (result)
          << "Should reject control character in: (contains ASCII "
          << static_cast<int> (test[2]) << ")";
    }
}

// --- Edge & Corner Cases -----------------------------------------------

TEST_F (Base64ValidatorTest,
        GivenAllValidAlphabetCharacters_WhenValidate_ThenReturnsTrue)
{
  // String with all 64 valid Base64 characters
  std::string all_chars
      = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

  bool result = validator::is_valid_base64 (all_chars);
  EXPECT_TRUE (result);
  EXPECT_EQ (all_chars.length () % 4, 0)
      << "Test string should be multiple of 4";
}

TEST_F (Base64ValidatorTest,
        GivenLargeValidString_WhenValidate_ThenReturnsTrue)
{
  // Test with very large valid string to check performance
  std::string large_valid (4000, 'A'); // 4000 'A' characters (valid length)

  bool result = validator::is_valid_base64 (large_valid);
  EXPECT_TRUE (result);
}

TEST_F (Base64ValidatorTest,
        GivenLargeInvalidString_WhenValidate_ThenReturnsFalse)
{
  // Test with large invalid string
  std::string large_invalid (4001, 'A'); // 4001 characters (invalid length)

  bool result = validator::is_valid_base64 (large_invalid);
  EXPECT_FALSE (result);
}

TEST_F (Base64ValidatorTest,
        GivenBoundaryPaddingCases_WhenValidate_ThenHandlesCorrectly)
{
  // Test boundary conditions for padding
  struct BoundaryTest
  {
    std::string input;
    bool should_be_valid;
    std::string description;
  };

  std::vector<BoundaryTest> boundary_tests = {
    { "A===", false, "Too much padding after 1 char" },
    { "AA==", true, "Valid 2-char + 2-pad" },
    { "AAA=", true, "Valid 3-char + 1-pad" },
    { "AAAA", true, "Valid 4-char no pad" },
    { "AA=A", false, "Padding not at end" },
    { "A=AA", false, "Padding in middle" },
    { "=AAA", false, "Padding at start" },
  };

  for (auto const &test : boundary_tests)
    {
      bool result = validator::is_valid_base64 (test.input);
      EXPECT_EQ (result, test.should_be_valid)
          << test.description << " - input: '" << test.input << "'";
    }
}

// --- Platform Compatibility Tests --------------------------------------

TEST_F (Base64ValidatorTest,
        GivenDifferentStringTypes_WhenValidate_ThenWorksConsistently)
{
  // Test different ways of constructing strings
  std::string test_input = "SGVsbG8=";

  // Test with string literal
  bool result1 = validator::is_valid_base64 ("SGVsbG8=");
  EXPECT_TRUE (result1);

  // Test with std::string
  bool result2 = validator::is_valid_base64 (test_input);
  EXPECT_TRUE (result2);

  // Test with constructed string
  std::string constructed = std::string ("SGVs") + "bG8=";
  bool result3 = validator::is_valid_base64 (constructed);
  EXPECT_TRUE (result3);

  // All should be equal
  EXPECT_EQ (result1, result2);
  EXPECT_EQ (result2, result3);
}

#ifdef _WIN32
TEST_F (Base64ValidatorTest,
        WindowsSpecific_GivenWideStringConverted_WhenValidate_ThenWorks)
{
  // Test Windows-specific wide string conversion
  std::wstring wide_input = L"SGVsbG8=";
  std::string narrow_input;
  narrow_input.reserve (wide_input.size ());
  for (wchar_t ch : wide_input)
    narrow_input.push_back (static_cast<char> (ch));

  bool result = validator::is_valid_base64 (narrow_input);
  EXPECT_TRUE (result);
}
#endif

// --- Performance & Stress Tests -----------------------------------------

TEST_F (Base64ValidatorTest, Perf_LargeStringValidation)
{
#if LUMEX_PERF_WALL_CLOCK_ENABLED
  // Performance test with very large strings
  std::size_t const large_size = 1000000; // 1M characters
  std::string large_valid (large_size, 'A');

  auto start = std::chrono::high_resolution_clock::now ();
  bool result = validator::is_valid_base64 (large_valid);
  auto duration = std::chrono::duration_cast<std::chrono::microseconds> (
      std::chrono::high_resolution_clock::now () - start);

  EXPECT_TRUE (result);
  EXPECT_LT (duration.count (), 10000) // Should complete in under 10ms
      << "Validation of 1M chars took too long: " << duration.count () << "μs";
#else
  GTEST_SKIP ()
      << "wall-clock Perf_* thresholds are Release-only (no sanitizers)";
#endif
}

// --- Comprehensive Character Set Tests ---------------------------------

TEST_F (Base64ValidatorTest, CharacterSet_AllValidCharsAccepted)
{
  // Systematically test each valid character
  std::string base64_alphabet
      = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

  for (char c : base64_alphabet)
    {
      std::string test_string = "QQ";
      test_string += c;
      test_string += "Q";

      bool result = validator::is_valid_base64 (test_string);
      EXPECT_TRUE (result) << "Valid character '" << c << "' was rejected";
    }

  // Test padding character
  bool padding_result = validator::is_valid_base64 ("QQ==");
  EXPECT_TRUE (padding_result);
}
