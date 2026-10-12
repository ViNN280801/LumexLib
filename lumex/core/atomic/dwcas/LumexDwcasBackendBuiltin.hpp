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
 * @file LumexDwcasBackendBuiltin.hpp
 * @brief The built-in backend: `__atomic_*` operations on a 128-bit integer.
 * @details This backend exists for ThreadSanitizer. The sanitizer instruments
 * `__atomic_*` calls and so sees the synchronization they create, but it does
 * not see inline assembly: with the assembly backend it would report races on
 * data that the word protects. GCC compiles every 16-byte `__atomic_*`
 * operation into a call of libatomic (link `-latomic`), with or without
 * `-mcx16`; Clang inlines them only with `-mcx16` and otherwise calls
 * libatomic too. libatomic picks `cmpxchg16b` or, on CPUs where it is known to
 * be atomic, an AVX load at run time (a 16-byte load through this backend may
 * therefore not write, unlike the other backends). The memory orders are
 * passed through, so a sanitizer run checks the orders the caller asked for.
 *
 * Use it by defining `LUMEX_DWCAS_BACKEND` as `LUMEX_DWCAS_BACKEND_BUILTIN`
 * (the value 3) before the first include. Production code uses the other
 * backends: they need no library.
 *
 * The 128-bit type is accessed through a `may_alias` typedef, so the pair of
 * 64-bit members of `dwcas_word` can be read as one integer without breaking
 * strict aliasing.
 *
 * This file is only meant to be included when `LUMEX_DWCAS_BACKEND` is
 * `LUMEX_DWCAS_BACKEND_BUILTIN`.
 */
#ifndef LUMEX_CORE_ATOMIC_DWCAS_DWCAS_BACKEND_BUILTIN_HPP
#define LUMEX_CORE_ATOMIC_DWCAS_DWCAS_BACKEND_BUILTIN_HPP

#include <atomic>
#include <cstdint>

#include "lumex/core/atomic/dwcas/LumexDwcasConfig.hpp"
#include "lumex/core/atomic/dwcas/LumexDwcasValue.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

#if LUMEX_ATOMIC_HAS_DWCAS                                                    \
    && LUMEX_DWCAS_BACKEND == LUMEX_DWCAS_BACKEND_BUILTIN

#if defined(__clang__) && defined(__has_warning)
#if __has_warning("-Watomic-alignment")
// Without -mcx16 Clang calls libatomic for 16 bytes and says so; that is
// what this backend is for.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Watomic-alignment"
#define LUMEX_DWCAS_BUILTIN_POP_DIAGNOSTICS 1
#endif
#endif

