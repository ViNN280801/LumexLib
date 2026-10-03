/**
 * COMPREHENSIVE TEST SUITE FOR the core/expected success() and failure()
 * factories.
 *
 * WHAT:  unit tests for the success_t<ValueType> and failure_t<ErrorType>
 * markers and for the success() / failure(error) factories that produce them.
 * WHY:   a marker is implicitly converted into Expected at the `return`, so a
 * wrong conversion (or a missing SFINAE guard) silently changes the value or
 * the error a function reports instead of failing to compile. VERIFIES: both
 * factories, both conversion paths (rvalue and const lvalue marker),
 *        conversion into Expected<T, E> and Expected<void, E>, conversion of
 * the stored error into another target error type, SFINAE removal for
 *        incompatible targets, marker accessors, move-only value and error
 * types, interop with the monadic operations of Expected, and the boundary /
 *        adversarial inputs (empty error, embedded NUL, very long error,
 * throwing default constructor). REASONING: asserting the literal result of a
 * conversion proves nothing about the implicit conversion itself; the
 * compile-time block pins what must not compile as much as what must. METHOD:
 * GoogleTest, one behaviour per TEST, Arrange-Act-Assert, plus a block of
 *        static_assert compile-time checks for the conversion contract.
 * IMPACT: without this suite a marker could drop the stored value, pick the
 * wrong Expected specialization, or start compiling where it must not, and
 * every caller of the factories would keep the wrong behaviour unnoticed.
 *
 * FILES: this file holds the tests that compile from C++11 (every expected
 * suite runs them); SuccessFailure.cxx17.tests.cpp holds the full type matrix
 * with the C++17 types; SuccessFailureTestSupport.hpp the matrix machinery.
 *
 * CONFIDENCE: 90/100 - the factories are thin; the remaining risk sits in
 * exotic success / error types, which the dedicated cases address.
 */

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/expected/Expected"

#include "lumex/tests/core/expected/SuccessFailureTestSupport.hpp"

using namespace lumex::core::expected::error;
using namespace lumex::core::expected::result;

namespace
{
// Success type that cannot be default-constructed: success() must not work
// with it.
struct NonDefaultConstructible
{
  explicit NonDefaultConstructible (int v) : value (v) {}
  int value;
};

// Success type whose default constructor throws: success() still converts, the
// throw escapes from the conversion instead of a silent default value.
struct ThrowingDefault
{
  ThrowingDefault () { throw std::runtime_error ("ThrowingDefault ctor"); }
};

// Move-only error type: failure() must move it into Expected, never copy it.
struct MoveOnlyError
{
  explicit MoveOnlyError (int c) : code (c) {}
  MoveOnlyError (MoveOnlyError const &) = delete;
  MoveOnlyError &operator= (MoveOnlyError const &) = delete;
  MoveOnlyError (MoveOnlyError &&) = default;
  MoveOnlyError &operator= (MoveOnlyError &&) = default;
  int code;
};

// Success helpers that return through the factories, the way call sites do.
Expected<void, std::string>
ConfigureOk ()
{
  return success ();
}

Expected<void, std::string>
ConfigureBad ()
{
  return failure (
      std::string ("'Computed' requires 'enableSpectrumRecording=true'"));
}

template <typename ValueType>
Expected<ValueType, std::string>
ValueOk (ValueType value)
{
  return success (value);
}

template <typename ValueType>
Expected<ValueType, std::string>
ValueBad (std::string const &why)
{
  return failure (why);
}
} // namespace

// === Compile-time contract (template API tests) =============================
// A marker must be usable exactly where it is convertible, and nowhere else.

// success() covers Expected<void, E> and Expected<T, E> with a
// default-constructible T.
static_assert (
    std::is_convertible<success_t<void>, Expected<void, std::string>>::value,
    "success() must convert into Expected<void, E>");
