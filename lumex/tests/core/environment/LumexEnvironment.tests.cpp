// LumexEnvironment.tests.cpp
#include <chrono> // For performance tests
#include <limits> // For numeric_limits
#include <string>
#include <thread> // For concurrency tests
#include <vector> // For concurrency tests

#include <gtest/gtest.h>

#include "lumex/core/environment/LumexEnvironment"

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

using namespace lumex::core::environment::env;

// --- Fixture ------------------------------------------------------------
class LumexEnvironmentTest : public ::testing::Test
{
protected:
  // Using a test fixture to ensure a clean state for each test
  // and to easily access the singleton instance.
  // Note: Environment variables are global, so tests that modify them
  // must be careful not to interfere with other tests.
  // For sensitive tests, consider using unique variable names or
  // resetting the environment in SetUp/TearDown, though full reset
  // can be problematic on some OSes or for standard variables like PATH.
  // For this reason, we'll mostly use unique, temporary variable names.

  void
  SetUp () override
  {
    // Ensure the singleton instance is ready
    env = &LumexEnvironment::instance ();
  }

  // Not strictly necessary for this class as it doesn't allocate external
  // resources that need explicit cleanup in TearDown, but good practice for
  // fixtures.
  void
  TearDown () override
  {
    // Clean up any specific environment variables set during tests
    env->unset_environment_variable ("LUMEX_TEST_VAR");
    env->unset_environment_variable ("LUMEX_ANOTHER_VAR");
    env->unset_environment_variable ("LUMEX_LONG_VAR");
    env->unset_environment_variable ("LUMEX_PERF_VAR");
    env->unset_environment_variable (
        "LUMEX_STATIC_VAR"); // Cleanup from static tests
    env->unset_environment_variable (
        "LUMEX_STATIC_SET_VAR"); // Cleanup from static tests
    env->unset_environment_variable (
        "LUMEX_MAX_VAR"); // Cleanup from long value test
    env->unset_environment_variable (
        "LUMEX_EMPTY_VAR"); // Cleanup from empty value test
    env->unset_environment_variable (
        "LUMEX_TEMP_VAR"); // Cleanup from unset test
    env->unset_environment_variable ("LUMEX_HAS_VAR"); // Cleanup from has test
    env->unset_environment_variable (
        "LUMEX_GET_OR_EXISTENT"); // Cleanup from get_or test
    env->unset_environment_variable (
        "LUMEX_UNSET_VIA_NULL"); // Cleanup from unset via null test
    env->unset_environment_variable (
        "LUMEX_OVERWRITE_VAR"); // Cleanup from overwrite tests
    env->unset_environment_variable (
        "LUMEX_OVERWRITE_NEW"); // Cleanup from overwrite tests
    env->unset_environment_variable (
        "LUMEX_TRUTHY_VAR"); // Cleanup from truthy tests
    env->unset_environment_variable (
        "LUMEX_SET_VAR"); // Cleanup from is_env_set tests
    env->unset_environment_variable ("LUMEX_FOUND_UNFOUND_VAR");
  }

  LumexEnvironment *env; // Pointer to the singleton instance
};

// --- API Contract Verifier Tests ----------------------------------------

TEST_F (
    LumexEnvironmentTest,
    GivenNonExistingVariable_WhenGetEnvironmentVariable_ThenReturnsUnsuccessful)
{
  // CoT: Request a variable that definitely doesn't exist -> expect
  // unsuccessful result.
  LumexEnvironment::EnvResult result
      = env->get_environment_variable ("NON_EXISTENT_LUMEX_VAR_12345");
  EXPECT_FALSE (result.success);
  EXPECT_FALSE (result); // Implicit bool conversion
  EXPECT_TRUE (result.value.empty ());
  // Error code might vary by OS, so we don't assert a specific value, just
  // that it's non-zero for "not found" equivalent.
#if LUMEX_OS_WINDOWS
  // ERROR_ENVVAR_NOT_FOUND (203)
  EXPECT_EQ (result.error_code, 203);
#else
  // POSIX getenv returns nullptr, which is mapped to -2 (Not found) in
  // LumexEnvironment.
  EXPECT_EQ (result.error_code, -2);
#endif
}

