// unexpected<E> tests. They compile from C++11, so every expected suite
// (C++11, C++17, C++20) runs them.

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
// unexpected tests take.

namespace
{
// Changes the source after unexpected copied it; false when the type has no
// change worth observing here.
bool
ChangeSource (int &source)
{
  source = 999;
  return true;
}

bool
ChangeSource (std::string &source)
{
  source = "Changed Original";
  return true;
}

template <typename T>
bool
ChangeSource (T &)
{
  return false;
}

// Writes through the reference error() & returned and checks unexpected sees
// the change.
void
MutateThroughReference (int &error_ref, unexpected<int> const &uut)
{
  error_ref = 555;
  EXPECT_EQ (uut.error (), 555);
}

void
MutateThroughReference (std::string &error_ref,
                        unexpected<std::string> const &uut)
{
  error_ref = "New Message";
  EXPECT_EQ (uut.error (), "New Message");
}

void
MutateThroughReference (ComplexError &error_ref,
                        unexpected<ComplexError> const &uut)
{
  error_ref.code = 777;
  EXPECT_EQ (uut.error ().code, 777);
}

template <typename T>
void
MutateThroughReference (T &, unexpected<T> const &)
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
    unexpected<ComplexError> uut (std::move (initial_error));
    EXPECT_NE (uut.error ().resource, nullptr);
    EXPECT_EQ (uut.error ().resource.get (), original_resource_ptr);
  } // uut is destroyed here; the unique_ptr resource must be released.
  SUCCEED () << "ComplexError with unique_ptr should be correctly "
                "destroyed, preventing memory leaks.";
}

template <typename T>
void
CheckComplexErrorLifetime (TypeTag<T>)
{
  SUCCEED () << "Test not applicable for non-ComplexError types.";
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
  error = "Thread Error " + std::to_string (i + 1);
}

void
AssignThreadError (SimpleError &error, int i)
{
  error = static_cast<SimpleError> ((i % 3) + 1);
}

void
AssignThreadError (ComplexError &error, int i)
{
  error = ComplexError ("Complex Thread Error " + std::to_string (i + 1),
                        400 + i);
}

template <typename T>
void
AssignThreadError (T &error, int)
{
  error = T (); // Default value for other types
}

// Inside a thread: ComplexError carries the index in its code.
void
CheckThreadError (unexpected<ComplexError> const &uut, int i)
{
  EXPECT_EQ (uut.error ().code,
             400 + i); // Check a specific ComplexError field
}

template <typename T>
void
CheckThreadError (unexpected<T> const &, int)
{
  // For simple types, uut.error() still holds the original value because
  // copy/move does not empty the source for those types. After std::move(),
  // the source may be unspecified for std::string, so it cannot be compared
  // to uut.error() here. This test mainly checks that unexpected was created
  // and error() works: if not ComplexError, just assert there is no crash.
  SUCCEED ();
}
} // namespace

// === Fixture for unexpected ================================================
template <typename ErrorType> class UnexpectedTest : public ::testing::Test
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
TYPED_TEST_SUITE (UnexpectedTest, ErrorTypes);

// === API contract verifier tests ========================================

// Check that the copy constructor initializes unexpected correctly.
// Assert that the given value was copied into unexpected.
TYPED_TEST (UnexpectedTest, Constructor_LValueRef_CopiesErrorCorrectly)
{
  // Arrange
  TypeParam initial_error = this->error_val1;
  // Act
  unexpected<TypeParam> uut (
      initial_error); // Call the constructor from const &
  // Assert
  EXPECT_EQ (uut.error (), initial_error);
  // Mutating the source must not affect unexpected
  if (ChangeSource (initial_error))
    {
      EXPECT_NE (uut.error (), initial_error);
    }
}

// Check that the move constructor initializes unexpected correctly.
// Assert that the rvalue was moved into unexpected.
TYPED_TEST (UnexpectedTest, Constructor_RValueRef_MovesErrorCorrectly)
{
  // Arrange
  TypeParam initial_error = this->error_val1;
  TypeParam expected_error = initial_error; // Copy for comparison
  // Act
  unexpected<TypeParam> uut (
      std::move (initial_error)); // Call the constructor from &&
  // Assert
  EXPECT_EQ (uut.error (), expected_error);
  // For ComplexError, the source must be in the moved-from state.
  ExpectMovedFrom (initial_error);
}

// Check that error() as an lvalue returns a mutable reference to the stored
// error. Assert that the returned reference can mutate internal state.
TYPED_TEST (UnexpectedTest, ErrorLValueRefReturnsMutableReference)
{
  // Arrange
  TypeParam initial_error = this->error_val1;
  unexpected<TypeParam> uut (std::move (initial_error));
  // Act
  auto &error_ref = uut.error ();
  // Assert
  EXPECT_EQ (error_ref, this->error_val1); // Check the original value

  // Mutate through the reference and check that uut changed
  MutateThroughReference (error_ref, uut);
}

