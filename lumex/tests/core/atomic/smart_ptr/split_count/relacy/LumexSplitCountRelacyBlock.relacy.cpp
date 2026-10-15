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
 * @file LumexSplitCountRelacyBlock.relacy.cpp
 * @brief Relacy models of the block-word paths of the split-count engine:
 * publication and loading (L3, L5 to L9), the writer's pin, deposit, swap and
 * payment (S1 to S6), the exchange hand-over (S7) and the compare-exchange
 * (S8 to S11), each with the reserve `k` as a template parameter.
 * @details Every model is a small program of two or three threads over one
 * cell and a pool of control blocks (`LumexSplitCountRelacyModel.hpp`). The
 * checks are the ones the file text of the model lists: a read of an object
 * after its disposal or without a happens-before edge from its creation is a
 * failure (no use after free), a block is disposed once, `count + ext >=
 * owners` at every step (G1), `ext >= 0`, and at the end every word has no
 * tick, every block has `ext == 0` and `count` equal to its owners (I4); a
 * payment whose decision was not an acquire read of the swap that made it
 * necessary is a data race on a ghost variable. The search of each model is
 * the scheduler and the iteration count in `main`; the context bound search is
 * exhaustive up to the bound, the random search is evidence and not proof.
 *
 * Run: `LumexSplitCountRelacyBlock [--only text] [--search random|bound|full]
 * [--iterations N] [--bound B] [--scale S]` (see `main_driver`); without
 * arguments every model runs with its own search.
 */
#include "LumexSplitCountRelacyModel.hpp"

using namespace split_count_relacy;

namespace
{
/// The names of the models; they appear in the report and in the CTest names.
char const *const model_publish_load = "block.publish_load";
char const *const model_load_store = "block.load_store";
char const *const model_two_loaders_store = "block.two_loaders_store";
char const *const model_two_writers_load = "block.two_writers_load";
char const *const model_exchange_load = "block.exchange_load";
char const *const model_publish_exchange = "block.publish_exchange";
char const *const model_cas_load_store = "block.cas_load_store";
char const *const model_load_reset = "block.load_reset";
char const *const model_exchange_reset = "block.exchange_reset";
char const *const model_cas_restore = "block.cas_restore";

/// A publisher stores a new block, a reader loads and reads it (L3 and S1:
/// the object written before the release swap is visible after the tick).
template <unsigned K> struct publish_load_t : model_t<publish_load_t<K>, 2, K>
{
  void
  setup ()
  {
    this->f.seed_block (1);
  }

  void
  thread (unsigned index)
  {
    if (index == 0)
      {
        sp_t const b = this->f.cell.create_object (2, 1);
        this->f.cell.store (b, rl::mo_release);
      }
    else
      {
        sp_t const p = this->f.cell.load (rl::mo_acquire);
        this->f.cell.use (p);
        this->f.cell.drop_ptr (p);
      }
  }
};

/// A loader against a store that replaces (and destroys) the loaded block
/// (L5 to L9 against S1 to S6; the last owner disposes).
template <unsigned K> struct load_store_t : model_t<load_store_t<K>, 2, K>
{
  void
  setup ()
  {
    this->f.seed_block (1);
  }

  void
  thread (unsigned index)
  {
    if (index == 0)
      {
        sp_t const p = this->f.cell.load (rl::mo_acquire);
        this->f.cell.use (p);
        this->f.cell.drop_ptr (p);
      }
    else
      {
        sp_t const b = this->f.cell.create_object (2, 1);
        this->f.cell.store (b, rl::mo_release);
      }
  }
};

/// Two loaders against a store: the writer sees more than one tick (top-up of
/// the deposit, a loader that un-ticks a tick of the other).
template <unsigned K>
struct two_loaders_store_t : model_t<two_loaders_store_t<K>, 3, K>
{
  void
  setup ()
  {
    this->f.seed_block (1);
  }

  void
  thread (unsigned index)
  {
    if (index != 2)
      {
        sp_t const p = this->f.cell.load (rl::mo_acquire);
        this->f.cell.use (p);
        this->f.cell.drop_ptr (p);
      }
    else
      {
        sp_t const b = this->f.cell.create_object (2, 1);
        this->f.cell.store (b, rl::mo_release);
      }
  }
};

/// A loader and two writers: the second writer's pin is swapped out by the
/// first (S5, the undo that pays `moved + 1` in one read-modify-write).
template <unsigned K>
struct two_writers_load_t : model_t<two_writers_load_t<K>, 3, K>
{
  void
  setup ()
  {
    this->f.seed_block (1);
  }

