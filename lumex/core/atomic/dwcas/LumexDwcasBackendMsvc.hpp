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
 * @file LumexDwcasBackendMsvc.hpp
 * @brief The MSVC backend: the `_InterlockedCompareExchange128` intrinsic.
 * @details `_InterlockedCompareExchange128 (destination, exchange_high,
 * exchange_low, comparand_result)` compares the 16 bytes at @c destination
 * (16-byte aligned) with the pair at @c comparand_result (low half first),
 * stores the exchange pair on equality and returns nonzero, and otherwise
 * overwrites @c comparand_result with the value it found and returns zero.
 * The pair at @c comparand_result is overwritten on success as well (with the
 * value that was compared, which is the expected one), so this wrapper copies
 * the expected value into a local array and returns that array as the
 * observed value. The intrinsic is a full barrier on x64.
 *
 * Status: this wrapper is compile-checked and run against a stand-in of the
 * intrinsic on Linux (the tests substitute an `<intrin.h>` whose function is
 * implemented with `lock cmpxchg16b`, so the order of the arguments, the
 * halves and the result are exercised), but it has not been run with the real
 * compiler. Run the dwcas suites on MSVC before relying on it.
 *
 * This file is only meant to be included when `LUMEX_DWCAS_BACKEND` is
 * `LUMEX_DWCAS_BACKEND_MSVC`.
 */
#ifndef LUMEX_CORE_ATOMIC_DWCAS_DWCAS_BACKEND_MSVC_HPP
#define LUMEX_CORE_ATOMIC_DWCAS_DWCAS_BACKEND_MSVC_HPP

#include <cstdint>

#if defined(_MSC_VER)                                                         \
    || (defined(LUMEX_DWCAS_BACKEND) && LUMEX_DWCAS_BACKEND == 2)
#include <intrin.h>
#endif

#include "lumex/core/atomic/dwcas/LumexDwcasConfig.hpp"
#include "lumex/core/atomic/dwcas/LumexDwcasFromCas.hpp"
#include "lumex/core/atomic/dwcas/LumexDwcasValue.hpp"
#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

#if LUMEX_ATOMIC_HAS_DWCAS && LUMEX_DWCAS_BACKEND == LUMEX_DWCAS_BACKEND_MSVC

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
inline namespace backend_msvc
{
/// The compare-and-swap primitive of the MSVC backend.
struct msvc_cas_t
{
  /**
   * @brief `_InterlockedCompareExchange128` on the 16 bytes at @p words.
   * @param words Address of the low half; the 16 bytes must be 16-byte
   * aligned.
   * @param expected The value the memory is compared with.
   * @param desired The value stored when the comparison succeeds.
   * @return The value found in memory (equal to @p expected on success).
   */
  static dwcas_value_t
  cas (std::uint64_t *words, dwcas_value_t expected,
       dwcas_value_t desired) LUMEX_NOEXCEPT
  {
    long long comparand[2] = { static_cast<long long> (expected.lo),
                               static_cast<long long> (expected.hi) };
    LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (::_InterlockedCompareExchange128 (
        reinterpret_cast<long long volatile *> (words),
        static_cast<long long> (desired.hi),
        static_cast<long long> (desired.lo), comparand));
    dwcas_value_t found = { static_cast<std::uint64_t> (comparand[0]),
                            static_cast<std::uint64_t> (comparand[1]) };
    return found;
  }
};

/// The operations the word calls.
typedef cas_operations_t<msvc_cas_t> backend_operations_t;
} // namespace backend_msvc
} // namespace Detail
} // namespace dwcas
} // namespace atomic
} // namespace core
} // namespace lumex

#endif // LUMEX_ATOMIC_HAS_DWCAS && LUMEX_DWCAS_BACKEND == ...

#endif // !LUMEX_CORE_ATOMIC_DWCAS_DWCAS_BACKEND_MSVC_HPP