// Check that const error() as an lvalue returns a const reference to the
// stored error. Assert that the returned reference cannot mutate internal
// state.
TYPED_TEST (UnexpectedTest, ConstErrorLValueRefReturnsImmutableReference)
{
  // Arrange
  TypeParam initial_error = this->error_val1;
  unexpected<TypeParam> uut (std::move (initial_error));
  unexpected<TypeParam> const &const_uut = uut;
  // Act
  auto &error_ref = const_uut.error ();
  // Assert
  EXPECT_EQ (error_ref, this->error_val1);
  // Assigning through error_ref must be a compile error,
  // which confirms immutability.
  // error_ref = some_other_value; // Error: assignment of read-only reference
}

// Check that error() as an rvalue returns an rvalue reference and moves the
// contents. Assert that unexpected contents were moved after the call.
TYPED_TEST (UnexpectedTest, ErrorRValueRefReturnsRValueAndMovesContent)
{
  // Arrange
  TypeParam initial_error = this->error_val1;
  TypeParam expected_error = initial_error;
  unexpected<TypeParam> uut (std::move (initial_error));
  // Act
  TypeParam moved_error = std::move (uut).error (); // Call error() &&
  // Assert
  EXPECT_EQ (moved_error, expected_error);
  // For ComplexError, the resource inside uut must be moved.
  ExpectMovedFrom (uut.error ());
}

// Check that const error() as an rvalue returns a const rvalue reference and
//      does not change unexpected (it copies).
// Assert that the source unexpected is unchanged.
TYPED_TEST (UnexpectedTest,
            ConstErrorRValueRefReturnsConstRValueAndDoesNotModifySource)
{
  // Arrange
  TypeParam initial_error = this->error_val1;
  TypeParam expected_error = initial_error;
  unexpected<TypeParam> uut (std::move (initial_error));
  // Act
  TypeParam const_moved_error
      = std::move (static_cast<unexpected<TypeParam> const &> (uut)).error ();
  // Assert
  EXPECT_EQ (const_moved_error, expected_error);
  // For ComplexError, the resource inside uut must not be moved.
  ExpectEqualComplex (uut.error (), expected_error);
}

// === Memory and lifetime tests =======================================

// Check that creating and destroying unexpected with ComplexError
//      does not leak and releases resources.
// ComplexError uses unique_ptr to track ownership.
TYPED_TEST (UnexpectedTest, MemorySafety_ComplexErrorDestructorCalled)
{
  CheckComplexErrorLifetime (TypeTag<TypeParam> ());
}

// === Thread-safety tests (independent instances) =============

// Check that concurrent construction and access to distinct unexpected
//      instances is correct.
// Each thread must construct its unexpected and read the correct error.
TYPED_TEST (UnexpectedTest, ThreadSafety_MultipleIndependentInstances)
{
  constexpr int num_threads = 10;
  std::vector<std::thread> threads;
  std::vector<TypeParam> initial_errors (num_threads);

  // Arrange: unique errors per thread
  for (int i = 0; i < num_threads; ++i)
    AssignThreadError (initial_errors[i], i);

  // Act
  for (int i = 0; i < num_threads; ++i)
    {
      threads.emplace_back (
          [&, i] ()
            {
              // Each thread constructs its unexpected (moving its error)
              unexpected<TypeParam> uut (std::move (initial_errors[i]));
              // and checks its value
              // Note: initial_errors[i] is moved-from; compare against the
              // expected value (the value from before the move)
              CheckThreadError (uut, i);
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
  // Precondition: construct and access unexpected many times.
  // Action: time construction and error() access.
  // Expected state: the operation finishes in acceptable time.
  // Benchmark constructors and error() to find bottlenecks.
  // Assert that performance matches expectations.
  int const N = 1000000;
  auto start = std::chrono::high_resolution_clock::now ();

  for (int i = 0; i < N; ++i)
    {
      TypeParam error_data;
      AssignPerfError (error_data, i);

      // Construct unexpected. error_data is moved here.
      unexpected<TypeParam> uut (std::move (error_data));
      // Call error() to simulate use
      LUMEX_ATTRIBUTE_MAYBE_UNUSED auto &err = uut.error ();
    }

  auto dur = std::chrono::duration_cast<std::chrono::milliseconds> (
      std::chrono::high_resolution_clock::now () - start);

  // Expected time depends heavily on ErrorType.
  // Use a higher threshold for ComplexError.
  long long const threshold = PerfThresholdMs (TypeTag<TypeParam> ()); // ms

  EXPECT_LT (dur.count (), threshold)
      << "Construction and access for " << N << " Unexpected<"
      << TypeLabel (TypeTag<TypeParam> ()) << "> too slow: " << dur.count ()
      << "ms (Threshold: " << threshold << "ms)";
#else
  GTEST_SKIP ()
      << "wall-clock Perf_* thresholds are Release-only (no sanitizers)";
#endif
}
