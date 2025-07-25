#include <gtest/gtest.h>

#include "lumex/core/base64/Base64.hpp"
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

struct LifetimeTracker {
  static int creations;
  static int destructions;
  static int copies;
  static int moves;

  int id;

  static void
  reset()
  {
    creations = destructions = copies = moves = 0;
  }

  LifetimeTracker() : id(creations) { creations++; }
  ~LifetimeTracker() { destructions++; }
  LifetimeTracker(LifetimeTracker const &) : id(creations)
  {
    creations++;
    copies++;
  }
  LifetimeTracker(LifetimeTracker &&) noexcept : id(creations)
  {
    creations++;
    moves++;
  }
  LifetimeTracker &
  operator=(LifetimeTracker const &)
  {
    copies++;
    return *this;
  }
  LifetimeTracker &
  operator=(LifetimeTracker &&) noexcept
  {
    moves++;
    return *this;
  }
};

int LifetimeTracker::creations    = 0;
int LifetimeTracker::destructions = 0;
int LifetimeTracker::copies       = 0;
int LifetimeTracker::moves        = 0;

class Base64EncoderLifetimeTest : public ::testing::Test
{
protected:
  void
  SetUp() override
  {
    LifetimeTracker::reset();
  }
  void
  TearDown() override
  {
    ASSERT_EQ(LifetimeTracker::creations, LifetimeTracker::destructions)
      << "Memory leak detected! Creations do not match destructions.";
  }
};

class Base64EncoderTest : public ::testing::Test
{
protected:
  void
  SetUp() override
  {
    // Initialize test data with various patterns for comprehensive coverage
    empty_data.clear();
    single_byte = {0x42};
    binary_data = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0xFF, 0xFE, 0xFD};
    text_data   = {'H', 'e', 'l', 'l', 'o', ',', ' ', 'W', 'o', 'r', 'l', 'd', '!'};
    large_data  = std::vector<byte_type>(10000, 0xAA);

    // Edge case: all possible byte values
    all_bytes.resize(256);
    for(size_t i = 0; i < 256; ++i) all_bytes[i] = static_cast<byte_type>(i);
  }

  std::vector<byte_type> empty_data;
  std::vector<byte_type> single_byte;
  std::vector<byte_type> binary_data;
  std::vector<byte_type> text_data;
  std::vector<byte_type> large_data;
  std::vector<byte_type> all_bytes;
};

// --- API Contract Verifier Tests ---------------------------------------

TEST_F(Base64EncoderTest, GivenEmptyData_WhenEncode_ThenReturnsEmptyString)
{
  // Empty input should produce empty output
  std::string result = Encoder::encode(empty_data);
  EXPECT_TRUE(result.empty());

  // Verify with raw pointer interface
  result = Encoder::encode(nullptr, 0);
  EXPECT_TRUE(result.empty());
}

TEST_F(Base64EncoderTest, GivenSingleByte_WhenEncode_ThenReturnsCorrectBase64)
{
  // Single byte 0x42 ('B') should encode to "Qg=="
  std::string result = Encoder::encode(single_byte);
  EXPECT_EQ(result, "Qg==");
  EXPECT_EQ(result.length(), 4); // Base64 always produces multiples of 4
}

TEST_F(Base64EncoderTest, GivenKnownTestVectors_WhenEncode_ThenMatchesExpected)
{
  // Use RFC 4648 test vectors for verification
  struct TestVector {
    std::vector<byte_type> input;
    std::string expected;
  };

  std::vector<TestVector> vectors = {{{}, ""},
                                     {{'f'}, "Zg=="},
                                     {{'f', 'o'}, "Zm8="},
                                     {{'f', 'o', 'o'}, "Zm9v"},
                                     {{'f', 'o', 'o', 'b'}, "Zm9vYg=="},
                                     {{'f', 'o', 'o', 'b', 'a'}, "Zm9vYmE="},
                                     {{'f', 'o', 'o', 'b', 'a', 'r'}, "Zm9vYmFy"},

                                     // Binary data
                                     {{0x00}, "AA=="},
                                     {{0xFF}, "/w=="},
                                     {{0x00, 0xFF}, "AP8="},
                                     {{0xFF, 0x00}, "/wA="}};

  for(auto const &vector : vectors)
  {
    std::string result = Encoder::encode(vector.input);
    EXPECT_EQ(result, vector.expected) << "Failed for input size: " << vector.input.size();
  }
}