TEST_F (
    LumexEnvironmentTest,
    GivenExistingVariable_WhenGetEnvironmentVariable_ThenReturnsSuccessfulWithValue)
{
  // CoT: Set a variable -> get it back -> expect successful result with
  // correct value.
  ASSERT_TRUE (env->set_environment_variable ("LUMEX_TEST_VAR", "HelloLumex"));

  LumexEnvironment::EnvResult result
      = env->get_environment_variable ("LUMEX_TEST_VAR");
  EXPECT_TRUE (result.success);
  EXPECT_TRUE (result);
  EXPECT_EQ (result.value, "HelloLumex");
  EXPECT_EQ (result.error_code, 0); // No error
}

TEST_F (LumexEnvironmentTest, Get_WhenFound_ThenSuccessAndValue)
{
  ASSERT_TRUE (
      env->set_environment_variable ("LUMEX_FOUND_UNFOUND_VAR", "present"));
  LumexEnvironment::EnvResult const result
      = env->get_environment_variable ("LUMEX_FOUND_UNFOUND_VAR");
  EXPECT_TRUE (result.success);
  EXPECT_EQ (result.value, "present");
}

TEST_F (LumexEnvironmentTest, Get_WhenUnfound_ThenEmptyAndUnsuccessful)
{
  LumexEnvironment::EnvResult const result
      = env->get_environment_variable ("LUMEX_UNFOUND_VAR_9F3A2C1B");
  EXPECT_FALSE (result.success);
  EXPECT_TRUE (result.value.empty ());
}

TEST_F (
    LumexEnvironmentTest,
    GivenEmptyValue_WhenSetEnvironmentVariable_ThenVariableIsEmptyButExists)
{
#if LUMEX_OS_WINDOWS
  // On Windows, SetEnvironmentVariableA with an empty string actually deletes
  // the variable This is documented Windows behavior, so we skip this test on
  // Windows
  GTEST_SKIP () << "Windows does not support setting environment variables to "
                   "empty strings - they get deleted instead";
#else
  // Setting an empty string value should succeed, and the variable should
  // exist but be empty.
  ASSERT_TRUE (env->set_environment_variable ("LUMEX_EMPTY_VAR", ""));

  LumexEnvironment::EnvResult result
      = env->get_environment_variable ("LUMEX_EMPTY_VAR");
  EXPECT_TRUE (result.success);
  EXPECT_TRUE (result);
  EXPECT_TRUE (result.value.empty ());
#endif
}

TEST_F (LumexEnvironmentTest,
        GivenValidNameValue_WhenSetEnvironmentVariable_ThenVariableIsSet)
{
  // Test setting a new variable.
  EXPECT_TRUE (
      env->set_environment_variable ("LUMEX_ANOTHER_VAR", "AnotherValue"));
  EXPECT_EQ (env->get_environment_variable_or ("LUMEX_ANOTHER_VAR", ""),
             "AnotherValue");
}

TEST_F (
    LumexEnvironmentTest,
    GivenExistingVariable_WhenUnsetEnvironmentVariable_ThenVariableIsRemoved)
{
  // CoT: Set a variable -> unset it -> expect it to be gone.
  ASSERT_TRUE (
      env->set_environment_variable ("LUMEX_TEMP_VAR", "ValueToUnset"));
  ASSERT_TRUE (env->has_environment_variable ("LUMEX_TEMP_VAR"));

  EXPECT_TRUE (env->unset_environment_variable ("LUMEX_TEMP_VAR"));
  EXPECT_FALSE (env->has_environment_variable ("LUMEX_TEMP_VAR"));
}

TEST_F (LumexEnvironmentTest,
        GivenNonExistingVariable_WhenUnsetEnvironmentVariable_ThenReturnsTrue)
{
  // Unsetting a non-existent variable should still return true (idempotent
  // behavior).
  EXPECT_TRUE (
      env->unset_environment_variable ("LUMEX_NON_EXISTENT_VAR_TO_UNSET"));
}

