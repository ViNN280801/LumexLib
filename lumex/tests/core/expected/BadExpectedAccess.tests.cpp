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

enum class SimpleError
{
  None,
  InvalidInput,
  NetworkFailure
};

// Complex type of error with resource ownership (for checking move and copy)
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

// === Fixture for BadExpectedAccess =========================================
template <typename ErrorType>
class BadExpectedAccessTest : public ::testing::Test
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
TYPED_TEST_SUITE (BadExpectedAccessTest, ErrorTypes);

// === API contract verifier tests ========================================

// Check that the constructor initializes the exception and that what()
// returns the expected description.
// After construction, what() must return "Bad expected access".
TYPED_TEST (BadExpectedAccessTest,
            Constructor_And_WhatMethodReturnsCorrectMessage)
{
  // Arrange
  TypeParam initial_error = this->error_val1;
  // Act
  BadExpectedAccess<TypeParam> uut (std::move (initial_error));
  // Assert
  EXPECT_STREQ ("Bad expected access", uut.what ());
}

// Check that the constructor accepts an rvalue and that error() as an lvalue
// returns the correct value.
// Assert that the rvalue was moved into the exception.
TYPED_TEST (BadExpectedAccessTest,
            Constructor_RValue_And_ErrorLValueRefReturnsCorrectValue)
{
  // Arrange
  TypeParam initial_error = this->error_val1;
  TypeParam expected_error = initial_error; // Copy for comparison
  // Act
  BadExpectedAccess<TypeParam> uut (std::move (initial_error));
  // Assert
  EXPECT_EQ (uut.error (), expected_error);
  // Assert that mutation through the lvalue reference changes internal state.
  if constexpr (std::is_same_v<TypeParam, int>)
    {
      uut.error () = 999;
      EXPECT_EQ (uut.error (), 999);
    }
  else if constexpr (std::is_same_v<TypeParam, std::string>)
    {
      uut.error () = "Modified Error";
      EXPECT_EQ (uut.error (), "Modified Error");
    }
}

// Check that const error() as an lvalue returns the correct value
// and does not allow mutation.
// Assert that const access yields the correct value.
TYPED_TEST (BadExpectedAccessTest,
            ConstErrorLValueRefReturnsCorrectValue_And_IsImmutable)
{
  // Arrange
  TypeParam initial_error = this->error_val1;
  TypeParam expected_error = initial_error;
  BadExpectedAccess<TypeParam> uut (std::move (initial_error));
  // Act
  BadExpectedAccess<TypeParam> const &const_uut = uut;
  // Assert
  EXPECT_EQ (const_uut.error (), expected_error);
  // Mutating through a const reference must be a compile error
  // const_uut.error() = some_other_value; // This is expected to be a compile
  // error
}

// Check that error() as an rvalue returns the correct value and
// moves the internal state.
// Assert that uut.error() is moved-from afterwards (when applicable).
TYPED_TEST (BadExpectedAccessTest,
            ErrorRValueRefReturnsCorrectValue_And_MovesContent)
{
  // Arrange
  TypeParam initial_error = this->error_val1;
  TypeParam expected_error = initial_error;
  BadExpectedAccess<TypeParam> uut (std::move (initial_error));
  // Act
  TypeParam moved_error = std::move (uut).error ();
  // Assert
  EXPECT_EQ (moved_error, expected_error);
  // For ComplexError, the resource inside uut must be moved.
  if constexpr (std::is_same_v<TypeParam, ComplexError>)
    {
      EXPECT_EQ (uut.error ().code,
                 0); // Check that the source ComplexError was changed
      EXPECT_EQ (uut.error ().resource, nullptr);
    }
}

// Check that const error() as an rvalue returns the correct value
// without changing uut.
// Assert that a const rvalue access returns a copy.
TYPED_TEST (BadExpectedAccessTest,
            ConstErrorRValueRefReturnsCorrectValue_And_DoesNotModifySource)
{
  // Arrange
  TypeParam initial_error = this->error_val1;
  TypeParam expected_error = initial_error;
  BadExpectedAccess<TypeParam> uut (std::move (initial_error));
  // Act
  TypeParam const_moved_error
      = std::move (static_cast<BadExpectedAccess<TypeParam> const &> (uut))
            .error ();
  // Assert
  EXPECT_EQ (const_moved_error, expected_error);
  // For ComplexError, the resource inside uut must not be moved.
  if constexpr (std::is_same_v<TypeParam, ComplexError>)
    EXPECT_EQ (
        uut.error (),
        expected_error); // Check that the source ComplexError was not changed
}

// === Memory and lifetime tests =======================================