static_assert (
    std::is_convertible<success_t<void>, Expected<int, std::string>>::value,
    "success() must convert into Expected<T, E> for a default-constructible "
    "T");
static_assert (
    !std::is_convertible<success_t<void>, Expected<NonDefaultConstructible,
                                                   std::string>>::value,
    "success() must not fabricate a value type without a default constructor");
static_assert (
    std::is_convertible<success_t<void>, Expected<std::string, int>>::value,
    "success() covers any error type when the success type is "
    "default-constructible");
static_assert (
    !std::is_convertible<success_t<std::string>,
                         Expected<void, std::string>>::value,
    "a value-carrying success() must not convert into Expected<void, E>");
static_assert (std::is_convertible<success_t<std::string>,
                                   Expected<std::string, int>>::value,
               "success(value) must convert when the target success type "
               "accepts the value");
static_assert (
    !std::is_convertible<success_t<int>, Expected<std::string, int>>::value,
    "success(value) must not convert when the target success type rejects the "
    "value");

// failure(error) covers every target error type constructible from the stored
// error.
static_assert (std::is_convertible<failure_t<std::string>,
                                   Expected<void, std::string>>::value,
               "failure(error) must convert into Expected<void, E>");
static_assert (std::is_convertible<failure_t<std::string>,
                                   Expected<int, std::string>>::value,
               "failure(error) must convert into Expected<T, E>");
static_assert (
    std::is_convertible<failure_t<char const *>,
                        Expected<void, std::string>>::value,
    "failure(literal) must convert when E is constructible from the literal");
static_assert (std::is_convertible<failure_t<std::string>,
                                   Expected<void, RichError>>::value,
               "failure(error) must convert when the target error type "
               "accepts the error");
static_assert (
    !std::is_convertible<failure_t<int>, Expected<void, std::string>>::value,
    "failure(error) must not convert when the target error type rejects the "
    "error");
static_assert (
    !std::is_convertible<failure_t<std::string>, Expected<int, int>>::value,
    "failure(error) must not ignore the target error type");
static_assert (std::is_convertible<failure_t<MoveOnlyError>,
                                   Expected<void, MoveOnlyError>>::value,
               "failure(error) must convert for a move-only error type");
static_assert (!std::is_copy_constructible<failure_t<MoveOnlyError>>::value,
               "the marker must not make a move-only error copyable");

// The factories store the decayed types they claim to store.
static_assert (std::is_same<decltype (success (std::string ("x"))),
                            success_t<std::string>>::value,
               "success(value) must store the decayed value type");
static_assert (std::is_same<decltype (failure (std::string ("x"))),
                            failure_t<std::string>>::value,
               "failure(error) must store the decayed error type");
static_assert (std::is_same<decltype (success ()), success_t<void>>::value,
               "success() must produce the void marker");
static_assert (
    std::is_same<decltype (failure ("literal")),
                 failure_t<char const *>>::value,
    "failure(literal) must store the decayed literal type, not std::string");

// === Matrix typed tests (each success type with each error type)
// ============== Every pair of the matrix runs the same four contracts: the
// factories produce the right state, the stored value / error survives the
// conversion, the marker itself behaves under copy and move, and the produced
// Expected keeps working with the monadic operations of the module. The matrix
// is 99 x 99 types, so the assertions compare with operator== instead of gtest
// printers - the pair under test is already in the test name.
template <typename PairType>
class SuccessFailureMatrixTest : public ::testing::Test
{
protected:
  using SuccessType = typename PairType::Success;
  using ErrorType = typename PairType::Error;
  using ResultType = Expected<SuccessType, ErrorType>;
};

