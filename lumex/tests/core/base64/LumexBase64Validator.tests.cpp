#include <algorithm>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/base64/LumexBase64"

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

#if defined(__clang__)
#endif

using namespace lumex::core::base64::codec;
using namespace lumex::core::base64::decode;
using namespace lumex::core::base64::encode;
using namespace lumex::core::base64::validate;

class Base64ValidatorTest : public ::testing::Test
{
protected:
  void
  SetUp () override
  {
    // Comprehensive test data covering all validation scenarios

    // Valid Base64 strings
    valid_inputs = {
      "",                 // Empty string (valid)
      "QQ==",             // 1 byte with padding
      "QUE=",             // 2 bytes with padding
      "QUFB",             // 3 bytes no padding
      "SGVsbG8=",         // "Hello"
      "Zm9vYmFy",         // "foobar" (no padding)
      "VGVzdCBzdHJpbmc=", // "Test string"

      // All valid characters
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/",

      // With padding
      "QUJDREVGRw==", // Multiple of 4 with padding
      "QUJDREVGRw",   // Same without padding (still valid length)
    };

    // Invalid inputs - wrong length (not multiple of 4)
    invalid_length = {
      "Q",       // 1 char
      "QQ",      // 2 chars
      "QQQ",     // 3 chars
      "QQQQQ",   // 5 chars
      "QQQQQQ",  // 6 chars
      "QQQQQQQ", // 7 chars
    };

    // Invalid inputs - bad characters
    invalid_characters = {
      "QQ@Q",  // @ symbol
      "QQ Q",  // space
      "QQ\tQ", // tab
      "QQ\nQ", // newline
      "QQ\rQ", // carriage return
      "QQ-Q",  // dash (URL-safe variant, not standard)
      "QQ_Q",  // underscore (URL-safe variant, not standard)
      "QQ.Q",  // period
      "QQ,Q",  // comma
      "QQ{Q",  // brace
      "QQ[Q",  // bracket
      "QQ\"Q", // quote
      "QQ'Q",  // apostrophe
      "QQ\\Q", // backslash
      "QQ|Q",  // pipe
      "QQ~Q",  // tilde
      "QQ`Q",  // backtick
      "QQ!Q",  // exclamation
      "QQ#Q",  // hash
      "QQ$Q",  // dollar
      "QQ%Q",  // percent
      "QQ^Q",  // caret
      "QQ&Q",  // ampersand
      "QQ*Q",  // asterisk
      "QQ(Q",  // parenthesis
      "QQ)Q",  // parenthesis
      "QQ<Q",  // less than
      "QQ>Q",  // greater than
      "QQ?Q",  // question mark
    };

    // Invalid inputs - bad padding
    invalid_padding = {
      "Q===",   // Too much padding
      "QQ=Q",   // Padding not at end
      "Q=QQ",   // Padding in middle
      "=QQQ",   // Padding at start
      "====",   // All padding
      "QQQ=Q",  // Wrong total length with padding
      "QQ==Q",  // Padding not at very end
      "QQQ===", // Too much padding for length
    };

    // Edge cases
    edge_cases = {
      std::string (1000, 'A'),         // Very long valid string
      std::string (4, '='),            // All padding
      std::string (1000, 'A') + "===", // Long with invalid padding
      std::string (1001, 'A'),         // Long with invalid length
    };
  }

  std::vector<std::string> valid_inputs;
  std::vector<std::string> invalid_length;
  std::vector<std::string> invalid_characters;
  std::vector<std::string> invalid_padding;
  std::vector<std::string> edge_cases;
};

// --- API Contract Verifier Tests ---------------------------------------

TEST_F (Base64ValidatorTest, GivenValidBase64_WhenValidate_ThenReturnsTrue)
{
  // All valid Base64 strings should pass validation
  for (auto const &valid : valid_inputs)
    {
      bool result = Validator::is_valid_base64 (valid);
      EXPECT_TRUE (result) << "Failed to validate valid input: '" << valid
                           << "'";
    }
}

