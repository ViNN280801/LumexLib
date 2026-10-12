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
 * @file LumexDwcasValue.hpp
 * @brief `dwcas_value_t`, the 16-byte value of a `dwcas_word`: two 64-bit
 * halves.
 * @details A plain aggregate that is passed and returned by value. `lo` is
 * the half at the lower address (the one `rax` carries in `cmpxchg16b`), `hi`
 * the half at the higher address (`rdx`). Equality compares both halves.
 * Declared only where the layer exists (`LUMEX_ATOMIC_HAS_DWCAS`).
 */
#ifndef LUMEX_CORE_ATOMIC_DWCAS_DWCAS_VALUE_HPP
#define LUMEX_CORE_ATOMIC_DWCAS_DWCAS_VALUE_HPP

#include <cstdint>

#include "lumex/core/atomic/dwcas/LumexDwcasConfig.hpp"
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
/**
 * @brief The two halves of a 128-bit value.
 */
struct dwcas_value_t
{
  /// The half at the lower address.
  std::uint64_t lo;
  /// The half at the higher address.
  std::uint64_t hi;
};

/**
 * @brief Tells whether both halves of @p a and @p b are equal.
 */
inline LUMEX_CONSTEXPR bool
operator== (dwcas_value_t const &a, dwcas_value_t const &b) LUMEX_NOEXCEPT
{
  return a.lo == b.lo && a.hi == b.hi;
}

/**
 * @brief Tells whether some half of @p a and @p b differs.
 */
inline LUMEX_CONSTEXPR bool
operator!= (dwcas_value_t const &a, dwcas_value_t const &b) LUMEX_NOEXCEPT
{
  return a.lo != b.lo || a.hi != b.hi;
}

namespace Detail
{
/**
 * @brief Reads one 64-bit half with relaxed ordering and without a lock.
 * @details An aligned 64-bit load is atomic on x86-64, but it is not part of
 * the 128-bit operation: two such loads may see a pair that never existed
 * (the halves of two different values). The result is a guess to be checked
 * by a compare-and-swap, never a value to act on.
 * @param half Address of the half.
 */
inline std::uint64_t
relaxed_load64 (std::uint64_t const *half) LUMEX_NOEXCEPT
{
#if defined(__GNUC__) || defined(__clang__)
  return __atomic_load_n (half, __ATOMIC_RELAXED);
#else
  return *static_cast<std::uint64_t const volatile *> (half);
#endif
}
} // namespace Detail
} // namespace dwcas
} // namespace atomic
} // namespace core
} // namespace lumex

#endif // LUMEX_ATOMIC_HAS_DWCAS

#endif // !LUMEX_CORE_ATOMIC_DWCAS_DWCAS_VALUE_HPP
