/*
 * Copyright (c) 2026 Vladislav Semykin <vladislav.semykin@gmail.com>
 *
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of
 * charge, to any person obtaining a copy
 * of this software and associated
 * documentation files (the "Software"), to deal
 * in the Software without
 * restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef LUMEX_TESTS_CORE_BASE64_HPP
#define LUMEX_TESTS_CORE_BASE64_HPP

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/base64/LumexBase64"

// Fixtures of the base64 tests. The test sources of every standard
// (LumexBase64.cxx11.tests.cpp, .cxx17, .cxx20) add tests to the same
// GoogleTest suites, and every test of a suite must use one fixture class.

class Base64EncoderTest : public ::testing::Test
{
protected:
  using byte_type = lumex::core::base64::codec::Types::byte_type;

  void
  SetUp () override
  {
    // Initialize test data with various patterns for comprehensive coverage
    empty_data.clear ();
    single_byte = { 0x42 };
    binary_data = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0xFF, 0xFE, 0xFD };
    text_data
        = { 'H', 'e', 'l', 'l', 'o', ',', ' ', 'W', 'o', 'r', 'l', 'd', '!' };
    large_data = std::vector<byte_type> (10000, 0xAA);

    // Edge case: all possible byte values
    all_bytes.resize (256);
    for (std::size_t i = 0; i < 256; ++i)
      all_bytes[i] = static_cast<byte_type> (i);
  }

  std::vector<byte_type> empty_data;
  std::vector<byte_type> single_byte;
  std::vector<byte_type> binary_data;
  std::vector<byte_type> text_data;
  std::vector<byte_type> large_data;
  std::vector<byte_type> all_bytes;
};

class Base64DecoderTest : public ::testing::Test
{
protected:
  using byte_type = lumex::core::base64::codec::Types::byte_type;

  void
  SetUp () override
  {
    // Initialize comprehensive test data for all scenarios
    valid_empty = "";
    valid_single = "Qg==";           // 'B' (0x42)
    valid_double = "QkM=";           // "BC"
    valid_triple = "QUJD";           // "ABC"
    valid_no_padding = "QUJDREVGRw"; // "ABCDEFG" - no padding needed

    // RFC 4648 test vectors
    rfc_vectors = { { "", "" },
                    { "Zg==", "f" },
                    { "Zm8=", "fo" },
                    { "Zm9v", "foo" },
                    { "Zm9vYg==", "foob" },
                    { "Zm9vYmE=", "fooba" },
                    { "Zm9vYmFy", "foobar" },
                    { "AA==", std::string (1, '\0') },
                    { "/w==", std::string (1, '\xFF') },
                    { "AP8=", std::string{ '\0', '\xFF' } },
                    { "/wA=", std::string{ '\xFF', '\0' } } };

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

#endif // !LUMEX_TESTS_CORE_BASE64_HPP
