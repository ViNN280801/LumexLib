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
 * @file LumexSplitCountRelacyModel.hpp
 * @brief The split-count engine transcribed for Relacy: the protocol of
 * `LumexSplitCountCell.hpp` with the same memory orders, on Relacy atomics.
 * @details Relacy (Dmitry Vyukov's race detector, used through the optional
 * CMake variable `LUMEX_RELACY_DIR`) explores the interleavings of a small
 * program under the C++11 memory model and reports data races on plain
 * variables, failed assertions and use of freed memory. The models of this
 * directory check the MEMORY ORDERS of the engine, which the sequentially
 * consistent simulator of the other suites cannot see.
 *
 * **What is transcribed.** `cell_t` is `split_count_cell` of the shared
 * engine, step by step: `pin` (L0 to L3), `count_and_settle` (L5 to L9),
 * `settle` (L7, L8), `swap_out` (S1 to S5), `store`, `exchange`,
 * `compare_exchange_strong` (S8 to S11), `compare_exchange_holder` (S12), the
 * deposit, withdrawal and payment helpers and the holder mirror. The counters
 * are `split_counter` (`LumexSmartPtrCounter.hpp`) in its two flavours: the
 * default one has a `release` drop followed by an `acquire` fence when the
 * counter became zero, and `LUMEX_RELACY_TSAN_COUNTER=1` has the
 * ThreadSanitizer flavour (an `acq_rel` drop and no fence). The default
 * counter MASKS the success order of the pinned swap (M9, see the README), so
 * every program is also built with the other flavour. Every order is chosen
 * in the namespace `orders` below, which is the only place a mutation
 * changes.
 *
 * **The 128-bit word.** Relacy has no 16-byte atomic. The slot word is ONE
 * `rl::atomic<std::uint64_t>` that packs `{lo: block id and the holder flag,
 * L, delta}`; every modification of it is one atomic read-modify-write,
 * exactly as `lock cmpxchg16b` is one locked read-modify-write on the real
 * word; its plain reads are the acquire loads of the decisions (L5, L8), the
 * relaxed guesses and the relaxed reloads of the back-off. This
 * is faithful for the questions asked here: the hardware instruction is at
 * least as strong as a sequentially consistent read-modify-write, while the
 * library only REQUESTS `acquire`, `release` or `acq_rel` on it (and
 * `seq_cst` when the caller does), so the model uses the REQUESTED order,
 * which is the weaker one and the one the portable claim rests on. What the
 * packing cannot show: a torn speculative read of the two halves (the
 * simulator's torn policy covers it; most guesses are validated by a
 * whole-word compare-and-swap, and the guess at L5 that decides to count is
 * not, which is harmless because the settle that follows re-validates it) and
 * the 24-bit tick limit (a compile-time constant of the protocol, not an
 * order question).
 *
 * **Plain data and the ghost state.** Control blocks are slots of a pool.
 * What the protocol protects is modelled as `rl::var` fields that Relacy
 * checks for races: the object of a block (`payload`), its liveness flag
 * (`alive`, cleared by the disposal: any access after it is an assertion or a
 * race) and the fields of a holder (`holder_owner`, `holder_ptr`, `owns`).
 * Relacy orders the read-modify-writes of one atomic by the schedule, so a
 * payment can never precede a deposit in the modification order of a counter
 * there, while the C++ memory model allows it when no happens-before edge
 * connects them. The rule "a decision that leads to a payment is taken on an
 * acquire read" is therefore checked through a ghost variable: a writer
 * writes `witness[image]` (a plain variable) before each swap attempt, and a
 * thread about to pay reads `witness` of the word it decided on; Relacy
 * reports a race if no happens-before edge connects the two. An empty word
 * has the witness of block 0, so a store or an exchange of an empty pointer
 * is guarded too (at most one writer of an empty pointer per model). The ghost
 * counters `owners` follow the unit conservation of the oracle note (live
 * pointers, slots, exchange results, built images) and give the check G1,
 * `count + ext >= owners`, at every step.
 *
 * **Not modelled.** The weak engine (same cell, the weak ledger), `wait` and
 * `notify` (the epoch counter), the tick limit and the back-off, the torn
 * guess, `use_count` as an observer, and blocks beyond the strong group
 * (weak counts, deallocation).
 *
 * Everything here is test support.
 */
#ifndef LUMEX_TESTS_CORE_ATOMIC_SMART_PTR_SPLIT_COUNT_RELACY_SPLIT_COUNT_RELACY_MODEL_HPP
#define LUMEX_TESTS_CORE_ATOMIC_SMART_PTR_SPLIT_COUNT_RELACY_SPLIT_COUNT_RELACY_MODEL_HPP

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>

#include "relacy/relacy.hpp"

// The mutation under test (0 = the faithful model). A mutant is a build of the
// same sources with a different value; see the orders namespace.
#ifndef LUMEX_RELACY_MUTANT
#define LUMEX_RELACY_MUTANT 0
#endif

// The counter flavour: 0 is the shipped one (release drop, acquire fence on
// zero), 1 is the one used under ThreadSanitizer (acq_rel drop, no fence).
#ifndef LUMEX_RELACY_TSAN_COUNTER
#define LUMEX_RELACY_TSAN_COUNTER 0
#endif

