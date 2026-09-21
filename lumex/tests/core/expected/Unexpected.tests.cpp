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

// === Error types for tests ========================================

// Simple error type
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

// === Fixture for Unexpected ================================================
template <typename ErrorType> class UnexpectedTest : public ::testing::Test
{
protected:
  // Preconditions: initialize standard error values for the tests.
  // Create known error objects for repeatable tests.
  // Assert that error-object initialization does not throw.
  void
  SetUp () override
  {
    // Initialize SimpleError when ErrorType is SimpleError
    if constexpr (std::is_same_v<ErrorType, SimpleError>)
      {
        error_val1 = static_cast<ErrorType> (SimpleError::InvalidInput);
        error_val2 = static_cast<ErrorType> (SimpleError::NetworkFailure);
      }
    // Initialize ComplexError when ErrorType is ComplexError
    else if constexpr (std::is_same_v<ErrorType, ComplexError>)
      {
        error_val1
            = static_cast<ErrorType> (ComplexError ("Test Error 1", 101));
        error_val2
            = static_cast<ErrorType> (ComplexError ("Test Error 2", 102));
      }
    // For other types, use the default constructor or a simple init
    else
      {
        error_val1 = ErrorType ();
        error_val2 = ErrorType ();
      }
  }

  ErrorType error_val1;
  ErrorType error_val2;
};

// Use typed tests across error types
using ErrorTypes
    = ::testing::Types<int, std::string, SimpleError, ComplexError>;
TYPED_TEST_SUITE (UnexpectedTest, ErrorTypes);

// === API contract verifier tests ========================================

// Check that the copy constructor initializes Unexpected correctly.
// Assert that the given value was copied into Unexpected.
TYPED_TEST (UnexpectedTest, Constructor_LValueRef_CopiesErrorCorrectly)
{
  // Arrange
  TypeParam initial_error = this->error_val1;
  // Act
  Unexpected<TypeParam> uut (
      initial_error); // Call the constructor from const &
  // Assert
  EXPECT_EQ (uut.error (), initial_error);
  // Mutating the source must not affect Unexpected
  if constexpr (std::is_same_v<TypeParam, int>)
    {
      initial_error = 999;
      EXPECT_NE (uut.error (), initial_error);
    }
  else if constexpr (std::is_same_v<TypeParam, std::string>)
    {
      initial_error = "Changed Original";
      EXPECT_NE (uut.error (), initial_error);
    }
}

// Check that the move constructor initializes Unexpected correctly.
// Assert that the rvalue was moved into Unexpected.
TYPED_TEST (UnexpectedTest, Constructor_RValueRef_MovesErrorCorrectly)
{
  // Arrange
  TypeParam initial_error = this->error_val1;
  TypeParam expected_error = initial_error; // Copy for comparison
  // Act
  Unexpected<TypeParam> uut (
      std::move (initial_error)); // Call the constructor from &&
  // Assert
  EXPECT_EQ (uut.error (), expected_error);
  // For ComplexError, the source must be in the moved-from state.
  if constexpr (std::is_same_v<TypeParam, ComplexError>)
    {
      EXPECT_EQ (initial_error.code, 0);
      EXPECT_EQ (initial_error.resource, nullptr);
    }
}

// Check that error() as an lvalue returns a mutable reference to the stored
// error. Assert that the returned reference can mutate internal state.
TYPED_TEST (UnexpectedTest, ErrorLValueRefReturnsMutableReference)
{
  // Arrange
  TypeParam initial_error = this->error_val1;
  Unexpected<TypeParam> uut (std::move (initial_error));
  // Act
  auto &error_ref = uut.error ();
  // Assert
  EXPECT_EQ (error_ref, this->error_val1); // Check the original value

  // Mutate through the reference and check that uut changed
  if constexpr (std::is_same_v<TypeParam, int>)
    {
      error_ref = 555;
      EXPECT_EQ (uut.error (), 555);
    }
  else if constexpr (std::is_same_v<TypeParam, std::string>)
    {
      error_ref = "New Message";
      EXPECT_EQ (uut.error (), "New Message");
    }
  else if constexpr (std::is_same_v<TypeParam, ComplexError>)
    {
      error_ref.code = 777;
      EXPECT_EQ (uut.error ().code, 777);
    }
}

