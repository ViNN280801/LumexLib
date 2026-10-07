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

// Kinds of smart pointers held by atomic_shared_ptr and atomic_weak_ptr that
// need C++20: std::make_shared for an array type (__cpp_lib_shared_ptr_arrays
// 201707). The feature check stays because a standard library may lack it at
// C++20 (libstdc++ 8); the test is then skipped.

#include <memory>

#include <gtest/gtest.h>

#include "lumex/core/atomic/LumexAtomic"
#include "lumex/tests/core/atomic/LumexAtomicTestSupport.hpp"

using namespace lumex_atomic_test;

TEST (LumexAtomicSharedPtrKindsTest,
      GivenMakeSharedForAnArray_WhenStored_ThenElementsAreReachable)
{
#if defined(__cpp_lib_shared_ptr_arrays)                                      \
    && __cpp_lib_shared_ptr_arrays >= 201707L
  atomic_shared_ptr<double[]> atom (std::make_shared<double[]> (4, 0.25));
  EXPECT_EQ (atom.load ()[3], 0.25);
  std::shared_ptr<double[]> const previous
      = atom.exchange (std::make_shared<double[]> (2));
  EXPECT_EQ (previous[0], 0.25);
  EXPECT_EQ (atom.load ()[1], 0.0);
#else
  GTEST_SKIP ()
      << "make_shared for arrays needs __cpp_lib_shared_ptr_arrays >= 201707";
#endif
}