namespace split_count_relacy
{
typedef std::uint64_t u64;
typedef rl::memory_order mo_t;

/// The mutation of this build.
static const int mutant = LUMEX_RELACY_MUTANT;

/// The counter flavour of this build: true is the ThreadSanitizer one.
static const bool tsan_counter = LUMEX_RELACY_TSAN_COUNTER != 0;

/// One unit of `ext` in a counter word.
static const u64 ext_unit = u64 (1) << 32;

/// Number of control blocks of a pool: objects first, holders after.
static const int pool_size = 12;

/// First pool index handed out to holders.
static const int first_holder = 6;

/// Offsets below this fit the word (a stand-in for the 40-bit window).
static const u64 window_limit = 0x100;

/// The offset of an alias that does not fit: it is stored through a holder.
static const u64 far_offset = 0x1000;

/**
 * @brief The memory orders of the engine, each with the engine line it
 * transcribes. The mutations M8, M9, M10, M25, M33 and the own mutants
 * (101 to 105, 331, 332) change one function. The mutants 201 to 203 change
 * the reserve of the compare-exchange instead of an order: the surplus of the
 * successful swap is one too big (201), the strong form keeps its deposit but
 * does not top it up (202), the deposit of the holder path is one short (203),
 * and the strong form never keeps its deposit (204: the balance still holds,
 * only the coverage requirements of `run_model` fail).
 */
namespace orders
{
/// `rmw_order`: the floor of every successful read-modify-write of the word.
inline mo_t
rmw (mo_t requested)
{
  if (mutant == 25)
    return rl::mo_acq_rel; // M25: seq_cst is ignored
  return requested == rl::mo_seq_cst ? rl::mo_seq_cst : rl::mo_acq_rel;
}

/// L3: the tick compare-and-swap of a load (`order` is already `rmw`).
inline mo_t
tick (mo_t order)
{
  return mutant == 8 ? rl::mo_relaxed : order; // M8
}

/// S2: the tick of a writer's pin.
inline mo_t
writer_pin (mo_t order)
{
  return mutant == 104 ? rl::mo_relaxed : order;
}

/// S1: the compare-and-swap of a word without ticks.
inline mo_t
fast_swap (mo_t order)
{
  return mutant == 103 ? rl::mo_relaxed : order;
}

/// S4 and S12: the success order of the swap of a pinned writer.
inline mo_t
swap_success (mo_t order)
{
  return mutant == 9 ? rl::mo_release : order; // M9
}

/// S4 and S12: the failure order of the swap (the decision value).
inline mo_t
swap_failure ()
{
  if (mutant == 33 || mutant == 332)
    return rl::mo_relaxed; // M33 (writer side)
  return rl::mo_acquire;
}

/// L5, L8: the read that decides "my tick is not in the word".
inline mo_t
decision ()
{
  if (mutant == 33 || mutant == 331)
    return rl::mo_relaxed; // M33 (loader side)
  return rl::mo_acquire;
}

/// L7: the success order of the un-tick.
inline mo_t
untick ()
{
  return mutant == 10 ? rl::mo_relaxed : rl::mo_release; // M10
}

/// S10: the success order of a compare-exchange swap.
inline mo_t
cas_success (mo_t order)
{
  return mutant == 105 ? rl::mo_relaxed : order;
}

/// The counter drop: a `release` read-modify-write, or `acq_rel` in the
/// ThreadSanitizer flavour (102 makes it `relaxed`).
inline mo_t
drop ()
{
  if (mutant == 102)
    return rl::mo_relaxed;
  return tsan_counter ? rl::mo_acq_rel : rl::mo_release;
}

/// The acquire fence after a drop that made the counter zero (the
/// ThreadSanitizer flavour has none; 101 removes it).
inline bool
drop_fence ()
{
  return !tsan_counter && mutant != 101;
}
} // namespace orders

/// The failure order that the one-order compare-exchange implies.
inline mo_t
failure_order (mo_t order)
{
  return order == rl::mo_acq_rel   ? rl::mo_acquire
         : order == rl::mo_release ? rl::mo_relaxed
                                   : order;
}

/// The protocol steps whose execution the report counts, so that a passing
/// search can be shown to have reached them.
enum cover_e
{
  cover_tick,          ///< L3: a load pinned the word
  cover_count_live,    ///< L6: a load counted while its tick was in the word
  cover_untick,        ///< L7: a tick was taken back
  cover_settle_pay,    ///< L8: the tick was gone after counting, paid
  cover_count_and_pay, ///< L9: counted and paid in one read-modify-write
  cover_fast_swap,     ///< S1: a swap of a word without ticks
  cover_writer_pin,    ///< S2: a writer pinned a word with ticks
  cover_deposit,       ///< S3: units were deposited
  cover_topup,         ///< S5: the deposit was topped up
  cover_undo,          ///< S5: the own tick was swapped out, paid back
  cover_consumed,      ///< S4: the pinned swap succeeded
  cover_surplus,       ///< S6: a surplus came back with the drop
  cover_cas_swap,      ///< S10: a compare-exchange swapped
  cover_cas_withdraw,  ///< S10: a failed swap withdrew its deposit
  cover_holder_swap,   ///< S12: a holder compare-exchange swapped
  cover_holder_miss,   ///< S12: the holder did not match, tick taken back
  cover_hand_over,     ///< S7: a holder word was handed over
  cover_dispose,       ///< a block was disposed
  cover_s12_undo,      ///< S12: the own tick was swapped out, paid back
  cover_s12_topup,     ///< S12: the deposit was topped up
  cover_s11_retry,     ///< S11: the strong form found an equivalent value
  cover_cas_keep,      ///< S10: a failed swap kept its deposit
  cover_cas_topup,     ///< S10: ... and topped it up
  cover_count
};

inline char const *
cover_name (int c)
{
  static char const *const names[cover_count]
      = { "L3-tick",      "L6-count-live", "L7-untick",     "L8-settle-pay",
          "L9-count-pay", "S1-fast-swap",  "S2-writer-pin", "S3-deposit",
          "S5-topup",     "S5-undo",       "S4-consumed",   "S6-surplus",
          "S10-cas-swap", "S10-withdraw",  "S12-swap",      "S12-miss",
          "S7-hand-over", "dispose",       "S12-undo",      "S12-topup",
          "S11-retry",    "S10-keep",      "S10-keep-topup" };
  return names[c];
}

inline unsigned long long *
cover_counts ()
{
  static unsigned long long counts[cover_count];
  return counts;
}

inline void
cover (cover_e c)
{
  ++cover_counts ()[c];
}

// -- The word: {lo: id << 1 | holder, L: 16 bits, delta: 32 bits} --

struct word_ops_t
{
  static u64
  make (unsigned lo, unsigned ticks, u64 delta)
  {
    return u64 (lo) | (u64 (ticks) << 16) | (delta << 32);
  }

