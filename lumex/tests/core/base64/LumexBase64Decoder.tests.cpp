#include <gtest/gtest.h>

#include "lumex/core/base64/Base64.hpp"
#include "lumex/core/base64/Decoder.hpp"
#include "lumex/core/base64/Encoder.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace Lumex::Core::Base64;
using namespace Lumex::Core::Base64::Types;

struct DecodeLifetimeTracker {
  static int creations;
  static int destructions;

  int id;

  static void
  reset()
  {
    creations = destructions = 0;
  }

  DecodeLifetimeTracker() : id(creations) { creations++; }
  ~DecodeLifetimeTracker() { destructions++; }
};

int DecodeLifetimeTracker::creations    = 0;
int DecodeLifetimeTracker::destructions = 0;

class Base64DecoderLifetimeTest : public ::testing::Test
{
protected:
  void
  SetUp() override
  {
    DecodeLifetimeTracker::reset();
  }
  void
  TearDown() override
  {
    ASSERT_EQ(DecodeLifetimeTracker::creations, DecodeLifetimeTracker::destructions)
      << "Memory leak detected! Creations do not match destructions.";
  }
};

class Base64DecoderTest : public ::testing::Test
{
protected:
  void
  SetUp() override
  {
    // Initialize comprehensive test data for all scenarios
    valid_empty      = "";
    valid_single     = "Qg==";       // 'B' (0x42)
    valid_double     = "QkM=";       // "BC"
    valid_triple     = "QUJD";       // "ABC"
    valid_no_padding = "QUJDREVGRw"; // "ABCDEFG" - no padding needed

    // RFC 4648 test vectors
    rfc_vectors = {{"", ""},
                   {"Zg==", "f"},
                   {"Zm8=", "fo"},
                   {"Zm9v", "foo"},
                   {"Zm9vYg==", "foob"},
                   {"Zm9vYmE=", "fooba"},
                   {"Zm9vYmFy", "foobar"},
                   {"AA==", std::string(1, '\0')},
                   {"/w==", std::string(1, '\xFF')},
                   {"AP8=", std::string{'\0', '\xFF'}},
                   {"/wA=", std::string{'\xFF', '\0'}}};

    // Invalid inputs for error testing
    invalid_inputs = {
      "A",     // Wrong length (not multiple of 4)
      "AB",    // Wrong length
      "ABC",   // Wrong length
      "A===",  // Too much padding
      "AB==",  // Invalid padding position
      "A===",  // Invalid padding
      "QQ@Q",  // Invalid character (@)
      "QQ Q",  // Invalid character (space)
      "QQ\nQ", // Invalid character (newline)
      "====",  // All padding
      "QQ=Q",  // Padding in wrong position
    };
  }

  std::string valid_empty;
  std::string valid_single;
  std::string valid_double;
  std::string valid_triple;
  std::string valid_no_padding;

  std::vector<std::pair<std::string, std::string>> rfc_vectors;
  std::vector<std::string> invalid_inputs;
};

// --- API Contract Verifier Tests ---------------------------------------

TEST_F(Base64DecoderTest, GivenEmptyString_WhenDecode_ThenReturnsEmpty)
{
  // Empty input should produce empty output
  std::vector<byte_type> result;
  bool success = Decoder::decode(valid_empty, result);

  EXPECT_TRUE(success);
  EXPECT_TRUE(result.empty());

  // Test return-by-value variant
  auto result2 = Decoder::decode(valid_empty);
  EXPECT_TRUE(result2.empty());
}

TEST_F(Base64DecoderTest, GivenValidBase64_WhenDecode_ThenReturnsCorrectData)
{
  // Test all RFC 4648 test vectors for comprehensive verification
  for(auto const &[encoded, expected] : rfc_vectors)
  {
    std::vector<byte_type> result;
    bool success = Decoder::decode(encoded, result);

    EXPECT_TRUE(success) << "Failed to decode: " << encoded;

    std::string result_str(result.begin(), result.end());
    EXPECT_EQ(result_str, expected) << "Decode mismatch for '" << encoded << "' - expected: '" << expected
                                    << "', got: '" << result_str << "'";

    // Test return-by-value variant
    auto result2 = Decoder::decode(encoded);
    std::string result2_str(result2.begin(), result2.end());
    EXPECT_EQ(result2_str, expected);
  }
}

TEST_F(Base64DecoderTest, GivenValidWithPadding_WhenDecode_ThenHandlesCorrectly)
{
  // Verify padding scenarios work correctly
  struct PaddingTest {
    std::string input;
    std::vector<byte_type> expected;
  };

  std::vector<PaddingTest> padding_tests = {
    {"QQ==", {0x41}},            // 1 byte with 2 padding chars
    {"QUE=", {0x41, 0x41}},      // 2 bytes with 1 padding char
    {"QUFB", {0x41, 0x41, 0x41}} // 3 bytes with no padding
  };

  for(auto const &test : padding_tests)
  {
    std::vector<byte_type> result;
    bool success = Decoder::decode(test.input, result);

    EXPECT_TRUE(success) << "Failed to decode: " << test.input;
    EXPECT_EQ(result, test.expected);
  }
}