TEST_F (LumexEnvironmentTest,
        GivenExistingVariable_WhenHasEnvironmentVariable_ThenReturnsTrue)
{
  ASSERT_TRUE (env->set_environment_variable ("LUMEX_HAS_VAR", "present"));
  EXPECT_TRUE (env->has_environment_variable ("LUMEX_HAS_VAR"));
}

TEST_F (LumexEnvironmentTest,
        GivenNonExistingVariable_WhenHasEnvironmentVariable_ThenReturnsFalse)
{
  EXPECT_FALSE (env->has_environment_variable ("LUMEX_HAS_NON_EXISTENT_VAR"));
}

TEST_F (
    LumexEnvironmentTest,
    GivenNonExistingVariable_WhenGetEnvironmentVariableOr_ThenReturnsDefault)
{
  std::string default_val = "MyDefault";
  std::string result = env->get_environment_variable_or (
      "LUMEX_GET_OR_NON_EXISTENT", default_val);
  EXPECT_EQ (result, default_val);
}

TEST_F (
    LumexEnvironmentTest,
    GivenExistingVariable_WhenGetEnvironmentVariableOr_ThenReturnsActualValue)
{
  ASSERT_TRUE (
      env->set_environment_variable ("LUMEX_GET_OR_EXISTENT", "ActualValue"));
  std::string result = env->get_environment_variable_or (
      "LUMEX_GET_OR_EXISTENT", "DefaultShouldNotBeUsed");
  EXPECT_EQ (result, "ActualValue");
}

TEST_F (LumexEnvironmentTest, GivenStaticGet_WhenCalled_ThenWorksCorrectly)
{
  // Test static get() convenience method
  LumexEnvironment::set ("LUMEX_STATIC_VAR", "StaticValue");
  auto result = LumexEnvironment::get ("LUMEX_STATIC_VAR");
  EXPECT_TRUE (result);
  EXPECT_EQ (result.value, "StaticValue");
}

TEST_F (LumexEnvironmentTest,
        GivenStaticSetAndHas_WhenCalled_ThenWorksCorrectly)
{
  // Test static set() and has() convenience methods
  EXPECT_TRUE (LumexEnvironment::set ("LUMEX_STATIC_SET_VAR", "StaticSetVal"));
  EXPECT_TRUE (LumexEnvironment::has ("LUMEX_STATIC_SET_VAR"));
  EXPECT_TRUE (LumexEnvironment::set (
      "LUMEX_STATIC_SET_VAR", nullptr)); // Unset via set(name, nullptr)
  EXPECT_FALSE (LumexEnvironment::has ("LUMEX_STATIC_SET_VAR"));
}

// --- set_environment_variable overwrite parameter Tests -----------------

TEST_F (
    LumexEnvironmentTest,
    GivenOverwriteDefaulted_WhenSetEnvironmentVariableTwice_ThenSecondValueWins)
{
  // CoT: overwrite defaults to true, so a second set() must replace the first
  // value
  //      (regression test for the bug where set_environment_variable used to
  //      always overwrite unconditionally with no way to opt out).
  ASSERT_TRUE (env->set_environment_variable ("LUMEX_OVERWRITE_VAR", "first"));
  ASSERT_TRUE (
      env->set_environment_variable ("LUMEX_OVERWRITE_VAR", "second"));
  EXPECT_EQ (env->get_environment_variable_or ("LUMEX_OVERWRITE_VAR", ""),
             "second");
}

TEST_F (
    LumexEnvironmentTest,
    GivenOverwriteFalseAndVariableAlreadySet_WhenSetEnvironmentVariable_ThenKeepsOriginalValue)
{
  ASSERT_TRUE (
      env->set_environment_variable ("LUMEX_OVERWRITE_VAR", "original"));

  EXPECT_TRUE (env->set_environment_variable ("LUMEX_OVERWRITE_VAR",
                                              "attempted-overwrite", false));
  EXPECT_EQ (env->get_environment_variable_or ("LUMEX_OVERWRITE_VAR", ""),
             "original");
}

