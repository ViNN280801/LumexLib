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
 * @file LumexSplitCountRelacyLitmus.relacy.cpp
 * @brief Store-buffering and independent-reads litmus tests for the
 * `seq_cst` requirement of the split-count engine (mutation M25,
 * `rmw_order` ignoring `seq_cst`).
 * @details The engine makes every successful read-modify-write of the word at
 * least `acq_rel` and `seq_cst` when the caller asks for it. The tests ask
 * whether the `seq_cst` floor is observable in the C++11 memory model:
 *
 * - the controls use plain atomics, so that the harness is shown to see the
 *   weak outcome when the orders are weak (found) and not when they are
 *   `seq_cst` (not found), for store buffering and for independent reads;
 * - the engine tests use two cells and the real `store` and `load` of the
 *   transcription (and a cell with a plain flag), in the same shapes.
 *
 * The argument for the engine tests to pass with and without `seq_cst`:
 * every operation that decides a user-visible result (`load`, `store`,
 * `exchange`, `compare_exchange_*`) ends in an `acq_rel` read-modify-write of
 * the word, and every modification of the word is at least `release`, so the
 * release sequences are unbroken and the modification order of the word is
 * contained in happens-before. The plain loads of the protocol (the
 * decisions, the guesses) only choose between paying a unit and taking a tick
 * back; they never decide a result. That chain closes the cycle of the weak
 * outcome in both shapes. `wait` and `notify` are not modelled. M25 is
 * therefore reported as a mutation that no model can tell from the faithful
 * engine (see the README of `lumex/core/atomic`). The searches differ by test:
 * the plain controls and the exhaustive ones are noted at the entries.
 */
#include "LumexSplitCountRelacyModel.hpp"

using namespace split_count_relacy;

namespace
{
char const *const model_sb_plain_weak = "litmus.sb_plain_weak_is_found";
char const *const model_sb_plain_seq_cst = "litmus.sb_plain_seq_cst";
char const *const model_sb_cells = "litmus.sb_cells";
char const *const model_sb_cells_seq_cst = "litmus.sb_cells_seq_cst";
char const *const model_sb_cell_flag = "litmus.sb_cell_and_flag_seq_cst";
char const *const model_iriw_cells = "litmus.iriw_cells_seq_cst";
char const *const model_iriw_plain_weak = "litmus.iriw_plain_weak_is_found";
char const *const model_iriw_plain_seq_cst = "litmus.iriw_plain_seq_cst";

/// Control: store buffering on plain atomics with release stores and acquire
/// loads. The weak outcome (both loads read the initial value) is allowed, so
/// the search must find it: the assertion fails and the test passes.
struct sb_plain_weak_t
    : rl::test_suite<sb_plain_weak_t, 2, rl::test_result_user_assert_failed>
{
  rl::atomic<int> x;
  rl::atomic<int> y;
  int r0;
  int r1;

  void
  before ()
  {
    x.store (0, rl::mo_relaxed, $);
    y.store (0, rl::mo_relaxed, $);
    r0 = r1 = -1;
  }

  void
  thread (unsigned index)
  {
    if (index == 0)
      {
        x.store (1, rl::mo_release, $);
        r0 = y.load (rl::mo_acquire, $);
      }
    else
      {
        y.store (1, rl::mo_release, $);
        r1 = x.load (rl::mo_acquire, $);
      }
  }

  void
  after ()
  {
    RL_ASSERT (!(r0 == 0 && r1 == 0));
  }
};

/// Control: the same with `seq_cst`; the weak outcome is forbidden.
struct sb_plain_seq_cst_t : rl::test_suite<sb_plain_seq_cst_t, 2>
{
  rl::atomic<int> x;
  rl::atomic<int> y;
  int r0;
  int r1;

  void
  before ()
  {
    x.store (0, rl::mo_relaxed, $);
    y.store (0, rl::mo_relaxed, $);
    r0 = r1 = -1;
  }

  void
  thread (unsigned index)
  {
    if (index == 0)
      {
        x.store (1, rl::mo_seq_cst, $);
        r0 = y.load (rl::mo_seq_cst, $);
      }
    else
      {
        y.store (1, rl::mo_seq_cst, $);
        r1 = x.load (rl::mo_seq_cst, $);
      }
  }

  void
  after ()
  {
    RL_ASSERT (!(r0 == 0 && r1 == 0));
  }
};

/// The engine in the shape of store buffering: thread 0 stores into cell X
/// and loads cell Y, thread 1 stores into Y and loads X. Both cells start
/// empty; the weak outcome is that both loads see the empty value. @p Order
/// is the order the callers request.
template <int Order> struct sb_cells_t : rl::test_suite<sb_cells_t<Order>, 2>
{
  pool_t pool;
  cell_t<0> x;
  cell_t<0> y;
  sp_t r0;
  sp_t r1;

  sb_cells_t () : x (pool), y (pool) { pool.reset (); }

  static mo_t
  order ()
  {
    return Order == 0 ? rl::mo_acq_rel : rl::mo_seq_cst;
  }

  void
  before ()
  {
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
    x.init (word::empty_word (0u));
    y.init (word::empty_word (0u));
    r0 = make_sp (-1, 0u);
    r1 = make_sp (-1, 0u);
  }

  void
  thread (unsigned index)
  {
    if (index == 0)
      {
        sp_t const a = x.create_object (1, 1);
        x.store (a, order ());
        r0 = y.load (order ());
      }
    else
      {
        sp_t const b = y.create_object (2, 1);
        y.store (b, order ());
        r1 = x.load (order ());
      }
  }

  void
  after ()
  {
    RL_ASSERT (!(r0.cb == 0 && r1.cb == 0));
    x.drop_ptr (r0);
    y.drop_ptr (r1);
  }
};

/// A cell and a plain `seq_cst` flag: thread 0 stores into the cell and
/// reads the flag, thread 1 sets the flag and loads the cell.
struct sb_cell_flag_t : rl::test_suite<sb_cell_flag_t, 2>
{
  pool_t pool;
  cell_t<0> x;
  rl::atomic<int> flag;
  int r0;
  sp_t r1;

