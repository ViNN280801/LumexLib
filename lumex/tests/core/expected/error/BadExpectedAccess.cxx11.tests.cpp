// bad_expected_access<E> tests. They compile from C++11, so every expected
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
// bad_expected_access tests take.

namespace
{
// Writes through error() & and checks the exception sees the change.
void
mutate_through_error (bad_expected_access<int> &uut)
{
  uut.error () = 999;
  EXPECT_EQ (uut.error (), 999);
}

void
mutate_through_error (bad_expected_access<std::string> &uut)
{
  uut.error () = "Modified Error";
  EXPECT_EQ (uut.error (), "Modified Error");
}

template <typename T>
void
mutate_through_error (bad_expected_access<T> &)
{
}

// The lifetime check only applies to ComplexError (it owns a resource).
void
check_complex_error_lifetime (TypeTag<ComplexError>)
{
  // Arrange
  ComplexError initial_error ("Memory Test Error", 200);
  int *original_resource_ptr = initial_error.resource.get ();
  // Act and assert (no leaks when leaving the scope)
  {
    bad_expected_access<ComplexError> uut (std::move (initial_error));
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
check_complex_error_lifetime (TypeTag<T>)
{
  SUCCEED () << "Test not applicable for non-ComplexError types.";
}

// Changes the error of `changed` after `kept` was copied or assigned from it,
// then checks that `kept` still holds `expected`.
void
expect_independent_copy (bad_expected_access<int> &changed,
                         bad_expected_access<int> const &kept,
                         int const &expected, int new_value)
{
  changed.error () = new_value;
  EXPECT_NE (kept.error (), changed.error ());
  EXPECT_EQ (kept.error (), expected);
}

void
expect_independent_copy (bad_expected_access<std::string> &changed,
                         bad_expected_access<std::string> const &kept,
                         std::string const &expected,
                         std::string const &new_value)
{
  changed.error () = new_value;
  EXPECT_NE (kept.error (), changed.error ());
  EXPECT_EQ (kept.error (), expected);
}

void
expect_independent_copy (bad_expected_access<ComplexError> &changed,
                         bad_expected_access<ComplexError> const &kept,
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
check_copy_after_copy_construction (bad_expected_access<int> &original,
                                    bad_expected_access<int> const &copy,
                                    int const &expected)
{
  expect_independent_copy (original, copy, expected, 123);
}

void
check_copy_after_copy_construction (
    bad_expected_access<std::string> &original,
    bad_expected_access<std::string> const &copy, std::string const &expected)
{
  expect_independent_copy (original, copy, expected, "Changed Original");
}

void
check_copy_after_copy_construction (
    bad_expected_access<ComplexError> &original,
    bad_expected_access<ComplexError> const &copy,
    ComplexError const &expected)
{
  expect_independent_copy (original, copy, expected,
                           "Changed Original Message", 500);
}

template <typename T>
void
check_copy_after_copy_construction (bad_expected_access<T> &,
                                    bad_expected_access<T> const &, T const &)
{
}

void
check_copy_after_copy_assignment (bad_expected_access<int> &source,
                                  bad_expected_access<int> const &target,
                                  int const &expected)
{
  expect_independent_copy (source, target, expected, 456);
}

void
check_copy_after_copy_assignment (
    bad_expected_access<std::string> &source,
    bad_expected_access<std::string> const &target,
    std::string const &expected)
{
  expect_independent_copy (source, target, expected, "Changed Source");
}

void
check_copy_after_copy_assignment (
    bad_expected_access<ComplexError> &source,
    bad_expected_access<ComplexError> const &target,
    ComplexError const &expected)
{
  expect_independent_copy (source, target, expected, "Changed Source Message",
                           600);
}

template <typename T>
void
check_copy_after_copy_assignment (bad_expected_access<T> &,
                                  bad_expected_access<T> const &, T const &)
{
}

// A distinct error per thread index.
void
assign_thread_error (int &error, int i)
{
  error = i + 1;
}

void
assign_thread_error (std::string &error, int i)
{
  error = "Error " + std::to_string (i + 1);
}

void
assign_thread_error (SimpleError &error, int i)
{
  error = static_cast<SimpleError> (i % 3 + 1);
}

void
assign_thread_error (ComplexError &error, int i)
{
  error = ComplexError ("Thread Error " + std::to_string (i + 1), 300 + i);
}

template <typename T>
void
assign_thread_error (T &error, int)
{
  error = T (); // Default value
}
} // namespace

// === Fixture for bad_expected_access
// =========================================
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
    init_error_pair (error_val1, error_val2);
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
  bad_expected_access<TypeParam> uut (std::move (initial_error));
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
  bad_expected_access<TypeParam> uut (std::move (initial_error));
  // Assert
  EXPECT_EQ (uut.error (), expected_error);
  // Assert that mutation through the lvalue reference changes internal state.
  mutate_through_error (uut);
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
  bad_expected_access<TypeParam> uut (std::move (initial_error));
  // Act
  bad_expected_access<TypeParam> const &const_uut = uut;
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
  bad_expected_access<TypeParam> uut (std::move (initial_error));
  // Act
  TypeParam moved_error = std::move (uut).error ();
  // Assert
  EXPECT_EQ (moved_error, expected_error);
  // For ComplexError, the resource inside uut must be moved (the source
  // ComplexError has code 0 and no resource).
  expect_moved_from (uut.error ());
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
  bad_expected_access<TypeParam> uut (std::move (initial_error));
  // Act
  TypeParam const_moved_error
      = std::move (static_cast<bad_expected_access<TypeParam> const &> (uut))
            .error ();
  // Assert
  EXPECT_EQ (const_moved_error, expected_error);
  // For ComplexError, the resource inside uut must not be moved: the source
  // ComplexError is not changed.
  expect_equal_complex (uut.error (), expected_error);
}

// === Memory and lifetime tests =======================================

// Check that creating and destroying bad_expected_access with ComplexError
// does not leak and releases resources.
// ComplexError uses unique_ptr to track ownership.
TYPED_TEST (BadExpectedAccessTest, MemorySafety_ComplexErrorDestructorCalled)
{
  check_complex_error_lifetime (TypeTag<TypeParam> ());
}

// Check the bad_expected_access copy constructor.
// Assert that the copy holds an independent error.
TYPED_TEST (BadExpectedAccessTest, CopyConstructor_CopiesErrorCorrectly)
{
  // Arrange
  TypeParam initial_error = this->error_val1;
  bad_expected_access<TypeParam> original_uut (std::move (initial_error));
  TypeParam expected_error_value
      = original_uut.error (); // Error value before the copy
  // Act
  bad_expected_access<TypeParam> copied_uut
      = original_uut; // Call the copy constructor
  // Assert
  EXPECT_EQ (copied_uut.error (), expected_error_value);
  // Mutating the original must not affect the copy
  check_copy_after_copy_construction (original_uut, copied_uut,
                                      expected_error_value);
}

// Check bad_expected_access copy assignment.
// Assert that the target gets an independent error copy and old resources are
// released.
TYPED_TEST (BadExpectedAccessTest, CopyAssignment_CopiesErrorCorrectly)
{
  // Arrange
  TypeParam initial_error_src = this->error_val1;
  bad_expected_access<TypeParam> src_uut (std::move (initial_error_src));
  TypeParam initial_error_dst = this->error_val2;
  bad_expected_access<TypeParam> dst_uut (std::move (initial_error_dst));
  TypeParam expected_error_value = src_uut.error ();

  // Act
  dst_uut = src_uut; // Call copy assignment
  // Assert
  EXPECT_EQ (dst_uut.error (), expected_error_value);
  // Mutating the source must not affect the target
  check_copy_after_copy_assignment (src_uut, dst_uut, expected_error_value);
}

// Check bad_expected_access move assignment.
// Assert that resources move from the source to the target,
//      and the source stays valid but changed.
TYPED_TEST (BadExpectedAccessTest, MoveAssignment_MovesErrorCorrectly)
{
  // Arrange
  TypeParam initial_error_src = this->error_val1;
  bad_expected_access<TypeParam> src_uut (std::move (initial_error_src));
  TypeParam initial_error_dst = this->error_val2;
  bad_expected_access<TypeParam> dst_uut (std::move (initial_error_dst));
  TypeParam expected_error_value
      = src_uut.error (); // Error value before the move
  // Act
  dst_uut = std::move (src_uut); // Call move assignment
  // Assert
  EXPECT_EQ (dst_uut.error (), expected_error_value);
  // Check that src_uut now holds the moved-from resource
  expect_moved_from (src_uut.error ());
  // For simple types such as int or std::string, the source may stay unchanged
  // or be valid but unspecified. Its value is not checked
  // after the move because that is not part of the contract.
}

// === Thread-safety tests (independent instances) =============

// Check that concurrent construction and access to distinct
// bad_expected_access instances is correct. Each thread must construct its
// bad_expected_access and read the correct error.
TYPED_TEST (BadExpectedAccessTest, ThreadSafety_MultipleIndependentInstances)
{
  constexpr int num_threads = 10;
  std::vector<std::thread> threads;
  std::vector<bad_expected_access<TypeParam>> errors;
  errors.reserve (num_threads);

  // Arrange
  // Create source errors per thread
  std::vector<TypeParam> initial_errors (num_threads);
  for (int i = 0; i < num_threads; ++i)
    assign_thread_error (initial_errors[i], i);

  // Act
  for (int i = 0; i < num_threads; ++i)
    {
      threads.emplace_back (
          [&, i] ()
            {
              // Each thread constructs its bad_expected_access
              TypeParam expected_error_in_thread = initial_errors[i];
              bad_expected_access<TypeParam> uut (
                  std::move (initial_errors[i]));
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
  // Precondition: construct and access bad_expected_access many times.
  // Action: time construction and error() access.
  // Expected state: the operation finishes in acceptable time.
  // Benchmark the constructor and error() to find bottlenecks.
  // Assert that performance matches expectations.
  int const N = 1000000;
  auto start = std::chrono::high_resolution_clock::now ();

  for (int i = 0; i < N; ++i)
    {
      TypeParam error_data;
      assign_perf_error (error_data, i);

      bad_expected_access<TypeParam> uut (std::move (error_data));
      // Call error() to simulate use
      LUMEX_ATTRIBUTE_MAYBE_UNUSED auto &err = uut.error ();
    }

  auto dur = std::chrono::duration_cast<std::chrono::milliseconds> (
      std::chrono::high_resolution_clock::now () - start);

  // Expected time depends heavily on ErrorType.
  // ComplexError is much slower because of unique_ptr and std::string.
  // Use a higher threshold for ComplexError.
  long long const threshold = perf_threshold_ms (TypeTag<TypeParam> ()); // ms

  EXPECT_LT (dur.count (), threshold)
      << "Construction and access for " << N << " BadExpectedAccess<"
      << type_label (TypeTag<TypeParam> ()) << "> too slow: " << dur.count ()
      << "ms (Threshold: " << threshold << "ms)";
#else
  GTEST_SKIP ()
      << "wall-clock Perf_* thresholds are Release-only (no sanitizers)";
#endif
}
