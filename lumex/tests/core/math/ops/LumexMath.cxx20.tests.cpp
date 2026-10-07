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

// LumexMath.cxx20.tests.cpp
//
// LumexMath over C++20 views. The C++20 suite (LumexMathOpsCxx20Tests)
// compiles this file together with LumexMath.cxx11.tests.cpp. The views need
// std::ranges (LUMEX_HAS_STD_RANGES): libstdc++ 8 has no <ranges> even with
// -std=c++2a, so there the tests skip.

#include <cmath>
#include <stdexcept>
#include <vector>
#if defined(__has_include)
#if __has_include(<ranges>)
#include <ranges>
#endif
#endif

#include <gtest/gtest.h>

#include "lumex/core/math/LumexMath"
#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"

using lumex::core::math::ops::avg;
using lumex::core::math::ops::rms;
using lumex::core::math::ops::rmse;

// --- C++20 views ------------------------------------------------------------

TEST (LumexMathTest,
      GivenFilterView_WhenAverage_ThenNonConstOverloadIteratesTheView)
{
#if LUMEX_HAS_STD_RANGES
  // std::views::filter caches its begin () and cannot be iterated through
  // a const reference. avg(Range&) used to hand the view to a const-ref
  // helper, which did not compile for such views (PeakExpertWeb
  // SpectrumArray: filter | transform over data points).
  std::vector<double> const points{ 1.0, 2.0, 3.0, 10.0, 20.0 };
  auto view = points | std::views::filter ([] (double v) { return v < 5.0; })
              | std::views::transform ([] (double v) { return v * 2.0; });
  EXPECT_DOUBLE_EQ (avg (view), 4.0); // (2 + 4 + 6) / 3
  EXPECT_DOUBLE_EQ (rms (view), std::sqrt (56.0 / 3.0));
  EXPECT_DOUBLE_EQ (rmse (view, 4.0), std::sqrt (8.0 / 3.0));
#else
  GTEST_SKIP () << "the standard library has no std::ranges";
#endif
}

TEST (LumexMathTest, GivenViewWithSentinelEnd_WhenAverage_ThenStopsAtSentinel)
{
#if LUMEX_HAS_STD_RANGES
  // take_while's end () is a sentinel of a different type than begin ().
  auto view = std::views::iota (1)
              | std::views::take_while ([] (int v) { return v < 4; });
  EXPECT_EQ (avg (view), 2);
  EXPECT_EQ (avg (std::views::iota (1, 4)), 2); // temporary view
#else
  GTEST_SKIP () << "the standard library has no std::ranges";
#endif
}

TEST (LumexMathTest,
      GivenFilterViewsOfDifferentLengths_WhenRMSE_ThenThrowsInvalidArgument)
{
#if LUMEX_HAS_STD_RANGES
  std::vector<int> const values{ 1, 2, 3, 4 };
  auto even = values | std::views::filter ([] (int v) { return v % 2 == 0; });
  auto all = values | std::views::filter ([] (int) { return true; });
  EXPECT_THROW (rmse (even, all), std::invalid_argument);
#else
  GTEST_SKIP () << "the standard library has no std::ranges";
#endif
}