  sb_cell_flag_t () : x (pool) { pool.reset (); }

  void
  before ()
  {
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
    x.init (word::empty_word (0u));
    flag.store (0, rl::mo_relaxed, $);
    r0 = -1;
    r1 = make_sp (-1, 0u);
  }

  void
  thread (unsigned index)
  {
    if (index == 0)
      {
        sp_t const a = x.create_object (1, 1);
        x.store (a, rl::mo_seq_cst);
        r0 = flag.load (rl::mo_seq_cst, $);
      }
    else
      {
        flag.store (1, rl::mo_seq_cst, $);
        r1 = x.load (rl::mo_seq_cst);
      }
  }

  void
  after ()
  {
    RL_ASSERT (!(r0 == 0 && r1.cb == 0));
    x.drop_ptr (r1);
  }
};

/// Independent reads of independent writes: two writers store into cell X
/// and cell Y; reader 0 loads X then Y, reader 1 loads Y then X. The weak
/// outcome is that the readers see the two writes in opposite orders.
struct iriw_cells_t : rl::test_suite<iriw_cells_t, 4>
{
  pool_t pool;
  cell_t<0> x;
  cell_t<0> y;
  sp_t r[4];

  iriw_cells_t () : x (pool), y (pool) { pool.reset (); }

  void
  before ()
  {
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
    x.init (word::empty_word (0u));
    y.init (word::empty_word (0u));
    for (int i = 0; i != 4; ++i)
      r[i] = make_sp (-1, 0u);
  }

  void
  thread (unsigned index)
  {
    if (index == 0)
      x.store (x.create_object (1, 1), rl::mo_seq_cst);
    else if (index == 1)
      y.store (y.create_object (2, 1), rl::mo_seq_cst);
    else if (index == 2)
      {
        r[0] = x.load (rl::mo_seq_cst);
        r[1] = y.load (rl::mo_seq_cst);
      }
    else
      {
        r[2] = y.load (rl::mo_seq_cst);
        r[3] = x.load (rl::mo_seq_cst);
      }
  }

  void
  after ()
  {
    // Reader 0 saw X set and Y empty while reader 1 saw Y set and X empty.
    RL_ASSERT (
        !(r[0].cb != 0 && r[1].cb == 0 && r[2].cb != 0 && r[3].cb == 0));
    x.drop_ptr (r[0]);
    y.drop_ptr (r[1]);
    y.drop_ptr (r[2]);
    x.drop_ptr (r[3]);
  }
};

/// Control: independent reads of independent writes on plain atomics. With
/// `Order` weak (release stores, acquire loads) the readers may see the two
/// writes in opposite orders; with `seq_cst` they may not.
template <int Order, rl::test_result_e Expected>
struct iriw_plain_t
    : rl::test_suite<iriw_plain_t<Order, Expected>, 4, Expected>
{
  rl::atomic<int> x;
  rl::atomic<int> y;
  int r[4];

  static mo_t
  store_order ()
  {
    return Order == 0 ? rl::mo_release : rl::mo_seq_cst;
  }

  static mo_t
  load_order ()
  {
    return Order == 0 ? rl::mo_acquire : rl::mo_seq_cst;
  }

  void
  before ()
  {
    x.store (0, rl::mo_relaxed, $);
    y.store (0, rl::mo_relaxed, $);
    for (int i = 0; i != 4; ++i)
      r[i] = -1;
  }

  void
  thread (unsigned index)
  {
    if (index == 0)
      x.store (1, store_order (), $);
    else if (index == 1)
      y.store (1, store_order (), $);
    else if (index == 2)
      {
        r[0] = x.load (load_order (), $);
        r[1] = y.load (load_order (), $);
      }
    else
      {
        r[2] = y.load (load_order (), $);
        r[3] = x.load (load_order (), $);
      }
  }

  void
  after ()
  {
    RL_ASSERT (!(r[0] == 1 && r[1] == 0 && r[2] == 1 && r[3] == 0));
  }
};
} // namespace

int
main (int argc, char **argv)
{
  static entry_t const entries[] = {
    { model_sb_plain_weak, -1, &run_model<sb_plain_weak_t>,
      full_search (1000000) },
    { model_sb_plain_seq_cst, -1, &run_model<sb_plain_seq_cst_t>,
      full_search (1000000) },
    { model_sb_cells, -1, &run_model<sb_cells_t<0>>,
      bound_search (4, 50000000) },
    { model_sb_cells_seq_cst, -1, &run_model<sb_cells_t<1>>,
      bound_search (4, 50000000) },
    { model_sb_cell_flag, -1, &run_model<sb_cell_flag_t>,
      bound_search (4, 50000000) },
    { model_iriw_cells, -1, &run_model<iriw_cells_t>,
      bound_search (2, 50000000) },
    { model_iriw_cells, -1, &run_model<iriw_cells_t>, random_search (200000) },
    { model_iriw_plain_weak, -1,
      &run_model<iriw_plain_t<0, rl::test_result_user_assert_failed>>,
      random_search (200000) },
    { model_iriw_plain_seq_cst, -1,
      &run_model<iriw_plain_t<1, rl::test_result_success>>,
      random_search (200000) },
  };
  return main_driver (argc, argv, entries,
                      int (sizeof (entries) / sizeof (entries[0])));
}
