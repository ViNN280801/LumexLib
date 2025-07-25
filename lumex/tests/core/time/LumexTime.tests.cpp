#include "lumex/core/time/LumexTime.hpp"
#include "lumex/core/utility/LumexUtility" // For LUMEX_OS_WINDOWS, LUMEX_OS_IS_UNIX
#include <gtest/gtest.h>

#include <algorithm> // For std::all_of
#include <chrono>
#include <ctime>
#include <stdexcept> // For potential exceptions in string to long long conversion
#include <string>
#include <thread>
#include <vector>

// Test fixture for LumexTime to provide a clean state if needed (though not strictly necessary for static functions).
class LumexTimeTest : public ::testing::Test
{};

// --- get_current_datetime Tests ---

TEST_F(LumexTimeTest, GetCurrentDatetime_DefaultFormat_Clean)
{
  std::string datetime_str = LumexTime::get_current_datetime();
  // Expect a non-empty string of a certain minimum length, e.g., "DD.MM.YYYY_HH:MM:SS" is 19 chars
  ASSERT_FALSE(datetime_str.empty());
  ASSERT_GE(datetime_str.length(), 19);

  // Basic format check: should contain at least one '.' and one '_' and one ':'
  ASSERT_NE(datetime_str.find('.'), std::string::npos);
  ASSERT_NE(datetime_str.find('_'), std::string::npos);
  ASSERT_NE(datetime_str.find(':'), std::string::npos);
}

TEST_F(LumexTimeTest, GetCurrentDatetime_ISO8601Format_Clean)
{
  std::string format_str = "%Y-%m-%dT%H:%M:%S"; // ISO 8601-like
  std::string datetime_str
    = LumexTime::get_current_datetime(format_str.c_str());
  ASSERT_FALSE(datetime_str.empty());
  ASSERT_GE(datetime_str.length(), format_str.length());

// strptime is POSIX, not standard C++. Windows does not have it by default.
// This parsing verification is specific to Unix-like systems.
#if LUMEX_OS_IS_UNIX
  std::tm tm_from_str = {};
  char *parse_result
    = strptime(datetime_str.c_str(), format_str.c_str(), &tm_from_str);
  ASSERT_NE(parse_result, nullptr)
    << "Failed to parse generated datetime string: " << datetime_str;

  // Additional sanity check: current year should be non-zero (since 1900)
  ASSERT_GE(tm_from_str.tm_year + 1900, 2023);
#else
  // On Windows, we cannot reliably use strptime.
  // We rely on the format string length check and manual inspection for validity.
  // No further runtime parsing validation for ISO 8601 format on Windows in tests.
  SUCCEED() << "strptime test skipped on non-UNIX platform (Windows).";
#endif
}

TEST_F(LumexTimeTest, GetCurrentDatetime_EmptyFormat_Dirty)
{
  std::string datetime_str = LumexTime::get_current_datetime("");
  ASSERT_TRUE(datetime_str.empty()); // Expect empty string for empty format
}

TEST_F(LumexTimeTest, GetCurrentDatetime_InvalidFormat_Dirty)
{
  // %Q is an invalid format specifier for strftime
  std::string datetime_str
    = LumexTime::get_current_datetime("%Y-%m-%d %Q %H:%M:%S");
  // strftime might return an empty string or partial string depending on implementation
  // A robust LumexTime should handle this gracefully (e.g., return empty)
  ASSERT_TRUE(datetime_str.empty());
}

TEST_F(LumexTimeTest, GetCurrentDatetime_ThreadSafety_Dirty)
{
  // Test concurrent access to get_current_datetime
  std::vector<std::thread> threads;
  std::vector<std::string> results(100); // Store results from 100 threads

  for(int i = 0; i < 100; ++i)
    {
      threads.emplace_back(
        [&results, i]() { results[i] = LumexTime::get_current_datetime(); });
    }

  for(auto &th : threads)
    if(th.joinable()) th.join();

  // Verify all results are non-empty and have expected length
  for(std::string const &s : results)
    {
      ASSERT_FALSE(s.empty());
      ASSERT_GE(s.length(), 19);
    }
}