  void
  thread (unsigned index)
  {
    if (index == 0)
      {
        sp_t const p = this->f.cell.load (rl::mo_acquire);
        this->f.cell.use (p);
        this->f.cell.drop_ptr (p);
      }
    else
      {
        sp_t const b = this->f.cell.create_object (int (index) + 1, 1);
        this->f.cell.store (b, rl::mo_release);
      }
  }
};

/// An exchange hands the old value over (S7) while a loader reads it.
template <unsigned K>
struct exchange_load_t : model_t<exchange_load_t<K>, 2, K>
{
  void
  setup ()
  {
    this->f.seed_block (1);
  }

  void
  thread (unsigned index)
  {
    if (index == 0)
      {
        sp_t const p = this->f.cell.load (rl::mo_acquire);
        this->f.cell.use (p);
        this->f.cell.drop_ptr (p);
      }
    else
      {
        sp_t const b = this->f.cell.create_object (2, 1);
        sp_t const old = this->f.cell.exchange (b, rl::mo_acq_rel);
        this->f.cell.use (old);
        this->f.cell.drop_ptr (old);
      }
  }
};

/// One thread publishes a block, another exchanges it out and reads it (the
/// acquire of the swap, S4, makes the published object visible for the
/// exchange; a third owner keeps the slot busy with a load).
template <unsigned K>
struct publish_exchange_t : model_t<publish_exchange_t<K>, 3, K>
{
  void
  setup ()
  {
    this->f.seed_block (1);
  }

  void
  thread (unsigned index)
  {
    if (index == 0)
      {
        sp_t const b = this->f.cell.create_object (2, 1);
        this->f.cell.store (b, rl::mo_release);
      }
    else if (index == 1)
      {
        sp_t const c = this->f.cell.create_object (3, 1);
        sp_t const old = this->f.cell.exchange (c, rl::mo_acq_rel);
        this->f.cell.use (old);
        this->f.cell.drop_ptr (old);
      }
    else
      {
        sp_t const p = this->f.cell.load (rl::mo_acquire);
        this->f.cell.use (p);
        this->f.cell.drop_ptr (p);
      }
  }
};

/// A compare-exchange (S8 to S11, no pin) against a loader and a store.
template <unsigned K>
struct cas_load_store_t : model_t<cas_load_store_t<K>, 3, K>
{
  void
  setup ()
  {
    this->f.seed_block (1);
  }

  void
  thread (unsigned index)
  {
    if (index == 0)
      {
        sp_t const p = this->f.cell.load (rl::mo_acquire);
        this->f.cell.use (p);
        this->f.cell.drop_ptr (p);
      }
    else if (index == 1)
      {
        sp_t expected = this->f.cell.load (rl::mo_acquire);
        sp_t const c = this->f.cell.create_object (3, 1);
        this->f.cell.compare_exchange_strong (expected, c, rl::mo_acq_rel);
        this->f.cell.use (expected);
        this->f.cell.drop_ptr (expected);
      }
    else
      {
        sp_t const b = this->f.cell.create_object (2, 1);
        this->f.cell.store (b, rl::mo_release);
      }
  }
};

/// A loader against a store of an EMPTY pointer (a reset): the swapped-in word
/// is empty, so the ghost witness of block 0 guards the decision.
template <unsigned K> struct load_reset_t : model_t<load_reset_t<K>, 2, K>
{
  void
  setup ()
  {
    this->f.seed_block (1);
  }

  void
  thread (unsigned index)
  {
    if (index == 0)
      {
        sp_t const p = this->f.cell.load (rl::mo_acquire);
        this->f.cell.use (p);
        this->f.cell.drop_ptr (p);
      }
    else
      this->f.cell.store (make_sp (0, 0u), rl::mo_release);
  }
};

/// A loader against an exchange with an empty pointer: the old value is
/// handed over (S7) and read while the loader reads it.
template <unsigned K>
struct exchange_reset_t : model_t<exchange_reset_t<K>, 2, K>
{
  void
  setup ()
  {
    this->f.seed_block (1);
  }