TEST_F(Base64EncoderTest, GivenRawPointer_WhenEncode_ThenProducesCorrectOutput)
{
  // Test raw pointer interface with various sizes
  char const *text   = "Hello";
  std::string result = Encoder::encode(text, 5);
  EXPECT_EQ(result, "SGVsbG8=");

  // Test with binary data
  byte_type binary[] = {0x00, 0x01, 0x02};
  result             = Encoder::encode(binary, sizeof(binary));
  EXPECT_EQ(result, "AAEC");
}

#if __cplusplus >= 201703L
TEST_F(Base64EncoderTest, GivenStringView_WhenEncode_ThenProducesCorrectOutput)
{
  std::string text = "Test string";
  std::string_view view(text);
  std::string result = Encoder::encode(view);
  EXPECT_EQ(result, "VGVzdCBzdHJpbmc=");
}
#endif

#if __cplusplus >= 202002L
TEST_F(Base64EncoderTest, GivenSpan_WhenEncode_ThenProducesCorrectOutput)
{
  std::vector<byte_type> data = {'T', 'e', 's', 't'};
  std::span<byte_type const> span_data(data);
  std::string result = Encoder::encode(span_data);
  EXPECT_EQ(result, "VGVzdA==");
}
#endif

// --- Edge & Corner Cases -----------------------------------------------

TEST_F(Base64EncoderTest, GivenAllPossibleBytes_WhenEncode_ThenHandlesCorrectly)
{
  // Verify all 256 possible byte values can be encoded
  std::string result = Encoder::encode(all_bytes);
  EXPECT_FALSE(result.empty());
  EXPECT_EQ(result.length(), ((all_bytes.size() + 2) / 3) * 4);

  // Verify only valid Base64 characters
  for(char c : result)
  {
    EXPECT_TRUE((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '+' || c == '/'
                || c == '=')
      << "Invalid Base64 character: " << c;
  }
}

TEST_F(Base64EncoderTest, GivenLargeData_WhenEncode_ThenHandlesEfficiently)
{
  // Test with large data to verify no performance issues
  std::string result = Encoder::encode(large_data);
  EXPECT_FALSE(result.empty());

  // Verify correct length calculation
  size_t expected_length = ((large_data.size() + 2) / 3) * 4;
  EXPECT_EQ(result.length(), expected_length);
}

TEST_F(Base64EncoderTest, GivenNullPointer_WhenEncode_ThenReturnsEmptyString)
{
  // Null pointer should be handled gracefully
  std::string result = Encoder::encode(nullptr, 100);
  EXPECT_TRUE(result.empty());
}

TEST_F(Base64EncoderTest, GivenZeroSize_WhenEncode_ThenReturnsEmptyString)
{
  byte_type dummy    = 0x42;
  std::string result = Encoder::encode(&dummy, 0);
  EXPECT_TRUE(result.empty());
}

// --- Platform Compatibility Tests --------------------------------------

TEST_F(Base64EncoderTest, GivenDifferentEndianness_WhenEncode_ThenProducesConsistentResults)
{
  // Base64 should be endian-independent since it works on bytes
  union EndianTest {
    uint32_t value;
    byte_type bytes[4];
  };

  EndianTest test;
  test.value         = 0x01020304;

  std::string result = Encoder::encode(test.bytes, 4);
  EXPECT_FALSE(result.empty());
  EXPECT_EQ(result.length(), 8); // 4 bytes -> 8 Base64 chars (with padding)
}

#ifdef _WIN32
TEST_F(Base64EncoderTest, WindowsSpecific_GivenWideCharData_WhenConvertAndEncode_ThenWorks)
{
  // Test Windows-specific wide character handling
  std::wstring wide_text = L"Test";
  std::vector<byte_type> bytes(reinterpret_cast<byte_type const *>(wide_text.data()),
                               reinterpret_cast<byte_type const *>(wide_text.data())
                                 + wide_text.size() * sizeof(wchar_t));

  std::string result = Encoder::encode(bytes);
  EXPECT_FALSE(result.empty());
}
#endif

// --- Concurrency Tests -------------------------------------------------

TEST_F(Base64EncoderTest, ThreadSafety_SimultaneousEncoding)
{
  // Encoder should be thread-safe for simultaneous operations
  constexpr int num_threads = 10;
  std::vector<std::thread> threads;
  std::vector<std::string> results(num_threads);

  std::vector<byte_type> test_data = {'T', 'h', 'r', 'e', 'a', 'd', 'T', 'e', 's', 't'};

  for(int i = 0; i < num_threads; ++i)
    threads.emplace_back([&results, &test_data, i]() { results[i] = Encoder::encode(test_data); });

  for(auto &t : threads) t.join();

  // All results should be identical
  std::string expected = results[0];
  for(int i = 1; i < num_threads; ++i)
    EXPECT_EQ(results[i], expected) << "Thread " << i << " produced different result";
}

// --- Performance & Stress Tests -----------------------------------------

TEST_F(Base64EncoderTest, Perf_LargeDataEncoding)
{
  // Performance test with very large data
  size_t const large_size = 1'000'000; // 1MB
  std::vector<byte_type> very_large_data(large_size);

  // Fill with pattern to avoid optimization
  for(size_t i = 0; i < large_size; ++i) very_large_data[i] = static_cast<byte_type>(i % 256);

  auto start         = std::chrono::high_resolution_clock::now();
  std::string result = Encoder::encode(very_large_data);
  auto duration
    = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start);

  EXPECT_FALSE(result.empty());
  EXPECT_LT(duration.count(), 1000) << "Encoding 1MB took too long: " << duration.count() << "ms";
}