TEST_F (
    LumexEnvironmentTest,
    GivenOverwriteFalseAndVariableNotSet_WhenSetEnvironmentVariable_ThenSetsValue)
{
  ASSERT_FALSE (env->has_environment_variable ("LUMEX_OVERWRITE_NEW"));

  EXPECT_TRUE (
      env->set_environment_variable ("LUMEX_OVERWRITE_NEW", "value", false));
  EXPECT_EQ (env->get_environment_variable_or ("LUMEX_OVERWRITE_NEW", ""),
             "value");
}

TEST_F (
    LumexEnvironmentTest,
    GivenOverwriteFalse_WhenSetEnvironmentVariableViaStringOverload_ThenKeepsOriginalValue)
{
  ASSERT_TRUE (env->set_environment_variable (
      std::string ("LUMEX_OVERWRITE_VAR"), std::string ("original")));

  EXPECT_TRUE (env->set_environment_variable (
      std::string ("LUMEX_OVERWRITE_VAR"), std::string ("new"), false));
  EXPECT_EQ (env->get_environment_variable_or ("LUMEX_OVERWRITE_VAR", ""),
             "original");
}

TEST_F (
    LumexEnvironmentTest,
    GivenStaticSetWithOverwriteFalse_WhenVariableAlreadySet_ThenKeepsOriginalValue)
{
  ASSERT_TRUE (LumexEnvironment::set ("LUMEX_OVERWRITE_VAR", "original"));

  EXPECT_TRUE (LumexEnvironment::set ("LUMEX_OVERWRITE_VAR", "new", false));
  EXPECT_EQ (LumexEnvironment::get_or ("LUMEX_OVERWRITE_VAR", ""), "original");
}

// --- is_environment_variable_truthy / is_truthy / is_env_* Tests --------

TEST_F (LumexEnvironmentTest,
        GivenUnsetVariable_WhenIsEnvironmentVariableTruthy_ThenReturnsFalse)
{
  ASSERT_FALSE (env->has_environment_variable ("LUMEX_TRUTHY_VAR"));
  EXPECT_FALSE (env->is_environment_variable_truthy ("LUMEX_TRUTHY_VAR"));
}

TEST_F (
    LumexEnvironmentTest,
    GivenEmptyOrZeroOrFalseValues_WhenIsEnvironmentVariableTruthy_ThenReturnsFalse)
{
  // "0", "false", "FALSE", "False" (case-insensitive) must all be falsy.
  for (std::string const &falsy_value :
       { std::string ("0"), std::string ("false"), std::string ("FALSE"),
         std::string ("False") })
    {
      ASSERT_TRUE (
          env->set_environment_variable ("LUMEX_TRUTHY_VAR", falsy_value));
      EXPECT_FALSE (env->is_environment_variable_truthy ("LUMEX_TRUTHY_VAR"))
          << "value was: " << falsy_value;
    }
}

TEST_F (
    LumexEnvironmentTest,
    GivenNonFalsyNonEmptyValues_WhenIsEnvironmentVariableTruthy_ThenReturnsTrue)
{
  for (std::string const &truthy_value :
       { std::string ("1"), std::string ("yes"), std::string ("true"),
         std::string ("TRUE"), std::string ("anything") })
    {
      ASSERT_TRUE (
          env->set_environment_variable ("LUMEX_TRUTHY_VAR", truthy_value));
      EXPECT_TRUE (env->is_environment_variable_truthy ("LUMEX_TRUTHY_VAR"))
          << "value was: " << truthy_value;
    }
}

TEST_F (LumexEnvironmentTest,
        GivenStaticIsTruthy_WhenCalled_ThenMatchesInstanceMethod)
{
  ASSERT_TRUE (env->set_environment_variable ("LUMEX_TRUTHY_VAR", "0"));
  EXPECT_FALSE (LumexEnvironment::is_truthy ("LUMEX_TRUTHY_VAR"));

  ASSERT_TRUE (env->set_environment_variable ("LUMEX_TRUTHY_VAR", "1"));
  EXPECT_TRUE (LumexEnvironment::is_truthy ("LUMEX_TRUTHY_VAR"));
}

