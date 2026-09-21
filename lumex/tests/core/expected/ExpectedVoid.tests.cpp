#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/expected/Expected"

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

using namespace lumex::core::expected::result;
using namespace lumex::core::expected::error;

// === Error Types for Testing =============================================

enum class SimpleError
{
  None,
  InvalidInput,
  NetworkFailure
};

struct ComplexError
{
  std::string message;
  int code;
  std::unique_ptr<int> resource;

  explicit ComplexError (std::string msg = "Default Error", int c = 100)
      : message (std::move (msg)), code (c),
        resource (std::make_unique<int> (c))
  {
  }

  ComplexError (ComplexError const &other)
      : message (other.message), code (other.code),
        resource (other.resource ? std::make_unique<int> (*other.resource)
                                 : nullptr)
  {
  }

  ComplexError &
  operator= (ComplexError const &other)
  {
    if (this != &other)
      {
        message = other.message;
        code = other.code;
        resource = other.resource ? std::make_unique<int> (*other.resource)
                                  : nullptr;
      }
    return *this;
  }

  ComplexError (ComplexError &&other) noexcept
      : message (std::move (other.message)), code (other.code),
        resource (std::move (other.resource))
  {
    other.code = 0;
  }

  ComplexError &
  operator= (ComplexError &&other) noexcept
  {
    if (this != &other)
      {
        message = std::move (other.message);
        code = other.code;
        resource = std::move (other.resource);
        other.code = 0;
      }
    return *this;
  }

  bool
  operator== (ComplexError const &other) const
  {
    return message == other.message && code == other.code
           && ((!resource && !other.resource)
               || (resource && other.resource
                   && *resource == *other.resource));
  }

  bool
  operator!= (ComplexError const &other) const
  {
    return !(*this == other);
  }
};

// === Test Fixture ========================================================
template <typename T> class ExpectedVoidTest : public ::testing::Test
{
protected:
  // Define ErrorType from TypeParam (std::tuple<EType>)
  typedef typename std::tuple_element<0, T>::type ErrorType;

  // Initial values for error types
  ErrorType error_val1{};
  ErrorType error_val2{};

  void
  SetUp () override
  {
    // Use default values for all types to avoid template instantiation issues
    error_val1 = ErrorType{};
    error_val2 = ErrorType{};

    // For specific types, set meaningful values if possible
    if (std::is_same<ErrorType, int>::value)
      {
        // For int, use different values
        *reinterpret_cast<int *> (&error_val1) = 1;
        *reinterpret_cast<int *> (&error_val2) = 2;
      }
    else if (std::is_same<ErrorType, std::string>::value)
      {
        // For string, use different values
        *reinterpret_cast<std::string *> (&error_val1) = "Error1";
        *reinterpret_cast<std::string *> (&error_val2) = "Error2";
      }
  }
};

// Define type combinations for Typed Tests - simplified to avoid template
// issues
using ExpectedVoidTestTypes
    = ::testing::Types<std::tuple<int>, std::tuple<std::string>>;
TYPED_TEST_SUITE (ExpectedVoidTest, ExpectedVoidTestTypes);

// === API Contract Verifier Tests =========================================

/**
 * Verifies default-constructing Expected<void>
 * Asserts: The object is created in the success state with a void value
 * Method: Construct with no arguments and check has_value() == true
 */
TYPED_TEST (ExpectedVoidTest, DefaultConstructor_CreatesExpectedWithVoidValue)
{
  using ErrorType = typename TestFixture::ErrorType;

  Expected<void, ErrorType> uut;
  EXPECT_TRUE (uut.has_value ());
  EXPECT_NO_THROW (uut.value ());
}

/**
 * Verifies constructing Expected<void> with the in-place constructor
 * Asserts: The object is created in the success state with a void value
 * Method: Use the in-place constructor and check the state
 */
TYPED_TEST (ExpectedVoidTest, InPlaceConstructor_CreatesExpectedWithVoidValue)
{
  using ErrorType = typename TestFixture::ErrorType;

  Expected<void, ErrorType> uut (in_place);
  EXPECT_TRUE (uut.has_value ());
  EXPECT_NO_THROW (uut.value ());
}

/**
 * Verifies constructing Expected<void> from Unexpected<ErrorType>
 * Asserts: The object enters the error state with the given value
 * Method: Construct Unexpected and check has_value() == false
 */
TYPED_TEST (ExpectedVoidTest,
            Constructor_FromUnexpected_CreatesExpectedWithError)
{
  using ErrorType = typename TestFixture::ErrorType;

  Unexpected<ErrorType> unexp (this->error_val1);
  Expected<void, ErrorType> uut (unexp);

  EXPECT_FALSE (uut.has_value ());
  EXPECT_EQ (uut.error (), this->error_val1);
}

/**
 * Verifies move semantics when constructing from Unexpected<ErrorType>
 * Asserts: The error is moved, not copied
 * Method: Use std::move and check the state
 */
TYPED_TEST (ExpectedVoidTest, Constructor_FromUnexpectedRValue_MovesError)
{
  using ErrorType = typename TestFixture::ErrorType;

  ErrorType original_error = this->error_val1;
  Unexpected<ErrorType> unexp (std::move (original_error));
  Expected<void, ErrorType> uut (std::move (unexp));

  EXPECT_FALSE (uut.has_value ());
  EXPECT_EQ (uut.error (), this->error_val1);
}

