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
 * @file LumexDwcasFromCas.hpp
 * @brief `cas_operations_t`: load, store and exchange of a 128-bit word
 * built on a backend that only has a compare-and-swap.
 * @details Both hardware backends (the assembly one and the MSVC intrinsic)
 * provide the single primitive `cas (words, expected, desired)` that returns
 * the value it observed; this template derives the other operations from it:
 *
 * - `load` is a compare-and-swap of zero for zero. Whatever the word holds,
 *   the result is what it held, and the word is left unchanged, but the
 *   operation is a locked read-modify-write: it needs the cache line in
 *   exclusive state and writable memory (see `dwcas_word`).
 * - `exchange` starts from a guess made of two relaxed 64-bit reads and
 *   retries with the value each failed attempt returned, so an uncontended
 *   exchange costs one locked instruction, not two. `store` is an exchange
 *   that drops the old value.
 *
 * The memory-order arguments are accepted and ignored: on x86-64 a locked
 * instruction is a full barrier, which is at least as strong as every order
 * the standard knows (see `LumexDwcasWord.hpp`).
 */
#ifndef LUMEX_CORE_ATOMIC_DWCAS_DWCAS_FROM_CAS_HPP
#define LUMEX_CORE_ATOMIC_DWCAS_DWCAS_FROM_CAS_HPP

#include <atomic>
#include <cstdint>

#include "lumex/core/atomic/dwcas/LumexDwcasConfig.hpp"
#include "lumex/core/atomic/dwcas/LumexDwcasValue.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

#if LUMEX_ATOMIC_HAS_DWCAS

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
/**
 * @brief The operations of a backend, derived from its `cas`.
 * @tparam Cas A type with `static dwcas_value_t cas (std::uint64_t *words,
 * dwcas_value_t expected, dwcas_value_t desired) noexcept` that performs a
 * 128-bit compare-and-swap on the 16 bytes at @c words (16-byte aligned) and
 * returns the value it found there; the swap happened exactly when the
 * result equals @c expected.
 */
template <typename Cas> struct cas_operations_t
{
  /// Reads the 16 bytes atomically.
  static dwcas_value_t
  load (std::uint64_t *words, std::memory_order) LUMEX_NOEXCEPT
  {
    dwcas_value_t const zero = { 0u, 0u };
    return Cas::cas (words, zero, zero);
  }

  /// Replaces the 16 bytes atomically and returns the previous value.
  static dwcas_value_t
  exchange (std::uint64_t *words, std::uint64_t const *high,
            dwcas_value_t desired, std::memory_order) LUMEX_NOEXCEPT
  {
    dwcas_value_t seen = { relaxed_load64 (words), relaxed_load64 (high) };
    for (;;)
      {
        dwcas_value_t const found = Cas::cas (words, seen, desired);
        if (found == seen)
          return found;
        seen = found;
      }
  }

  /// Replaces the 16 bytes atomically.
  static void
  store (std::uint64_t *words, std::uint64_t const *high,
         dwcas_value_t desired, std::memory_order order) LUMEX_NOEXCEPT
  {
    exchange (words, high, desired, order);
  }

  /// Compare-and-swap; returns the value found, equal to @p expected on
  /// success.
  static dwcas_value_t
  compare_exchange (std::uint64_t *words, dwcas_value_t expected,
                    dwcas_value_t desired, std::memory_order,
                    std::memory_order) LUMEX_NOEXCEPT
  {
    return Cas::cas (words, expected, desired);
  }
};
} // namespace Detail
} // namespace dwcas
} // namespace atomic
} // namespace core
} // namespace lumex

#endif // LUMEX_ATOMIC_HAS_DWCAS

#endif // !LUMEX_CORE_ATOMIC_DWCAS_DWCAS_FROM_CAS_HPP
