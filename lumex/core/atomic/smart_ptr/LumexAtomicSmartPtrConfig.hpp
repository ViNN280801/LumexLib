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
 * @brief The one place that decides which engines exist and which one the
 * common names `atomic_shared_ptr` and `atomic_weak_ptr` stand for, and the
 * memory order checks of the atomic smart pointers.
 * @details Three engines implement the interface of the C++20
 * `std::atomic<std::shared_ptr<T>>` and `std::atomic<std::weak_ptr<T>>`, each
 * under an explicit class template name in
 * `lumex::core::atomic::smart_ptr`:
 *
 * | Class templates | Engine | Exists |
 * | --- | --- | --- |
 * | `atomic_shared_ptr_lock_free`, `atomic_weak_ptr_lock_free` | an immutable
 * heap box behind one `std::atomic` pointer, protected by
 * `core/hazard_pointer` | `LUMEX_ATOMIC_SMART_PTR_HAS_LOCK_FREE` is 1 | |
 * `atomic_shared_ptr_lock_based`, `atomic_weak_ptr_lock_based` | an ordinary
 * smart pointer behind a two-bit lock | always | |
 * `atomic_shared_ptr_std_backed`, `atomic_weak_ptr_std_backed` | the standard
 * library's `std::atomic<std::shared_ptr<T>>` and
 * `std::atomic<std::weak_ptr<T>>` | `LUMEX_ATOMIC_SMART_PTR_HAS_STD_BACKED` is
 * 1 (C++20, libstdc++ 12 and later or the MSVC STL) |
 * `atomic_shared_ptr_lock_free_split_count`,
 * `atomic_weak_ptr_lock_free_split_count` | a 16-byte word per slot (a control
 * block address, the loads in flight and an offset) updated by a 128-bit
 * compare-and-swap, over `lumex::core::smart_ptr` |
 * `LUMEX_ATOMIC_SMART_PTR_HAS_SPLIT_COUNT` is 1 |
 *
 * The common names are alias templates: the lock-free engine where it
 * exists, the lock-based one otherwise. The standard-backed wrapper is an
 * explicit opt-in name and is never what a common name resolves to. A name
 * of an engine that does not exist is not declared, so naming it is a
 * compile error; the macros above let code branch instead.
 *
 * Origin: the lock-based engine is a port of the own implementation by
 * Vladislav Semykin, the author of this library, of
 * `std::atomic<std::shared_ptr<T>>` and `std::atomic<std::weak_ptr<T>>` for
 * LLVM libc++ (P0718R2, llvm-project pull request 194215). That implementation
 * has two methods, a lock-based one and a lock-free one (a double-width
 * compare-and-swap over the pointer and a control block word that also
 * carries a split reference count); it is not derived from libstdc++, the MSVC
 * STL or Folly. Only the lock-based method is ported, with the lock and
 * sleeper bits in a separate word, because a library cannot take two bits of
 * the control block pointer of another standard library's `std::shared_ptr`.
 * The lock-free engine is a different algorithm (see
 * `LumexAtomicSmartPtrLockFreeCell.hpp`).
 *
 * Switches, for tests and comparisons, defined before the first include:
 * `LUMEX_ATOMIC_SMART_PTR_FORCE_LOCK_BASED` makes the common names
 * lock-based even where the lock-free engine exists;
 * `LUMEX_ATOMIC_SMART_PTR_DISABLE_LOCK_FREE` makes the lock-free engine not
 * exist (the build as it is without `core/hazard_pointer`);
 * `LUMEX_ATOMIC_SMART_PTR_DISABLE_SPLIT_COUNT` makes the split-count engine
 * not exist, and the family alias then resolves to the lock-based engine;
 * `LUMEX_ATOMIC_WAIT_FORCE_TABLE` (see
 * `lumex/core/atomic/sync/LumexAtomicWait.hpp`) selects the way `wait ()`
 * sleeps. Every translation unit of a program that shares an atomic smart
 * pointer object must make the same choices: the engine is part of the class
 * template name and the way of sleeping is an inline namespace, so a mismatch
 * in a function signature fails to link, but a mismatch inside a user type
 * that holds the object is not detected.
 */
#ifndef LUMEX_CORE_ATOMIC_SMART_PTR_ATOMIC_SMART_PTR_CONFIG_HPP
#define LUMEX_CORE_ATOMIC_SMART_PTR_ATOMIC_SMART_PTR_CONFIG_HPP

#include <atomic>
#include <memory>

#include "lumex/core/atomic/dwcas/LumexDwcasConfig.hpp"
#include "lumex/core/atomic/sync/LumexAtomicWait.hpp"
#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"

/**
 * @brief 1 when the `*_std_backed` class templates exist, 0 otherwise.
 * @details They wrap the standard library's atomic smart pointers, which
 * exist when the library has them (`LUMEX_HAS_STD_ATOMIC_SHARED_PTR`). Their
 * `wait ()` is not used (see `LumexAtomicSmartPtrCell.hpp`), so
 * `std::atomic::wait` is not a precondition. No common name resolves to them.
 */
#if LUMEX_HAS_STD_ATOMIC_SHARED_PTR
#define LUMEX_ATOMIC_SMART_PTR_HAS_STD_BACKED 1
#else
#define LUMEX_ATOMIC_SMART_PTR_HAS_STD_BACKED 0
#endif