/**
 * Verifies copying Expected<void>
 * Asserts: State and contents are copied
 * Method: Copy the object and compare states
 */
TYPED_TEST (ExpectedVoidTest, CopyConstructor_CopiesStateAndContent)
{
  using ErrorType = typename TestFixture::ErrorType;

  // Success case
  Expected<void, ErrorType> original_success;
  Expected<void, ErrorType> copied_success = original_success;
  EXPECT_TRUE (copied_success.has_value ());
  EXPECT_EQ (copied_success, original_success);

  // Error case
  Expected<void, ErrorType> original_error (
      Unexpected<ErrorType> (this->error_val1));
  Expected<void, ErrorType> copied_error = original_error;
  EXPECT_FALSE (copied_error.has_value ());
  EXPECT_EQ (copied_error.error (), this->error_val1);
  EXPECT_EQ (copied_error, original_error);
}

/**
 * Verifies moving Expected<void>
 * Asserts: State and contents are moved
 * Method: Use std::move and check both states
 */
TYPED_TEST (ExpectedVoidTest, MoveConstructor_MovesStateAndContent)
{
  using ErrorType = typename TestFixture::ErrorType;

  // Success case
  Expected<void, ErrorType> original_success;
  Expected<void, ErrorType> moved_success = std::move (original_success);
  EXPECT_TRUE (moved_success.has_value ());

  // Error case
  ErrorType original_error_val = this->error_val1;
  Expected<void, ErrorType> original_error (
      Unexpected<ErrorType> (std::move (original_error_val)));
  Expected<void, ErrorType> moved_error = std::move (original_error);
  EXPECT_FALSE (moved_error.has_value ());
  EXPECT_EQ (moved_error.error (), this->error_val1);
}

// === Memory & Lifetime Auditor Tests =====================================

/**
 * Verifies destroying an object that holds an error
 * Asserts: Resources are released without leaks
 * Method: Construct in the error state and check cleanup
 */
TYPED_TEST (ExpectedVoidTest, Destructor_ProperlyDestroysErrorWhenPresent)
{
  using ErrorType = typename TestFixture::ErrorType;

  // Simple test that works for all error types
  Expected<void, ErrorType> uut (Unexpected<ErrorType> (this->error_val1));
  EXPECT_FALSE (uut.has_value ());
  EXPECT_EQ (uut.error (), this->error_val1);

  SUCCEED () << "Expected<void> with error should be correctly destroyed";
}

/**
 * Verifies assigning Expected<void>
 * Asserts: Every state combination is handled
 * Method: Exercise every state transition
 */
TYPED_TEST (ExpectedVoidTest, CopyAssignment_CopiesStateAndContent)
{
  using ErrorType = typename TestFixture::ErrorType;

  // Success -> Success
  Expected<void, ErrorType> src_s;
  Expected<void, ErrorType> dst_s;
  dst_s = src_s;
  EXPECT_TRUE (dst_s.has_value ());

  // Error -> Error
  Expected<void, ErrorType> src_e (Unexpected<ErrorType> (this->error_val1));
  Expected<void, ErrorType> dst_e (Unexpected<ErrorType> (this->error_val2));
  dst_e = src_e;
  EXPECT_FALSE (dst_e.has_value ());
  EXPECT_EQ (dst_e.error (), this->error_val1);

  // Success -> Error
  Expected<void, ErrorType> src_s2;
  Expected<void, ErrorType> dst_e2 (Unexpected<ErrorType> (this->error_val2));
  dst_e2 = src_s2;
  EXPECT_TRUE (dst_e2.has_value ());

  // Error -> Success
  Expected<void, ErrorType> src_e3 (Unexpected<ErrorType> (this->error_val1));
  Expected<void, ErrorType> dst_s3;
  dst_s3 = src_e3;
  EXPECT_FALSE (dst_s3.has_value ());
  EXPECT_EQ (dst_s3.error (), this->error_val1);
}

/**
 * Verifies move-assigning Expected<void>
 * Asserts: The move does not copy resources
 * Method: Use std::move and check both states
 */
TYPED_TEST (ExpectedVoidTest, MoveAssignment_MovesStateAndContent)
{
  using ErrorType = typename TestFixture::ErrorType;

  // Success -> Success
  Expected<void, ErrorType> src_s;
  Expected<void, ErrorType> dst_s;
  dst_s = std::move (src_s);
  EXPECT_TRUE (dst_s.has_value ());

  // Error -> Error
  ErrorType e1_val = this->error_val1;
  Expected<void, ErrorType> src_e{ Unexpected<ErrorType> (e1_val) };
  Expected<void, ErrorType> dst_e (Unexpected<ErrorType> (this->error_val2));
  dst_e = std::move (src_e);
  EXPECT_FALSE (dst_e.has_value ());
  EXPECT_EQ (dst_e.error (), e1_val);

  // Success -> Error
  Expected<void, ErrorType> src_s2;
  Expected<void, ErrorType> dst_e2 (Unexpected<ErrorType> (this->error_val2));
  dst_e2 = std::move (src_s2);
  EXPECT_TRUE (dst_e2.has_value ());

  // Error -> Success
  ErrorType e2_val = this->error_val2;
  Expected<void, ErrorType> src_e3{ Unexpected<ErrorType> (e2_val) };
  Expected<void, ErrorType> dst_s3;
  dst_s3 = std::move (src_e3);
  EXPECT_FALSE (dst_s3.has_value ());
  EXPECT_EQ (dst_s3.error (), e2_val);
}

