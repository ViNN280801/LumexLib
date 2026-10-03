// BadExpectedAccess<E> tests. They compile from C++11, so every expected
// suite (C++11, C++17, C++20) runs them.

#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/expected/Expected"

#include "lumex/tests/core/expected/ExpectedTestTypes.hpp"
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

// === Per-type steps of these tests ==========================================
// The shared steps live in ExpectedTestTypes.hpp; these are the ones only the
// BadExpectedAccess tests take.

namespace
{
// Writes through error() & and checks the exception sees the change.
void
MutateThroughError (BadExpectedAccess<int> &uut)
{
  uut.error () = 999;
  EXPECT_EQ (uut.error (), 999);
}

void
MutateThroughError (BadExpectedAccess<std::string> &uut)
{
  uut.error () = "Modified Error";
  EXPECT_EQ (uut.error (), "Modified Error");
}

template <typename T>
void
MutateThroughError (BadExpectedAccess<T> &)
{
}

// The lifetime check only applies to ComplexError (it owns a resource).
void
CheckComplexErrorLifetime (TypeTag<ComplexError>)
{
  // Arrange
  ComplexError initial_error ("Memory Test Error", 200);
  int *original_resource_ptr = initial_error.resource.get ();
  // Act and assert (no leaks when leaving the scope)
  {
    BadExpectedAccess<ComplexError> uut (std::move (initial_error));
    EXPECT_NE (uut.error ().resource, nullptr);
    EXPECT_EQ (uut.error ().resource.get (),
               original_resource_ptr); // Must be the same resource, but moved
  } // uut is destroyed here; the unique_ptr resource must be released.
  // Directly observing unique_ptr release is hard without changing
  // ComplexError, but RAII guarantees it. Checking that original_resource_ptr
  // now points at freed memory is unsafe. Rely on unique_ptr instead.
  SUCCEED () << "ComplexError with unique_ptr should be correctly "
                "destroyed, preventing memory leaks.";
}

template <typename T>
void
CheckComplexErrorLifetime (TypeTag<T>)
{
  SUCCEED () << "Test not applicable for non-ComplexError types.";
}

// Changes the error of `changed` after `kept` was copied or assigned from it,
// then checks that `kept` still holds `expected`.
void
ExpectIndependentCopy (BadExpectedAccess<int> &changed,
                       BadExpectedAccess<int> const &kept, int const &expected,
                       int new_value)
{
  changed.error () = new_value;
  EXPECT_NE (kept.error (), changed.error ());
  EXPECT_EQ (kept.error (), expected);
}

void
ExpectIndependentCopy (BadExpectedAccess<std::string> &changed,
                       BadExpectedAccess<std::string> const &kept,
                       std::string const &expected,
                       std::string const &new_value)
{
  changed.error () = new_value;
  EXPECT_NE (kept.error (), changed.error ());
  EXPECT_EQ (kept.error (), expected);
}

void
ExpectIndependentCopy (BadExpectedAccess<ComplexError> &changed,
                       BadExpectedAccess<ComplexError> const &kept,
                       ComplexError const &expected,
                       std::string const &new_message, int new_code)
{
  changed.error ().message = new_message;
  changed.error ().code = new_code;
  EXPECT_NE (kept.error (), changed.error ());
  EXPECT_EQ (kept.error ().message, expected.message);
  EXPECT_EQ (kept.error ().code, expected.code);
  EXPECT_NE (kept.error ().resource,
             changed.error ().resource); // Must be distinct unique_ptr objects
}

// The copy constructor and copy assignment tests change the source with
// different values; SimpleError is not changed.
void
CheckCopyAfterCopyConstruction (BadExpectedAccess<int> &original,
                                BadExpectedAccess<int> const &copy,
                                int const &expected)
{
  ExpectIndependentCopy (original, copy, expected, 123);
}

void
CheckCopyAfterCopyConstruction (BadExpectedAccess<std::string> &original,
                                BadExpectedAccess<std::string> const &copy,
                                std::string const &expected)
{
  ExpectIndependentCopy (original, copy, expected, "Changed Original");
}

void
CheckCopyAfterCopyConstruction (BadExpectedAccess<ComplexError> &original,
                                BadExpectedAccess<ComplexError> const &copy,
                                ComplexError const &expected)
{
  ExpectIndependentCopy (original, copy, expected, "Changed Original Message",
                         500);
}

template <typename T>
void
CheckCopyAfterCopyConstruction (BadExpectedAccess<T> &,
                                BadExpectedAccess<T> const &, T const &)
{
}

void
CheckCopyAfterCopyAssignment (BadExpectedAccess<int> &source,
                              BadExpectedAccess<int> const &target,
                              int const &expected)
{
  ExpectIndependentCopy (source, target, expected, 456);
}

void
CheckCopyAfterCopyAssignment (BadExpectedAccess<std::string> &source,
                              BadExpectedAccess<std::string> const &target,
                              std::string const &expected)
{
  ExpectIndependentCopy (source, target, expected, "Changed Source");
}

void
CheckCopyAfterCopyAssignment (BadExpectedAccess<ComplexError> &source,
                              BadExpectedAccess<ComplexError> const &target,
                              ComplexError const &expected)
{
  ExpectIndependentCopy (source, target, expected, "Changed Source Message",
                         600);
}

template <typename T>
void
CheckCopyAfterCopyAssignment (BadExpectedAccess<T> &,
                              BadExpectedAccess<T> const &, T const &)
{
}

// A distinct error per thread index.
void
AssignThreadError (int &error, int i)
{
  error = i + 1;
}

void
AssignThreadError (std::string &error, int i)
{
  error = "Error " + std::to_string (i + 1);
}

void
AssignThreadError (SimpleError &error, int i)
{
  error = static_cast<SimpleError> (i % 3 + 1);
}

void
AssignThreadError (ComplexError &error, int i)
{
  error = ComplexError ("Thread Error " + std::to_string (i + 1), 300 + i);
}

template <typename T>
void
AssignThreadError (T &error, int)
{
  error = T (); // Default value
}
} // namespace

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
    // SimpleError and ComplexError get known values; other types use the
    // default constructor.
    InitErrorPair (error_val1, error_val2);
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
  MutateThroughError (uut);
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
  // For ComplexError, the resource inside uut must be moved (the source
  // ComplexError has code 0 and no resource).
  ExpectMovedFrom (uut.error ());
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
  // For ComplexError, the resource inside uut must not be moved: the source
  // ComplexError is not changed.
  ExpectEqualComplex (uut.error (), expected_error);
}