TEST_F (LumexEnvironmentTest,
        GivenIsEnvTruthyFreeFunction_WhenCalled_ThenMatchesStaticMethod)
{
  ASSERT_TRUE (env->set_environment_variable ("LUMEX_TRUTHY_VAR", "false"));
  EXPECT_FALSE (
      lumex::core::environment::env::is_env_truthy ("LUMEX_TRUTHY_VAR"));

  ASSERT_TRUE (env->set_environment_variable ("LUMEX_TRUTHY_VAR", "yes"));
  EXPECT_TRUE (
      lumex::core::environment::env::is_env_truthy ("LUMEX_TRUTHY_VAR"));
}

TEST_F (LumexEnvironmentTest,
        GivenIsEnvSetFreeFunction_WhenVariableUnset_ThenReturnsFalse)
{
  ASSERT_FALSE (env->has_environment_variable ("LUMEX_SET_VAR"));
  EXPECT_FALSE (lumex::core::environment::env::is_env_set ("LUMEX_SET_VAR"));
}

TEST_F (
    LumexEnvironmentTest,
    GivenIsEnvSetFreeFunction_WhenVariableSetToNonEmptyValue_ThenReturnsTrue)
{
  ASSERT_TRUE (env->set_environment_variable ("LUMEX_SET_VAR", "value"));
  EXPECT_TRUE (lumex::core::environment::env::is_env_set ("LUMEX_SET_VAR"));
}

// --- Edge & Corner Cases ------------------------------------------------

TEST_F (LumexEnvironmentTest,
        GivenNullName_WhenGetEnvironmentVariable_ThenReturnsUnsuccessful)
{
  LumexEnvironment::EnvResult result = env->get_environment_variable (nullptr);
  EXPECT_FALSE (result.success);
  EXPECT_TRUE (result.value.empty ());
#if LUMEX_OS_WINDOWS
  // Windows GetEnvironmentVariableA returns 0 for null, GetLastError might be
  // ERROR_INVALID_PARAMETER (87) or ERROR_BAD_ENVIRONMENT (10), but
  // LumexEnvironment maps null to -1.
  EXPECT_EQ (result.error_code, -1);
#else
  // POSIX getenv with nullptr should not happen, but LumexEnvironment maps it
  // to -1.
  EXPECT_EQ (result.error_code, -1);
#endif
}

TEST_F (LumexEnvironmentTest,
        GivenEmptyName_WhenGetEnvironmentVariable_ThenReturnsUnsuccessful)
{
  LumexEnvironment::EnvResult result = env->get_environment_variable ("");
  EXPECT_FALSE (result.success);
  EXPECT_TRUE (result.value.empty ());
#if LUMEX_OS_WINDOWS
  // _dupenv_s handles "" as invalid, GetEnvironmentVariableA might return
  // success with empty string, but LumexEnvironment maps it to
  // ERROR_INVALID_PARAMETER (87).
  EXPECT_EQ (result.error_code, ERROR_INVALID_PARAMETER);
#else
  // getenv("") is undefined behavior, LumexEnvironment maps it to -1.
  EXPECT_EQ (result.error_code, -1);
#endif
}

TEST_F (LumexEnvironmentTest,
        GivenNullName_WhenSetEnvironmentVariable_ThenReturnsFalse)
{
  EXPECT_FALSE (env->set_environment_variable (nullptr, "SomeValue"));
}

TEST_F (LumexEnvironmentTest,
        GivenEmptyName_WhenSetEnvironmentVariable_ThenReturnsFalse)
{
  EXPECT_FALSE (env->set_environment_variable ("", "SomeValue"));
}

TEST_F (LumexEnvironmentTest,
        GivenNullName_WhenUnsetEnvironmentVariable_ThenReturnsFalse)
{
  EXPECT_FALSE (env->unset_environment_variable (nullptr));
}

TEST_F (LumexEnvironmentTest,
        GivenEmptyName_WhenUnsetEnvironmentVariable_ThenReturnsFalse)
{
  EXPECT_FALSE (env->unset_environment_variable (""));
}