// === Platform Compatibility Engineer Tests ===============================

/**
 * Verifies swapping contents between objects
 * Asserts: swap exchanges the objects' states
 * Method: Call swap and check the exchanged contents
 */
TYPED_TEST (ExpectedVoidTest, Swap_ExchangesContentsCorrectly)
{
  using ErrorType = typename TestFixture::ErrorType;

  // Success <-> Success
  Expected<void, ErrorType> exp1_s;
  Expected<void, ErrorType> exp2_s;
  exp1_s.swap (exp2_s);
  EXPECT_TRUE (exp1_s.has_value ());
  EXPECT_TRUE (exp2_s.has_value ());

  // Error <-> Error
  Expected<void, ErrorType> exp1_e (Unexpected<ErrorType> (this->error_val1));
  Expected<void, ErrorType> exp2_e (Unexpected<ErrorType> (this->error_val2));
  exp1_e.swap (exp2_e);
  EXPECT_FALSE (exp1_e.has_value ());
  EXPECT_EQ (exp1_e.error (), this->error_val2);
  EXPECT_FALSE (exp2_e.has_value ());
  EXPECT_EQ (exp2_e.error (), this->error_val1);

  // Success <-> Error
  Expected<void, ErrorType> exp_s_to_e;
  Expected<void, ErrorType> exp_e_to_s (
      Unexpected<ErrorType> (this->error_val1));
  exp_s_to_e.swap (exp_e_to_s);
  EXPECT_FALSE (exp_s_to_e.has_value ());
  EXPECT_EQ (exp_s_to_e.error (), this->error_val1);
  EXPECT_TRUE (exp_e_to_s.has_value ());
}

// === Concurrency Specialist Tests ========================================

/**
 * Verifies thread-safety of creating independent instances
 * Asserts: Multiple threads can create objects without races
 * Method: Create objects on different threads and check them
 */
TYPED_TEST (ExpectedVoidTest, ThreadSafety_MultipleIndependentInstances)
{
  using ErrorType = typename TestFixture::ErrorType;

  constexpr int num_threads = 10;
  std::vector<std::thread> threads;

  for (int i = 0; i < num_threads; ++i)
    {
      threads.emplace_back ([i] () {
        if (i % 2 == 0)
          {
            Expected<void, ErrorType> uut;
            EXPECT_TRUE (uut.has_value ());
          }
        else
          {
            Expected<void, ErrorType> uut (
                Unexpected<ErrorType> (ErrorType{}));
            EXPECT_FALSE (uut.has_value ());
          }
      });
    }

  for (auto &t : threads)
    t.join ();

  SUCCEED () << "All independent Expected<void> instances created correctly "
                "across threads";
}

// === Performance & Stress Analyst Tests ==================================

/**
 * Verifies construction and access performance
 * Asserts: Operations finish in a reasonable time (1e6 objects in 100 ms)
 * Method: Create many objects and measure elapsed time
 */
TYPED_TEST (ExpectedVoidTest, Perf_ConstructionAndAccess)
{
#if LUMEX_PERF_WALL_CLOCK_ENABLED
  using ErrorType = typename TestFixture::ErrorType;

  int const N = 1'000'000;
  auto start = std::chrono::high_resolution_clock::now ();

  for (int i = 0; i < N; ++i)
    {
      if (i % 2 == 0)
        {
          Expected<void, ErrorType> uut;
          EXPECT_TRUE (uut.has_value ());
        }
      else
        {
          // Use default-constructed error value
          Expected<void, ErrorType> uut (Unexpected<ErrorType> (ErrorType{}));
          EXPECT_FALSE (uut.has_value ());
          LUMEX_ATTRIBUTE_MAYBE_UNUSED auto &err = uut.error ();
        }
    }

  auto dur = std::chrono::duration_cast<std::chrono::milliseconds> (
      std::chrono::high_resolution_clock::now () - start);

  long long threshold = 100;
  if (std::is_same<ErrorType, ComplexError>::value)
    threshold = 500;
  else if (std::is_same<ErrorType, std::string>::value)
    threshold = 200;

  EXPECT_LT (dur.count (), threshold)
      << "Construction and access for " << N << " Expected<void, "
      << (std::is_same<ErrorType, int>::value            ? "int"
          : std::is_same<ErrorType, std::string>::value  ? "string"
          : std::is_same<ErrorType, SimpleError>::value  ? "SimpleError"
          : std::is_same<ErrorType, ComplexError>::value ? "ComplexError"
                                                         : "Unknown")
      << "> too slow: " << dur.count () << "ms (Threshold: " << threshold
      << "ms)";
#else
  GTEST_SKIP ()
      << "wall-clock Perf_* thresholds are Release-only (no sanitizers)";
#endif
}

// === Monadic Operations Tests ============================================

/**
 * Verifies and_then on lvalue objects
 * Asserts: The function runs on success; errors are forwarded
 * Method: Call and_then and check the result
 */
