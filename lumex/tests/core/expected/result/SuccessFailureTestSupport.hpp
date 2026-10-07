/*
 * Copyright (c) 2026 Vladislav Semykin <vladislav.semykin@gmail.com>
 *
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of
 * charge, to any person obtaining a copy
 * of this software and associated
 * documentation files (the "Software"), to deal
 * in the Software without
 * restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef LUMEX_TESTS_CORE_EXPECTED_SUCCESS_FAILURE_TEST_SUPPORT_HPP
#define LUMEX_TESTS_CORE_EXPECTED_SUCCESS_FAILURE_TEST_SUPPORT_HPP

#include <array>
#include <bitset>
#include <chrono>
#include <complex>
#include <cstddef>
#include <deque>
#include <map>
#include <string>
#include <system_error>
#include <tuple>
#include <typeinfo>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/expected/Expected"
#include "lumex/core/utility/attr/LumexAttributes.hpp"

// The type matrix of the success() / failure() tests: the composite types,
// one sample value per type, the compile-time product of two type lists and
// the per-pair checker. SuccessFailure.cxx11.tests.cpp runs the matrix over
// the types that exist in C++11 (MatrixTypesCxx11 below);
// SuccessFailure.cxx17.tests.cpp adds std::string_view, std::optional and
// std::variant types and runs the full matrix. The types stay in an
// unnamed namespace: the CTest names of the typed slice spell the type
// parameters, "(anonymous namespace)::PlainStruct" and so on.

namespace
{
using lumex::core::expected::result::expected;
using lumex::core::expected::result::failure;
using lumex::core::expected::result::failure_t;
using lumex::core::expected::result::success;
using lumex::core::expected::result::success_t;

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
// The helper is called Get in every specialization of the block.
// NOLINTBEGIN(readability-identifier-naming)
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
// NOLINTEND(readability-identifier-naming)

// === Nested composite aliases (one per nesting level)
// ======================== Levels 2-5 prove the conversion machinery is
// depth-agnostic. One alias per level is deliberate: the matrix below is
// MatrixTypes x MatrixTypes, so every alias costs as many pairs as any other
// type (see the cost note at MatrixTypes).

using L2_ChronoText
    = std::tuple<std::chrono::milliseconds, std::string, unsigned char>;
using L3_UnionTexts
    = std::tuple<L2_ChronoText, std::deque<std::string>, float>;
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

// The types of the matrix that exist in C++11, in the order of the full matrix
// of SuccessFailure.cxx17.tests.cpp (see the cost note there before adding a
// type).
using MatrixTypesCxx11 = TypeList<
    // level 0: primitives
    bool, char, int, long long, double,
    // level 1: standard value and container types
    std::string, std::vector<int>, std::map<int, std::string>,
    std::pair<int, double>, std::tuple<int, double, char>, std::bitset<8>,
    std::chrono::milliseconds, std::error_code,
    // user-defined shapes: enum, aggregate, a class with an invariant, unions
    Mode, PlainStruct, TextHolder, RawUnion, NumericUnion,
    // levels 2-3: nested composites
    L2_ChronoText, L3_UnionTexts>;

// Every pair of the matrix runs the same four contracts: the factories produce
// the right state, the stored value and error survive the conversion, and the
// produced expected keeps working with copy, move and the monadic operations.
// Comparing with operator== instead of the gtest printers keeps the
// instantiation cost down for the 625 pairs; the label names the pair in the
// failure text.
template <typename SuccessType, typename ErrorType>
void
check_pair (std::string const &label)
{
  using ResultType = expected<SuccessType, ErrorType>;

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
type_name ()
{
  return typeid (T).name ();
}
#else
template <typename T>
std::string
type_name ()
{
  return "<unnamed>";
}
#endif

// Runs check_pair for every pair of the list, labelled with its position in
// the list and the type names. The elements of a braced list are evaluated in
// order, so the index counts the pairs in list order.
template <typename... PairTypes>
void
run_matrix (TypeList<PairTypes...>)
{
  std::size_t index = 0;
  int unused[]
      = { (check_pair<typename PairTypes::Success, typename PairTypes::Error> (
               "matrix pair " + std::to_string (index++) + " ["
               + type_name<typename PairTypes::Success> () + " | "
               + type_name<typename PairTypes::Error> () + "]"),
           0)...,
          0 };
  LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (unused);
}
} // namespace

#endif // !LUMEX_TESTS_CORE_EXPECTED_SUCCESS_FAILURE_TEST_SUPPORT_HPP