TEST_F (LumexEnvironmentTest,
        GivenVeryLongValue_WhenSetAndGet_ThenWorksCorrectly)
{
  // Test setting and retrieving a very long environment variable value.
  // MAX_ENV_BUFFER_SIZE is 32767. Let's use something close to it.
  std::string long_value (LumexEnvironment::MAX_ENV_BUFFER_SIZE - 100, 'X');
  ASSERT_TRUE (
      env->set_environment_variable ("LUMEX_LONG_VAR", long_value.c_str ()));

  LumexEnvironment::EnvResult result
      = env->get_environment_variable ("LUMEX_LONG_VAR");
  EXPECT_TRUE (result.success);
  EXPECT_EQ (result.value, long_value);

  // Test with value exactly at max size
  std::string max_value (LumexEnvironment::MAX_ENV_BUFFER_SIZE - 1,
                         'Y'); // Null terminator counts for Windows
  ASSERT_TRUE (
      env->set_environment_variable ("LUMEX_MAX_VAR", max_value.c_str ()));
  result = env->get_environment_variable ("LUMEX_MAX_VAR");
  EXPECT_TRUE (result.success);
  EXPECT_EQ (result.value, max_value);

#if LUMEX_OS_WINDOWS
  // On Windows, if GetEnvironmentVariableA returns size > MAX_ENV_BUFFER_SIZE,
  // LumexEnvironment returns ERROR_BUFFER_OVERFLOW. Test this scenario if
  // possible. However, directly testing overflow is hard without external
  // manipulation or a mock. The current implementation guards against local
  // buffer overflow, but not the system's limit.
#else
  // On POSIX, if std::strlen(result) > MAX_ENV_BUFFER_SIZE, it returns -3 (Too
  // large). This is hard to trigger without setting an environment variable
  // larger than the internal buffer, which putenv/setenv might also limit.
#endif
}

TEST_F (LumexEnvironmentTest,
        GivenNullValue_WhenSetEnvironmentVariable_ThenUnsetsVariable)
{
  // set_environment_variable with nullptr value should unset the variable.
  ASSERT_TRUE (
      env->set_environment_variable ("LUMEX_UNSET_VIA_NULL", "InitialValue"));
  EXPECT_TRUE (env->has_environment_variable ("LUMEX_UNSET_VIA_NULL"));

  EXPECT_TRUE (
      env->set_environment_variable ("LUMEX_UNSET_VIA_NULL", nullptr));
  EXPECT_FALSE (env->has_environment_variable ("LUMEX_UNSET_VIA_NULL"));
}

// --- Concurrency Tests --------------------------------------------------

TEST_F (LumexEnvironmentTest, ThreadSafety_SimultaneousReads)
{
  // Test that multiple threads can read environment variables concurrently
  // without issues.
  ASSERT_TRUE (
      env->set_environment_variable ("LUMEX_CONCURRENCY_READ", "SharedValue"));

  constexpr int num_threads = 10;
  std::vector<std::thread> threads;
  std::vector<std::string> results (num_threads);
  std::vector<bool> successes (num_threads);

  for (int i = 0; i < num_threads; ++i)
    {
      threads.emplace_back ([this, i, &results, &successes] () {
        LumexEnvironment::EnvResult res
            = env->get_environment_variable ("LUMEX_CONCURRENCY_READ");
        successes[i] = res.success;
        if (res.success)
          results[i] = res.value;
      });
    }

  for (auto &t : threads)
    t.join ();

  for (int i = 0; i < num_threads; ++i)
    {
      EXPECT_TRUE (successes[i]) << "Thread " << i << " failed to read";
      EXPECT_EQ (results[i], "SharedValue")
          << "Thread " << i << " read incorrect value";
    }
}

