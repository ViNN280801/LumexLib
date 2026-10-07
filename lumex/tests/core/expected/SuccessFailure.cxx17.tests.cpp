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

// success() / failure() tests that need C++17: the full type matrix, which
// adds std::string_view, std::optional and std::variant types to the C++11
// ones. The expected suites of C++17 and C++20 compile this file next to
// SuccessFailure.cxx11.tests.cpp.

#include <bitset>
#include <chrono>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/expected/Expected"

#include "lumex/tests/core/expected/SuccessFailureTestSupport.hpp"

namespace
{
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

template <> struct Sample<std::string_view>
{
  static std::string_view
  Get ()
  {
    return "sample";
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

using L4_DeepA = std::variant<L3_UnionTexts, std::map<int, L2_ChronoText>>;
using L5_TopA = std::tuple<L4_DeepA, std::optional<std::set<int>>,
                           std::vector<std::wstring>>;

// The matrix is MatrixTypes x MatrixTypes, so it costs N * N pairs and the
// compile time grows quadratically: 25 types are 625 pairs, 99 types were 9801
// pairs - the latter ran for 20+ minutes on MSVC with the compiler's memory
// peaking in the 5-8 GB range again and again (clang++ needed ~40 s of
// -fsyntax-only). Grow this list one or two types at a time and re-measure;
// types that are not worth a full matrix row belong in the typed slice of
// SuccessFailure.cxx11.tests.cpp.
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
} // namespace

TEST (SuccessFailure, Matrix_EveryPair_ThenEveryContractHolds)
{
  // 1. WHAT: every success type of the matrix with every error type of it.
  // 2. WHY: the conversion contract is per type pair; partial coverage hides
  // bugs.
  // 3. VERIFIES: the four contracts of check_pair for all 625 matrix pairs.
  // 4. WHY VERIFY: a typed suite cannot take that many type parameters (the
  // gtest machinery exceeds the template instantiation depth), so the full
  // matrix runs as one test with a per-pair label.
  // 5. METHOD: pack-expand the type product and run check_pair for each pair.
  // 6. IMPACT: a pair used by a single caller would stay untested.
  run_matrix (typename Product<MatrixTypes, MatrixTypes>::type ());
}