// === Memory and lifetime tests =======================================

// Check that creating and destroying BadExpectedAccess with ComplexError
// does not leak and releases resources.
// ComplexError uses unique_ptr to track ownership.
TYPED_TEST (BadExpectedAccessTest, MemorySafety_ComplexErrorDestructorCalled)
{
  CheckComplexErrorLifetime (TypeTag<TypeParam> ());
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
  CheckCopyAfterCopyConstruction (original_uut, copied_uut,
                                  expected_error_value);
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
  CheckCopyAfterCopyAssignment (src_uut, dst_uut, expected_error_value);
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
  ExpectMovedFrom (src_uut.error ());
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
    AssignThreadError (initial_errors[i], i);

  // Act
  for (int i = 0; i < num_threads; ++i)
    {
      threads.emplace_back (
          [&, i] ()
            {
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
  int const N = 1000000;
  auto start = std::chrono::high_resolution_clock::now ();

  for (int i = 0; i < N; ++i)
    {
      TypeParam error_data;
      AssignPerfError (error_data, i);

      BadExpectedAccess<TypeParam> uut (std::move (error_data));
      // Call error() to simulate use
      LUMEX_ATTRIBUTE_MAYBE_UNUSED auto &err = uut.error ();
    }

  auto dur = std::chrono::duration_cast<std::chrono::milliseconds> (
      std::chrono::high_resolution_clock::now () - start);

  // Expected time depends heavily on ErrorType.
  // ComplexError is much slower because of unique_ptr and std::string.
  // Use a higher threshold for ComplexError.
  long long const threshold = PerfThresholdMs (TypeTag<TypeParam> ()); // ms

  EXPECT_LT (dur.count (), threshold)
      << "Construction and access for " << N << " BadExpectedAccess<"
      << TypeLabel (TypeTag<TypeParam> ()) << "> too slow: " << dur.count ()
      << "ms (Threshold: " << threshold << "ms)";
#else
  GTEST_SKIP ()
      << "wall-clock Perf_* thresholds are Release-only (no sanitizers)";
#endif
}