  static u64
  block_word (int id, u64 offset)
  {
    return make (unsigned (id) << 1, 0u, offset);
  }

  static u64
  holder_word (int id)
  {
    return make ((unsigned (id) << 1) | 1u, 0u, 0u);
  }

  static u64
  empty_word (u64 raw)
  {
    return make (0u, 0u, raw);
  }

  static unsigned
  lo_of (u64 w)
  {
    return unsigned (w & 0xFFFFu);
  }

  static unsigned
  ticks_of (u64 w)
  {
    return unsigned ((w >> 16) & 0xFFFFu);
  }

  static u64
  delta_of (u64 w)
  {
    return w >> 32;
  }

  static bool
  is_empty (u64 w)
  {
    return lo_of (w) == 0u;
  }

  static bool
  is_holder (u64 w)
  {
    return (lo_of (w) & 1u) != 0u;
  }

  static int
  block_of (u64 w)
  {
    return int (lo_of (w) >> 1);
  }

  static u64
  with_tick (u64 w)
  {
    return w + (u64 (1) << 16);
  }

  static u64
  without_tick (u64 w)
  {
    return w - (u64 (1) << 16);
  }

  static bool
  same_value (u64 a, u64 b)
  {
    return lo_of (a) == lo_of (b) && delta_of (a) == delta_of (b);
  }

  static bool
  fits (u64 offset)
  {
    return offset < window_limit;
  }
};

typedef word_ops_t word;

/// A smart pointer value: a block (0 = empty) and the stored pointer (an
/// offset from the anchor, or the raw pointer of an empty one).
struct sp_t
{
  int cb;
  u64 stored;
};

inline sp_t
make_sp (int cb, u64 stored)
{
  sp_t p;
  p.cb = cb;
  p.stored = stored;
  return p;
}

/// One control block (an object block or a holder) of the pool.
struct block_t
{
  rl::atomic<u64> z;         ///< the strong group {count, ext}
  rl::var<int> payload;      ///< the managed object (object blocks)
  rl::var<int> alive;        ///< 1 from creation to disposal
  rl::var<int> holder_owner; ///< holders: the block they refer to
  rl::var<int> holder_ptr;   ///< holders: the stored pointer
  rl::var<int> owns;         ///< holders: owns a reference on the owner
  rl::var<int> witness;      ///< ghost: written before a swap installs it
};

/// The expected payload of an object block.
inline int
payload_of (int id)
{
  return id * 10;
}

/// The pool of control blocks and the ghost state of one run.
struct pool_t
{
  block_t blocks[pool_size];
  int owners[pool_size];         ///< ghost: units that make a block's owners
  int created[pool_size];        ///< 1: a live block, 2: a holder
  int disposed[pool_size];       ///< number of disposals
  int holder_owner_g[pool_size]; ///< ghost copy of a holder's owner
  int next_holder;