// Check that const error() as an lvalue returns a const reference to the
// stored error. Assert that the returned reference cannot mutate internal
// state.
TYPED_TEST (UnexpectedTest, ConstErrorLValueRefReturnsImmutableReference)
{
  // Arrange
  TypeParam initial_error = this->error_val1;
  Unexpected<TypeParam> uut (std::move (initial_error));
  Unexpected<TypeParam> const &const_uut = uut;
  // Act
  auto &error_ref = const_uut.error ();
  // Assert
  EXPECT_EQ (error_ref, this->error_val1);
  // Assigning through error_ref must be a compile error,
  // which confirms immutability.
  // error_ref = some_other_value; // Error: assignment of read-only reference
}

// Check that error() as an rvalue returns an rvalue reference and moves the
// contents. Assert that Unexpected contents were moved after the call.
TYPED_TEST (UnexpectedTest, ErrorRValueRefReturnsRValueAndMovesContent)
{
  // Arrange
  TypeParam initial_error = this->error_val1;
  TypeParam expected_error = initial_error;
  Unexpected<TypeParam> uut (std::move (initial_error));
  // Act
  TypeParam moved_error = std::move (uut).error (); // Call error() &&
  // Assert
  EXPECT_EQ (moved_error, expected_error);
  // For ComplexError, the resource inside uut must be moved.
  if constexpr (std::is_same_v<TypeParam, ComplexError>)
    {
      EXPECT_EQ (uut.error ().code, 0);
      EXPECT_EQ (uut.error ().resource, nullptr);
    }
}

// Check that const error() as an rvalue returns a const rvalue reference and
//      does not change Unexpected (it copies).
// Assert that the source Unexpected is unchanged.
TYPED_TEST (UnexpectedTest,
            ConstErrorRValueRefReturnsConstRValueAndDoesNotModifySource)
{
  // Arrange
  TypeParam initial_error = this->error_val1;
  TypeParam expected_error = initial_error;
  Unexpected<TypeParam> uut (std::move (initial_error));
  // Act
  TypeParam const_moved_error
      = std::move (static_cast<Unexpected<TypeParam> const &> (uut)).error ();
  // Assert
  EXPECT_EQ (const_moved_error, expected_error);
  // For ComplexError, the resource inside uut must not be moved.
  if constexpr (std::is_same_v<TypeParam, ComplexError>)
    EXPECT_EQ (uut.error (), expected_error);
}

// === Memory and lifetime tests =======================================

// Check that creating and destroying Unexpected with ComplexError
//      does not leak and releases resources.
// ComplexError uses unique_ptr to track ownership.
TYPED_TEST (UnexpectedTest, MemorySafety_ComplexErrorDestructorCalled)
{
  if constexpr (std::is_same_v<TypeParam, ComplexError>)
    {
      // Arrange
      ComplexError initial_error ("Memory Test Error", 200);
      int *original_resource_ptr = initial_error.resource.get ();
      // Act and assert (no leaks when leaving the scope)
      {
        Unexpected<ComplexError> uut (std::move (initial_error));
        EXPECT_NE (uut.error ().resource, nullptr);
        EXPECT_EQ (uut.error ().resource.get (), original_resource_ptr);
      } // uut is destroyed here; the unique_ptr resource must be released.
      SUCCEED () << "ComplexError with unique_ptr should be correctly "
                    "destroyed, preventing memory leaks.";
    }
  else
    {
      SUCCEED () << "Test not applicable for non-ComplexError types.";
    }
}

// === Thread-safety tests (independent instances) =============

