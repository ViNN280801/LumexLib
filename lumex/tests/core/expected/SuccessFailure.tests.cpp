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
 * CONFIDENCE: 90/100 - the factories are thin; the remaining risk sits in
 * exotic success / error types, which the dedicated cases address.
 */

#include <array>
#include <bitset>
#include <chrono>
#include <complex>
#include <cstddef>
#include <deque>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/expected/Expected"

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

// Error type reachable from a std::string but not identical to it. Default
// constructible and comparable as well, so it can stand in the type matrix.
struct RichError
{
  RichError () = default;
  RichError (std::string const &t) : text (t) {}
  RichError (char const *t) : text (t) {}
  bool
  operator== (RichError const &other) const
  {
    return text == other.text;
  }
  std::string text;
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
// === Hand-written composite types for the matrix
// ============================= The matrix is one list used on both axes
// (success and error), so every type is default-constructible, copyable,
// movable and comparable.

enum class Mode
{
  Idle,
  Running,
  Failed
};

enum PlainEnum
{
  PlainIdle,
  PlainRunning,
  PlainFailed
};

enum class ErrorFlag : unsigned char
{
  Ok = 0,
  Warn = 1,
  Fatal = 2
};

struct PlainStruct
{
  int number;
  double weight;
};

inline bool
operator== (PlainStruct const &lhs, PlainStruct const &rhs)
{
  return lhs.number == rhs.number && lhs.weight == rhs.weight;
}

struct MixedStruct
{
  unsigned char flags;
  float gain;
  std::string label;
  std::optional<unsigned> retries;
};

inline bool
operator== (MixedStruct const &lhs, MixedStruct const &rhs)
{
  return lhs.flags == rhs.flags && lhs.gain == rhs.gain
         && lhs.label == rhs.label && lhs.retries == rhs.retries;
}

// Class with an invariant and a non-trivial destructor.
class TextHolder
{
public:
  TextHolder () = default;
  explicit TextHolder (std::string text, std::size_t maxSize = 64)
      : m_text (std::move (text)), m_maxSize (maxSize)
  {
    if (m_text.size () > m_maxSize)
      m_text.resize (m_maxSize);
  }
  std::string const &
  text () const
  {
    return m_text;
  }
  bool
  operator== (TextHolder const &other) const
  {
    return m_text == other.m_text && m_maxSize == other.m_maxSize;
  }

private:
  std::string m_text;
  std::size_t m_maxSize = 64;
};

// Union member of the matrix: one member is initialized by the default
// constructor, so T() is well defined and the comparison is deterministic.
union RawUnion
{
  int asInt;
  double asDouble;
  char asChar;

  RawUnion () : asInt (7) {}
};

inline bool
operator== (RawUnion const &lhs, RawUnion const &rhs)
{
  return lhs.asInt == rhs.asInt;
}

union NumericUnion
{
  long long asBig;
  float asFloat;
  unsigned char asByte;

