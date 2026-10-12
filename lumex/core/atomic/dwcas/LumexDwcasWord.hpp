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
 * @file LumexDwcasWord.hpp
 * @brief `dwcas_word`, a 16-byte atomic word with a 128-bit compare-and-swap
 * (x86-64 only), and the header that brings the whole layer.
 * @details The word holds two 64-bit halves, `lo` and `hi`, in 16 aligned
 * bytes and offers `load`, `store`, `exchange` and `compare_exchange_strong`
 * and `_weak` on both halves at once. It is the hardware layer of the
 * split-count engine of the atomic smart pointers and is usable on its own.
 * It exists where `LUMEX_ATOMIC_HAS_DWCAS` is 1 (x86-64 with GCC, Clang or
 * MSVC; see `LumexDwcasConfig.hpp`); elsewhere nothing is declared. The
 * header includes the configuration, the value type and the CPU check, so
 * including it is enough.
 *
 * **Operations return values.** `compare_exchange_strong (expected,
 * desired)` returns the value it observed in the word: the swap happened
 * exactly when the result equals @c expected (`operator==` of
 * `dwcas_value_t`). That is what `cmpxchg16b` delivers, and a caller that
 * retries uses the result as its next @c expected. `compare_exchange_weak`
 * is the same operation: on x86-64 the instruction never fails spuriously.
 *
 * **A load is a write.** The CPU has no atomic 16-byte load that is
 * guaranteed on every x86-64 CPU (an SSE load is atomic only on some). The
 * only instruction that reads 16 bytes atomically on all of them is the
 * locked `cmpxchg16b`, a read-modify-write that needs write access to the
 * destination whatever the comparison says (this wrapper compares with zero
 * and stores zero, so the memory is not changed). Consequences, all by
 * design:
 *
 * - the members are `mutable` and `load` is a `const` function: a `const
 *   dwcas_word` is not placed in read-only memory by the compiler;
 * - the word must live in writable memory: a word in a page mapped read-only
 *   (for example through `mmap (PROT_READ)` or a `const_cast` of a constant
 *   in a read-only section) faults on `load`;
 * - a load takes the cache line exclusively, like a store, so readers do not
 *   scale among themselves; `speculative_load` avoids the write when a guess
 *   is enough;
 * - the built-in backend (ThreadSanitizer builds) may use a plain 16-byte
 *   read instead; a program must not depend on either.
 *
 * **Memory orders.** The member functions take `std::memory_order` arguments
 * with the meaning of `std::atomic`. The hardware backends (assembly and MSVC)
 * ignore them: a locked instruction is a full barrier on x86-64, so each call
 * behaves as a `seq_cst` read-modify-write whatever the argument, and the
 * compiler may not move any memory access across it. In the C++ memory model
 * that means: the call synchronizes with the other calls on the same word
 * like a `seq_cst` operation would, and an operation that asks for less (for
 * example `release`) gets more, never less. The model itself does not know
 * the instruction (it is not an atomic operation in the standard's sense,
 * only in the instruction set's), the compiler and ThreadSanitizer treat it
 * as opaque, so this is a contract of the layer for the supported compilers
 * and not a derived property; the built-in backend is the one the model and
 * ThreadSanitizer understand, and the suites run on both. The failure order
 * of a failed compare-and-swap is likewise at least as strong as asked.
 *
 * **Alignment and aliasing.** The word is `alignas (16)`: `cmpxchg16b` raises
 * a general-protection fault on a destination that is not 16-byte aligned,
 * and a word placed through a packed layout, a misaligned placement `new` or
 * a cast from a smaller aligned buffer is such a destination. Over-aligned
 * `operator new` is needed for a heap word before C++17 (a plain `new`
 * guarantees only `alignof (std::max_align_t)`, which is 16 on x86-64
 * glibc and the MSVC x64 runtime but is not required). The class
 * `static_assert`s its size and alignment. The halves are plain `uint64_t`
 * members that the assembly accesses through a memory operand with a
 * `"memory"` clobber (and the built-in backend through a `may_alias` type), so
 * the operations are correct under strict aliasing and under
 * `-fno-strict-aliasing`. Do not read or write the halves of a live word
 * by other means than the member functions.
 *
 * **CPU support.** The constructors do not check that the CPU has
 * `CMPXCHG16B` (they are `constexpr` so that a static word needs no dynamic
 * initialization); `require_dwcas ()` does, and the objects built on the layer
 * call it from their constructors.
 */
#ifndef LUMEX_CORE_ATOMIC_DWCAS_DWCAS_WORD_HPP
#define LUMEX_CORE_ATOMIC_DWCAS_DWCAS_WORD_HPP

#include <atomic>
#include <cstddef>
#include <cstdint>

#include "lumex/core/atomic/dwcas/LumexDwcasConfig.hpp"
#include "lumex/core/atomic/dwcas/LumexDwcasCpu.hpp"
#include "lumex/core/atomic/dwcas/LumexDwcasValue.hpp"
#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

#if LUMEX_ATOMIC_HAS_DWCAS

#if LUMEX_DWCAS_BACKEND == LUMEX_DWCAS_BACKEND_ASM
#include "lumex/core/atomic/dwcas/LumexDwcasBackendAsm.hpp"
#elif LUMEX_DWCAS_BACKEND == LUMEX_DWCAS_BACKEND_MSVC
#include "lumex/core/atomic/dwcas/LumexDwcasBackendMsvc.hpp"
#elif LUMEX_DWCAS_BACKEND == LUMEX_DWCAS_BACKEND_BUILTIN
#include "lumex/core/atomic/dwcas/LumexDwcasBackendBuiltin.hpp"
#endif

namespace lumex
{
namespace core
{
namespace atomic
{
namespace dwcas
{
/**
 * @brief A 16-byte atomic word of two 64-bit halves with a 128-bit
 * compare-and-swap.
 * @details See the file description for the memory model, the cost of a load
 * and the alignment rules. The word is neither copyable nor movable.
 */
class alignas (16) dwcas_word
{
public:
  /// The type the operations take and return.
  typedef dwcas_value_t value_type;

  /**
   * @brief Creates a word holding zero in both halves.
   */
  LUMEX_CONSTEXPR
  dwcas_word () LUMEX_NOEXCEPT : lo_ (0u), hi_ (0u) {}

  /**
   * @brief Creates a word holding @p initial. Not an atomic operation.
   */
  LUMEX_CONSTEXPR explicit dwcas_word (value_type initial) LUMEX_NOEXCEPT
      : lo_ (initial.lo),
        hi_ (initial.hi)
  {
  }

  dwcas_word (dwcas_word const &) = delete;
  dwcas_word &operator= (dwcas_word const &) = delete;

  /**
   * @brief Tells whether the operations are lock-free.
   * @return Always true: they are single instructions (or libatomic calls
   * on a CPU that has the instruction, in the built-in backend).
   */
  bool
  is_lock_free () const LUMEX_NOEXCEPT
  {
    return true;
  }

  /**
   * @brief Reads the two halves one after the other with relaxed 64-bit
   * loads.
   * @details Not atomic as a pair: the result can combine the halves of two
   * different values, one that the word never held. Use it as a guess, to
   * be validated by a compare-and-swap (which fails for a pair that is not
   * the current one), or when a half alone is meaningful. It does not write
   * and does not order anything.
   * @return The two halves read.
   */
  value_type
  speculative_load () const LUMEX_NOEXCEPT
  {
    value_type guess
        = { Detail::relaxed_load64 (&lo_), Detail::relaxed_load64 (&hi_) };
    return guess;
  }

  /**
   * @brief Reads the word atomically.
   * @details A locked read-modify-write that writes back what it read (see
   * the file description), so the memory must be writable.
   * @param order The memory order; must not be `release` or `acq_rel`.
   * @return The value of the word.
   */
  LUMEX_ATTRIBUTE_NODISCARD ("the loaded value")
  value_type
  load (std::memory_order order
        = std::memory_order_seq_cst) const LUMEX_NOEXCEPT
  {
    return Detail::backend_operations_t::load (words (), order);
  }

  /**
   * @brief Replaces the word atomically.
   * @param desired The new value.
   * @param order The memory order; must not be `consume`, `acquire` or
   * `acq_rel`.
   */
  void
  store (value_type desired,
         std::memory_order order = std::memory_order_seq_cst) LUMEX_NOEXCEPT
  {
    Detail::backend_operations_t::store (words (), &hi_, desired, order);
  }

  /**
   * @brief Replaces the word atomically and returns the previous value.
   * @param desired The new value.
   * @param order The memory order.
   * @return The value the word held before.
   */
  LUMEX_ATTRIBUTE_NODISCARD ("the previous value")
  value_type
  exchange (value_type desired,
            std::memory_order order = std::memory_order_seq_cst) LUMEX_NOEXCEPT
  {
    return Detail::backend_operations_t::exchange (words (), &hi_, desired,
                                                   order);
  }

  /**
   * @brief Replaces the word by @p desired if it holds @p expected.
   * @param expected The value the word is compared with.
   * @param desired The value stored when the comparison succeeds.
   * @param success The memory order of a successful swap.
   * @param failure The memory order of a failed comparison; must not be
   * `release` or `acq_rel`.
   * @return The value the word held: equal to @p expected when the swap
   * happened, the current value otherwise.
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "the observed value, which tells whether the swap happened")
  value_type
  compare_exchange_strong (value_type expected, value_type desired,
                           std::memory_order success
                           = std::memory_order_seq_cst,
                           std::memory_order failure
                           = std::memory_order_seq_cst) LUMEX_NOEXCEPT
  {
    return Detail::backend_operations_t::compare_exchange (
        words (), expected, desired, success, failure);
  }

  /**
   * @brief As `compare_exchange_strong`; on x86-64 the instruction does not
   * fail spuriously, so the two are the same operation.
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "the observed value, which tells whether the swap happened")
  value_type
  compare_exchange_weak (value_type expected, value_type desired,
                         std::memory_order success = std::memory_order_seq_cst,
                         std::memory_order failure
                         = std::memory_order_seq_cst) LUMEX_NOEXCEPT
  {
    return Detail::backend_operations_t::compare_exchange (
        words (), expected, desired, success, failure);
  }

private:
  /// Address of the low half, from a `const` function: the members are
  /// `mutable` because even a load writes.
  std::uint64_t *
  words () const LUMEX_NOEXCEPT
  {
    return &lo_;
  }

  mutable std::uint64_t lo_;
  mutable std::uint64_t hi_;
};

static_assert (sizeof (dwcas_word) == 16, "dwcas_word is exactly 16 bytes");
static_assert (alignof (dwcas_word) == 16,
               "dwcas_word is 16-byte aligned: cmpxchg16b faults otherwise");
static_assert (sizeof (dwcas_value_t) == 16, "the value is two 64-bit halves");
} // namespace dwcas
} // namespace atomic
} // namespace core
} // namespace lumex

#endif // LUMEX_ATOMIC_HAS_DWCAS

#endif // !LUMEX_CORE_ATOMIC_DWCAS_DWCAS_WORD_HPP
