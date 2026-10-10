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

// Table-driven tests of the slot word of the split-count engine
// (LumexSplitCountWord.hpp): the window of the signed offset, the block and
// holder words, the empty word, the tick field and the value comparison.
// Every expectation is a literal of a table; nothing is computed from the
// codec under test. The block and holder words name storage that is never
// dereferenced.

#include <cstdint>

#include <gtest/gtest.h>

#include "lumex/core/atomic/smart_ptr/split_count/LumexSplitCountWord.hpp"

#if LUMEX_ATOMIC_HAS_DWCAS

namespace asp = ::lumex::core::atomic::smart_ptr;

namespace
{
typedef asp::Detail::split_word_t split_word;
typedef asp::Detail::split_value_t split_value;
typedef asp::Detail::split_block_t split_block;

// Storage whose addresses stand for a control block and for a holder.
alignas (16) unsigned char block_storage[64];
alignas (16) unsigned char holder_storage[64];

split_block const *
block_address ()
{
  return reinterpret_cast<split_block const *> (block_storage);
}

split_block const *
holder_address ()
{
  return reinterpret_cast<split_block const *> (holder_storage);
}

std::uint64_t
block_lo ()
{
  return static_cast<std::uint64_t> (
      reinterpret_cast<std::uintptr_t> (block_storage));
}

std::uint64_t
holder_lo ()
{
  return static_cast<std::uint64_t> (
      reinterpret_cast<std::uintptr_t> (holder_storage));
}

struct fits_row
{
  char const *name;
  std::uint64_t offset;
  bool expected;
};

struct offset_row
{
  char const *name;
  std::uint64_t offset;
};

struct same_row
{
  char const *name;
  split_value a;
  split_value b;
  bool expected;
};
} // namespace

TEST (LumexSplitCountWordTest,
      GivenTheOffsetWindow_WhenChecked_ThenItIsMinus2To39ToPlus2To39Minus1)
{
  static fits_row const table[] = {
    { "zero", 0x0ull, true },
    { "one", 0x1ull, true },
    { "largest in window", 0x7FFFFFFFFFull, true },
    { "first above window", 0x8000000000ull, false },
    { "smallest in window", 0xFFFFFF8000000000ull, true },
    { "first below window", 0xFFFFFF7FFFFFFFFFull, false },
    { "largest signed 64-bit", 0x7FFFFFFFFFFFFFFFull, false },
    { "smallest signed 64-bit", 0x8000000000000000ull, false },
    { "minus one", 0xFFFFFFFFFFFFFFFFull, true },
  };
  for (fits_row const &row : table)
    {
      SCOPED_TRACE (row.name);
      EXPECT_EQ (split_word::fits (row.offset), row.expected);
    }
}

TEST (LumexSplitCountWordTest,
      GivenABlockWord_WhenBuiltAndRead_ThenTheOffsetRoundTrips)
{
  static offset_row const table[] = {
    { "zero", 0x0ull },
    { "one", 0x1ull },
    { "eight", 0x8ull },
    { "minus eight", 0xFFFFFFFFFFFFFFF8ull },
    { "largest in window", 0x7FFFFFFFFFull },
    { "smallest in window", 0xFFFFFF8000000000ull },
  };
  for (offset_row const &row : table)
    {
      SCOPED_TRACE (row.name);
      split_value const w
          = split_word::block_word (block_address (), row.offset);
      EXPECT_FALSE (split_word::is_empty (w));
      EXPECT_FALSE (split_word::is_holder (w));
      EXPECT_EQ (split_word::block_of (w), block_address ());
      EXPECT_EQ (split_word::offset_of (w), row.offset);
      EXPECT_EQ (split_word::ticks_of (w), 0u);
      EXPECT_EQ (w.lo, block_lo ());
    }
}

TEST (LumexSplitCountWordTest,
      GivenAHolder_WhenBuilt_ThenItIsAHolderAtOffsetZero)
{
  split_value const w = split_word::holder_word (holder_address ());
  EXPECT_TRUE (split_word::is_holder (w));
  EXPECT_FALSE (split_word::is_empty (w));
  EXPECT_EQ (split_word::block_of (w), holder_address ());
  EXPECT_EQ (split_word::offset_of (w), 0u);
  EXPECT_EQ (w.lo & 1u, 1u);
  EXPECT_EQ (w.lo, holder_lo () + 1u);
}