TEST_F (LumexEnvironmentTest, ThreadSafety_SimultaneousWritesAndReads)
{
  // Test concurrent writes and reads. While set/unset are mutex-protected,
  // reading system-wide environment variables can still have race conditions
  // with other processes or non-mutexed calls. This test verifies the mutex
  // within LumexEnvironment.
  constexpr int num_threads = 20;
  std::vector<std::thread> threads;

  for (int i = 0; i < num_threads; ++i)
    {
      threads.emplace_back ([this, i] () {
        std::string var_name = "LUMEX_CONCURRENCY_VAR_" + std::to_string (i);
        std::string var_value = "Value_" + std::to_string (i);

        // Set
        EXPECT_TRUE (env->set_environment_variable (var_name.c_str (),
                                                    var_value.c_str ()));
        // Read
        LumexEnvironment::EnvResult res
            = env->get_environment_variable (var_name.c_str ());
        EXPECT_TRUE (res.success) << "Failed to read " << var_name;
        EXPECT_EQ (res.value, var_value)
            << "Read incorrect value for " << var_name;
        // Unset
        EXPECT_TRUE (env->unset_environment_variable (var_name.c_str ()));
        // Verify unset
        EXPECT_FALSE (env->has_environment_variable (var_name.c_str ()));
      });
    }

  for (auto &t : threads)
    t.join ();
}

// --- Performance & Stress Tests -----------------------------------------

TEST_F (LumexEnvironmentTest, Perf_RepeatedGetOperations)
{
#if LUMEX_PERF_WALL_CLOCK_ENABLED
  // Measure performance of repeated get operations for an existing variable.
  ASSERT_TRUE (env->set_environment_variable ("LUMEX_PERF_VAR",
                                              "PerformanceTestValue"));

  constexpr int iterations = 10000;
  auto start = std::chrono::high_resolution_clock::now ();
  for (int i = 0; i < iterations; ++i)
    {
      LumexEnvironment::EnvResult result
          = env->get_environment_variable ("LUMEX_PERF_VAR");
      // Minimal assertion to avoid skewing performance, but ensure correctness
      EXPECT_TRUE (result.success);
    }
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds> (
      std::chrono::high_resolution_clock::now () - start);

  // Isolated runs sit around 60 ms. Full parallel ctest on a busy box can
  // add another 70+ ms of scheduler delay without the lookup path itself
  // slowing down; 250 ms still flags a 4x regression.
  EXPECT_LT (duration.count (), 250)
      << "Repeated GET operations took too long: " << duration.count ()
      << "ms";
#else
  GTEST_SKIP ()
      << "wall-clock Perf_* thresholds are Release-only (no sanitizers)";
#endif
}

TEST_F (LumexEnvironmentTest, Perf_RepeatedSetAndUnsetOperations)
{
#if LUMEX_PERF_WALL_CLOCK_ENABLED
  // Measure performance of repeated set and unset operations.
  constexpr int iterations
      = 1000; // Fewer iterations due to higher cost of set/unset
  auto start = std::chrono::high_resolution_clock::now ();
  for (int i = 0; i < iterations; ++i)
    {
      std::string var_name = "LUMEX_PERF_SET_UNSET_" + std::to_string (i);
      std::string var_value = "Value_" + std::to_string (i);

      EXPECT_TRUE (env->set_environment_variable (var_name.c_str (),
                                                  var_value.c_str ()));
      EXPECT_TRUE (env->unset_environment_variable (var_name.c_str ()));
    }
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds> (
      std::chrono::high_resolution_clock::now () - start);

  EXPECT_LT (duration.count (),
             1000) // Expect it to be reasonably fast, e.g., under 1000ms
      << "Repeated SET/UNSET operations took too long: " << duration.count ()
      << "ms";
#else
  GTEST_SKIP ()
      << "wall-clock Perf_* thresholds are Release-only (no sanitizers)";
#endif
}

// --- Memory & Lifetime Auditor Tests ------------------------------------

// For EnvResult struct, since it uses std::string, its memory management is
// handled by STL. We just need to ensure our usage doesn't lead to issues.

TEST_F (LumexEnvironmentTest, MemorySafety_EnvResultDestructorCalled)
{
  // Ensure EnvResult objects are properly destructed when going out of scope.
  // This is implicitly tested by normal usage, but we can make it explicit.
  {
    LumexEnvironment::EnvResult result
        = env->get_environment_variable ("PATH");
    // result goes out of scope here. No manual delete needed.
  }
  SUCCEED (); // If no crash or leak detected, test passes
}