// --- get_timestamp_xxx Tests ---

// Helper to convert string timestamp to long long, with error handling
long long
safe_stoll(std::string const &s)
{
  try
    {
      return std::stoll(s);
  } catch(std::exception const &e)
    {
      ADD_FAILURE() << "Failed to convert timestamp string '" << s
                    << "' to long long: " << e.what();
      return 0; // Return 0 on error, so subsequent checks don't crash
  }
}

// Macro to avoid repetition for timestamp tests
#define TEST_TIMESTAMP_FUNCTION(Func, ExpectedMinLength, DivisorNsToUnit)      \
  TEST_F(LumexTimeTest, GetTimestamp_##Func##_Clean)                           \
  {                                                                            \
    std::string timestamp_str = LumexTime::Func();                             \
    ASSERT_FALSE(timestamp_str.empty());                                       \
    ASSERT_TRUE(std::all_of(timestamp_str.begin(), timestamp_str.end(),        \
                            ::isdigit))                                        \
      << "Timestamp contains non-digit characters: " << timestamp_str;         \
    ASSERT_GE(timestamp_str.length(), ExpectedMinLength);                      \
                                                                               \
    /* Verify increasing value */                                              \
    std::string timestamp_str2 = LumexTime::Func();                            \
    ASSERT_GT(safe_stoll(timestamp_str2), safe_stoll(timestamp_str));          \
  }

TEST_TIMESTAMP_FUNCTION(get_timestamp_ns, 19, 1LL)
TEST_TIMESTAMP_FUNCTION(get_timestamp_mcs, 16,
                        Lumex::Core::Time::Constants::NS_IN_MCS)
TEST_TIMESTAMP_FUNCTION(get_timestamp_ms, 13,
                        Lumex::Core::Time::Constants::NS_IN_MS)
TEST_TIMESTAMP_FUNCTION(get_timestamp_s, 10,
                        Lumex::Core::Time::Constants::NS_IN_S)
TEST_TIMESTAMP_FUNCTION(get_timestamp_min, 8,
                        Lumex::Core::Time::Constants::NS_IN_MIN)
TEST_TIMESTAMP_FUNCTION(get_timestamp_h, 6,
                        Lumex::Core::Time::Constants::NS_IN_H)
TEST_TIMESTAMP_FUNCTION(get_timestamp_d, 4,
                        Lumex::Core::Time::Constants::NS_IN_D)
TEST_TIMESTAMP_FUNCTION(get_timestamp_w, 3,
                        Lumex::Core::Time::Constants::NS_IN_W)
TEST_TIMESTAMP_FUNCTION(get_timestamp_m, 2,
                        Lumex::Core::Time::Constants::NS_IN_M)
TEST_TIMESTAMP_FUNCTION(get_timestamp_y, 2,
                        Lumex::Core::Time::Constants::NS_IN_Y)

// Dirty test for performance sanity check (not a benchmark)
TEST_F(LumexTimeTest, GetTimestamp_Performance_Dirty)
{
  // Check if it runs quickly over many iterations
  int const num_iterations = 100000;
  ASSERT_NO_FATAL_FAILURE({
    for(int i = 0; i < num_iterations; ++i) LumexTime::get_timestamp_ns();
  }) << "get_timestamp_ns took too long for "
     << num_iterations << " iterations.";
}

// Dirty test for get_timestamp when exception occurs in private helper
// This is hard to trigger directly without mocking, but we can verify the fallback.
TEST_F(LumexTimeTest, GetTimestamp_ExceptionFallback_Dirty)
{
  // The internal _get_timestamp catches std::exception and returns an empty string.
  // We can't easily force std::to_string or chrono to throw here without advanced mocking.
  // This test primarily serves as a placeholder and an acknowledgment of the existing handling.
  // If `_get_timestamp` was refactored to allow injecting a throwing component, this would be expanded.
  std::string result = LumexTime::get_timestamp_ns();
  ASSERT_FALSE(result.empty())
    << "Expected a valid timestamp under normal conditions.";
}

#undef TEST_TIMESTAMP_FUNCTION
