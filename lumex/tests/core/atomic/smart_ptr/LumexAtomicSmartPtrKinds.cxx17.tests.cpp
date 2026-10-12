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
// need C++17: weak_from_this (__cpp_lib_enable_shared_from_this). The
// feature check stays because a standard library may lack it at C++17; the
// test is then skipped.

#include <memory>

#include <gtest/gtest.h>

#include "lumex/core/atomic/LumexAtomic"
#include "lumex/tests/core/atomic/LumexAtomicTestSupport.hpp"

using namespace lumex_atomic_test;

namespace
{
struct SelfAware : std::enable_shared_from_this<SelfAware>
{
  int value = 9;
};
} // namespace

TEST (LumexAtomicSharedPtrKindsTest,
      GivenWeakFromThis_WhenStoredInAnAtomicWeakPtr_ThenRefersToTheObject)
{
#if defined(__cpp_lib_enable_shared_from_this)
  std::shared_ptr<SelfAware> const object = std::make_shared<SelfAware> ();
  atomic_weak_ptr<SelfAware> atom (object->weak_from_this ());
  EXPECT_TRUE (refers_to (atom.load (), object));
  std::weak_ptr<SelfAware> expected = object->weak_from_this ();
  EXPECT_TRUE (
      atom.compare_exchange_strong (expected, std::weak_ptr<SelfAware> ()));
#else
  GTEST_SKIP () << "weak_from_this needs __cpp_lib_enable_shared_from_this";
#endif
}