  void
  reset ()
  {
    for (int i = 0; i != pool_size; ++i)
      {
        owners[i] = 0;
        created[i] = 0;
        disposed[i] = 0;
        holder_owner_g[i] = 0;
      }
    next_holder = first_holder;
  }
};

/**
 * @brief The split-count cell of the shared engine on Relacy atomics.
 * @tparam Reserve the reserve `k` of the policy (0 takes the top-up path).
 */
template <unsigned Reserve> class cell_t
{
public:
  /// The reserve of a successful compare-exchange: the same `k` (0 is the
  /// path of a compare-exchange without reserve).
  static unsigned const cas_reserve = Reserve;

  explicit cell_t (pool_t &pool) : pool_ (pool) {}

  /// Initializes the word (before the threads start).
  void
  init (u64 value)
  {
    word_.store (value, rl::mo_relaxed, $);
  }

  /// The current word, for the invariants and the final check.
  u64
  peek () const
  {
    return word_.load (rl::mo_relaxed, $);
  }

  // -- blocks and pointers (what `make_shared` and a copy do) --

  /// Creates an object block with @p count units and returns a pointer.
  sp_t
  create_object (int id, unsigned count)
  {
    block_t &b = pool_.blocks[id];
    b.z.store (count, rl::mo_relaxed, $);
    b.payload ($) = payload_of (id);
    b.alive ($) = 1;
    b.witness ($) = 0;
    pool_.created[id] = 1;
    pool_.owners[id] = int (count);
    return make_sp (id, 0u);
  }

  /// A copy of @p p (an alias with @p stored): one more strong unit.
  sp_t
  copy (sp_t p, u64 stored)
  {
    add (p.cb);
    ++pool_.owners[p.cb];
    return make_sp (p.cb, stored);
  }

  /// The user drops @p p: the unit is released.
  void
  drop_ptr (sp_t p)
  {
    if (p.cb == 0)
      return;
    --pool_.owners[p.cb];
    release_strong (p.cb);
  }

  /// The user reads the object @p p points to: no use after free.
  void
  use (sp_t p)
  {
    if (p.cb == 0)
      return;
    block_t &b = pool_.blocks[p.cb];
    int const value = b.payload ($);
    RL_ASSERT (b.alive ($) == 1);
    RL_ASSERT (value == payload_of (p.cb));
  }

  // -- the operations --

  sp_t
  load (mo_t order)
  {
    u64 const pinned = pin (orders::rmw (order));
    if (word::is_empty (pinned))
      return make_sp (0, word::delta_of (pinned));
    int const block = word::block_of (pinned);
    int owner = block;
    u64 stored = 0u;
    block_t &b = pool_.blocks[block];
    if (word::is_holder (pinned))
      {
        owner = b.holder_owner ($);
        stored = u64 (b.holder_ptr ($));
        RL_ASSERT (b.alive ($) == 1);
      }
    else
      {
        // The anchor: a read of the pinned block.
        int const anchor = b.payload ($);
        RL_ASSERT (b.alive ($) == 1);
        RL_ASSERT (anchor == payload_of (block));
        stored = word::delta_of (pinned);
      }
    count_and_settle (pinned, owner);
    ++pool_.owners[owner];
    return make_sp (owner, stored);
  }

  // The operations take @p desired like the by-value parameter of the real
  // class: the call owns it, and what it did not move into the word is
  // released when the call returns (a holder-shaped value, an unused one).

  void
  store (sp_t desired, mo_t order)
  {
    u64 const image = take_image (desired);
    swapped_t const swapped = swap_out (image, orders::rmw (order));
    gh_remove_old (swapped.old);
    drop_word_with_ext (swapped.old, swapped.surplus);
    drop_ptr (desired);
  }

  sp_t
  exchange (sp_t desired, mo_t order)
  {
    u64 const image = take_image (desired);
    swapped_t const swapped = swap_out (image, orders::rmw (order));
    withdraw (swapped.old, swapped.surplus);
    sp_t const result = hand_over (swapped.old);
    drop_ptr (desired);
    return result;
  }

  bool
  compare_exchange_strong (sp_t &expected, sp_t desired, mo_t success,
                           mo_t failure)
  {
    bool const swapped
        = compare_exchange (expected, desired, false, success, failure);
    drop_ptr (desired);
    return swapped;
  }

  bool
  compare_exchange_strong (sp_t &expected, sp_t desired, mo_t order)
  {
    return compare_exchange_strong (expected, desired, order,
                                    failure_order (order));
  }

  /// The weak form: one attempt; a failed attempt withdraws its whole
  /// deposit.
  bool
  compare_exchange_weak (sp_t &expected, sp_t desired, mo_t order)
  {
    bool const swapped = compare_exchange (expected, desired, true, order,
                                           failure_order (order));
    drop_ptr (desired);
    return swapped;
  }

private:
  struct swapped_t
  {
    u64 old;
    unsigned surplus;
  };

  // -- counters of a block (split_counter) --

  void
  add (int id)
  {
    pool_.blocks[id].z.fetch_add (1u, rl::mo_relaxed, $);
  }

  /// A drop of @p unit; true when the word became zero (acquire fence).
  bool
  drop (int id, u64 unit)
  {
    u64 const old = pool_.blocks[id].z.fetch_sub (unit, orders::drop (), $);
    if (old != unit)
      return false;
    if (orders::drop_fence ())
      rl::atomic_thread_fence (rl::mo_acquire, $);
    return true;
  }

  void
  release_strong (int id)
  {
    if (drop (id, 1u))
      finish (id);
  }

  void
  release_strong_with_ext (int id, unsigned n)
  {
    if (drop (id, u64 (1) + (u64 (n) << 32)))
      finish (id);
  }

  void
  settle_strong (int id)
  {
    if (drop (id, ext_unit))
      finish (id);
  }

  void
  settle_strong_n (int id, unsigned n)
  {
    if (drop (id, u64 (n) << 32))
      finish (id);
  }

  void
  transfer_ext (int id, unsigned n)
  {
    pool_.blocks[id].z.fetch_add (u64 (n) << 32, rl::mo_relaxed, $);
  }

  void
  untransfer_ext (int id, unsigned n)
  {
    u64 const before
        = pool_.blocks[id].z.fetch_sub (u64 (n) << 32, rl::mo_relaxed, $);
    RL_ASSERT (before != (u64 (n) << 32));
  }

  void
  take_and_settle_strong (int id)
  {
    pool_.blocks[id].z.fetch_add (u64 (1) - ext_unit, rl::mo_relaxed, $);
  }

  /// `finish_strong` and `dispose`: the counter reached zero.
  void
  finish (int id)
  {
    block_t &b = pool_.blocks[id];
    RL_ASSERT (b.alive ($) == 1); // no double disposal
    b.alive ($) = 0;
    ++pool_.disposed[id];
    cover (cover_dispose);
    if (pool_.created[id] == 2)
      {
        int const owns = b.owns ($);
        int const owner = b.holder_owner ($);
        b.holder_owner ($) = -1;
        b.holder_ptr ($) = -1;
        if (owns != 0)
          release_strong (owner);
      }
    else
      b.payload ($) = -1;
  }

  // -- the ghost state --

  int
  owner_of_word (u64 w) const
  {
    if (word::is_empty (w))
      return 0;
    int const block = word::block_of (w);
    return word::is_holder (w) ? pool_.holder_owner_g[block] : block;
  }

  void
  gh_remove_old (u64 old)
  {
    int const owner = owner_of_word (old);
    if (owner != 0)
      --pool_.owners[owner];
  }

  void
  gh_drop_image (u64 image)
  {
    int const owner = owner_of_word (image);
    if (owner != 0)
      --pool_.owners[owner];
  }

  /// The witness of the image a writer is about to swap in: a plain write
  /// before the (release) swap, so that a payer's read of it proves that the
  /// payer's decision synchronizes with the writer.
  void
  witness_write (u64 image)
  {
    pool_.blocks[word::block_of (image)].witness ($) = 1;
  }

  /// The decision of a payer: a read of the witness of the word it decided
  /// on. A race here means the decision was not an acquire read of the
  /// writer's swap.
  void
  witness_read (u64 decided)
  {
    int const w = pool_.blocks[word::block_of (decided)].witness ($);
    (void)w;
  }

  // -- the counter group of a word (deposit, withdraw, pay) --

  void
  deposit (u64 w, unsigned n)
  {
    if (n == 0u)
      return;
    cover (cover_deposit);
    int const block = word::block_of (w);
    if (word::is_holder (w))
      {
        transfer_ext (block, n);
        transfer_ext (pool_.blocks[block].holder_owner ($), n);
      }
    else
      transfer_ext (block, n);
  }

  void
  withdraw (u64 w, unsigned n)
  {
    if (n == 0u)
      return;
    int const block = word::block_of (w);
    if (word::is_holder (w))
      {
        untransfer_ext (pool_.blocks[block].holder_owner ($), n);
        untransfer_ext (block, n);
      }
    else
      untransfer_ext (block, n);
  }

  void
  pay (u64 w, unsigned n)
  {
    int const block = word::block_of (w);
    if (word::is_holder (w))
      {
        int const owner = pool_.blocks[block].holder_owner ($);
        if (n == 1u)
          settle_strong (owner);
        else
          settle_strong_n (owner, n);
        if (n == 1u)
          settle_strong (block);
        else
          settle_strong_n (block, n);
      }
    else if (n == 1u)
      settle_strong (block);
    else
      settle_strong_n (block, n);
  }

  void
  take_and_pay (u64 w, int owner)
  {
    int const block = word::block_of (w);
    if (word::is_holder (w))
      {
        take_and_settle_strong (owner);
        settle_strong (block);
      }
    else
      take_and_settle_strong (block);
  }

  void
  drop_word (u64 w)
  {
    if (word::is_empty (w))
      return;
    int const block = word::block_of (w);
    if (word::is_holder (w))
      {
        release_owner (block, 0u);
        release_strong (block);
      }
    else
      release_strong (block);
  }

  void
  release_owner (int holder, unsigned surplus)
  {
    block_t &h = pool_.blocks[holder];
    int const owner = h.holder_owner ($);
    h.owns ($) = 0; // disown
    if (surplus == 0u)
      release_strong (owner);
    else
      release_strong_with_ext (owner, surplus);
  }

  void
  drop_word_with_ext (u64 w, unsigned surplus)
  {
    if (surplus == 0u || word::is_empty (w))
      {
        drop_word (w);
        return;
      }
    cover (cover_surplus);
    int const block = word::block_of (w);
    if (word::is_holder (w))
      {
        release_owner (block, surplus);
        release_strong_with_ext (block, surplus);
      }
    else
      release_strong_with_ext (block, surplus);
  }

  void
  pay_back (u64 w, unsigned deposited)
  {
    pay (w, deposited + 1u);
  }

  sp_t
  hand_over (u64 old)
  {
    if (word::is_empty (old))
      return make_sp (0, word::delta_of (old));
    int const block = word::block_of (old);
    if (!word::is_holder (old))
      {
        block_t &b = pool_.blocks[block];
        int const anchor = b.payload ($);
        RL_ASSERT (b.alive ($) == 1);
        RL_ASSERT (anchor == payload_of (block));
        return make_sp (block, word::delta_of (old));
      }
    block_t &h = pool_.blocks[block];
    int const owner = h.holder_owner ($);
    int const stored = h.holder_ptr ($);
    RL_ASSERT (h.alive ($) == 1);
    cover (cover_hand_over);
    h.owns ($) = 0; // disown: the result takes the holder's reference
    sp_t const result = make_sp (owner, u64 (stored));
    release_strong (block);
    return result;
  }

  // -- the image of a pointer --

  /// A holder for an alias of @p owner (F1: one strong unit on the owner).
  int
  make_holder (int owner, u64 stored)
  {
    int const id = pool_.next_holder++;
    block_t &h = pool_.blocks[id];
    h.z.store (1u, rl::mo_relaxed, $);
    h.holder_owner ($) = owner;
    h.holder_ptr ($) = int (stored);
    h.owns ($) = 1;
    h.alive ($) = 1;
    h.witness ($) = 0;
    pool_.created[id] = 2;
    pool_.holder_owner_g[id] = owner;
    add (owner);
    ++pool_.owners[owner]; // the built image (d_img)
    return id;
  }

  u64
  take_image (sp_t &desired)
  {
    if (desired.cb == 0)
      return word::empty_word (desired.stored);
    if (word::fits (desired.stored))
      {
        u64 const image = word::block_word (desired.cb, desired.stored);
        desired.cb = 0;
        return image;
      }
    return word::holder_word (make_holder (desired.cb, desired.stored));
  }

  // -- the compare-and-swap of the word: returns the value found --
  //
  // The engine passes a failure order that is stronger than the success order
  // only in the mutants (a success weakened to `release` or `relaxed` next to
  // the `acquire` failure order of the swap). The standard allows it since
  // C++17, Relacy does not take it, so it is modelled as a failure order of
  // `relaxed` followed by an `acquire` fence: a relaxed read followed by an
  // acquire fence synchronizes like an acquire read of the same value.

  u64
  cas_found (u64 expected, u64 desired, mo_t success, mo_t failure)
  {
    bool const stronger_failure
        = (success == rl::mo_relaxed || success == rl::mo_release)
          && (failure == rl::mo_acquire || failure == rl::mo_seq_cst);
    u64 found = expected;
    bool const ok = word_.compare_exchange_strong (
        found, desired, success, stronger_failure ? rl::mo_relaxed : failure,
        $);
    if (!ok && stronger_failure)
      rl::atomic_thread_fence (rl::mo_acquire, $);
    return ok ? expected : found;
  }

  // -- L0 to L3 --

  u64
  pin (mo_t order)
  {
    u64 seen = word_.load (rl::mo_relaxed, $); // L0, the guess
    for (;;)
      {
        if (word::is_empty (seen))
          {
            u64 const found = cas_found (seen, seen, order, rl::mo_relaxed);
            if (found == seen)
              return seen;
            seen = found;
            continue;
          }
        u64 const found = cas_found (seen, word::with_tick (seen),
                                     orders::tick (order), rl::mo_relaxed);
        if (found == seen)
          {
            cover (cover_tick);
            return seen;
          }
        seen = found;
      }
  }

  // -- L5 to L9 --

  void
  count_and_settle (u64 pinned, int owner)
  {
    unsigned const tag = word::lo_of (pinned);
    u64 seen = word_.load (rl::mo_relaxed, $); // the guess
    for (;;)
      {
        if (word::lo_of (seen) == tag && word::ticks_of (seen) != 0u)
          {
            cover (cover_count_live);
            add (owner); // L6, the linearization point of a live load
            settle (pinned);
            return;
          }
        seen = word_.load (orders::decision (), $);
        if (word::lo_of (seen) == tag && word::ticks_of (seen) != 0u)
          continue;
        witness_read (seen);
        cover (cover_count_and_pay);
        take_and_pay (pinned, owner); // L9
        return;
      }
  }

  void
  settle (u64 pinned)
  {
    unsigned const tag = word::lo_of (pinned);
    u64 seen = word_.load (rl::mo_relaxed, $);
    for (;;)
      {
        if (word::lo_of (seen) == tag && word::ticks_of (seen) != 0u)
          {
            u64 const found
                = cas_found (seen, word::without_tick (seen),
                             orders::untick (), rl::mo_relaxed); // L7
            if (found == seen)
              {
                cover (cover_untick);
                return;
              }
            seen = found;
            continue;
          }
        seen = word_.load (orders::decision (), $);
        if (word::lo_of (seen) == tag && word::ticks_of (seen) != 0u)
          continue;
        witness_read (seen);
        cover (cover_settle_pay);
        pay (pinned, 1u); // L8
        return;
      }
  }

  // -- S1 to S5 --

  swapped_t
  swap_out (u64 image, mo_t order)
  {
    u64 seen = word_.load (rl::mo_relaxed, $);
    for (;;)
      {
        if (word::is_empty (seen) || word::ticks_of (seen) == 0u)
          {
            witness_write (image);
            u64 const found = cas_found (
                seen, image, orders::fast_swap (order), rl::mo_relaxed);
            if (found == seen)
              {
                cover (cover_fast_swap);
                swapped_t const result = { seen, 0u };
                return result;
              }
            seen = found;
            continue;
          }
        u64 const ticked
            = cas_found (seen, word::with_tick (seen),
                         orders::writer_pin (order), rl::mo_relaxed); // S2
        if (!(ticked == seen))
          {
            seen = ticked;
            continue;
          }
        cover (cover_writer_pin);
        u64 const pinned = seen;
        u64 current = word::with_tick (seen);
        unsigned moved = word::ticks_of (seen) + Reserve;
        deposit (current, moved); // S3
        for (;;)
          {
            witness_write (image);
            u64 const found
                = cas_found (current, image, orders::swap_success (order),
                             orders::swap_failure ()); // S4
            if (found == current)
              {
                cover (cover_consumed);
                swapped_t const result
                    = { current, moved - (word::ticks_of (current) - 1u) };
                return result;
              }
            if (word::lo_of (found) != word::lo_of (pinned)
                || word::ticks_of (found) == 0u)
              {
                witness_read (found);
                cover (cover_undo);
                pay_back (current, moved); // S5, the undo
                seen = found;
                break;
              }
            unsigned const need = word::ticks_of (found) - 1u;
            if (need > moved)
              {
                unsigned const total = need + Reserve;
                cover (cover_topup);
                deposit (found, total - moved);
                moved = total;
              }
            current = found;
          }
      }
  }

  // -- S8 to S11 --

  bool
  compare_exchange (sp_t &expected, sp_t &desired, bool weak, mo_t success,
                    mo_t failure)
  {
    u64 want = word::empty_word (expected.stored);
    if (expected.cb != 0)
      {
        if (!word::fits (expected.stored))
          return compare_exchange_holder (expected, desired, weak, success,
                                          failure);
        want = word::block_word (expected.cb, expected.stored);
      }
    bool built = false;
    u64 image = word::empty_word (0u);
    u64 seen = word_.load (rl::mo_relaxed, $);
    for (;;)
      {
        if (word::same_value (seen, want))
          {
            if (!built)
              {
                image = take_image (desired);
                built = true;
              }
            unsigned const k = cas_reserve;
            unsigned moved = cas_ticks (seen);
            if (moved != 0u)
              moved += k;
            deposit (seen, moved); // S9
            for (;;)
              {
                witness_write (image);
                u64 const found = cas_found (
                    seen, image, orders::cas_success (orders::rmw (success)),
                    rl::mo_relaxed); // S10
                if (found == seen)
                  {
                    cover (cover_cas_swap);
                    gh_remove_old (seen);
                    drop_word_with_ext (
                        seen, moved - cas_ticks (seen)
                                  + (mutant == 201 ? 1u : 0u)); // M201
                    return true;
                  }
                if (k != 0u && !weak && mutant != 204
                    && word::same_value (found, want))
                  {
                    unsigned const need = cas_ticks (found);
                    cover (cover_cas_keep);
                    if (need > moved)
                      {
                        cover (cover_cas_topup);
                        if (mutant != 202) // M202: no top-up
                          deposit (found, need + k - moved);
                        moved = need + k;
                      }
                    seen = found;
                    continue;
                  }
                cover (cover_cas_withdraw);
                withdraw (seen, moved);
                seen = found;
                break;
              }
            if (!weak)
              continue;
          }
        sp_t const current = load (failure); // S11
        if (!weak && equivalent (current, expected))
          {
            // The loaded value is a new owner; the strong form retries and
            // releases it, as `current` goes out of scope in the engine.
            cover (cover_s11_retry);
            drop_ptr (current);
            seen = word_.load (rl::mo_relaxed, $);
            continue;
          }
        if (built)
          {
            gh_drop_image (image);
            drop_word (image);
          }
        drop_ptr (expected);
        expected = current;
        return false;
      }
  }

  static unsigned
  cas_ticks (u64 w)
  {
    return word::is_empty (w) ? 0u : word::ticks_of (w);
  }

  static bool
  equivalent (sp_t a, sp_t b)
  {
    return a.cb == b.cb && a.stored == b.stored;
  }

  // -- S12 --

  bool
  compare_exchange_holder (sp_t &expected, sp_t &desired, bool weak,
                           mo_t success, mo_t failure)
  {
    bool built = false;
    u64 image = word::empty_word (0u);
    u64 seen = word_.load (rl::mo_relaxed, $);
    for (;;)
      {
        if (word::is_holder (seen))
          {
            u64 const found
                = cas_found (seen, word::with_tick (seen),
                             orders::writer_pin (orders::rmw (success)),
                             rl::mo_relaxed); // the pin
            if (!(found == seen))
              {
                seen = found;
                continue;
              }
            u64 const pinned = seen;
            block_t &holder = pool_.blocks[word::block_of (pinned)];
            int const holder_owner = holder.holder_owner ($);
            int const holder_ptr = holder.holder_ptr ($);
            RL_ASSERT (holder.alive ($) == 1);
            if (holder_owner == expected.cb
                && u64 (holder_ptr) == expected.stored)
              {
                if (!built)
                  {
                    image = take_image (desired);
                    built = true;
                  }
                unsigned const k = cas_reserve;
                u64 current = word::with_tick (pinned);
                unsigned moved = word::ticks_of (pinned);
                if (moved != 0u)
                  moved += k;
                deposit (current,
                         moved - (mutant == 203 && moved != 0u ? 1u : 0u));
                // M203: the holder deposit is short by one
                for (;;)
                  {
                    witness_write (image);
                    u64 const swapped = cas_found (
                        current, image,
                        orders::swap_success (
                            orders::cas_success (orders::rmw (success))),
                        orders::swap_failure ());
                    if (swapped == current)
                      {
                        cover (cover_holder_swap);
                        gh_remove_old (current);
                        drop_word_with_ext (
                            current, moved - (word::ticks_of (current) - 1u));
                        return true;
                      }
                    if (word::lo_of (swapped) != word::lo_of (pinned)
                        || word::ticks_of (swapped) == 0u)
                      {
                        witness_read (swapped);
                        cover (cover_s12_undo);
                        pay_back (current, moved);
                        seen = swapped;
                        break;
                      }
                    unsigned const need = word::ticks_of (swapped) - 1u;
                    if (need > moved)
                      {
                        cover (cover_s12_topup);
                        deposit (swapped, need + k - moved);
                        moved = need + k;
                      }
                    current = swapped;
                  }
                continue;
              }
            cover (cover_holder_miss);
            settle (pinned);
          }
        sp_t const current = load (failure);
        if (!weak && equivalent (current, expected))
          {
            cover (cover_s11_retry);
            drop_ptr (current);
            seen = word_.load (rl::mo_relaxed, $);
            continue;
          }
        if (built)
          {
            gh_drop_image (image);
            drop_word (image);
          }
        drop_ptr (expected);
        expected = current;
        return false;
      }
  }

  pool_t &pool_;
  rl::atomic<u64> word_;
};

/**
 * @brief What every model shares: the pool, the cell, the checks.
 * @tparam Reserve the reserve of the cell.
 */
template <unsigned Reserve> struct fixture_t
{
  pool_t pool;
  cell_t<Reserve> cell;

  fixture_t () : cell (pool) { pool.reset (); }

  /// Initializes the plain variables of every block (call from before()).
  void
  prepare ()
  {
    pool.reset ();
    for (int i = 0; i != pool_size; ++i)
      {
        block_t &b = pool.blocks[i];
        b.payload ($) = 0;
        b.alive ($) = 0;
        b.holder_owner ($) = 0;
        b.holder_ptr ($) = 0;
        b.owns ($) = 0;
        b.witness ($) = 0;
        b.z.store (0u, rl::mo_relaxed, $);
      }
  }

  /// The word of the slot with a fresh object block @p id holding one unit.
  void
  seed_block (int id)
  {
    sp_t p = cell.create_object (id, 1u);
    cell.init (word::block_word (id, 0u));
    (void)p;
  }

  /// The invariants that hold at every step (Relacy's `invariant`).
  void
  check_step ()
  {
    for (int i = 1; i != pool_size; ++i)
      {
        if (pool.created[i] == 0)
          continue;
        u64 const z = pool.blocks[i].z.load (rl::mo_relaxed, $);
        long const count
            = long (std::int32_t (std::uint32_t (z & 0xFFFFFFFFu)));
        long const ext = long (std::int32_t (std::uint32_t (z >> 32)));
        RL_ASSERT (count >= 0); // NONNEG
        RL_ASSERT (ext >= 0);
        if (pool.disposed[i] != 0)
          {
            RL_ASSERT (pool.disposed[i] == 1);
            RL_ASSERT (pool.owners[i] == 0 || pool.created[i] == 2);
          }
        else if (pool.created[i] == 1)
          {
            RL_ASSERT (count + ext >= long (pool.owners[i])); // G1
          }
      }
  }

  /// The balance at the end (I4): nothing in flight, every count exact.
  void
  check_end ()
  {
    u64 const w = cell.peek ();
    if (!word::is_empty (w))
      RL_ASSERT (word::ticks_of (w) == 0u);
    for (int i = 1; i != pool_size; ++i)
      {
        if (pool.created[i] == 0)
          continue;
        u64 const z = pool.blocks[i].z.load (rl::mo_relaxed, $);
        long const count
            = long (std::int32_t (std::uint32_t (z & 0xFFFFFFFFu)));
        long const ext = long (std::int32_t (std::uint32_t (z >> 32)));
        RL_ASSERT (ext == 0);
        if (pool.created[i] == 1)
          {
            RL_ASSERT (count == long (pool.owners[i])); // G2 at quiescence
            RL_ASSERT ((count == 0) == (pool.disposed[i] == 1));
            RL_ASSERT (pool.disposed[i] <= 1);
          }
        else
          {
            bool const in_word = !word::is_empty (w) && word::is_holder (w)
                                 && word::block_of (w) == i;
            RL_ASSERT (count == (in_word ? 1 : 0));
            RL_ASSERT (pool.disposed[i] == (in_word ? 0 : 1));
          }
      }
  }
};

/**
 * @brief A model: Relacy test suite with a fixture and the common checks.
 * @tparam Derived the model; it provides `setup ()` and `thread (unsigned)`.
 * @tparam Threads the number of threads.
 * @tparam Reserve the reserve of the cell.
 * @tparam Expected the result the search must end with (the default: none
 * of the checks fails).
 */
template <typename Derived, int Threads, unsigned Reserve,
          rl::test_result_e Expected = rl::test_result_success>
struct model_t : rl::test_suite<Derived, Threads, Expected>
{
  fixture_t<Reserve> f;