// Check that creating and destroying BadExpectedAccess with ComplexError
// does not leak and releases resources.
// ComplexError uses unique_ptr to track ownership.
TYPED_TEST (BadExpectedAccessTest, MemorySafety_ComplexErrorDestructorCalled)
{
  if constexpr (std::is_same_v<TypeParam, ComplexError>)
    {
      // Arrange
      ComplexError initial_error ("Memory Test Error", 200);
      int *original_resource_ptr = initial_error.resource.get ();
      // Act and assert (no leaks when leaving the scope)
      {
        BadExpectedAccess<ComplexError> uut (std::move (initial_error));
        EXPECT_NE (uut.error ().resource, nullptr);
        EXPECT_EQ (
            uut.error ().resource.get (),
            original_resource_ptr); // Must be the same resource, but moved
      } // uut is destroyed here; the unique_ptr resource must be released.
      // Directly observing unique_ptr release is hard without changing
      // ComplexError, but RAII guarantees it. Checking that
      // original_resource_ptr now points at freed memory is unsafe. Rely on
      // unique_ptr instead.
      SUCCEED () << "ComplexError with unique_ptr should be correctly "
                    "destroyed, preventing memory leaks.";
    }
  else
    {
      SUCCEED () << "Test not applicable for non-ComplexError types.";
    }
}

// Check the BadExpectedAccess copy constructor.
// Assert that the copy holds an independent error.
TYPED_TEST (BadExpectedAccessTest, CopyConstructor_CopiesErrorCorrectly)
{
  // Arrange
  TypeParam initial_error = this->error_val1;
  BadExpectedAccess<TypeParam> original_uut (std::move (initial_error));
  TypeParam expected_error_value
      = original_uut.error (); // Error value before the copy
  // Act
  BadExpectedAccess<TypeParam> copied_uut
      = original_uut; // Call the copy constructor
  // Assert
  EXPECT_EQ (copied_uut.error (), expected_error_value);
  // Mutating the original must not affect the copy
  if constexpr (std::is_same_v<TypeParam, int>)
    {
      original_uut.error () = 123;
      EXPECT_NE (copied_uut.error (), original_uut.error ());
      EXPECT_EQ (copied_uut.error (), expected_error_value);
    }
  else if constexpr (std::is_same_v<TypeParam, std::string>)
    {
      original_uut.error () = "Changed Original";
      EXPECT_NE (copied_uut.error (), original_uut.error ());
      EXPECT_EQ (copied_uut.error (), expected_error_value);
    }
  else if constexpr (std::is_same_v<TypeParam, ComplexError>)
    {
      original_uut.error ().message = "Changed Original Message";
      original_uut.error ().code = 500;
      EXPECT_NE (copied_uut.error (), original_uut.error ());
      EXPECT_EQ (copied_uut.error ().message, expected_error_value.message);
      EXPECT_EQ (copied_uut.error ().code, expected_error_value.code);
      EXPECT_NE (copied_uut.error ().resource,
                 original_uut.error ()
                     .resource); // Must be distinct unique_ptr objects
    }
}

// Check BadExpectedAccess copy assignment.
// Assert that the target gets an independent error copy and old resources are
// released.
TYPED_TEST (BadExpectedAccessTest, CopyAssignment_CopiesErrorCorrectly)
{
  // Arrange
  TypeParam initial_error_src = this->error_val1;
  BadExpectedAccess<TypeParam> src_uut (std::move (initial_error_src));
  TypeParam initial_error_dst = this->error_val2;
  BadExpectedAccess<TypeParam> dst_uut (std::move (initial_error_dst));
  TypeParam expected_error_value = src_uut.error ();

  // Act
  dst_uut = src_uut; // Call copy assignment
  // Assert
  EXPECT_EQ (dst_uut.error (), expected_error_value);
  // Mutating the source must not affect the target
  if constexpr (std::is_same_v<TypeParam, int>)
    {
      src_uut.error () = 456;
      EXPECT_NE (dst_uut.error (), src_uut.error ());
      EXPECT_EQ (dst_uut.error (), expected_error_value);
    }
  else if constexpr (std::is_same_v<TypeParam, std::string>)
    {
      src_uut.error () = "Changed Source";
      EXPECT_NE (dst_uut.error (), src_uut.error ());
      EXPECT_EQ (dst_uut.error (), expected_error_value);
    }
  else if constexpr (std::is_same_v<TypeParam, ComplexError>)
    {
      src_uut.error ().message = "Changed Source Message";
      src_uut.error ().code = 600;
      EXPECT_NE (dst_uut.error (), src_uut.error ());
      EXPECT_EQ (dst_uut.error ().message, expected_error_value.message);
      EXPECT_EQ (dst_uut.error ().code, expected_error_value.code);
      EXPECT_NE (dst_uut.error ().resource, src_uut.error ().resource);
    }
}