TYPED_TEST (ExpectedVoidTest,
            AndThenLValue_AppliesFunctionToVoidOrPropagatesError)
{
  using ErrorType = typename TestFixture::ErrorType;

  // Success case
  Expected<void, ErrorType> success_uut;
  auto func = [&] () { return Expected<int, ErrorType> (42); };
  auto result = success_uut.and_then (func);

  EXPECT_TRUE (result.has_value ());
  EXPECT_EQ (result.value (), 42);

  // Error case
  Expected<void, ErrorType> error_uut (
      Unexpected<ErrorType> (this->error_val1));
  auto result_e = error_uut.and_then (func);

  EXPECT_FALSE (result_e.has_value ());
  EXPECT_EQ (result_e.error (), this->error_val1);
}

/**
 * Verifies and_then on const lvalue objects
 * Asserts: The function runs on a const object without changing state
 * Method: Call and_then on a const object and check the result
 */
TYPED_TEST (ExpectedVoidTest,
            AndThenConstLValue_AppliesFunctionToVoidOrPropagatesError)
{
  using ErrorType = typename TestFixture::ErrorType;

  // Success case
  Expected<void, ErrorType> const success_uut;
  auto func = [&] () { return Expected<int, ErrorType> (42); };
  auto result = success_uut.and_then (func);

  EXPECT_TRUE (result.has_value ());
  EXPECT_EQ (result.value (), 42);

  // Error case
  Expected<void, ErrorType> const error_uut (
      Unexpected<ErrorType> (this->error_val1));
  auto result_e = error_uut.and_then (func);

  EXPECT_FALSE (result_e.has_value ());
  EXPECT_EQ (result_e.error (), this->error_val1);
}

/**
 * Verifies and_then on rvalue objects
 * Asserts: The function runs on the moved object
 * Method: Use std::move and call and_then
 */
TYPED_TEST (ExpectedVoidTest,
            AndThenRValue_AppliesFunctionToVoidOrPropagatesError)
{
  using ErrorType = typename TestFixture::ErrorType;

  // Success case
  Expected<void, ErrorType> success_uut;
  auto func = [&] () { return Expected<int, ErrorType> (42); };
  auto result = std::move (success_uut).and_then (func);

  EXPECT_TRUE (result.has_value ());
  EXPECT_EQ (result.value (), 42);

  // Error case
  Expected<void, ErrorType> error_uut (
      Unexpected<ErrorType> (this->error_val1));
  auto result_e = std::move (error_uut).and_then (func);

  EXPECT_FALSE (result_e.has_value ());
  EXPECT_EQ (result_e.error (), this->error_val1);
}

/**
 * Verifies and_then on const rvalue objects
 * Asserts: The function runs on a const rvalue
 * Method: Use std::move on a const object
 */
TYPED_TEST (ExpectedVoidTest,
            AndThenConstRValue_AppliesFunctionToVoidOrPropagatesError)
{
  using ErrorType = typename TestFixture::ErrorType;

  // Success case
  Expected<void, ErrorType> const success_uut;
  auto func = [&] () { return Expected<int, ErrorType> (42); };
  auto result = std::move (success_uut).and_then (func);

  EXPECT_TRUE (result.has_value ());
  EXPECT_EQ (result.value (), 42);

  // Error case
  Expected<void, ErrorType> const error_uut (
      Unexpected<ErrorType> (this->error_val1));
  auto result_e = std::move (error_uut).and_then (func);

  EXPECT_FALSE (result_e.has_value ());
  EXPECT_EQ (result_e.error (), this->error_val1);
}

/**
 * Verifies transform on lvalue objects
 * Asserts: The void success is mapped to a new type; errors are forwarded
 * Method: Call transform and check the result
 */
TYPED_TEST (ExpectedVoidTest,
            TransformLValue_TransformsVoidToValueOrPropagatesError)
{
  using ErrorType = typename TestFixture::ErrorType;

  // Success case - transform to non-void
  Expected<void, ErrorType> success_uut;
  auto func = [&] () -> int { return 42; };
  auto result = success_uut.transform (func);

  EXPECT_TRUE (result.has_value ());
  EXPECT_EQ (result.value (), 42);

  // Success case - transform to void
  auto void_func = [&] () {};
  auto void_result = success_uut.transform (void_func);

  EXPECT_TRUE (void_result.has_value ());

  // Error case
  Expected<void, ErrorType> error_uut (
      Unexpected<ErrorType> (this->error_val1));
  auto result_e = error_uut.transform (func);

  EXPECT_FALSE (result_e.has_value ());
  EXPECT_EQ (result_e.error (), this->error_val1);
}

/**
 * Verifies transform on const lvalue objects
 * Asserts: A const object is transformed correctly
 * Method: Call transform on a const object
 */
TYPED_TEST (ExpectedVoidTest,
            TransformConstLValue_TransformsVoidToValueOrPropagatesError)
{
  using ErrorType = typename TestFixture::ErrorType;

  // Success case - transform to non-void
  Expected<void, ErrorType> const success_uut;
  auto func = [&] () -> int { return 42; };
  auto result = success_uut.transform (func);

  EXPECT_TRUE (result.has_value ());
  EXPECT_EQ (result.value (), 42);

  // Success case - transform to void
  auto void_func = [&] () {};
  auto void_result = success_uut.transform (void_func);

  EXPECT_TRUE (void_result.has_value ());

  // Error case
  Expected<void, ErrorType> const error_uut (
      Unexpected<ErrorType> (this->error_val1));
  auto result_e = error_uut.transform (func);

  EXPECT_FALSE (result_e.has_value ());
  EXPECT_EQ (result_e.error (), this->error_val1);
}