/**
 * @brief 1 when the `*_lock_free` class templates exist, 0 otherwise.
 * @details The lock-free engine needs `core/hazard_pointer` (the compiled
 * library, linked by the CMake target `lumex::atomic` when it is built, which
 * also defines `LUMEX_ATOMIC_HAS_HAZARD_POINTER`; a build without CMake
 * defines the macro itself and links the library), pointer atomics that are
 * always lock-free (`ATOMIC_POINTER_LOCK_FREE == 2`), and no definition of
 * `LUMEX_ATOMIC_SMART_PTR_DISABLE_LOCK_FREE`. The macro is not derived from
 * `__has_include`: the header can be present while the library is not linked.
 */
#if defined(LUMEX_ATOMIC_HAS_HAZARD_POINTER)                                  \
    && !defined(LUMEX_ATOMIC_SMART_PTR_DISABLE_LOCK_FREE)                     \
    && defined(ATOMIC_POINTER_LOCK_FREE) && (ATOMIC_POINTER_LOCK_FREE == 2)
#define LUMEX_ATOMIC_SMART_PTR_HAS_LOCK_FREE 1
#else
#define LUMEX_ATOMIC_SMART_PTR_HAS_LOCK_FREE 0
#endif

/**
 * @brief 1 when `atomic_shared_ptr` and `atomic_weak_ptr` resolve to the
 * lock-free engine, 0 when they resolve to the lock-based one.
 * @details The lock-free engine when it exists, unless
 * `LUMEX_ATOMIC_SMART_PTR_FORCE_LOCK_BASED` is defined. The standard-backed
 * wrapper is never chosen.
 */
#if LUMEX_ATOMIC_SMART_PTR_HAS_LOCK_FREE                                      \
    && !defined(LUMEX_ATOMIC_SMART_PTR_FORCE_LOCK_BASED)
#define LUMEX_ATOMIC_SMART_PTR_COMMON_IS_LOCK_FREE 1
#else
#define LUMEX_ATOMIC_SMART_PTR_COMMON_IS_LOCK_FREE 0
#endif

/**
 * @brief 1 when the atomic smart pointers over the module's own pointer
 * family (`lumex::core::smart_ptr::shared_ptr` / `weak_ptr`) exist, 0
 * otherwise.
 * @details The family is the header-only module `core/smart_ptr`. The CMake
 * target `lumex::atomic` links `lumex::smart_ptr` when that target exists and
 * then defines `LUMEX_ATOMIC_HAS_SMART_PTR` (a soft edge, like the one to
 * `core/hazard_pointer`); a build without CMake defines the macro itself.
 * Where it is 1, `atomic_shared_ptr_lock_based_lumex`,
 * `atomic_weak_ptr_lock_based_lumex` and the alias pair
 * `lumex::core::smart_ptr::atomic_shared_ptr` / `atomic_weak_ptr` are
 * declared.
 */
#if defined(LUMEX_ATOMIC_HAS_SMART_PTR)
#define LUMEX_ATOMIC_SMART_PTR_HAS_LUMEX_FAMILY 1
#else
#define LUMEX_ATOMIC_SMART_PTR_HAS_LUMEX_FAMILY 0
#endif

/**
 * @brief 1 when `atomic_shared_ptr_lock_free_split_count` and
 * `atomic_weak_ptr_lock_free_split_count` exist, 0 otherwise.
 * @details The split-count engine needs the pointer family
 * (`LUMEX_ATOMIC_SMART_PTR_HAS_LUMEX_FAMILY`), the 128-bit compare-and-swap
 * layer (`LUMEX_ATOMIC_HAS_DWCAS`: x86-64 with GCC, Clang or MSVC) and no
 * definition of `LUMEX_ATOMIC_SMART_PTR_DISABLE_SPLIT_COUNT`. Elsewhere the
 * two class templates are not declared, so naming one is a compile error.
 */
#if LUMEX_ATOMIC_SMART_PTR_HAS_LUMEX_FAMILY && LUMEX_ATOMIC_HAS_DWCAS         \
    && !defined(LUMEX_ATOMIC_SMART_PTR_DISABLE_SPLIT_COUNT)
#define LUMEX_ATOMIC_SMART_PTR_HAS_SPLIT_COUNT 1
#else
#define LUMEX_ATOMIC_SMART_PTR_HAS_SPLIT_COUNT 0
#endif

/**
 * @brief 1 when `lumex::core::smart_ptr::atomic_shared_ptr` and
 * `atomic_weak_ptr` resolve to the split-count engine, 0 when they resolve to
 * the lock-based engine over the module's own pointers (or do not exist).
 * @details The split-count engine where it exists, unless
 * `LUMEX_ATOMIC_SMART_PTR_FORCE_LOCK_BASED` is defined (the switch forces both
 * alias pairs).
 */
#if LUMEX_ATOMIC_SMART_PTR_HAS_SPLIT_COUNT                                    \
    && !defined(LUMEX_ATOMIC_SMART_PTR_FORCE_LOCK_BASED)
#define LUMEX_ATOMIC_SMART_PTR_FAMILY_IS_SPLIT_COUNT 1
#else
#define LUMEX_ATOMIC_SMART_PTR_FAMILY_IS_SPLIT_COUNT 0
#endif

/**
 * @brief Name of the inline namespace that holds the smart pointers.
 * @details Only the way of sleeping selects it: the ways are not
 * interchangeable (see `LUMEX_ATOMIC_WAIT_ABI_NAMESPACE`), and every engine
 * uses one for `wait ()`. The engine is in the class template name, so the
 * engines of one translation unit coexist and a mismatch between translation
 * units still fails to link.
 */
#define LUMEX_ATOMIC_SMART_PTR_ABI_NAMESPACE LUMEX_ATOMIC_WAIT_ABI_NAMESPACE

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