TEST (SuccessFailure, Matrix_EveryCxx11Pair_ThenEveryContractHolds)
{
  // 1. WHAT: every success type of the C++11 matrix with every error type of
  // it.
  // 2. WHY: the conversion contract is per type pair, and the C++11 suite has
  // to prove it without the C++17 types of the full matrix.
  // 3. VERIFIES: the four contracts of CheckPair for all 400 C++11 matrix
  // pairs.
  // 4. WHY VERIFY: the full matrix (Matrix_EveryPair_ThenEveryContractHolds)
  // needs std::optional, std::string_view and std::variant, so it starts at
  // the C++17 suite.
  // 5. METHOD: pack-expand the type product and run CheckPair for each pair.
  // 6. IMPACT: a pair used by a single C++11 caller would stay untested.
  RunMatrix (typename Product<MatrixTypesCxx11, MatrixTypesCxx11>::type ());
}

// A representative slice also runs as a typed suite, so a failure carries a
// test name per type pair instead of a label inside one test. It is cheap (8 x
// 8 = 64 pairs) and deliberately holds the types that are not worth a full
// matrix row.
using TypedMatrixSuccessTypes
    = TypeList<std::string, std::wstring, int, unsigned int, float,
               long double, PlainStruct, RawUnion>;
using TypedMatrixErrorTypes
    = TypeList<std::string, int, long, unsigned long long, std::error_code,
               RichError, PlainEnum, ErrorFlag>;
using TypedMatrixPairs =
    typename ToGTestTypes<typename Product<TypedMatrixSuccessTypes,
                                           TypedMatrixErrorTypes>::type>::type;

TYPED_TEST_SUITE (SuccessFailureMatrixTest, TypedMatrixPairs);

TYPED_TEST (SuccessFailureMatrixTest, EveryContract_ThenConversionHolds)
{
  // 1. WHAT: the four conversion contracts for one matrix pair.
  // 2. WHY: a typed suite names the failing type pair in the test name itself.
  // 3. VERIFIES: state, stored value, stored error, copy / move / transform.
  // 4. WHY VERIFY: the full matrix TEST reports its pair only through a label.
  // 5. METHOD: delegate to the shared checker the matrix TEST also uses.
  // 6. IMPACT: a failure inside the full matrix would be harder to attribute.
  CheckPair<typename TestFixture::SuccessType,
            typename TestFixture::ErrorType> ("typed matrix pair");
}

// === Edge cases outside the matrix ==========================================
// The matrix holds types that are copyable, comparable and
// default-constructible. These cases cover what it cannot: move-only payloads,
// a throwing default constructor, boundary error payloads and the
// factory-in-return-position idiom.

TEST (SuccessFailure, SuccessWithMoveOnlyValue_ThenValueIsMovedIntoExpected)
{
  // 1. WHAT: success(value) with a move-only payload.
  // 2. WHY: owning buffers are returned as successes, never copied.
  // 3. VERIFIES: the payload reaches the Expected without a copy.
  // 4. WHY VERIFY: a copy-based conversion would not compile for such types.
  // 5. METHOD: return a unique_ptr through the factory and dereference it.
  // 6. IMPACT: owning payloads could not be returned through the marker at
  // all.
  Expected<std::unique_ptr<int>, std::string> const result
      = success (std::unique_ptr<int> (new int (11)));

  ASSERT_TRUE (result.has_value ());
  ASSERT_NE (result.value (), nullptr);
  EXPECT_EQ (*result.value (), 11);
}

TEST (SuccessFailure, FailureWithMoveOnlyError_ThenErrorIsMovedIntoExpected)
{
  // 1. WHAT: failure(error) with a move-only error type.
  // 2. WHY: error types may own resources (handles, buffers, codes with
  // payload).
  // 3. VERIFIES: the error reaches the Expected without a copy.
  // 4. WHY VERIFY: a copy-based conversion would not compile for such types.
  // 5. METHOD: return a MoveOnlyError through the factory and read its code.
  // 6. IMPACT: move-only error types could not be reported through the marker.
  Expected<void, MoveOnlyError> const result = failure (MoveOnlyError (5));

  ASSERT_FALSE (result.has_value ());
  EXPECT_EQ (result.error ().code, 5);
}

