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
 * @file LumexSplitCountWord.hpp
 * @brief The slot word of the split-count engine: a control block, the ticks
 * of the loads in flight and the stored pointer in one 16-byte word.
 * @details The engine (`LumexSplitCountCell.hpp`) keeps its value in one
 * `dwcas_word` (`lumex/core/atomic/dwcas/`). The two halves mean:
 *
 * | Half | Bits | Meaning |
 * | --- | --- | --- |
 * | `lo` | 0 | 1: the block is a holder (`ctl_holder` / `ctl_weak_holder`) |
 * | `lo` | 1..63 | the rest of the control block address; `lo == 0` is the
 * empty value | | `hi` (block) | 0..23 | `L`, the ticks: loads that have
 * pinned the block and not settled yet | | `hi` (block) | 24..63 | the offset
 * of the stored pointer from the block's anchor, signed, 40 bits | | `hi`
 * (empty) | 0..63 | the stored pointer of an empty smart pointer (an alias of
 * an empty owner) |
 *
 * A control block is at least 8-byte aligned (`alignof (ctl_base)`), so bit 0
 * of its address is free for the holder flag. The offset is `stored -
 * anchor` in bytes, two's complement, so the creating pointer has offset 0
 * and a base-class conversion or a member alias a small one. A pointer whose
 * offset does not fit in 40 bits (an alias of an object outside the owner's
 * allocation) is stored through a holder block whose word has offset 0.
 * Nothing here depends on the width of the address space: the block address
 * fills `lo` and the offset is a difference of two addresses.
 *
 * `L` never carries into the offset: a pin (a load or a writer) does not
 * tick a word whose `L` is at the limit of the policy (at most 2^24 - 1), and
 * an un-tick is made only on a word whose `L` is not zero. The packing is a
 * pure function of the control block and the stored pointer: a value that
 * fits is always a block word, one that does not is always a holder word.
 *
 * Everything here is an implementation detail of `lumex/core/atomic`.
 */
#ifndef LUMEX_CORE_ATOMIC_SMART_PTR_SPLIT_COUNT_SPLIT_COUNT_WORD_HPP
#define LUMEX_CORE_ATOMIC_SMART_PTR_SPLIT_COUNT_SPLIT_COUNT_WORD_HPP

#include <cstdint>

#include "lumex/core/atomic/dwcas/LumexDwcasWord.hpp"
#include "lumex/core/atomic/smart_ptr/LumexAtomicSmartPtrConfig.hpp"
#include "lumex/core/smart_ptr/ctl/LumexSmartPtrCtlBase.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

#if LUMEX_ATOMIC_HAS_DWCAS