// --- Error & Exception Flow Tests --------------------------------------

TEST_F(Base64DecoderTest, GivenInvalidCharacters_WhenDecode_ThenReturnsFalse)
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

  for(auto const &invalid : invalid_chars)
  {
    std::vector<byte_type> result;
    bool success = Decoder::decode(invalid, result);

    EXPECT_FALSE(success) << "Should reject invalid character in: " << invalid;
  }
}

TEST_F(Base64DecoderTest, GivenInvalidPadding_WhenDecode_ThenReturnsFalse)
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

  for(auto const &invalid : invalid_padding)
  {
    std::vector<byte_type> result;
    bool success = Decoder::decode(invalid, result);

    EXPECT_FALSE(success) << "Should reject invalid padding: " << invalid;
  }
}

// --- Edge & Corner Cases -----------------------------------------------

TEST_F(Base64DecoderTest, GivenAllValidCharacters_WhenDecode_ThenWorks)
{
  // Test string containing all valid Base64 alphabet characters
  std::string all_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

  std::vector<byte_type> result;
  bool success = Decoder::decode(all_chars, result);

  EXPECT_TRUE(success);
  EXPECT_FALSE(result.empty());
  EXPECT_EQ(result.size(), (all_chars.length() / 4) * 3); // No padding
}

TEST_F(Base64DecoderTest, GivenMaximalPadding_WhenDecode_ThenHandlesCorrectly)
{
  // Test maximum valid padding scenarios
  std::string two_pad = "QQ=="; // Results in 1 byte
  std::string one_pad = "QUE="; // Results in 2 bytes

  std::vector<byte_type> result1, result2;

  EXPECT_TRUE(Decoder::decode(two_pad, result1));
  EXPECT_EQ(result1.size(), 1);

  EXPECT_TRUE(Decoder::decode(one_pad, result2));
  EXPECT_EQ(result2.size(), 2);
}

TEST_F(Base64DecoderTest, GivenBinaryData_WhenDecode_ThenPreservesAllBytes)
{
  // Verify all possible byte values can be decoded correctly
  std::string encoded_all_bytes
    = "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+"
      "P0BBQkNERUZHSElKS0xNTk9QUVJTVFVWV1hZWltcXV5fYGFiY2RlZmdoaWprbG1ub3BxcnN0dXZ3eHl6e3x9fn+"
      "AgYKDhIWGh4iJiouMjY6PkJGSk5SVlpeYmZqbnJ2en6ChoqOkpaanqKmqq6ytrq+wsbKztLW2t7i5uru8vb6/"
      "wMHCw8TFxsfIycrLzM3Oz9DR0tPU1dbX2Nna29zd3t/g4eLj5OXm5+jp6uvs7e7v8PHy8/T19vf4+fr7/P3+/w==";

  std::vector<byte_type> result;
  bool success = Decoder::decode(encoded_all_bytes, result);

  EXPECT_TRUE(success);
  EXPECT_EQ(result.size(), 256);

  // Verify all byte values are present and correct
  for(size_t i = 0; i < 256; ++i)
    EXPECT_EQ(result[i], static_cast<byte_type>(i)) << "Byte " << i << " not decoded correctly";
}

// --- Platform Compatibility Tests --------------------------------------

TEST_F(Base64DecoderTest, GivenDifferentLineEndings_WhenDecode_ThenRejectsCorrectly)
{
  // Base64 should not accept line endings in standard implementation
  std::vector<std::string> with_linebreaks = {
    "QU\nJD",   // Unix line ending
    "QU\r\nJD", // Windows line ending
    "QU\rJD",   // Old Mac line ending
    "QU JD",    // Space (should be rejected)
    "QU\tJD",   // Tab (should be rejected)
  };

  for(auto const &input : with_linebreaks)
  {
    std::vector<byte_type> result;
    bool success = Decoder::decode(input, result);

    EXPECT_FALSE(success) << "Should reject line breaks in: " << input;
  }
}

#ifdef _WIN32
TEST_F(Base64DecoderTest, WindowsSpecific_GivenWideStringInput_WhenConverted_ThenWorks)
{
  // Test Windows-specific wide string handling
  std::wstring wide_input = L"SGVsbG8="; // "Hello" in Base64
  std::string narrow_input(wide_input.begin(), wide_input.end());

  std::vector<byte_type> result;
  bool success = Decoder::decode(narrow_input, result);

  EXPECT_TRUE(success);
  std::string decoded(result.begin(), result.end());
  EXPECT_EQ(decoded, "Hello");
}
#endif

// --- Concurrency Tests -------------------------------------------------

