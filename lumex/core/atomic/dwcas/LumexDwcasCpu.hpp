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
 * @file LumexDwcasCpu.hpp
 * @brief Run-time check that the CPU has the `CMPXCHG16B` instruction:
 * `dwcas_supported ()` and `require_dwcas ()`.
 * @details Every x86-64 CPU sold since 2006 has the instruction (the first
 * AMD64 steppings did not; Windows 8.1 and later require it). The layer
 * therefore does not emulate it: a program that reaches `require_dwcas ()` on
 * a CPU without it writes one line to `stderr` and calls `std::abort ()`.
 * Code that wants to degrade gracefully asks `dwcas_supported ()` first.
 *
 * The answer comes from CPUID leaf 1, ECX bit 13. It is computed on first use
 * and cached in a function-local `std::atomic<int>` that is constant
 * initialized, so there is no static-initialization-order problem and no
 * guard variable, and two threads that race on the first call compute the
 * same value. `dwcas_word` itself does not check: its constructors are
 * `constexpr` so a word with static storage needs no run-time initialization.
 * The objects built on the layer call `require_dwcas ()` from their
 * constructors.
 *
 * The functions in `Detail` take the CPUID reading as a parameter or through a
 * replaceable function pointer, so the tests can inject the answer of a CPU
 * without the instruction.
 */
#ifndef LUMEX_CORE_ATOMIC_DWCAS_DWCAS_CPU_HPP
#define LUMEX_CORE_ATOMIC_DWCAS_DWCAS_CPU_HPP

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#if defined(_MSC_VER)
#include <intrin.h>
#elif defined(__x86_64__) || defined(__amd64__)
#include <cpuid.h>
#endif

#include "lumex/core/atomic/dwcas/LumexDwcasConfig.hpp"
#include "lumex/core/utility/attr/LumexAttributes.hpp"
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
/// Bit 13 of ECX in CPUID leaf 1: the CMPXCHG16B instruction.
enum : std::uint32_t
{
  cmpxchg16b_ecx_bit = 1u << 13
};

/// Signature of the function that reads ECX of CPUID leaf 1.
typedef std::uint32_t (*cpuid_reader_t) ();

/**
 * @brief Reads ECX of CPUID leaf 1 from the CPU.
 * @return The register, or 0 when the CPU has no leaf 1.
 */
inline std::uint32_t
read_cpuid_leaf1_ecx () LUMEX_NOEXCEPT
{
#if defined(_MSC_VER)
  int registers[4] = { 0, 0, 0, 0 };
  __cpuid (registers, 1);
  return static_cast<std::uint32_t> (registers[2]);
#else
  unsigned int eax = 0, ebx = 0, ecx = 0, edx = 0;
  if (__get_cpuid (1, &eax, &ebx, &ecx, &edx) == 0)
    return 0;
  return static_cast<std::uint32_t> (ecx);
#endif
}

/**
 * @brief The reader `dwcas_supported ()` calls; the test seam.
 * @details Points at `read_cpuid_leaf1_ecx` until a test replaces it. It is
 * a plain pointer: replace it only while no other thread uses the layer, and
 * call `reset_cpu_probe ()` afterwards.
 */
inline cpuid_reader_t &
cpuid_reader () LUMEX_NOEXCEPT
{
  static cpuid_reader_t reader = &read_cpuid_leaf1_ecx;
  return reader;
}

/// The cached answer: 0 unknown, 1 supported, 2 not supported.
inline std::atomic<int> &
support_state () LUMEX_NOEXCEPT
{
  static std::atomic<int> state (0);
  return state;
}

/// Forgets the cached answer, so the next call reads the CPU again.
inline void
reset_cpu_probe () LUMEX_NOEXCEPT
{
  support_state ().store (0, std::memory_order_relaxed);
}

/**
 * @brief Tells whether a CPUID leaf 1 ECX value has the CMPXCHG16B bit.
 * @param ecx The register.
 */
inline LUMEX_CONSTEXPR bool
ecx_has_cmpxchg16b (std::uint32_t ecx) LUMEX_NOEXCEPT
{
  return (ecx & cmpxchg16b_ecx_bit) != 0u;
}

/// The line `terminate_without_cmpxchg16b ()` writes to `stderr`.
inline char const *
missing_cpu_message () LUMEX_NOEXCEPT
{
  return "lumex::core::atomic::dwcas: this CPU has no CMPXCHG16B "
         "instruction (CPUID leaf 1, ECX bit 13), which the 128-bit "
         "compare-and-swap layer needs; terminating. Define "
         "LUMEX_ATOMIC_DISABLE_DWCAS to build without the layer.\n";
}

/// Writes `missing_cpu_message ()` to `stderr` and aborts.
LUMEX_ATTRIBUTE_NORETURN inline void
terminate_without_cmpxchg16b () LUMEX_NOEXCEPT
{
  std::fputs (missing_cpu_message (), stderr);
  std::fflush (stderr);
  std::abort ();
}
} // namespace Detail

/**
 * @brief Tells whether this CPU has the CMPXCHG16B instruction.
 * @details Thread-safe, lock-free and cached: the first call reads CPUID, the
 * later ones load an atomic. Never terminates.
 * @return true when `dwcas_word` can be used on this CPU.
 */
inline bool
dwcas_supported () LUMEX_NOEXCEPT
{
  std::atomic<int> &state = Detail::support_state ();
  int known = state.load (std::memory_order_relaxed);
  if (known == 0)
    {
      known = Detail::ecx_has_cmpxchg16b (Detail::cpuid_reader () ()) ? 1 : 2;
      state.store (known, std::memory_order_relaxed);
    }
  return known == 1;
}

/**
 * @brief Terminates the program with a message unless the CPU has the
 * CMPXCHG16B instruction.
 * @details Call it once from the constructor of an object that uses
 * `dwcas_word`; it costs a relaxed load afterwards. On a CPU without the
 * instruction the use of `dwcas_word` would otherwise end in an illegal
 * instruction signal with no explanation.
 */
inline void
require_dwcas () LUMEX_NOEXCEPT
{
  if (!dwcas_supported ())
    Detail::terminate_without_cmpxchg16b ();
}
} // namespace dwcas
} // namespace atomic
} // namespace core
} // namespace lumex

#endif // LUMEX_ATOMIC_HAS_DWCAS

#endif // !LUMEX_CORE_ATOMIC_DWCAS_DWCAS_CPU_HPP
