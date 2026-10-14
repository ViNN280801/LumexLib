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
// C++20 (libstdc++ 8); the test is then skipped. Also the one difference
// between the engines and the wrapper of the standard library's type: the
// comparison of expired weak pointers made from aliasing shared pointers.

#include <memory>

#include <gtest/gtest.h>

#include "lumex/core/atomic/LumexAtomic"
#include "lumex/tests/core/atomic/LumexAtomicTestSupport.hpp"

#if LUMEX_ATOMIC_SMART_PTR_HAS_LUMEX_FAMILY
#include "lumex/core/smart_ptr/LumexSmartPtr"
#endif

using namespace lumex_atomic_test;

namespace smart_ptr = lumex::core::atomic::smart_ptr;

// Std-only: make_shared for arrays (C++20) is not provided by the library
// pointer family
#if !defined(LUMEX_ATOMIC_TEST_POINTER_FAMILY_LUMEX)
TEST (LumexAtomicSharedPtrKindsTest,
      GivenMakeSharedForAnArray_WhenStored_ThenElementsAreReachable)
{
#if defined(__cpp_lib_shared_ptr_arrays)                                      \
    && __cpp_lib_shared_ptr_arrays >= 201707L
  atomic_shared_ptr<double[]> atom (pf::make_shared<double[]> (4, 0.25));
  EXPECT_EQ (atom.load ()[3], 0.25);
  pf::shared_ptr<double[]> const previous
      = atom.exchange (pf::make_shared<double[]> (2));
  EXPECT_EQ (previous[0], 0.25);
  EXPECT_EQ (atom.load ()[1], 0.0);
#else
  GTEST_SKIP ()
      << "make_shared for arrays needs __cpp_lib_shared_ptr_arrays >= 201707";
#endif
}
#endif

namespace
{
struct Pair
{
  int a;
  int b;
};

/// Two expired weak pointers with one owner and different stored pointers.
template <typename Atomic>
bool
exchanges_expired_aliases ()
{
  pf::weak_ptr<int> wa;
  pf::weak_ptr<int> wb;
  {
    pf::shared_ptr<Pair> const owner = pf::make_shared<Pair> (Pair{ 10, 20 });
    wa = pf::shared_ptr<int> (owner, &owner->a);
    wb = pf::shared_ptr<int> (owner, &owner->b);
  }
  Atomic atom (wa);
  pf::weak_ptr<int> expected = wb;
  return atom.compare_exchange_strong (expected, pf::weak_ptr<int> ());
}

#if LUMEX_ATOMIC_SMART_PTR_HAS_LUMEX_FAMILY
/// The same, over the library's own pointers (whatever the pf layer names).
template <typename Atomic>
bool
family_exchanges_expired_aliases ()
{
  namespace family = lumex::core::smart_ptr;
  family::weak_ptr<int> wa;
  family::weak_ptr<int> wb;
  {
    family::shared_ptr<Pair> const owner
        = family::make_shared<Pair> (Pair{ 10, 20 });
    wa = family::shared_ptr<int> (owner, &owner->a);
    wb = family::shared_ptr<int> (owner, &owner->b);
  }
  Atomic atom (wa);
  family::weak_ptr<int> expected = wb;
  return atom.compare_exchange_strong (expected, family::weak_ptr<int> ());
}
#endif
} // namespace

// Std-only: the test names the engines over std pointers
// (atomic_weak_ptr_lock_based, _lock_free, _std_backed), not the variant
// engine
#if !defined(LUMEX_ATOMIC_TEST_POINTER_FAMILY_LUMEX)
TEST (LumexAtomicWeakPtrKindsTest,
      GivenExpiredAliasesWithDifferentPointers_WhenCompare_ThenPerEngine)
{
  // std::weak_ptr does not expose the stored pointer once the object is
  // gone: the library's engines compare ownership only, the wrapper of the
  // standard library's type still sees the hidden stored pointers.
  EXPECT_TRUE (exchanges_expired_aliases<smart_ptr::atomic_weak_ptr<int>> ());
  EXPECT_TRUE (exchanges_expired_aliases<
               smart_ptr::atomic_weak_ptr_lock_based<int>> ());
#if LUMEX_ATOMIC_SMART_PTR_HAS_LOCK_FREE
  EXPECT_TRUE (
      exchanges_expired_aliases<smart_ptr::atomic_weak_ptr_lock_free<int>> ());
#endif
#if LUMEX_ATOMIC_SMART_PTR_HAS_STD_BACKED
  EXPECT_FALSE (exchanges_expired_aliases<
                smart_ptr::atomic_weak_ptr_std_backed<int>> ());
#endif
}
#endif

#if LUMEX_ATOMIC_SMART_PTR_HAS_LUMEX_FAMILY
TEST (
    LumexAtomicWeakPtrKindsTest,
    GivenExpiredAliasesWithDifferentPointers_WhenCompareOnFamilyEngines_ThenNoExchange)
{
  // The engines over the library's own weak pointer keep the stored pointer
  // after the object is gone and compare it exactly: two aliases with
  // different stored pointers are different values.
  EXPECT_FALSE (family_exchanges_expired_aliases<
                smart_ptr::atomic_weak_ptr_lock_based_lumex<int>> ());
#if LUMEX_ATOMIC_SMART_PTR_HAS_SPLIT_COUNT
  EXPECT_FALSE (family_exchanges_expired_aliases<
                smart_ptr::atomic_weak_ptr_lock_free_split_count<int>> ());
#endif
}
#endif