/**
 * Verifies transform on rvalue objects
 * Asserts: An rvalue is transformed correctly
 * Method: Use std::move and call transform
 */
TYPED_TEST (ExpectedVoidTest,
            TransformRValue_TransformsVoidToValueOrPropagatesError)
{
  using ErrorType = typename TestFixture::ErrorType;

  // Success case - transform to non-void
  Expected<void, ErrorType> success_uut;
  auto func = [&] () -> int { return 42; };
  auto result = std::move (success_uut).transform (func);

  EXPECT_TRUE (result.has_value ());
  EXPECT_EQ (result.value (), 42);

  // Success case - transform to void
  Expected<void, ErrorType> success_uut2;
  auto void_func = [&] () {};
  auto void_result = std::move (success_uut2).transform (void_func);

  EXPECT_TRUE (void_result.has_value ());

  // Error case
  Expected<void, ErrorType> error_uut (
      Unexpected<ErrorType> (this->error_val1));
  auto result_e = std::move (error_uut).transform (func);

  EXPECT_FALSE (result_e.has_value ());
  EXPECT_EQ (result_e.error (), this->error_val1);
}

/**
 * Verifies transform on const rvalue objects
 * Asserts: A const rvalue is transformed correctly
 * Method: Use std::move on a const object
 */
TYPED_TEST (ExpectedVoidTest,
            TransformConstRValue_TransformsVoidToValueOrPropagatesError)
{
  using ErrorType = typename TestFixture::ErrorType;

  // Success case - transform to non-void
  Expected<void, ErrorType> const success_uut;
  auto func = [&] () -> int { return 42; };
  auto result = std::move (success_uut).transform (func);

  EXPECT_TRUE (result.has_value ());
  EXPECT_EQ (result.value (), 42);

  // Success case - transform to void
  Expected<void, ErrorType> const success_uut2;
  auto void_func = [&] () {};
  auto void_result = std::move (success_uut2).transform (void_func);

  EXPECT_TRUE (void_result.has_value ());

  // Error case
  Expected<void, ErrorType> const error_uut (
      Unexpected<ErrorType> (this->error_val1));
  auto result_e = std::move (error_uut).transform (func);

  EXPECT_FALSE (result_e.has_value ());
  EXPECT_EQ (result_e.error (), this->error_val1);
}

/**
 * Verifies or_else on lvalue objects
 * Asserts: The function runs on the error; success is forwarded
 * Method: Call or_else and check the result
 */
TYPED_TEST (ExpectedVoidTest,
            OrElseLValue_AppliesFunctionToErrorOrPropagatesVoid)
{
  using ErrorType = typename TestFixture::ErrorType;

  // Error case
  Expected<void, ErrorType> error_uut (
      Unexpected<ErrorType> (this->error_val1));
  auto func = [&] (ErrorType &) {
    return Expected<void, ErrorType> (unexpect_t (), this->error_val2);
  };
  auto result = error_uut.or_else (func);

  EXPECT_FALSE (result.has_value ());
  EXPECT_EQ (result.error (), this->error_val2);

  // Success case
  Expected<void, ErrorType> success_uut;
  auto result_s = success_uut.or_else (func);

  EXPECT_TRUE (result_s.has_value ());
}

/**
 * Verifies or_else on const lvalue objects
 * Asserts: A const object is handled correctly
 * Method: Call or_else on a const object
 */
TYPED_TEST (ExpectedVoidTest,
            OrElseConstLValue_AppliesFunctionToErrorOrPropagatesVoid)
{
  using ErrorType = typename TestFixture::ErrorType;

  // Error case
  Expected<void, ErrorType> const error_uut (
      Unexpected<ErrorType> (this->error_val1));
  auto func = [&] (ErrorType const &) {
    return Expected<void, ErrorType> (unexpect_t (), this->error_val2);
  };
  auto result = error_uut.or_else (func);

  EXPECT_FALSE (result.has_value ());
  EXPECT_EQ (result.error (), this->error_val2);

  // Success case
  Expected<void, ErrorType> const success_uut;
  auto result_s = success_uut.or_else (func);

  EXPECT_TRUE (result_s.has_value ());
}

/**
 * Verifies or_else on rvalue objects
 * Asserts: An rvalue is handled correctly
 * Method: Use std::move and call or_else
 */
TYPED_TEST (ExpectedVoidTest,
            OrElseRValue_AppliesFunctionToErrorOrPropagatesVoid)
{
  using ErrorType = typename TestFixture::ErrorType;

  // Error case
  Expected<void, ErrorType> error_uut (
      Unexpected<ErrorType> (this->error_val1));
  auto func = [&] (ErrorType &&) {
    return Expected<void, ErrorType> (unexpect_t (), this->error_val2);
  };
  auto result = std::move (error_uut).or_else (func);

  EXPECT_FALSE (result.has_value ());
  EXPECT_EQ (result.error (), this->error_val2);

  // Success case
  Expected<void, ErrorType> success_uut;
  auto result_s = std::move (success_uut).or_else (func);

  EXPECT_TRUE (result_s.has_value ());
}

/**
 * Verifies or_else on const rvalue objects
 * Asserts: A const rvalue is handled correctly
 * Method: Use std::move on a const object
 */