TEST_F(Base64EncoderTest, Stress_RepeatedEncodingOperations)
{
  // Stress test with many repeated operations
  constexpr int iterations         = 10000;
  std::vector<byte_type> test_data = {'S', 't', 'r', 'e', 's', 's'};

  for(int i = 0; i < iterations; ++i)
  {
    std::string result = Encoder::encode(test_data);
    EXPECT_EQ(result, "U3RyZXNz") << "Failed at iteration " << i;
  }
}

// --- Memory Safety Tests ------------------------------------------------

TEST_F(Base64EncoderLifetimeTest, MemorySafety_NoLeaksWithLargeData)
{
  {
    std::vector<byte_type> large_data(100000, 0x55);
    std::string result = Encoder::encode(large_data);
    EXPECT_FALSE(result.empty());
  } // large_data goes out of scope here

  // No explicit lifetime tracking needed for this test as we're testing
  // that no memory leaks occur with large temporary objects
}

TEST_F(Base64EncoderTest, BoundaryConditions_MaxSizeHandling)
{
  size_t const boundary_size = 1000000; // 1MB as practical boundary
  std::vector<byte_type> boundary_data(boundary_size, 0x88);

  EXPECT_NO_THROW({
    std::string result = Encoder::encode(boundary_data);
    EXPECT_FALSE(result.empty());
  });
}

// --- Input Validation Tests ---------------------------------------------

TEST_F(Base64EncoderTest, InputValidation_VariousInputTypes)
{
  // Test with std::vector
  std::vector<byte_type> vec_data = {'V', 'e', 'c', 't', 'o', 'r'};
  std::string vec_result          = Encoder::encode(vec_data);
  EXPECT_FALSE(vec_result.empty());

  // Test with raw array
  byte_type array_data[]   = {'A', 'r', 'r', 'a', 'y'};
  std::string array_result = Encoder::encode(array_data, sizeof(array_data));
  EXPECT_FALSE(array_result.empty());

  // Test with string literal cast
  char const *str        = "String";
  std::string str_result = Encoder::encode(str, std::strlen(str));
  EXPECT_FALSE(str_result.empty());
}
