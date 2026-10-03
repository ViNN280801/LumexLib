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
 * @file LumexAtomicSmartPtrConfig.hpp
 * @brief Chooses between the standard `std::atomic<std::shared_ptr<T>>` and
 * the lock-based implementation, and defines the memory order checks of the
 * atomic smart pointers.
 * @details `atomic_shared_ptr<T>` and `atomic_weak_ptr<T>` wrap
 * `std::atomic<std::shared_ptr<T>>` and `std::atomic<std::weak_ptr<T>>` when
 * the standard library provides them (`__cpp_lib_atomic_shared_ptr`, for
 * example libstdc++ 12 and later or the MSVC STL, at C++20). Otherwise they
 * use a
 * lock-based implementation over an ordinary `std::shared_ptr` or
 * `std::weak_ptr`, which works from C++11 on with any standard library.
 *
 * Origin: the lock-based implementation is a port of the own implementation
 * by Vladislav Semykin, the author of this library, of
 * `std::atomic<std::shared_ptr<T>>` and
 * `std::atomic<std::weak_ptr<T>>` for LLVM libc++ (P0718R2, llvm-project pull
 * request 194215). That implementation has two methods, a lock-based one and
 * a lock-free one (a double-width compare-and-swap over the pointer and a
 * control block word that also carries a split reference count); it is not
 * derived from libstdc++, the MSVC STL or Folly. Only the lock-based method is
 * ported. Both methods of the libc++ code keep their state in libc++'s own
 * `std::shared_ptr` (the lock-free one packs the split count into the control
 * block word, the lock-based one takes the two low bits of the control block
 * pointer), which a library cannot do to the `std::shared_ptr` of another
 * standard library without undefined behaviour. The port keeps the lock and
 * sleeper bits in a separate word instead.
 *
 * `LUMEX_ATOMIC_SMART_PTR_USES_STD` reports the choice: 1 for the standard
 * types, 0 for the lock-based implementation. Defining
 * `LUMEX_ATOMIC_SMART_PTR_FORCE_LOCK_BASED` before the first include selects
 * the lock-based implementation even where the standard types exist. It is
 * meant for tests and comparisons, as is `LUMEX_ATOMIC_WAIT_FORCE_TABLE` (see
 * `lumex/core/atomic/sync/LumexAtomicWait.hpp`), which selects the way
 * `wait ()` sleeps. Every translation unit of a program that shares an atomic
 * smart pointer object must make the same choices: each combination of
 * implementation and way of sleeping lives in its own inline namespace, so a
 * mismatch in a function signature fails to link, but a mismatch inside a
 * user type that holds the object is not detected.
 */
#ifndef LUMEX_CORE_ATOMIC_SMART_PTR_ATOMIC_SMART_PTR_CONFIG_HPP
#define LUMEX_CORE_ATOMIC_SMART_PTR_ATOMIC_SMART_PTR_CONFIG_HPP

#include <atomic>
#include <memory>

#include "lumex/core/atomic/sync/LumexAtomicWait.hpp"
#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"

/**
 * @brief 1 when `atomic_shared_ptr` and `atomic_weak_ptr` wrap the standard
 * atomic smart pointers, 0 when they use the lock-based implementation.
 * @details The standard types are used when the library has them
 * (`LUMEX_HAS_STD_ATOMIC_SHARED_PTR`), unless
 * `LUMEX_ATOMIC_SMART_PTR_FORCE_LOCK_BASED` is defined. Their `wait ()` is
 * not used (see `LumexAtomicSmartPtrCell.hpp`), so `std::atomic::wait` is not
 * a precondition.
 */
#if defined(LUMEX_ATOMIC_SMART_PTR_FORCE_LOCK_BASED)
#define LUMEX_ATOMIC_SMART_PTR_USES_STD 0
#elif LUMEX_HAS_STD_ATOMIC_SHARED_PTR
#define LUMEX_ATOMIC_SMART_PTR_USES_STD 1
#else
#define LUMEX_ATOMIC_SMART_PTR_USES_STD 0
#endif

/**
 * @brief Name of the inline namespace that holds the selected
 * implementation.
 * @details One name per implementation and way of sleeping: the ways of
 * sleeping are not interchangeable (see `LUMEX_ATOMIC_WAIT_ABI_NAMESPACE`),
 * and both implementations use them for `wait ()`.
 */
#if LUMEX_ATOMIC_SMART_PTR_USES_STD && LUMEX_ATOMIC_WAIT_USES_STD
#define LUMEX_ATOMIC_SMART_PTR_ABI_NAMESPACE std_backed_std_wait
#elif LUMEX_ATOMIC_SMART_PTR_USES_STD
#define LUMEX_ATOMIC_SMART_PTR_ABI_NAMESPACE std_backed_table_wait
#elif LUMEX_ATOMIC_WAIT_USES_STD
#define LUMEX_ATOMIC_SMART_PTR_ABI_NAMESPACE lock_based_std_wait
#else
#define LUMEX_ATOMIC_SMART_PTR_ABI_NAMESPACE lock_based_table_wait
#endif

/**
 * @brief Compile-time checks of constant memory order arguments.
 * @details The standard makes some orders a precondition violation: `store`
 * with `consume`, `acquire` or `acq_rel`; `load` and `wait` with `release`
 * or `acq_rel`; the failure order of `compare_exchange_*` with `release` or
 * `acq_rel`. Where the compiler supports the `diagnose_if` attribute (Clang),
 * a constant argument of that kind is reported as a warning, as libc++ does
 * for `std::atomic`. Other compilers accept the call; the lock-based
 * implementation then behaves as with `seq_cst`, and the standard types do
 * whatever their library does.
 */
#if defined(__has_attribute)
#if __has_attribute(diagnose_if)
#define LUMEX_ATOMIC_SMART_PTR_HAS_DIAGNOSE_IF 1
#endif
#endif

#if defined(LUMEX_ATOMIC_SMART_PTR_HAS_DIAGNOSE_IF)
#define LUMEX_ATOMIC_SMART_PTR_CHECK_LOAD_ORDER(order)                        \
  __attribute__ ((diagnose_if ((order) == std::memory_order_release           \
                                   || (order) == std::memory_order_acq_rel,   \
                               "memory order argument to atomic operation "   \
                               "is invalid",                                  \
                               "warning")))
#define LUMEX_ATOMIC_SMART_PTR_CHECK_STORE_ORDER(order)                       \
  __attribute__ ((diagnose_if ((order) == std::memory_order_consume           \
                                   || (order) == std::memory_order_acquire    \
                                   || (order) == std::memory_order_acq_rel,   \
                               "memory order argument to atomic operation "   \
                               "is invalid",                                  \
                               "warning")))
#define LUMEX_ATOMIC_SMART_PTR_CHECK_FAILURE_ORDER(order)                     \
  LUMEX_ATOMIC_SMART_PTR_CHECK_LOAD_ORDER (order)
#else
#define LUMEX_ATOMIC_SMART_PTR_CHECK_LOAD_ORDER(order)
#define LUMEX_ATOMIC_SMART_PTR_CHECK_STORE_ORDER(order)
#define LUMEX_ATOMIC_SMART_PTR_CHECK_FAILURE_ORDER(order)
#endif

#endif // !LUMEX_CORE_ATOMIC_SMART_PTR_ATOMIC_SMART_PTR_CONFIG_HPP