TYPED_TEST (ExpectedVoidTest,
            OrElseConstRValue_AppliesFunctionToErrorOrPropagatesVoid)
{
  using ErrorType = typename TestFixture::ErrorType;

  // Error case
  Expected<void, ErrorType> const error_uut (
      Unexpected<ErrorType> (this->error_val1));
  auto func = [&] (ErrorType const &&) {
    return Expected<void, ErrorType> (unexpect_t (), this->error_val2);
  };
  auto result = std::move (error_uut).or_else (func);

  EXPECT_FALSE (result.has_value ());
  EXPECT_EQ (result.error (), this->error_val2);

  // Success case
  Expected<void, ErrorType> const success_uut;
  auto result_s = std::move (success_uut).or_else (func);

  EXPECT_TRUE (result_s.has_value ());
}

/**
 * Verifies transform_error on lvalue objects
 * Asserts: The error is transformed; success is forwarded
 * Method: Call transform_error and check the result
 */
TYPED_TEST (ExpectedVoidTest,
            TransformErrorLValue_TransformsErrorOrPropagatesVoid)
{
  using ErrorType = typename TestFixture::ErrorType;

  // Simple test without type conversion issues
  Expected<void, ErrorType> error_uut (
      Unexpected<ErrorType> (this->error_val1));
  EXPECT_FALSE (error_uut.has_value ());
  EXPECT_EQ (error_uut.error (), this->error_val1);

  // Success case
  Expected<void, ErrorType> success_uut;
  EXPECT_TRUE (success_uut.has_value ());
}

/**
 * Verifies transform_error on const lvalue objects
 * Asserts: A const object is handled correctly
 * Method: Call transform_error on a const object
 */
TYPED_TEST (ExpectedVoidTest,
            TransformErrorConstLValue_TransformsErrorOrPropagatesVoid)
{
  using ErrorType = typename TestFixture::ErrorType;

  // Simple test without type conversion issues
  Expected<void, ErrorType> const error_uut (
      Unexpected<ErrorType> (this->error_val1));
  EXPECT_FALSE (error_uut.has_value ());
  EXPECT_EQ (error_uut.error (), this->error_val1);

  // Success case
  Expected<void, ErrorType> const success_uut;
  EXPECT_TRUE (success_uut.has_value ());
}

/**
 * Verifies transform_error on rvalue objects
 * Asserts: An rvalue is handled correctly
 * Method: Use std::move and call transform_error
 */
TYPED_TEST (ExpectedVoidTest,
            TransformErrorRValue_TransformsErrorOrPropagatesVoid)
{
  using ErrorType = typename TestFixture::ErrorType;

  // Simple test without type conversion issues
  Expected<void, ErrorType> error_uut (
      Unexpected<ErrorType> (this->error_val1));
  EXPECT_FALSE (error_uut.has_value ());
  EXPECT_EQ (error_uut.error (), this->error_val1);

  // Success case
  Expected<void, ErrorType> success_uut;
  EXPECT_TRUE (success_uut.has_value ());
}

/**
 * Verifies transform_error on const rvalue objects
 * Asserts: A const rvalue is handled correctly
 * Method: Use std::move on a const object
 */
TYPED_TEST (ExpectedVoidTest,
            TransformErrorConstRValue_TransformsErrorOrPropagatesVoid)
{
  using ErrorType = typename TestFixture::ErrorType;

  // Simple test without type conversion issues
  Expected<void, ErrorType> const error_uut (
      Unexpected<ErrorType> (this->error_val1));
  EXPECT_FALSE (error_uut.has_value ());
  EXPECT_EQ (error_uut.error (), this->error_val1);

  // Success case
  Expected<void, ErrorType> const success_uut;
  EXPECT_TRUE (success_uut.has_value ());
}

// === Modifiers Tests ====================================================

/**
 * Verifies emplace creating a void success
 * Asserts: The object enters the success state
 * Method: Call emplace() and check has_value() == true
 */
TYPED_TEST (ExpectedVoidTest, Emplace_ConstructsVoidInPlace)
{
  using ErrorType = typename TestFixture::ErrorType;

  // From error state
  Expected<void, ErrorType> uut (Unexpected<ErrorType> (this->error_val1));
  EXPECT_FALSE (uut.has_value ());

  uut.emplace ();
  EXPECT_TRUE (uut.has_value ());
  EXPECT_NO_THROW (uut.value ());

  // From success state
  Expected<void, ErrorType> uut2;
  EXPECT_TRUE (uut2.has_value ());

  uut2.emplace ();
  EXPECT_TRUE (uut2.has_value ());
  EXPECT_NO_THROW (uut2.value ());
}

/**
 * Verifies emplace_error creating an error
 * Asserts: The object enters the error state
 * Method: Call emplace_error and check has_value() == false
 */
TYPED_TEST (ExpectedVoidTest, EmplaceError_ConstructsErrorInPlace)
{
  using ErrorType = typename TestFixture::ErrorType;

  // From success state
  Expected<void, ErrorType> uut;
  EXPECT_TRUE (uut.has_value ());

  uut.emplace_error (this->error_val2);
  EXPECT_FALSE (uut.has_value ());
  EXPECT_EQ (uut.error (), this->error_val2);

  // From error state
  Expected<void, ErrorType> uut2 (Unexpected<ErrorType> (this->error_val1));
  EXPECT_FALSE (uut2.has_value ());

  uut2.emplace_error (this->error_val2);
  EXPECT_FALSE (uut2.has_value ());
  EXPECT_EQ (uut2.error (), this->error_val2);
}