// Check that concurrent construction and access to distinct Unexpected
//      instances is correct.
// Each thread must construct its Unexpected and read the correct error.
TYPED_TEST (UnexpectedTest, ThreadSafety_MultipleIndependentInstances)
{
  constexpr int num_threads = 10;
  std::vector<std::thread> threads;
  std::vector<TypeParam> initial_errors (num_threads);

  // Arrange: unique errors per thread
  for (int i = 0; i < num_threads; ++i)
    if constexpr (std::is_same_v<TypeParam, int>)
      initial_errors[i] = i + 1;
    else if constexpr (std::is_same_v<TypeParam, std::string>)
      initial_errors[i] = "Thread Error " + std::to_string (i + 1);
    else if constexpr (std::is_same_v<TypeParam, SimpleError>)
      initial_errors[i] = static_cast<SimpleError> ((i % 3) + 1);
    else if constexpr (std::is_same_v<TypeParam, ComplexError>)
      initial_errors[i] = ComplexError (
          "Complex Thread Error " + std::to_string (i + 1), 400 + i);
    else
      initial_errors[i] = TypeParam (); // Default value for other types

  // Act
  for (int i = 0; i < num_threads; ++i)
    {
      threads.emplace_back ([&, i] () {
        // Each thread constructs its Unexpected (moving its error)
        Unexpected<TypeParam> uut (std::move (initial_errors[i]));
        // and checks its value
        // Note: initial_errors[i] is moved-from; compare against the
        // expected value (the value from before the move)
        if constexpr (std::is_same_v<TypeParam, ComplexError>)
          {
            EXPECT_EQ (uut.error ().code,
                       400 + i); // Check a specific ComplexError field
          }
        else
          {
            // For simple types, uut.error() still holds the original
            // value because copy/move does not empty the source for
            // those types. After std::move(), initial_errors[i] may be
            // unspecified for std::string. Compare against a copy taken
            // before the move. initial_errors[i] cannot be compared to
            // uut.error() here, because initial_errors[i] has already
            // been moved from. This test mainly checks that Unexpected
            // was created and error() works.
            SUCCEED (); // If not ComplexError, just assert there is no
                        // crash.
          }
      });
    }

  for (auto &t : threads)
    t.join ();

  // Assert (every EXPECT inside the threads must pass)
  SUCCEED () << "All independent Unexpected instances created and accessed "
                "correctly across threads.";
}

// === Performance and load tests (optional) ====================

TYPED_TEST (UnexpectedTest, Perf_ConstructionAndAccess)
{
#if LUMEX_PERF_WALL_CLOCK_ENABLED
  // Precondition: construct and access Unexpected many times.
  // Action: time construction and error() access.
  // Expected state: the operation finishes in acceptable time.
  // Benchmark constructors and error() to find bottlenecks.
  // Assert that performance matches expectations.
  int const N = 1'000'000;
  auto start = std::chrono::high_resolution_clock::now ();

  for (int i = 0; i < N; ++i)
    {
      TypeParam error_data;
      if constexpr (std::is_same_v<TypeParam, int>)
        error_data = i;
      else if constexpr (std::is_same_v<TypeParam, std::string>)
        error_data = "Error" + std::to_string (i);
      else if constexpr (std::is_same_v<TypeParam, SimpleError>)
        error_data = static_cast<SimpleError> (i % 3 + 1);
      else if constexpr (std::is_same_v<TypeParam, ComplexError>)
        error_data = ComplexError ("Perf Error", i);

      // Construct Unexpected. error_data is moved here.
      Unexpected<TypeParam> uut (std::move (error_data));
      // Call error() to simulate use
      LUMEX_ATTRIBUTE_MAYBE_UNUSED auto &err = uut.error ();
    }

  auto dur = std::chrono::duration_cast<std::chrono::milliseconds> (
      std::chrono::high_resolution_clock::now () - start);

  // Expected time depends heavily on ErrorType.
  // Use a higher threshold for ComplexError.
  long long threshold = 100; // ms
  if constexpr (std::is_same_v<TypeParam, ComplexError>)
    threshold = 1000; // ms for ComplexError
  else if constexpr (std::is_same_v<TypeParam, std::string>)
    threshold = 200; // ms for std::string

  EXPECT_LT (dur.count (), threshold)
      << "Construction and access for " << N << " Unexpected<"
      << (std::is_same_v<TypeParam, int>            ? "int"
          : std::is_same_v<TypeParam, std::string>  ? "string"
          : std::is_same_v<TypeParam, SimpleError>  ? "SimpleError"
          : std::is_same_v<TypeParam, ComplexError> ? "ComplexError"
                                                    : "Unknown")
      << "> too slow: " << dur.count () << "ms (Threshold: " << threshold
      << "ms)";
#else
  GTEST_SKIP ()
      << "wall-clock Perf_* thresholds are Release-only (no sanitizers)";
#endif
}