  void
  thread (unsigned index)
  {
    if (index == 0)
      {
        sp_t const p = this->f.cell.load (rl::mo_acquire);
        this->f.cell.use (p);
        this->f.cell.drop_ptr (p);
      }
    else
      {
        sp_t const old
            = this->f.cell.exchange (make_sp (0, 0u), rl::mo_acq_rel);
        this->f.cell.use (old);
        this->f.cell.drop_ptr (old);
      }
  }
};

/// The strong compare-exchange retry (S11): a thread replaces the value and
/// puts the same value back, so that the failure load of the compare-exchange
/// can find a value equivalent to `expected`.
template <unsigned K> struct cas_restore_t : model_t<cas_restore_t<K>, 2, K>
{
  sp_t kept;

  void
  setup ()
  {
    this->f.seed_block (1);
    kept = this->f.cell.copy (make_sp (1, 0u), 0u);
  }

  void
  thread (unsigned index)
  {
    if (index == 0)
      {
        sp_t expected = kept;
        sp_t const c = this->f.cell.create_object (3, 1);
        this->f.cell.compare_exchange_strong (expected, c, rl::mo_acq_rel);
        this->f.cell.use (expected);
        this->f.cell.drop_ptr (expected);
      }
    else
      {
        sp_t const b = this->f.cell.create_object (2, 1);
        sp_t const old = this->f.cell.exchange (b, rl::mo_acq_rel);
        this->f.cell.store (old, rl::mo_release);
      }
  }
};
} // namespace

int
main (int argc, char **argv)
{
  // Two threads: the whole schedule tree (full search). Three threads: every
  // schedule with at most four preemptions, then random schedules.
  static entry_t const entries[] = {
    { model_publish_load, 0, &run_model<publish_load_t<0>>,
      full_search (50000000) },
    { model_publish_load, 2, &run_model<publish_load_t<2>>,
      full_search (50000000) },
    { model_load_store, 0, &run_model<load_store_t<0>>,
      full_search (50000000) },
    { model_load_store, 2, &run_model<load_store_t<2>>,
      full_search (50000000) },
    { model_exchange_load, 0, &run_model<exchange_load_t<0>>,
      full_search (50000000) },
    { model_exchange_load, 2, &run_model<exchange_load_t<2>>,
      full_search (50000000) },
    { model_two_loaders_store, 0, &run_model<two_loaders_store_t<0>>,
      bound_search (4, 50000000) },
    { model_two_loaders_store, 0, &run_model<two_loaders_store_t<0>>,
      random_search (2000000) },
    { model_two_loaders_store, 2, &run_model<two_loaders_store_t<2>>,
      bound_search (4, 50000000) },
    { model_two_loaders_store, 2, &run_model<two_loaders_store_t<2>>,
      random_search (2000000) },
    { model_two_writers_load, 0, &run_model<two_writers_load_t<0>>,
      bound_search (4, 50000000) },
    { model_two_writers_load, 0, &run_model<two_writers_load_t<0>>,
      random_search (2000000) },
    { model_two_writers_load, 2, &run_model<two_writers_load_t<2>>,
      bound_search (4, 50000000) },
    { model_two_writers_load, 2, &run_model<two_writers_load_t<2>>,
      random_search (2000000) },
    { model_publish_exchange, 0, &run_model<publish_exchange_t<0>>,
      bound_search (4, 50000000) },
    { model_publish_exchange, 0, &run_model<publish_exchange_t<0>>,
      random_search (2000000) },
    { model_publish_exchange, 2, &run_model<publish_exchange_t<2>>,
      bound_search (4, 50000000) },
    { model_publish_exchange, 2, &run_model<publish_exchange_t<2>>,
      random_search (2000000) },
    { model_cas_load_store, 0, &run_model<cas_load_store_t<0>>,
      bound_search (4, 50000000) },
    { model_cas_load_store, 0, &run_model<cas_load_store_t<0>>,
      random_search (2000000) },
    { model_cas_load_store, 2, &run_model<cas_load_store_t<2>>,
      bound_search (4, 50000000) },
    { model_cas_load_store, 2, &run_model<cas_load_store_t<2>>,
      random_search (2000000) },
    { model_load_reset, 0, &run_model<load_reset_t<0>>,
      full_search (50000000) },
    { model_load_reset, 2, &run_model<load_reset_t<2>>,
      full_search (50000000) },
    { model_exchange_reset, 0, &run_model<exchange_reset_t<0>>,
      full_search (50000000) },
    { model_exchange_reset, 2, &run_model<exchange_reset_t<2>>,
      full_search (50000000) },
    { model_cas_restore, 0, &run_model<cas_restore_t<0>>,
      bound_search (6, 50000000) },
    { model_cas_restore, 0, &run_model<cas_restore_t<0>>,
      random_search (2000000) },
    { model_cas_restore, 2, &run_model<cas_restore_t<2>>,
      bound_search (6, 50000000) },
    { model_cas_restore, 2, &run_model<cas_restore_t<2>>,
      random_search (2000000) },
    { model_two_writers_load, 8, &run_model<two_writers_load_t<8>>,
      random_search (2000000) },
    { model_publish_exchange, 8, &run_model<publish_exchange_t<8>>,
      random_search (2000000) },
  };
  return main_driver (argc, argv, entries,
                      int (sizeof (entries) / sizeof (entries[0])));
}