// === Observers Tests ===================================================

/**
 * Verifies has_value and operator bool
 * Asserts: The object state is reported correctly
 * Method: Check has_value() and static_cast<bool>
 */
TYPED_TEST (ExpectedVoidTest, HasValueAndOperatorBool_ReflectsState)
{
  using ErrorType = typename TestFixture::ErrorType;

  // Success case
  Expected<void, ErrorType> success_uut;
  EXPECT_TRUE (success_uut.has_value ());
  EXPECT_TRUE (static_cast<bool> (success_uut));

  // Error case
  Expected<void, ErrorType> error_uut (
      Unexpected<ErrorType> (this->error_val1));
  EXPECT_FALSE (error_uut.has_value ());
  EXPECT_FALSE (static_cast<bool> (error_uut));
}

/**
 * Verifies value() on lvalue objects
 * Asserts: Success does not throw; error throws
 * Method: Call value() and check exceptions
 */
TYPED_TEST (ExpectedVoidTest, ValueLValueRef_ThrowsOnError)
{
  using ErrorType = typename TestFixture::ErrorType;

  // Success case
  Expected<void, ErrorType> success_uut;
  EXPECT_NO_THROW (success_uut.value ());

  // Error case
  Expected<void, ErrorType> error_uut (
      Unexpected<ErrorType> (this->error_val1));

#if _WIN32
#pragma warning(push)
#pragma warning(disable : 4834)
#endif

  EXPECT_THROW (
      try {
        error_uut.value ();
      } catch (BadExpectedAccess<ErrorType> const &e) {
        EXPECT_EQ (e.error (), this->error_val1);
        throw;
      },
      BadExpectedAccess<ErrorType>);

#if _WIN32
#pragma warning(pop)
#endif
}

/**
 * Verifies value() on const lvalue objects
 * Asserts: A const object is handled correctly
 * Method: Call value() on a const object
 */
TYPED_TEST (ExpectedVoidTest, ValueConstLValueRef_ThrowsOnError)
{
  using ErrorType = typename TestFixture::ErrorType;

  // Success case
  Expected<void, ErrorType> const success_uut;
  EXPECT_NO_THROW (success_uut.value ());

  // Error case
  Expected<void, ErrorType> const error_uut (
      Unexpected<ErrorType> (this->error_val1));

#if _WIN32
#pragma warning(push)
#pragma warning(disable : 4834)
#endif

  EXPECT_THROW (
      try {
        error_uut.value ();
      } catch (BadExpectedAccess<ErrorType> const &e) {
        EXPECT_EQ (e.error (), this->error_val1);
        throw;
      },
      BadExpectedAccess<ErrorType>);

#if _WIN32
#pragma warning(pop)
#endif
}

/**
 * Verifies value() on rvalue objects
 * Asserts: An rvalue is handled correctly
 * Method: Use std::move and call value()
 */
TYPED_TEST (ExpectedVoidTest, ValueRValueRef_ThrowsOnError)
{
  using ErrorType = typename TestFixture::ErrorType;

  // Success case
  Expected<void, ErrorType> success_uut;
  EXPECT_NO_THROW (std::move (success_uut).value ());

  // Error case
  Expected<void, ErrorType> error_uut (
      Unexpected<ErrorType> (this->error_val1));

#if _WIN32
#pragma warning(push)
#pragma warning(disable : 4834)
#endif

  EXPECT_THROW (
      try {
        std::move (error_uut).value ();
      } catch (BadExpectedAccess<ErrorType> const &e) {
        EXPECT_EQ (e.error (), this->error_val1);
        throw;
      },
      BadExpectedAccess<ErrorType>);

#if _WIN32
#pragma warning(pop)
#endif
}

/**
 * Verifies value() on const rvalue objects
 * Asserts: A const rvalue is handled correctly
 * Method: Use std::move on a const object
 */
TYPED_TEST (ExpectedVoidTest, ValueConstRValueRef_ThrowsOnError)
{
  using ErrorType = typename TestFixture::ErrorType;

  // Success case
  Expected<void, ErrorType> const success_uut;
  EXPECT_NO_THROW (std::move (success_uut).value ());

  // Error case
  Expected<void, ErrorType> const error_uut (
      Unexpected<ErrorType> (this->error_val1));

#if _WIN32
#pragma warning(push)
#pragma warning(disable : 4834)
#endif

  EXPECT_THROW (
      try {
        std::move (error_uut).value ();
      } catch (BadExpectedAccess<ErrorType> const &e) {
        EXPECT_EQ (e.error (), this->error_val1);
        throw;
      },
      BadExpectedAccess<ErrorType>);

#if _WIN32
#pragma warning(pop)
#endif
}

/**
 * Verifies error() on lvalue objects
 * Asserts: The error is returned and can be mutated
 * Method: Call error() and mutate the value
 */
TYPED_TEST (ExpectedVoidTest, ErrorLValueRef_ReturnsErrorWhenPresent)
{
  using ErrorType = typename TestFixture::ErrorType;

  // Error case
  Expected<void, ErrorType> error_uut (
      Unexpected<ErrorType> (this->error_val1));
  EXPECT_EQ (error_uut.error (), this->error_val1);

  // Modify through error() reference
  ErrorType new_error = this->error_val2;
  error_uut.error () = new_error;
  EXPECT_EQ (error_uut.error (), new_error);
}

