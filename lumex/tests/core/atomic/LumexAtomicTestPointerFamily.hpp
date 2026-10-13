/**
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
 * to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the
 * Software, and to permit persons to whom the Software is
 * furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice
 * and this permission notice shall be included in
 * all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT
 * WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO
 * THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH
 * THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/**
 * @file LumexAtomicTestPointerFamily.hpp
 * @brief The pointer-family layer of the atomic smart pointer tests: which
 * shared and weak pointers the suites hold their values in.
 * @details Included by every shared header of the suites. A variant of the
 * library's own family defines `LUMEX_ATOMIC_TEST_POINTER_FAMILY_LUMEX`; where
 * the engine of that variant does not exist the macro is dropped here, so the
 * suite then runs on the standard pointers (the engine macros fall back in
 * `LumexAtomicTestSupport.hpp` under the same conditions).
 */
#ifndef LUMEX_TESTS_CORE_ATOMIC_ATOMIC_TEST_POINTER_FAMILY_HPP
#define LUMEX_TESTS_CORE_ATOMIC_ATOMIC_TEST_POINTER_FAMILY_HPP

#include <memory>
#include <utility>

#include "lumex/core/atomic/LumexAtomic"
#include "lumex/core/utility/macros/LumexKeywords.hpp"
#include "lumex/tests/support/LumexTestReusePool.hpp"

#if defined(LUMEX_ATOMIC_TEST_POINTER_FAMILY_LUMEX)                           \
    && ((defined(LUMEX_ATOMIC_TEST_ENGINE_REQUIRES_SPLIT_COUNT)               \
         && !LUMEX_ATOMIC_SMART_PTR_HAS_SPLIT_COUNT)                          \
        || (defined(LUMEX_ATOMIC_TEST_ENGINE_REQUIRES_LUMEX_FAMILY)           \
            && !LUMEX_ATOMIC_SMART_PTR_HAS_LUMEX_FAMILY))
#undef LUMEX_ATOMIC_TEST_POINTER_FAMILY_LUMEX
#endif

#if defined(LUMEX_ATOMIC_TEST_POINTER_FAMILY_LUMEX)
#include "lumex/core/smart_ptr/LumexSmartPtr"
#endif

namespace lumex_atomic_test
{
/**
 * @namespace lumex_atomic_test::pf
 * @brief The pointer-family layer: the shared and weak pointers the suites
 * hold their values in.
 * @details The engines over `std::shared_ptr` and the engines over the
 * library's own `lumex::core::smart_ptr::shared_ptr` run the same suites, so
 * the suites never spell `std::shared_ptr`: they spell `pf::shared_ptr`.
 * `pf` names the standard family by default and the library's own family when
 * `LUMEX_ATOMIC_TEST_POINTER_FAMILY_LUMEX` is defined (the split-count and
 * lock_based_lumex variants). Using-declarations, not aliases, so a name
 * behaves exactly as the entity it stands for (deduction, overloads, ADL).
 *
 * The fixed replacement table (old spelling to new spelling), applied by
 * hand or mechanically to every file of the suites:
 *
 * | Old | New |
 * | --- | --- |
 * | `std::shared_ptr` | `pf::shared_ptr` |
 * | `std::weak_ptr` | `pf::weak_ptr` |
 * | `std::make_shared` | `pf::make_shared` |
 * | `std::allocate_shared` | `pf::allocate_shared` |
 * | `std::static_pointer_cast` | `pf::static_pointer_cast` |
 * | `std::dynamic_pointer_cast` | `pf::dynamic_pointer_cast` |
 * | `std::const_pointer_cast` | `pf::const_pointer_cast` |
 * | `std::enable_shared_from_this` | `pf::enable_shared_from_this` |
 * | `std::bad_weak_ptr` | `pf::bad_weak_ptr` |
 * | `std::owner_less` | `pf::owner_less` |
 * | `lumex_test::make_pooled` | `pf::make_pooled` |
 *
 * Left as they are: `std::unique_ptr`, `std::hash` (the library specializes
 * it for its own pointers), `std::atomic`. Not replaced on purpose: the
 * differential checks against `std::atomic<std::shared_ptr<T>>` and the
 * array forms of `make_shared` / `allocate_shared` (C++20), which the
 * library's family does not provide; those files stay on the standard
 * pointers and are not part of a variant of the library's family.
 * Inside the suites, `use_count ()` of the library's family is
 * `count + ext` (never below the number of owners, exact only when nothing
 * is in flight on that atomic); where a test needs an exact count while
 * other threads operate on the same atomic it uses `pf::use_count_settled`.
 */
namespace pf
{
#if defined(LUMEX_ATOMIC_TEST_POINTER_FAMILY_LUMEX)
using ::lumex::core::smart_ptr::allocate_shared;
using ::lumex::core::smart_ptr::bad_weak_ptr;
using ::lumex::core::smart_ptr::const_pointer_cast;
using ::lumex::core::smart_ptr::dynamic_pointer_cast;
using ::lumex::core::smart_ptr::enable_shared_from_this;
using ::lumex::core::smart_ptr::make_shared;
using ::lumex::core::smart_ptr::owner_less;
using ::lumex::core::smart_ptr::shared_ptr;
using ::lumex::core::smart_ptr::static_pointer_cast;
using ::lumex::core::smart_ptr::weak_ptr;
LUMEX_CONSTEXPR bool const is_lumex_family = true;
#else
using std::allocate_shared;
using std::bad_weak_ptr;
using std::const_pointer_cast;
using std::dynamic_pointer_cast;
using std::enable_shared_from_this;
using std::make_shared;
using std::owner_less;
using std::shared_ptr;
using std::static_pointer_cast;
using std::weak_ptr;
LUMEX_CONSTEXPR bool const is_lumex_family = false;
#endif

/// A shared object whose object and control block come from @p pool (the
/// `lumex_test::make_pooled` of the pointer family in use).
template <typename T, typename... Args>
pf::shared_ptr<T>
make_pooled (lumex_test::ReusePool &pool, Args &&...args)
{
  return pf::allocate_shared<T> (lumex_test::ReuseAllocator<T> (pool),
                                 std::forward<Args> (args)...);
}

/// The number of owners once nothing is in flight: `use_count_settled ()` of
/// the library's family (it waits for owners in transit), `use_count ()` of
/// the standard family (exact there).
template <typename Pointer>
long
use_count_settled (Pointer const &pointer)
{
#if defined(LUMEX_ATOMIC_TEST_POINTER_FAMILY_LUMEX)
  return static_cast<long> (pointer.use_count_settled ());
#else
  return static_cast<long> (pointer.use_count ());
#endif
}
} // namespace pf
} // namespace lumex_atomic_test

#endif // !LUMEX_TESTS_CORE_ATOMIC_ATOMIC_TEST_POINTER_FAMILY_HPP