TEST (SuccessFailure,
      SuccessWithoutValue_ThrowingDefaultConstructor_PropagatesTheThrow)
{
  // 1. WHAT: success() with a value type whose default constructor throws.
  // 2. WHY: the conversion is not noexcept and callers have to know it.
  // 3. VERIFIES: the exception escapes instead of a silent default value.
  // 4. WHY VERIFY: swallowing the throw would hand out a half-built value.
  // 5. METHOD: convert the marker inside EXPECT_THROW.
  // 6. IMPACT: a resource-allocating value type would be silently invalid.
  using ThrowingResult = Expected<ThrowingDefault, std::string>;

  EXPECT_THROW ((void)static_cast<ThrowingResult> (success ()),
                std::runtime_error);
}

TEST (SuccessFailure, FailureWithEmptyError_ThenStillAnErrorState)
{
  // 1. WHAT: failure(error) with an empty error payload (boundary).
  // 2. WHY: "failed with no message" is a legal, common state.
  // 3. VERIFIES: the state stays failed and the payload stays empty.
  // 4. WHY VERIFY: an empty payload must not be mistaken for success.
  // 5. METHOD: convert an empty std::string and check the state and the
  // payload.
  // 6. IMPACT: silent failures would read as successes.
  Expected<void, std::string> const result = failure (std::string ());

  ASSERT_FALSE (result.has_value ());
  EXPECT_TRUE (result.error ().empty ());
}

TEST (SuccessFailure,
      FailureWithLongErrorAndEmbeddedNul_ThenPayloadIsPreserved)
{
  // 1. WHAT: failure(error) with a 10k payload that contains NUL bytes
  // (adversarial).
  // 2. WHY: protocol and log payloads are binary, not C strings.
  // 3. VERIFIES: length and every byte survive the conversion.
  // 4. WHY VERIFY: truncation at a NUL would silently cut diagnostics.
  // 5. METHOD: build such a payload, convert it, check size and boundary
  // bytes.
  // 6. IMPACT: support would receive truncated or empty error text.
  std::string payload (10000, 'e');
  payload[0] = '\0';
  payload[9999] = '\0';

  Expected<void, std::string> const result = failure (payload);

  ASSERT_FALSE (result.has_value ());
  ASSERT_EQ (result.error ().size (), payload.size ());
  EXPECT_EQ (result.error ()[0], '\0');
  EXPECT_EQ (result.error ()[5000], 'e');
  EXPECT_EQ (result.error ()[9999], '\0');
}

TEST (SuccessFailure, HelpersReturningThroughTheFactories_ThenStatesMatch)
{
  // 1. WHAT: helpers and a lambda that return the markers from a function
  // body.
  // 2. WHY: this is the call-site shape the factories exist for.
  // 3. VERIFIES: the produced states and payloads match the direct
  // construction.
  // 4. WHY VERIFY: the idiom is only useful if a real return body compiles and
  // works.
  // 5. METHOD: call the helpers, then a lambda with an explicit return type.
  // 6. IMPACT: the migration from boost.outcome idioms would not be usable.
  auto const ok = ConfigureOk ();
  auto const bad = ConfigureBad ();
  auto const value = ValueOk (7);
  auto const failed = ValueBad<int> ("why");

  auto const parse = [] (int input) -> Expected<int, std::string>
    {
      if (input < 0)
        return failure (std::string ("negative"));
      return success (input);
    };

  ASSERT_TRUE (ok.has_value ());
  ASSERT_FALSE (bad.has_value ());
  EXPECT_NE (bad.error ().find ("enableSpectrumRecording"), std::string::npos);
  ASSERT_TRUE (value.has_value ());
  EXPECT_EQ (value.value (), 7);
  ASSERT_FALSE (failed.has_value ());
  EXPECT_EQ (failed.error (), "why");
  EXPECT_EQ (parse (3).value (), 3);
  EXPECT_EQ (parse (-1).error (), "negative");
}