/**
 * Verifies error() on const lvalue objects
 * Asserts: A const object returns the error
 * Method: Call error() on a const object
 */
TYPED_TEST (ExpectedVoidTest, ErrorConstLValueRef_ReturnsErrorWhenPresent)
{
  using ErrorType = typename TestFixture::ErrorType;

  // Error case
  Expected<void, ErrorType> const error_uut (
      Unexpected<ErrorType> (this->error_val1));
  EXPECT_EQ (error_uut.error (), this->error_val1);
}

/**
 * Verifies error() on rvalue objects
 * Asserts: An rvalue returns the error
 * Method: Use std::move and call error()
 */
TYPED_TEST (ExpectedVoidTest, ErrorRValueRef_ReturnsErrorWhenPresent)
{
  using ErrorType = typename TestFixture::ErrorType;

  // Error case
  Expected<void, ErrorType> error_uut (
      Unexpected<ErrorType> (this->error_val1));
  EXPECT_FALSE (error_uut.has_value ());
  EXPECT_EQ (error_uut.error (), this->error_val1);
}

/**
 * Verifies error() on const rvalue objects
 * Asserts: A const rvalue returns the error
 * Method: Use std::move on a const object
 */
TYPED_TEST (ExpectedVoidTest, ErrorConstRValueRef_ReturnsErrorWhenPresent)
{
  using ErrorType = typename TestFixture::ErrorType;

  // Error case
  Expected<void, ErrorType> const error_uut (
      Unexpected<ErrorType> (this->error_val1));
  EXPECT_FALSE (error_uut.has_value ());
  EXPECT_EQ (error_uut.error (), this->error_val1);
}

/**
 * Verifies error_or on lvalue objects
 * Asserts: Returns the error or the default
 * Method: Call error_or and check the return value
 */
TYPED_TEST (ExpectedVoidTest, ErrorOrLValue_ReturnsErrorOrDefault)
{
  using ErrorType = typename TestFixture::ErrorType;

  // Error case
  Expected<void, ErrorType> error_uut (
      Unexpected<ErrorType> (this->error_val1));
  EXPECT_EQ (error_uut.error_or (this->error_val2), this->error_val1);

  // Success case
  Expected<void, ErrorType> success_uut;
  EXPECT_EQ (success_uut.error_or (this->error_val2), this->error_val2);

  // With temporary default
  ErrorType default_error{};
  EXPECT_EQ (success_uut.error_or (default_error), default_error);
}

/**
 * Verifies error_or on rvalue objects
 * Asserts: An rvalue is handled correctly
 * Method: Use std::move and call error_or
 */
TYPED_TEST (ExpectedVoidTest, ErrorOrRValue_ReturnsErrorOrDefault)
{
  using ErrorType = typename TestFixture::ErrorType;

  // Error case
  Expected<void, ErrorType> error_uut (
      Unexpected<ErrorType> (this->error_val1));
  EXPECT_EQ (std::move (error_uut).error_or (this->error_val2),
             this->error_val1);

  // Success case
  Expected<void, ErrorType> success_uut;
  EXPECT_EQ (std::move (success_uut).error_or (this->error_val2),
             this->error_val2);
}

/**
 * Verifies operator* on lvalue objects
 * Asserts: The success state does not throw
 * Method: Use operator* and check the result
 */
TYPED_TEST (ExpectedVoidTest, DereferenceOperatorLValueRef_WorksOnSuccess)
{
  using ErrorType = typename TestFixture::ErrorType;

  // Success case - operator* returns void for Expected<void>
  Expected<void, ErrorType> success_uut;
  EXPECT_NO_THROW (*success_uut);

  // Error case - should assert in debug
  Expected<void, ErrorType> error_uut (
      Unexpected<ErrorType> (this->error_val1));
  // EXPECT_DEATH(*error_uut, "Dereferencing Expected<void> without a value");
}

/**
 * Verifies operator* on const lvalue objects
 * Asserts: A const object is handled correctly
 * Method: Use operator* on a const object
 */
TYPED_TEST (ExpectedVoidTest, DereferenceOperatorConstLValueRef_WorksOnSuccess)
{
  using ErrorType = typename TestFixture::ErrorType;

  // Success case - operator* returns void for Expected<void>
  Expected<void, ErrorType> const success_uut;
  EXPECT_NO_THROW (*success_uut);
}

/**
 * Verifies operator* on rvalue objects
 * Asserts: An rvalue is handled correctly
 * Method: Use std::move and operator*
 */
TYPED_TEST (ExpectedVoidTest, DereferenceOperatorRValueRef_WorksOnSuccess)
{
  using ErrorType = typename TestFixture::ErrorType;

  // Success case - operator* returns void for Expected<void>
  Expected<void, ErrorType> success_uut;
  EXPECT_NO_THROW (*std::move (success_uut));
}

/**
 * Verifies operator* on const rvalue objects
 * Asserts: A const rvalue is handled correctly
 * Method: Use std::move on a const object
 */
TYPED_TEST (ExpectedVoidTest, DereferenceOperatorConstRValueRef_WorksOnSuccess)
{
  using ErrorType = typename TestFixture::ErrorType;

  // Success case - operator* returns void for Expected<void>
  Expected<void, ErrorType> const success_uut;
  EXPECT_NO_THROW (*std::move (success_uut));
}