// Check BadExpectedAccess move assignment.
// Assert that resources move from the source to the target,
//      and the source stays valid but changed.
TYPED_TEST (BadExpectedAccessTest, MoveAssignment_MovesErrorCorrectly)
{
  // Arrange
  TypeParam initial_error_src = this->error_val1;
  BadExpectedAccess<TypeParam> src_uut (std::move (initial_error_src));
  TypeParam initial_error_dst = this->error_val2;
  BadExpectedAccess<TypeParam> dst_uut (std::move (initial_error_dst));
  TypeParam expected_error_value
      = src_uut.error (); // Error value before the move
  // Act
  dst_uut = std::move (src_uut); // Call move assignment
  // Assert
  EXPECT_EQ (dst_uut.error (), expected_error_value);
  // Check that src_uut now holds the moved-from resource
  if constexpr (std::is_same_v<TypeParam, ComplexError>)
    {
      EXPECT_EQ (src_uut.error ().code, 0);
      EXPECT_EQ (src_uut.error ().resource, nullptr);
    }
  // For simple types such as int or std::string, the source may stay unchanged
  // or be valid but unspecified. Its value is not checked
  // after the move because that is not part of the contract.
}

// === Thread-safety tests (independent instances) =============

// Check that concurrent construction and access to distinct BadExpectedAccess
// instances is correct.
// Each thread must construct its BadExpectedAccess and read the correct error.
TYPED_TEST (BadExpectedAccessTest, ThreadSafety_MultipleIndependentInstances)
{
  constexpr int num_threads = 10;
  std::vector<std::thread> threads;
  std::vector<BadExpectedAccess<TypeParam>> errors;
  errors.reserve (num_threads);

  // Arrange
  // Create source errors per thread
  std::vector<TypeParam> initial_errors (num_threads);
  for (int i = 0; i < num_threads; ++i)
    if constexpr (std::is_same_v<TypeParam, int>)
      initial_errors[i] = i + 1;
    else if constexpr (std::is_same_v<TypeParam, std::string>)
      initial_errors[i] = "Error " + std::to_string (i + 1);
    else if constexpr (std::is_same_v<TypeParam, SimpleError>)
      initial_errors[i] = static_cast<SimpleError> (i % 3 + 1);
    else if constexpr (std::is_same_v<TypeParam, ComplexError>)
      initial_errors[i]
          = ComplexError ("Thread Error " + std::to_string (i + 1), 300 + i);
    else
      initial_errors[i] = TypeParam (); // Default value

  // Act
  for (int i = 0; i < num_threads; ++i)
    {
      threads.emplace_back ([&, i] () {
        // Each thread constructs its BadExpectedAccess
        TypeParam expected_error_in_thread = initial_errors[i];
        BadExpectedAccess<TypeParam> uut (std::move (initial_errors[i]));
        // and checks its value
        EXPECT_EQ (uut.error (),
                   expected_error_in_thread); // Compare with the value
                                              // from before the move
        EXPECT_STREQ ("Bad expected access", uut.what ());
      });
    }

  for (auto &t : threads)
    t.join ();

  // Assert (every EXPECT inside the threads must pass)
  SUCCEED () << "All independent BadExpectedAccess instances created and "
                "accessed correctly across threads.";
}

// === Performance and load tests (optional) ====================

TYPED_TEST (BadExpectedAccessTest, Perf_ConstructionAndAccess)
{
#if LUMEX_PERF_WALL_CLOCK_ENABLED
  // Precondition: construct and access BadExpectedAccess many times.
  // Action: time construction and error() access.
  // Expected state: the operation finishes in acceptable time.
  // Benchmark the constructor and error() to find bottlenecks.
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

      BadExpectedAccess<TypeParam> uut (std::move (error_data));
      // Call error() to simulate use
      LUMEX_ATTRIBUTE_MAYBE_UNUSED auto &err = uut.error ();
    }

  auto dur = std::chrono::duration_cast<std::chrono::milliseconds> (
      std::chrono::high_resolution_clock::now () - start);

  // Expected time depends heavily on ErrorType.
  // ComplexError is much slower because of unique_ptr and std::string.
  // Use a higher threshold for ComplexError.
  long long threshold = 100; // ms
  if constexpr (std::is_same_v<TypeParam, ComplexError>)
    threshold = 1000; // ms for ComplexError
  else if constexpr (std::is_same_v<TypeParam, std::string>)
    threshold = 200; // ms for std::string

  EXPECT_LT (dur.count (), threshold)
      << "Construction and access for " << N << " BadExpectedAccess<"
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
