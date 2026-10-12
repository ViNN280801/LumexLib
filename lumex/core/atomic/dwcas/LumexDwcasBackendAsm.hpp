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
 * @file LumexDwcasBackendAsm.hpp
 * @brief The `lock cmpxchg16b` backend in GNU inline assembly (GCC, Clang,
 * clang-cl, MinGW).
 * @details One instruction, written out by hand, so the compiler needs no
 * `-mcx16` and no libatomic is linked: GCC sends every 16-byte `__atomic_*`
 * operation to libatomic, with or without `-mcx16`, and `__sync_*_16` without
 * the flag is an undefined symbol. The instruction compares `rdx:rax` with the
 * 16 bytes at the (16-byte aligned) operand; if equal it stores `rcx:rbx`,
 * otherwise it loads the 16 bytes into `rdx:rax`. Either way `rdx:rax` ends as
 * the value that was in memory, so no flag output is needed: the caller
 * compares the result with the expected value.
 *
 * `lock` makes the instruction a full barrier, and the `"memory"` clobber
 * makes it a compiler barrier, so every C++ memory order is satisfied with the
 * strength of `seq_cst`; the order arguments are ignored. The address is
 * passed in a register, so the template spells the same instruction for the
 * AT&T and the Intel assembler dialect (`-masm=intel`); the clobber tells the
 * compiler that memory changes, which is what makes the access safe under
 * strict aliasing (and under `-fno-strict-aliasing`).
 *
 * This file is only meant to be included when `LUMEX_DWCAS_BACKEND` is
 * `LUMEX_DWCAS_BACKEND_ASM`.
 */
#ifndef LUMEX_CORE_ATOMIC_DWCAS_DWCAS_BACKEND_ASM_HPP
#define LUMEX_CORE_ATOMIC_DWCAS_DWCAS_BACKEND_ASM_HPP

#include <cstdint>

#include "lumex/core/atomic/dwcas/LumexDwcasConfig.hpp"
#include "lumex/core/atomic/dwcas/LumexDwcasFromCas.hpp"
#include "lumex/core/atomic/dwcas/LumexDwcasValue.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

#if LUMEX_ATOMIC_HAS_DWCAS && LUMEX_DWCAS_BACKEND == LUMEX_DWCAS_BACKEND_ASM

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
inline namespace backend_asm
{
/// The compare-and-swap primitive of the assembly backend.
struct asm_cas_t
{
  /**
   * @brief `lock cmpxchg16b` on the 16 bytes at @p words.
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
    std::uint64_t low = expected.lo;
    std::uint64_t high = expected.hi;
    __asm__ __volatile__ ("lock cmpxchg16b {(%[words])|xmmword ptr [%[words]]}"
                          : "+a"(low), "+d"(high)
                          : [words] "r"(words), "b"(desired.lo),
                            "c"(desired.hi)
                          : "memory", "cc");
    dwcas_value_t found = { low, high };
    return found;
  }
};

/// The operations the word calls.
typedef cas_operations_t<asm_cas_t> backend_operations_t;
} // namespace backend_asm
} // namespace Detail
} // namespace dwcas
} // namespace atomic
} // namespace core
} // namespace lumex

#endif // LUMEX_ATOMIC_HAS_DWCAS && LUMEX_DWCAS_BACKEND == ...

#endif // !LUMEX_CORE_ATOMIC_DWCAS_DWCAS_BACKEND_ASM_HPP