namespace lumex
{
namespace core
{
namespace atomic
{
namespace dwcas
{
namespace Detail
{
inline namespace backend_builtin
{
/// An unsigned 128-bit integer that may alias any object.
__extension__ typedef unsigned __int128 u128_alias_t
    __attribute__ ((may_alias));

/// An unsigned 128-bit integer.
__extension__ typedef unsigned __int128 u128_t;

/// The `__ATOMIC_*` constant of @p order, with `consume` as `acquire`.
inline int
builtin_order (std::memory_order order) LUMEX_NOEXCEPT
{
  switch (order)
    {
    case std::memory_order_relaxed:
      return __ATOMIC_RELAXED;
    case std::memory_order_consume:
    case std::memory_order_acquire:
      return __ATOMIC_ACQUIRE;
    case std::memory_order_release:
      return __ATOMIC_RELEASE;
    case std::memory_order_acq_rel:
      return __ATOMIC_ACQ_REL;
    case std::memory_order_seq_cst:
      return __ATOMIC_SEQ_CST;
    }
  return __ATOMIC_SEQ_CST;
}

/// The order of a load: `release` and `acq_rel` are not loads and fall back
/// to the strongest load their ordering implies.
inline int
builtin_load_order (std::memory_order order) LUMEX_NOEXCEPT
{
  if (order == std::memory_order_release)
    return __ATOMIC_RELAXED;
  if (order == std::memory_order_acq_rel)
    return __ATOMIC_ACQUIRE;
  return builtin_order (order);
}

/// The order of a store: `consume`, `acquire` and `acq_rel` are not stores
/// and fall back to `release`.
inline int
builtin_store_order (std::memory_order order) LUMEX_NOEXCEPT
{
  if (order == std::memory_order_consume || order == std::memory_order_acquire
      || order == std::memory_order_acq_rel)
    return __ATOMIC_RELEASE;
  return builtin_order (order);
}

/// The failure order of a compare-and-swap: a load order that is not
/// stronger than the success order (the `__ATOMIC_*` constants of the load
/// orders grow with their strength).
inline int
builtin_failure_order (std::memory_order success,
                       std::memory_order failure) LUMEX_NOEXCEPT
{
  int const wanted = builtin_load_order (failure);
  int const allowed = builtin_load_order (success);
  return wanted > allowed ? allowed : wanted;
}

/// The 128-bit integer of @p value.
inline u128_t
pack (dwcas_value_t value) LUMEX_NOEXCEPT
{
  return (static_cast<u128_t> (value.hi) << 64) | value.lo;
}

/// The halves of @p value.
inline dwcas_value_t
unpack (u128_t value) LUMEX_NOEXCEPT
{
  dwcas_value_t halves = { static_cast<std::uint64_t> (value),
                           static_cast<std::uint64_t> (value >> 64) };
  return halves;
}

/// The operations of the built-in backend.
struct builtin_operations_t
{
  /// Reads the 16 bytes atomically.
  static dwcas_value_t
  load (std::uint64_t *words, std::memory_order order) LUMEX_NOEXCEPT
  {
    return unpack (__atomic_load_n (reinterpret_cast<u128_alias_t *> (words),
                                    builtin_load_order (order)));
  }

  /// Replaces the 16 bytes atomically and returns the previous value.
  static dwcas_value_t
  exchange (std::uint64_t *words, std::uint64_t const *, dwcas_value_t desired,
            std::memory_order order) LUMEX_NOEXCEPT
  {
    return unpack (
        __atomic_exchange_n (reinterpret_cast<u128_alias_t *> (words),
                             pack (desired), builtin_order (order)));
  }

  /// Replaces the 16 bytes atomically.
  static void
  store (std::uint64_t *words, std::uint64_t const *, dwcas_value_t desired,
         std::memory_order order) LUMEX_NOEXCEPT
  {
    __atomic_store_n (reinterpret_cast<u128_alias_t *> (words), pack (desired),
                      builtin_store_order (order));
  }

  /// Compare-and-swap; returns the value found, equal to @p expected on
  /// success.
  static dwcas_value_t
  compare_exchange (std::uint64_t *words, dwcas_value_t expected,
                    dwcas_value_t desired, std::memory_order success,
                    std::memory_order failure) LUMEX_NOEXCEPT
  {
    u128_alias_t seen = pack (expected);
    __atomic_compare_exchange_n (
        reinterpret_cast<u128_alias_t *> (words), &seen, pack (desired), false,
        builtin_order (success), builtin_failure_order (success, failure));
    return unpack (seen);
  }
};

/// The operations the word calls.
typedef builtin_operations_t backend_operations_t;
} // namespace backend_builtin
} // namespace Detail
} // namespace dwcas
} // namespace atomic
} // namespace core
} // namespace lumex

#if defined(LUMEX_DWCAS_BUILTIN_POP_DIAGNOSTICS)
#pragma clang diagnostic pop
#undef LUMEX_DWCAS_BUILTIN_POP_DIAGNOSTICS
#endif

#endif // LUMEX_ATOMIC_HAS_DWCAS && LUMEX_DWCAS_BACKEND == ...

#endif // !LUMEX_CORE_ATOMIC_DWCAS_DWCAS_BACKEND_BUILTIN_HPP