TEST_F (Base64ValidatorTest, GivenEmptyString_WhenValidate_ThenReturnsTrue)
{
  // Empty string is valid Base64 (encodes empty data)
  bool result = Validator::is_valid_base64 ("");
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
      bool result = Validator::is_valid_base64 (test.input);
      EXPECT_TRUE (result) << "Failed for " << test.description << ": "
                           << test.input;
    }
}

// --- Error Detection Tests ---------------------------------------------

TEST_F (Base64ValidatorTest,
        GivenInvalidCharacters_WhenValidate_ThenReturnsFalse)
{
  // Only Base64 alphabet characters and padding should be accepted
  for (auto const &invalid : invalid_characters)
    {
      bool result = Validator::is_valid_base64 (invalid);
      EXPECT_FALSE (result)
          << "Should reject invalid character in: '" << invalid << "'";
    }
}

TEST_F (Base64ValidatorTest, Alphabet_WhenFound_ThenTrue)
{
  EXPECT_TRUE (Validator::is_valid_base64 ("SGVsbG8="));
}

TEST_F (Base64ValidatorTest, Alphabet_WhenUnfound_ThenFalse)
{
  EXPECT_FALSE (Validator::is_valid_base64 ("SGVsbG8!"));
}

TEST_F (Base64ValidatorTest, GivenInvalidPadding_WhenValidate_ThenReturnsFalse)
{
  // Padding must only appear at end and in correct amounts
  for (auto const &invalid : invalid_padding)
    {
      bool result = Validator::is_valid_base64 (invalid);
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
      bool result = Validator::is_valid_base64 (test);
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

  bool result = Validator::is_valid_base64 (all_chars);
  EXPECT_TRUE (result);
  EXPECT_EQ (all_chars.length () % 4, 0)
      << "Test string should be multiple of 4";
}

TEST_F (Base64ValidatorTest,
        GivenLargeValidString_WhenValidate_ThenReturnsTrue)
{
  // Test with very large valid string to check performance
  std::string large_valid (4000, 'A'); // 4000 'A' characters (valid length)

  bool result = Validator::is_valid_base64 (large_valid);
  EXPECT_TRUE (result);
}

TEST_F (Base64ValidatorTest,
        GivenLargeInvalidString_WhenValidate_ThenReturnsFalse)
{
  // Test with large invalid string
  std::string large_invalid (4001, 'A'); // 4001 characters (invalid length)

  bool result = Validator::is_valid_base64 (large_invalid);
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
      bool result = Validator::is_valid_base64 (test.input);
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
  bool result1 = Validator::is_valid_base64 ("SGVsbG8=");
  EXPECT_TRUE (result1);

  // Test with std::string
  bool result2 = Validator::is_valid_base64 (test_input);
  EXPECT_TRUE (result2);

  // Test with constructed string
  std::string constructed = std::string ("SGVs") + "bG8=";
  bool result3 = Validator::is_valid_base64 (constructed);
  EXPECT_TRUE (result3);

  // All should be equal
  EXPECT_EQ (result1, result2);
  EXPECT_EQ (result2, result3);
}

#if __cplusplus >= 201703L
TEST_F (Base64ValidatorTest, GivenStringView_WhenValidate_ThenWorksCorrectly)
{
  // Test C++17 string_view interface
  std::string base_string = "VGVzdERhdGE=";
  std::string_view view (base_string);

  bool result = Validator::is_valid_base64 (view);
  EXPECT_TRUE (result);

  // Test substring view
  std::string longer = "PrefixVGVzdERhdGE=Suffix";
  std::string_view sub_view (longer.data () + 6, 12); // Extract "VGVzdERhdGE="

  bool result2 = Validator::is_valid_base64 (sub_view);
  EXPECT_TRUE (result2);
}
#endif

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

  bool result = Validator::is_valid_base64 (narrow_input);
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
  bool result = Validator::is_valid_base64 (large_valid);
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

      bool result = Validator::is_valid_base64 (test_string);
      EXPECT_TRUE (result) << "Valid character '" << c << "' was rejected";
    }

  // Test padding character
  bool padding_result = Validator::is_valid_base64 ("QQ==");
  EXPECT_TRUE (padding_result);
}