TEST_F(Base64DecoderTest, ThreadSafety_SimultaneousDecoding)
{
  // Decoder should be thread-safe for read-only operations
  constexpr int num_threads = 10;
  std::vector<std::thread> threads;
  std::vector<std::pair<bool, std::vector<byte_type>>> results(num_threads);

  std::string test_input = "VGhyZWFkVGVzdA=="; // "ThreadTest"

  for(int i = 0; i < num_threads; ++i)
  {
    threads.emplace_back([&results, &test_input, i]()
                         { results[i].first = Decoder::decode(test_input, results[i].second); });
  }

  for(auto &t : threads) t.join();

  // All results should be identical and successful
  for(int i = 0; i < num_threads; ++i)
  {
    EXPECT_TRUE(results[i].first) << "Thread " << i << " failed to decode";

    std::string decoded(results[i].second.begin(), results[i].second.end());
    EXPECT_EQ(decoded, "ThreadTest") << "Thread " << i << " produced wrong result";
  }
}

// --- Performance & Stress Tests -----------------------------------------

TEST_F(Base64DecoderTest, Perf_LargeDataDecoding)
{
  // Create large Base64 string for performance testing
  size_t const large_size = 1'000'000; // ~1MB of original data
  std::vector<byte_type> original_data(large_size);
  for(size_t i = 0; i < large_size; ++i) original_data[i] = static_cast<byte_type>(i % 256);

  // First encode to get valid Base64 string
  std::string encoded = Encoder::encode(original_data);

  auto start          = std::chrono::high_resolution_clock::now();
  std::vector<byte_type> decoded;
  bool success = Decoder::decode(encoded, decoded);
  auto duration
    = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start);

  EXPECT_TRUE(success);
  EXPECT_EQ(decoded.size(), original_data.size());
  EXPECT_LT(duration.count(), 1000) << "Decoding 1MB took too long: " << duration.count() << "ms";
}

TEST_F(Base64DecoderTest, Stress_RepeatedDecodingOperations)
{
  // Stress test with many repeated operations
  constexpr int iterations = 10000;
  std::string test_input   = "U3RyZXNz"; // "Stress"

  for(int i = 0; i < iterations; ++i)
  {
    std::vector<byte_type> result;
    bool success = Decoder::decode(test_input, result);

    EXPECT_TRUE(success) << "Failed at iteration " << i;

    std::string decoded(result.begin(), result.end());
    EXPECT_EQ(decoded, "Stress") << "Wrong result at iteration " << i;
  }
}

// --- Memory Safety Tests ------------------------------------------------

TEST_F(Base64DecoderLifetimeTest, MemorySafety_OutputVectorLifetime)
{
  // Verify output vector memory management
  std::string input = "VGVzdERhdGE="; // "TestData"

  {
    std::vector<byte_type> result;
    bool success = Decoder::decode(input, result);
    EXPECT_TRUE(success);
    EXPECT_FALSE(result.empty());
  } // result goes out of scope here - should not cause issues
}

TEST_F(Base64DecoderTest, BoundaryConditions_EmptyAndMinimalInputs)
{
  // Test boundary conditions
  std::vector<std::string> boundary_inputs = {
    "",     // Empty (valid)
    "QQ==", // Minimal valid (1 byte)
    "QUFB", // 3 bytes, no padding
  };

  for(auto const &input : boundary_inputs)
  {
    std::vector<byte_type> result;
    bool success = Decoder::decode(input, result);

    if(input.empty())
    {
      EXPECT_TRUE(success);
      EXPECT_TRUE(result.empty());
    }
    else
    {
      EXPECT_TRUE(success) << "Failed for boundary input: " << input;
      EXPECT_FALSE(result.empty());
    }
  }
}

// --- Round-trip Verification Tests -------------------------------------

TEST_F(Base64DecoderTest, RoundTrip_EncodeDecodeCycle)
{
  // Verify encode->decode produces original data
  std::vector<std::vector<byte_type>> test_datasets = {
    {},                        // Empty
    {0x00},                    // Single null
    {0xFF},                    // Single 0xFF
    {0x00, 0xFF, 0x00, 0xFF},  // Alternating pattern
    {'H', 'e', 'l', 'l', 'o'}, // Text
  };

  // Add large random dataset
  std::vector<byte_type> random_data(1000);
  for(size_t i = 0; i < random_data.size(); ++i) random_data[i] = static_cast<byte_type>((i * 73 + 17) % 256);
  test_datasets.push_back(random_data);

  for(auto const &original : test_datasets)
  {
    std::string encoded = Encoder::encode(original);
    std::vector<byte_type> decoded;
    bool success = Decoder::decode(encoded, decoded);

    EXPECT_TRUE(success) << "Decode failed for data size: " << original.size();
    EXPECT_EQ(decoded, original) << "Round-trip failed for data size: " << original.size();
  }
}