  void
  before ()
  {
    f.prepare ();
    static_cast<Derived *> (this)->setup ();
  }

  void
  invariant ()
  {
    f.check_step ();
  }

  void
  after ()
  {
    f.check_end ();
  }
};

/// How a model is searched: the scheduler, the number of iterations (the
/// cap of an exhaustive search) and the context bound of the bounded search.
struct search_t
{
  rl::scheduler_type_e scheduler;
  rl::iteration_t iterations;
  unsigned bound;
};

inline char const *
scheduler_name (rl::scheduler_type_e scheduler)
{
  return scheduler == rl::random_scheduler_type               ? "random"
         : scheduler == rl::fair_context_bound_scheduler_type ? "bound"
                                                              : "full";
}

/// A step that a model exists to reach: when a search of the model ends
/// without having reached it, the run fails (a model whose branch is
/// unreachable proves nothing about it).
struct cover_requirement_t
{
  char const *model;
  int cover;
};

inline cover_requirement_t const *
cover_requirements (std::size_t &count)
{
  static cover_requirement_t const table[]
      = { { "block.cas_load_store/k2", cover_cas_keep },
          { "block.cas_load_store/k2", cover_cas_topup },
          { "block.cas_load/k2", cover_cas_keep },
          { "block.cas_weak_load_store/k2", cover_cas_swap },
          { "block.cas_weak_load_store/k2", cover_cas_withdraw } };
  count = sizeof (table) / sizeof (table[0]);
  return table;
}

/// Prints the requirements of @p name that the last search did not meet;
/// true when all were met.
inline bool
cover_requirements_met (char const *name)
{
  std::size_t count = 0;
  cover_requirement_t const *const table = cover_requirements (count);
  bool met = true;
  for (std::size_t i = 0; i != count; ++i)
    if (std::string (name) == table[i].model
        && cover_counts ()[table[i].cover] == 0)
      {
        std::cout << "COVERAGE FAILED (model " << name << " never reached "
                  << cover_name (table[i].cover) << ")" << std::endl;
        met = false;
      }
  return met;
}

/// Runs @p Test with @p search; prints the report lines.
template <typename Test>
inline bool
run_model (char const *name, search_t const &search)
{
  rl::test_params params;
  params.search_type = search.scheduler;
  params.iteration_count = search.iterations;
  if (search.bound != 0)
    params.context_bound = search.bound;
  params.execution_depth_limit = 20000;
  for (int c = 0; c != cover_count; ++c)
    cover_counts ()[c] = 0;
  std::cout << "== " << name << " search=" << scheduler_name (search.scheduler)
            << " iterations=" << search.iterations
            << " bound=" << params.context_bound << " mutant=" << mutant
            << " counter=" << (tsan_counter ? "tsan" : "default") << std::endl;
  bool ok = rl::simulate<Test> (params);
  std::cout << "reached:";
  for (int c = 0; c != cover_count; ++c)
    if (cover_counts ()[c] != 0)
      std::cout << ' ' << cover_name (c) << '=' << cover_counts ()[c];
  std::cout << std::endl;
  if (ok && !cover_requirements_met (name))
    ok = false;
  std::cout << (ok ? "RESULT pass " : "RESULT FAIL ") << name << std::endl;
  return ok;
}

typedef bool (*runner_t) (char const *, search_t const &);

/// One model of a program and its default search.
struct entry_t
{
  char const *name;
  int reserve; ///< shown as "/k<reserve>"; -1: not a property of the model
  runner_t runner;
  search_t search;
};

/// The search for random runs: @p runs iterations.
inline search_t
random_search (rl::iteration_t runs)
{
  search_t const s = { rl::random_scheduler_type, runs, 0u };
  return s;
}

/// The bounded exhaustive search: every schedule with at most @p bound
/// preemptions, up to @p cap iterations.
inline search_t
bound_search (unsigned bound, rl::iteration_t cap)
{
  search_t const s = { rl::fair_context_bound_scheduler_type, cap, bound };
  return s;
}

/// The full search, up to @p cap iterations.
inline search_t
full_search (rl::iteration_t cap)
{
  search_t const s = { rl::fair_full_search_scheduler_type, cap, 0u };
  return s;
}

/**
 * @brief The `main` of a model program.
 * @details `prog [--only text] [--search random|bound|full] [--iterations N]
 * [--bound B] [--scale S]`: runs the entries whose name contains the text,
 * with the default search of the entry unless overridden; `--scale` multiplies
 * the iterations of the random entries. Returns 0 when every model ended as
 * expected.
 */
inline int
main_driver (int argc, char **argv, entry_t const *entries, int count)
{
  char const *only = 0;
  char const *search_override = 0;
  long long iterations = 0;
  long long bound = 0;
  long long scale = 1;
  for (int i = 1; i + 1 < argc; i += 2)
    {
      std::string const key (argv[i]);
      if (key == "--only")
        only = argv[i + 1];
      else if (key == "--search")
        search_override = argv[i + 1];
      else if (key == "--iterations")
        iterations = std::atoll (argv[i + 1]);
      else if (key == "--bound")
        bound = std::atoll (argv[i + 1]);
      else if (key == "--scale")
        scale = std::atoll (argv[i + 1]);
    }
  bool ok = true;
  int ran = 0;
  for (int i = 0; i != count; ++i)
    {
      std::string name (entries[i].name);
      if (entries[i].reserve >= 0)
        name += "/k" + std::to_string (entries[i].reserve);
      if (only != 0 && name.find (only) == std::string::npos)
        continue;
      search_t search = entries[i].search;
      if (search.scheduler == rl::random_scheduler_type)
        search.iterations *= rl::iteration_t (scale);
      if (search_override != 0)
        {
          std::string const kind (search_override);
          search.scheduler = kind == "random" ? rl::random_scheduler_type
                             : kind == "bound"
                                 ? rl::fair_context_bound_scheduler_type
                                 : rl::fair_full_search_scheduler_type;
        }
      if (iterations > 0)
        search.iterations = rl::iteration_t (iterations);
      if (bound > 0)
        search.bound = unsigned (bound);
      ++ran;
      ok = entries[i].runner (name.c_str (), search) && ok;
    }
  if (ran == 0)
    {
      std::cout << "no model matches" << std::endl;
      return 2;
    }
  return ok ? 0 : 1;
}
} // namespace split_count_relacy

#endif // !LUMEX_TESTS_CORE_ATOMIC_SMART_PTR_SPLIT_COUNT_RELACY_SPLIT_COUNT_RELACY_MODEL_HPP