TEST (LumexSplitCountWordTest,
      GivenAnEmptyRaw_WhenBuilt_ThenItIsEmptyAndKeepsTheRaw)
{
  static std::uint64_t const table[] = {
    0x0ull,
    0x1ull,
    0xFFFFFFFFFFFFFFFFull,
  };
  for (std::uint64_t raw : table)
    {
      SCOPED_TRACE (raw);
      split_value const w = split_word::empty_word (raw);
      EXPECT_TRUE (split_word::is_empty (w));
      EXPECT_FALSE (split_word::is_holder (w));
      EXPECT_EQ (split_word::raw_of (w), raw);
    }
}

TEST (LumexSplitCountWordTest,
      GivenAFreshBlockWord_WhenTicked_ThenTheOffsetIsKept)
{
  static offset_row const table[] = {
    { "minus one", 0xFFFFFFFFFFFFFFFFull },
    { "largest in window", 0x7FFFFFFFFFull },
  };
  for (offset_row const &row : table)
    {
      SCOPED_TRACE (row.name);
      split_value w = split_word::block_word (block_address (), row.offset);
      w = split_word::with_tick (w);
      w = split_word::with_tick (w);
      w = split_word::with_tick (w);
      EXPECT_EQ (split_word::ticks_of (w), 3u);
      EXPECT_EQ (split_word::offset_of (w), row.offset);
      EXPECT_EQ (split_word::block_of (w), block_address ());
    }
}

TEST (LumexSplitCountWordTest,
      GivenATickedBlockWord_WhenUntickedAgain_ThenTheWordIsRestored)
{
  static offset_row const table[] = {
    { "minus one", 0xFFFFFFFFFFFFFFFFull },
    { "largest in window", 0x7FFFFFFFFFull },
  };
  for (offset_row const &row : table)
    {
      SCOPED_TRACE (row.name);
      split_value const w
          = split_word::block_word (block_address (), row.offset);
      split_value const ticked = split_word::with_tick (w);
      EXPECT_EQ (split_word::ticks_of (ticked), 1u);
      split_value const back = split_word::without_tick (ticked);
      EXPECT_EQ (back.lo, w.lo);
      EXPECT_EQ (back.hi, w.hi);
    }
}

TEST (LumexSplitCountWordTest,
      GivenTwoWords_WhenCompared_ThenSameValueFollowsBlockOffsetAndRaw)
{
  // The literal words: block lo 0x10000, holder lo 0x10001 (never
  // dereferenced); the offset field sits at bit 24 of hi, the ticks below it.
  static same_row const table[] = {
    { "same block and offset, other ticks",
      { 0x10000ull, 0x5000000ull },
      { 0x10000ull, 0x5000003ull },
      true },
    { "same block, other offset",
      { 0x10000ull, 0x5000000ull },
      { 0x10000ull, 0x6000000ull },
      false },
    { "same block and minus one offset, other ticks",
      { 0x10000ull, 0xFFFFFFFFFF000000ull },
      { 0x10000ull, 0xFFFFFFFFFF000002ull },
      true },
    { "block against holder",
      { 0x10000ull, 0x0ull },
      { 0x10001ull, 0x0ull },
      false },
    { "empty raw 5 against 5", { 0x0ull, 0x5ull }, { 0x0ull, 0x5ull }, true },
    { "empty raw 5 against 6", { 0x0ull, 0x5ull }, { 0x0ull, 0x6ull }, false },
    { "empty against block",
      { 0x0ull, 0x5ull },
      { 0x10000ull, 0x5000000ull },
      false },
  };
  for (same_row const &row : table)
    {
      SCOPED_TRACE (row.name);
      EXPECT_EQ (split_word::same_value (row.a, row.b), row.expected);
    }
}

TEST (LumexSplitCountWordTest,
      GivenTheLayoutConstants_WhenRead_ThenTheValuesAreTheDocumentedOnes)
{
  EXPECT_EQ (split_word::tick_mask (), 0xFFFFFFu);
  EXPECT_EQ (split_word::offset_shift (), 24u);
  EXPECT_EQ (split_word::offset_mask (), 0xFFFFFFFFFFull);
  EXPECT_EQ (split_word::holder_flag (), 1u);
}

#else

TEST (LumexSplitCountWordTest, GivenNoDwcas_WhenCompiled_ThenNoCodec)
{
  EXPECT_EQ (LUMEX_ATOMIC_HAS_DWCAS, 0);
}

#endif