  NumericUnion () : asBig (-9LL) {}
};

inline bool
operator== (NumericUnion const &lhs, NumericUnion const &rhs)
{
  return lhs.asBig == rhs.asBig;
}

// === Sample values for the matrix
// ============================================ One sample per type serves both
// axes. The generic fallback is a default-constructed object; the
// specializations give a distinct value so the "value / error is preserved"
// checks are not vacuous. Composite and nested types rely on the fallback: for
// them the storage round trip is what matters.
template <typename T> struct Sample
{
  static T
  Get ()
  {
    return T ();
  }
};

template <> struct Sample<bool>
{
  static bool
  Get ()
  {
    return true;
  }
};
template <> struct Sample<char>
{
  static char
  Get ()
  {
    return 'x';
  }
};
template <> struct Sample<signed char>
{
  static signed char
  Get ()
  {
    return -8;
  }
};
template <> struct Sample<unsigned char>
{
  static unsigned char
  Get ()
  {
    return 250;
  }
};
template <> struct Sample<short>
{
  static short
  Get ()
  {
    return -32000;
  }
};
template <> struct Sample<unsigned short>
{
  static unsigned short
  Get ()
  {
    return 65000;
  }
};
template <> struct Sample<int>
{
  static int
  Get ()
  {
    return 42;
  }
};
template <> struct Sample<unsigned int>
{
  static unsigned int
  Get ()
  {
    return 42u;
  }
};
template <> struct Sample<long>
{
  static long
  Get ()
  {
    return -424242L;
  }
};
template <> struct Sample<unsigned long>
{
  static unsigned long
  Get ()
  {
    return 424242UL;
  }
};
template <> struct Sample<long long>
{
  static long long
  Get ()
  {
    return -4242424242LL;
  }
};
template <> struct Sample<unsigned long long>
{
  static unsigned long long
  Get ()
  {
    return 4242424242ULL;
  }
};
template <> struct Sample<float>
{
  static float
  Get ()
  {
    return 1.5f;
  }
};
template <> struct Sample<double>
{
  static double
  Get ()
  {
    return 2.5;
  }
};
template <> struct Sample<long double>
{
  static long double
  Get ()
  {
    return 3.5L;
  }
};
template <> struct Sample<std::string>
{
  static std::string
  Get ()
  {
    return "sample";
  }
};
template <> struct Sample<std::wstring>
{
  static std::wstring
  Get ()
  {
    return L"sample";
  }
};
template <> struct Sample<std::string_view>
{
  static std::string_view
  Get ()
  {
    return "sample";
  }
};
template <> struct Sample<std::vector<int>>
{
  static std::vector<int>
  Get ()
  {
    return { 1, 2, 3 };
  }
};
// Not in the matrix today: kept so a future growth step can re-add the type
// without rewriting its sample.
template <> struct Sample<std::array<int, 4>>
{
  static std::array<int, 4>
  Get ()
  {
    return { { 1, 2, 3, 4 } };
  }
};
template <> struct Sample<std::pair<int, double>>
{
  static std::pair<int, double>
  Get ()
  {
    return { 1, 2.5 };
  }
};
template <> struct Sample<std::tuple<int, double, char>>
{
  static std::tuple<int, double, char>
  Get ()
  {
    return { 1, 2.5, 'x' };
  }
};
template <> struct Sample<std::optional<int>>
{
  static std::optional<int>
  Get ()
  {
    return std::optional<int> (5);
  }
};
template <> struct Sample<std::bitset<8>>
{
  static std::bitset<8>
  Get ()
  {
    return std::bitset<8> ("10101010");
  }
};
// Not in the matrix today: kept so a future growth step can re-add the type
// without rewriting its sample.
template <> struct Sample<std::complex<double>>
{
  static std::complex<double>
  Get ()
  {
    return { 1.5, -2.5 };
  }
};
template <> struct Sample<std::chrono::milliseconds>
{
  static std::chrono::milliseconds
  Get ()
  {
    return std::chrono::milliseconds (7);
  }
};
template <> struct Sample<std::error_code>
{
  static std::error_code
  Get ()
  {
    return std::make_error_code (std::errc::invalid_argument);
  }
};
template <> struct Sample<Mode>
{
  static Mode
  Get ()
  {
    return Mode::Running;
  }
};
template <> struct Sample<PlainEnum>
{
  static PlainEnum
  Get ()
  {
    return PlainRunning;
  }
};
template <> struct Sample<ErrorFlag>
{
  static ErrorFlag
  Get ()
  {
    return ErrorFlag::Warn;
  }
};
template <> struct Sample<PlainStruct>
{
  static PlainStruct
  Get ()
  {
    return PlainStruct{ 3, 1.5 };
  }
};
template <> struct Sample<TextHolder>
{
  static TextHolder
  Get ()
  {
    return TextHolder ("sample");
  }
};
template <> struct Sample<RawUnion>
{
  static RawUnion
  Get ()
  {
    RawUnion value;
    value.asInt = 21;
    return value;
  }
};
template <> struct Sample<NumericUnion>
{
  static NumericUnion
  Get ()
  {
    NumericUnion value;
    value.asBig = 123456789LL;
    return value;
  }
};

// === Nested composite aliases (one per nesting level)
// ======================== Levels 2-5 prove the conversion machinery is
// depth-agnostic. One alias per level is deliberate: the matrix below is
// MatrixTypes x MatrixTypes, so every alias costs as many pairs as any other
// type (see the cost note at MatrixTypes).

using L2_ChronoText
    = std::tuple<std::chrono::milliseconds, std::string, unsigned char>;
using L3_UnionTexts
    = std::tuple<L2_ChronoText, std::deque<std::string>, float>;
using L4_DeepA = std::variant<L3_UnionTexts, std::map<int, L2_ChronoText>>;
using L5_TopA = std::tuple<L4_DeepA, std::optional<std::set<int>>,
                           std::vector<std::wstring>>;

// === Type matrix (one list for both axes, each type with each)
// ================ GTest type parameters are a flat list, so the product is
// built at compile time: TypeList x TypeList -> TypeList<Pair<S, E>...> ->
// ::testing::Types<...>. Adding one type to MatrixTypes grows both axes
// automatically.
template <typename... Types> struct TypeList
{
};

template <typename SuccessType, typename ErrorType> struct Pair
{
  using Success = SuccessType;
  using Error = ErrorType;
};

template <typename... Lists> struct Concat;

template <> struct Concat<>
{
  using type = TypeList<>;
};

template <typename... Types> struct Concat<TypeList<Types...>>
{
  using type = TypeList<Types...>;
};

template <typename... Left, typename... Right, typename... Rest>
struct Concat<TypeList<Left...>, TypeList<Right...>, Rest...>
{
  using type = typename Concat<TypeList<Left..., Right...>, Rest...>::type;
};

template <typename SuccessType, typename... ErrorTypes> struct Row
{
  using type = TypeList<Pair<SuccessType, ErrorTypes>...>;
};

template <typename SuccessList, typename ErrorList> struct Product;

template <typename... SuccessTypes, typename... ErrorTypes>
struct Product<TypeList<SuccessTypes...>, TypeList<ErrorTypes...>>
{
  using type = typename Concat<
      typename Row<SuccessTypes, ErrorTypes...>::type...>::type;
};

template <typename List> struct ToGTestTypes;

template <typename... Types> struct ToGTestTypes<TypeList<Types...>>
{
  using type = ::testing::Types<Types...>;
};

// The matrix is MatrixTypes x MatrixTypes, so it costs N * N pairs and the
// compile time grows quadratically: 25 types are 625 pairs, 99 types were 9801
// pairs - the latter ran for 20+ minutes on MSVC with the compiler's memory
// peaking in the 5-8 GB range again and again (clang++ needed ~40 s of
// -fsyntax-only). Grow this list one or two types at a time and re-measure;
// types that are not worth a full matrix row belong in the typed slice below.
using MatrixTypes = TypeList<
    // level 0: primitives
    bool, char, int, long long, double,
    // level 1: standard value and container types
    std::string, std::string_view, std::vector<int>,
    std::map<int, std::string>, std::pair<int, double>,
    std::tuple<int, double, char>, std::optional<int>, std::bitset<8>,
    std::chrono::milliseconds, std::error_code,
    // user-defined shapes: enum, aggregates, a class with an invariant, unions
    Mode, PlainStruct, MixedStruct, TextHolder, RawUnion, NumericUnion,
    // levels 2-5: nested composites
    L2_ChronoText, L3_UnionTexts, L4_DeepA, L5_TopA>;

using MatrixPairs = typename ToGTestTypes<
    typename Product<MatrixTypes, MatrixTypes>::type>::type;
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

// Every pair of the matrix runs the same four contracts: the factories produce
// the right state, the stored value and error survive the conversion, and the
// produced Expected keeps working with copy, move and the monadic operations.
// Comparing with operator== instead of the gtest printers keeps the
// instantiation cost down for the 625 pairs; the label names the pair in the
// failure text.
template <typename SuccessType, typename ErrorType>
void
CheckPair (std::string const &label)
{
  using ResultType = Expected<SuccessType, ErrorType>;

  ResultType const defaultResult = success ();
  ASSERT_TRUE (defaultResult.has_value ()) << label;
  EXPECT_TRUE (defaultResult.value () == SuccessType ()) << label;

  SuccessType const valueSample = Sample<SuccessType>::Get ();
  ResultType const valueResult = success (valueSample);
  ASSERT_TRUE (valueResult.has_value ()) << label;
  EXPECT_TRUE (valueResult.value () == valueSample) << label;

  ErrorType const errorSample = Sample<ErrorType>::Get ();
  ResultType const errorResult = failure (errorSample);
  ASSERT_FALSE (errorResult.has_value ()) << label;
  EXPECT_TRUE (errorResult.error () == errorSample) << label;

  success_t<SuccessType> const reusable
      = success (Sample<SuccessType>::Get ());
  ResultType const first = reusable;
  ResultType const second = reusable;
  ASSERT_TRUE (first.has_value ()) << label;
  ASSERT_TRUE (second.has_value ()) << label;
  EXPECT_TRUE (first.value () == second.value ()) << label;

  failure_t<ErrorType> movable = failure (Sample<ErrorType>::Get ());
  ResultType const moved = std::move (movable);
  EXPECT_FALSE (moved.has_value ()) << label;

  ResultType const transformed
      = first.transform ([] (SuccessType const &value) { return value; });
  ASSERT_TRUE (transformed.has_value ()) << label;
  EXPECT_TRUE (transformed.value () == first.value ()) << label;
}

// Type names for the failure labels; without RTTI the label keeps its index.
#if defined(__cpp_rtti) || defined(_CPPRTTI)
template <typename T>
std::string
TypeName ()
{
  return typeid (T).name ();
}
#else
template <typename T>
std::string
TypeName ()
{
  return "<unnamed>";
}
#endif

template <typename... PairTypes, std::size_t... Indices>
void
RunMatrixImpl (TypeList<PairTypes...>, std::index_sequence<Indices...>)
{
  int unused[]
      = { (CheckPair<typename PairTypes::Success, typename PairTypes::Error> (
               "matrix pair " + std::to_string (Indices) + " ["
               + TypeName<typename PairTypes::Success> () + " | "
               + TypeName<typename PairTypes::Error> () + "]"),
           0)...,
          0 };
  (void)unused;
}

template <typename... PairTypes>
void
RunMatrix (TypeList<PairTypes...> pairs)
{
  RunMatrixImpl (pairs, std::make_index_sequence<sizeof...(PairTypes)> ());
}

TEST (SuccessFailure, Matrix_EveryPair_ThenEveryContractHolds)
{
  // 1. WHAT: every success type of the matrix with every error type of it.
  // 2. WHY: the conversion contract is per type pair; partial coverage hides
  // bugs.
  // 3. VERIFIES: the four contracts of CheckPair for all 625 matrix pairs.
  // 4. WHY VERIFY: a typed suite cannot take that many type parameters (the
  // gtest machinery exceeds the template instantiation depth), so the full
  // matrix runs as one test with a per-pair label.
  // 5. METHOD: pack-expand the type product and run CheckPair for each pair.
  // 6. IMPACT: a pair used by a single caller would stay untested.
  RunMatrix (typename Product<MatrixTypes, MatrixTypes>::type ());
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
      = success (std::make_unique<int> (11));

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