namespace lumex
{
namespace core
{
namespace atomic
{
namespace smart_ptr
{
inline namespace LUMEX_ATOMIC_SMART_PTR_ABI_NAMESPACE
{
namespace Detail
{
/// The value of a slot word: the two halves of a `dwcas_word`.
typedef ::lumex::core::atomic::dwcas::dwcas_value_t split_value_t;

/// The control block of the module's own smart pointers.
typedef ::lumex::core::smart_ptr::detail::ctl_base split_block_t;

static_assert (sizeof (void *) == 8,
               "the split-count word packs 64-bit addresses");
static_assert (sizeof (split_value_t) == 16,
               "the slot word is two 64-bit halves");
static_assert (alignof (split_block_t) >= 2,
               "bit 0 of a control block address holds the holder flag");

/**
 * @brief Encoding and decoding of the slot word (see the file text).
 * @details Only static functions; every function is a pure computation on
 * values, the word itself is read and written by the engine.
 */
struct split_word_t
{
  /// Bit 0 of `lo`: the block is a holder.
  static LUMEX_CONSTEXPR std::uint64_t
  holder_flag () LUMEX_NOEXCEPT
  {
    return 1u;
  }

  /// Mask of `L` in `hi`: 24 bits.
  static LUMEX_CONSTEXPR std::uint64_t
  tick_mask () LUMEX_NOEXCEPT
  {
    return 0xFFFFFFu;
  }

  /// Position of the offset in `hi`.
  static LUMEX_CONSTEXPR unsigned
  offset_shift () LUMEX_NOEXCEPT
  {
    return 24u;
  }

  /// Mask of the 40-bit offset field (after the shift).
  static LUMEX_CONSTEXPR std::uint64_t
  offset_mask () LUMEX_NOEXCEPT
  {
    return 0xFFFFFFFFFFull;
  }

  /// The sign bit of the 40-bit offset field (after the shift).
  static LUMEX_CONSTEXPR std::uint64_t
  offset_sign () LUMEX_NOEXCEPT
  {
    return 0x8000000000ull;
  }

  /// The integer value of an address.
  static std::uint64_t
  address_of (void const volatile *pointer) LUMEX_NOEXCEPT
  {
    return static_cast<std::uint64_t> (
        reinterpret_cast<std::uintptr_t> (pointer));
  }

  /// True when @p offset (two's complement) fits in the 40-bit field.
  static bool
  fits (std::uint64_t offset) LUMEX_NOEXCEPT
  {
    return ((offset + offset_sign ()) >> 40) == 0u;
  }

  /// The word of an empty smart pointer that stores @p raw.
  static split_value_t
  empty_word (std::uint64_t raw) LUMEX_NOEXCEPT
  {
    split_value_t const value = { 0u, raw };
    return value;
  }

  /// The word of @p block with @p offset (which `fits`) and no ticks.
  static split_value_t
  block_word (split_block_t const *block, std::uint64_t offset) LUMEX_NOEXCEPT
  {
    split_value_t const value
        = { address_of (block), (offset & offset_mask ()) << offset_shift () };
    return value;
  }

  /// The word of the holder @p holder (offset 0, no ticks).
  static split_value_t
  holder_word (split_block_t const *holder) LUMEX_NOEXCEPT
  {
    split_value_t const value = { address_of (holder) | holder_flag (), 0u };
    return value;
  }

  /// True for the word of an empty smart pointer.
  static bool
  is_empty (split_value_t value) LUMEX_NOEXCEPT
  {
    return value.lo == 0u;
  }

  /// True for the word of a holder.
  static bool
  is_holder (split_value_t value) LUMEX_NOEXCEPT
  {
    return (value.lo & holder_flag ()) != 0u;
  }

  /// The block (or holder) a non-empty word names.
  static split_block_t *
  block_of (split_value_t value) LUMEX_NOEXCEPT
  {
    return reinterpret_cast<split_block_t *> (
        static_cast<std::uintptr_t> (value.lo & ~holder_flag ()));
  }

  /// `L` of a non-empty word.
  static std::uint32_t
  ticks_of (split_value_t value) LUMEX_NOEXCEPT
  {
    return static_cast<std::uint32_t> (value.hi & tick_mask ());
  }

  /// The offset of a non-empty word, sign-extended to 64 bits (two's
  /// complement).
  static std::uint64_t
  offset_of (split_value_t value) LUMEX_NOEXCEPT
  {
    std::uint64_t const field = value.hi >> offset_shift ();
    return (field ^ offset_sign ()) - offset_sign ();
  }

  /// The stored pointer of an empty word.
  static std::uint64_t
  raw_of (split_value_t value) LUMEX_NOEXCEPT
  {
    return value.hi;
  }

  /// @p value with one more tick (`L` is below the limit).
  static split_value_t
  with_tick (split_value_t value) LUMEX_NOEXCEPT
  {
    split_value_t const next = { value.lo, value.hi + 1u };
    return next;
  }

  /// @p value with one tick less (`L` is not zero).
  static split_value_t
  without_tick (split_value_t value) LUMEX_NOEXCEPT
  {
    split_value_t const next = { value.lo, value.hi - 1u };
    return next;
  }

  /// True when @p a and @p b name the same value: the same block and offset
  /// whatever their ticks, or the same empty value.
  static bool
  same_value (split_value_t a, split_value_t b) LUMEX_NOEXCEPT
  {
    if (a.lo != b.lo)
      return false;
    if (a.lo == 0u)
      return a.hi == b.hi;
    return (a.hi >> offset_shift ()) == (b.hi >> offset_shift ());
  }
};
} // namespace Detail
} // namespace LUMEX_ATOMIC_SMART_PTR_ABI_NAMESPACE
} // namespace smart_ptr
} // namespace atomic
} // namespace core
} // namespace lumex

#endif // LUMEX_ATOMIC_HAS_DWCAS

#endif // !LUMEX_CORE_ATOMIC_SMART_PTR_SPLIT_COUNT_SPLIT_COUNT_WORD_HPP
