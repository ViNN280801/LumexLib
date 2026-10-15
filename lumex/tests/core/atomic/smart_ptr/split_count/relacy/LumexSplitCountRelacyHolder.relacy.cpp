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
 * @file LumexSplitCountRelacyHolder.relacy.cpp
 * @brief Relacy models of the holder word of the split-count engine: an alias
 * whose offset does not fit the word is stored through a holder block, the
 * pins of a holder are mirrored in the `ext` of its owner (F1, HG), a loader
 * of a holder word returns the owner, the replaced holder is destroyed by
 * the last thread to pay for it, and a compare-exchange with a holder-shaped
 * `expected` pins the word (S12).
 * @details The checks and the search are those of
 * `LumexSplitCountRelacyBlock.relacy.cpp`. What these models add is the plain
 * data of the holder: its owner and pointer are read by a loader that owns no
 * count on the holder, and the destruction of the holder must happen after
 * those reads. The edges are the un-tick (`release`, L7) of the loader and a
 * later `acquire` of the writer: its pin (S2), the failure value of its swap
 * (S4) or the success of its swap (S4), followed by the drop of the word's
 * unit. Weakening the un-tick (M10) is a data race. The success order of the
 * swap (M9) is the only direct edge from the SECOND un-tick of a loader that
 * loads twice (the `two_loads_in_one_thread` models): the acquire fence of the
 * disposing drop of the shipped counter masks a weakened success order there,
 * and the ThreadSanitizer counter (an `acq_rel` drop and no fence) does not,
 * so M9 is a data race with that counter.
 */
#include "LumexSplitCountRelacyModel.hpp"

using namespace split_count_relacy;

namespace
{
char const *const model_holder_load_store = "holder.load_store";
char const *const model_holder_exchange_load = "holder.exchange_load";
char const *const model_holder_publish_load = "holder.publish_load";
char const *const model_holder_cas_load = "holder.cas_load";
char const *const model_holder_two_loaders = "holder.two_loaders_store";
char const *const model_holder_cas_miss = "holder.cas_miss_load";
char const *const model_holder_cas_load_store = "holder.cas_load_store";
char const *const model_holder_three_loaders = "holder.three_loaders_store";
char const *const model_holder_two_loads_store
    = "holder.two_loads_in_one_thread_store";
char const *const model_holder_two_loads_exchange
    = "holder.two_loads_in_one_thread_exchange";
char const *const model_holder_load_reset = "holder.load_reset";
char const *const model_holder_exchange_reset = "holder.exchange_reset";

/// Puts an alias of a fresh owner (block 1) that does not fit the word into
/// the slot: the slot owns a holder (block `first_holder`), the holder one
/// unit of the owner. Returns the alias, which the caller owns.
template <typename Model>
sp_t
seed_holder_keep (Model &m)
{
  sp_t const p = m.f.cell.create_object (1, 1);
  sp_t const alias = m.f.cell.copy (p, far_offset);
  sp_t const kept = m.f.cell.copy (p, far_offset);
  m.f.cell.init (word::empty_word (0u));
  m.f.cell.store (alias, rl::mo_relaxed);
  m.f.cell.drop_ptr (p);
  return kept;
}

/// The same, with the alias dropped: the owner is kept alive by the holder.
template <typename Model>
void
seed_holder (Model &m)
{
  m.f.cell.drop_ptr (seed_holder_keep (m));
}

/// A loader against a store that replaces the holder (and destroys it).
template <unsigned K> struct load_store_t : model_t<load_store_t<K>, 2, K>
{
  void
  setup ()
  {
    seed_holder (*this);
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

/// Two loaders (ticks of one holder, the mirror twice) against a store.
template <unsigned K>
struct two_loaders_store_t : model_t<two_loaders_store_t<K>, 3, K>
{
  void
  setup ()
  {
    seed_holder (*this);
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

/// An exchange takes the holder's reference over (`disown`, S7) while a
/// loader reads the holder.
template <unsigned K>
struct exchange_load_t : model_t<exchange_load_t<K>, 2, K>
{
  void
  setup ()
  {
    seed_holder (*this);
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

/// A thread builds a holder (an alias that does not fit) and stores it; a
/// loader reads the holder it finds (publication of the holder's fields, L3
/// and the writer's pin S2).
template <unsigned K> struct publish_load_t : model_t<publish_load_t<K>, 3, K>
{
  void
  setup ()
  {
    seed_holder (*this);
  }

  void
  thread (unsigned index)
  {
    if (index == 0)
      {
        sp_t const p = this->f.cell.create_object (2, 1);
        sp_t const alias = this->f.cell.copy (p, far_offset);
        this->f.cell.store (alias, rl::mo_release);
        this->f.cell.drop_ptr (p);
      }
    else if (index == 1)
      {
        sp_t const p = this->f.cell.load (rl::mo_acquire);
        this->f.cell.use (p);
        this->f.cell.drop_ptr (p);
      }
    else
      {
        sp_t const b = this->f.cell.create_object (3, 1);
        this->f.cell.store (b, rl::mo_release);
      }
  }
};

/// S12: a compare-exchange whose `expected` is the alias in the slot
/// (holder-shaped), against a loader.
template <unsigned K> struct cas_load_t : model_t<cas_load_t<K>, 2, K>
{
  sp_t alias;

  void
  setup ()
  {
    alias = seed_holder_keep (*this);
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
        // The alias that was put into the slot (owned by this thread).
        sp_t expected = alias;
        sp_t const c = this->f.cell.create_object (3, 1);
        this->f.cell.compare_exchange_strong (expected, c, rl::mo_acq_rel);
        this->f.cell.drop_ptr (expected);
      }
  }
};

/// S12 against a store: the compare-exchange on a holder word fails, tops up
/// or undoes (the pinned writer's paths on a holder, with the mirror).
template <unsigned K>
struct cas_load_store_t : model_t<cas_load_store_t<K>, 3, K>
{
  sp_t alias;

  void
  setup ()
  {
    alias = seed_holder_keep (*this);
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
        sp_t expected = alias;
        sp_t const c = this->f.cell.create_object (3, 1);
        this->f.cell.compare_exchange_strong (expected, c, rl::mo_acq_rel);
        this->f.cell.drop_ptr (expected);
      }
    else
      {
        sp_t const b = this->f.cell.create_object (2, 1);
        this->f.cell.store (b, rl::mo_release);
      }
  }
};

/// Three loaders against a store: ticks of one holder arrive and leave while
/// the writer is pinned (the release sequence of the un-ticks).
template <unsigned K>
struct three_loaders_store_t : model_t<three_loaders_store_t<K>, 4, K>
{
  void
  setup ()
  {
    seed_holder (*this);
  }

  void
  thread (unsigned index)
  {
    if (index != 3)
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

/// S12 with an `expected` that is a holder-shaped alias of ANOTHER owner: the
/// compare-exchange pins the holder word, finds that it does not match, takes
/// its tick back and fails with the loaded value (against a loader).
template <unsigned K> struct cas_miss_t : model_t<cas_miss_t<K>, 2, K>
{
  sp_t other;

  void
  setup ()
  {
    seed_holder (*this);
    sp_t const p = this->f.cell.create_object (4, 1);
    other = this->f.cell.copy (p, far_offset);
    this->f.cell.drop_ptr (p);
    // `other` keeps block 4 alive; the slot holds the holder of block 1.
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
        sp_t expected = other;
        sp_t const c = this->f.cell.create_object (3, 1);
        bool const swapped = this->f.cell.compare_exchange_strong (
            expected, c, rl::mo_acq_rel);
        RL_ASSERT (!swapped);
        this->f.cell.use (expected);
        this->f.cell.drop_ptr (expected);
      }
  }
};

/// TWO loads of the holder word in ONE thread against a store: the writer's
/// pinned swap can fail on the first un-tick and then succeed reading directly
/// from the SECOND un-tick of the same loader, so the success order of the
/// swap (S4) is the only direct edge from that second un-tick to the writer's
/// disposal of the holder. With the shipped counter (release drop, acquire
/// fence on zero) the fence of the disposing drop masks a weakened success
/// order; with the ThreadSanitizer counter (acq_rel drop, no fence) it does
/// not (M9).
template <unsigned K>
struct two_loads_store_t : model_t<two_loads_store_t<K>, 2, K>
{
  void
  setup ()
  {
    seed_holder (*this);
  }

  void
  thread (unsigned index)
  {
    if (index == 0)
      {
        sp_t const p1 = this->f.cell.load (rl::mo_acquire);
        sp_t const p2 = this->f.cell.load (rl::mo_acquire);
        this->f.cell.use (p1);
        this->f.cell.use (p2);
        this->f.cell.drop_ptr (p1);
        this->f.cell.drop_ptr (p2);
      }
    else
      {
        sp_t const b = this->f.cell.create_object (2, 1);
        this->f.cell.store (b, rl::mo_release);
      }
  }
};

/// The same with an exchange, which hands the holder's owner over.
template <unsigned K>
struct two_loads_exchange_t : model_t<two_loads_exchange_t<K>, 2, K>
{
  void
  setup ()
  {
    seed_holder (*this);
  }

  void
  thread (unsigned index)
  {
    if (index == 0)
      {
        sp_t const p1 = this->f.cell.load (rl::mo_acquire);
        sp_t const p2 = this->f.cell.load (rl::mo_acquire);
        this->f.cell.use (p1);
        this->f.cell.use (p2);
        this->f.cell.drop_ptr (p1);
        this->f.cell.drop_ptr (p2);
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

/// A loader of the holder word against a store of an empty pointer.
template <unsigned K> struct load_reset_t : model_t<load_reset_t<K>, 2, K>
{
  void
  setup ()
  {
    seed_holder (*this);
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

/// A loader of the holder word against an exchange with an empty pointer.
template <unsigned K>
struct exchange_reset_t : model_t<exchange_reset_t<K>, 2, K>
{
  void
  setup ()
  {
    seed_holder (*this);
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
} // namespace

int
main (int argc, char **argv)
{
  static entry_t const entries[] = {
    { model_holder_load_store, 0, &run_model<load_store_t<0>>,
      full_search (50000000) },
    { model_holder_load_store, 2, &run_model<load_store_t<2>>,
      full_search (50000000) },
    { model_holder_exchange_load, 0, &run_model<exchange_load_t<0>>,
      full_search (50000000) },
    { model_holder_exchange_load, 2, &run_model<exchange_load_t<2>>,
      full_search (50000000) },
    { model_holder_cas_load, 0, &run_model<cas_load_t<0>>,
      full_search (50000000) },
    { model_holder_cas_load, 2, &run_model<cas_load_t<2>>,
      full_search (50000000) },
    { model_holder_cas_miss, 0, &run_model<cas_miss_t<0>>,
      bound_search (8, 50000000) },
    { model_holder_cas_miss, 0, &run_model<cas_miss_t<0>>,
      random_search (2000000) },
    { model_holder_cas_miss, 2, &run_model<cas_miss_t<2>>,
      bound_search (8, 50000000) },
    { model_holder_cas_miss, 2, &run_model<cas_miss_t<2>>,
      random_search (2000000) },
    { model_holder_two_loads_store, 0, &run_model<two_loads_store_t<0>>,
      bound_search (8, 50000000) },
    { model_holder_two_loads_store, 0, &run_model<two_loads_store_t<0>>,
      random_search (2000000) },
    { model_holder_two_loads_store, 2, &run_model<two_loads_store_t<2>>,
      bound_search (8, 50000000) },
    { model_holder_two_loads_store, 2, &run_model<two_loads_store_t<2>>,
      random_search (2000000) },
    { model_holder_two_loads_exchange, 0, &run_model<two_loads_exchange_t<0>>,
      bound_search (8, 50000000) },
    { model_holder_two_loads_exchange, 0, &run_model<two_loads_exchange_t<0>>,
      random_search (2000000) },
    { model_holder_two_loads_exchange, 2, &run_model<two_loads_exchange_t<2>>,
      bound_search (8, 50000000) },
    { model_holder_two_loads_exchange, 2, &run_model<two_loads_exchange_t<2>>,
      random_search (2000000) },
    { model_holder_load_reset, 0, &run_model<load_reset_t<0>>,
      full_search (50000000) },
    { model_holder_load_reset, 2, &run_model<load_reset_t<2>>,
      full_search (50000000) },
    { model_holder_exchange_reset, 0, &run_model<exchange_reset_t<0>>,
      full_search (50000000) },
    { model_holder_exchange_reset, 2, &run_model<exchange_reset_t<2>>,
      full_search (50000000) },
    { model_holder_two_loaders, 0, &run_model<two_loaders_store_t<0>>,
      bound_search (4, 50000000) },
    { model_holder_two_loaders, 0, &run_model<two_loaders_store_t<0>>,
      random_search (2000000) },
    { model_holder_two_loaders, 2, &run_model<two_loaders_store_t<2>>,
      bound_search (4, 50000000) },
    { model_holder_two_loaders, 2, &run_model<two_loaders_store_t<2>>,
      random_search (2000000) },
    { model_holder_publish_load, 0, &run_model<publish_load_t<0>>,
      bound_search (4, 50000000) },
    { model_holder_publish_load, 0, &run_model<publish_load_t<0>>,
      random_search (2000000) },
    { model_holder_publish_load, 2, &run_model<publish_load_t<2>>,
      bound_search (4, 50000000) },
    { model_holder_publish_load, 2, &run_model<publish_load_t<2>>,
      random_search (2000000) },
    { model_holder_cas_load_store, 0, &run_model<cas_load_store_t<0>>,
      bound_search (4, 50000000) },
    { model_holder_cas_load_store, 0, &run_model<cas_load_store_t<0>>,
      random_search (2000000) },
    { model_holder_cas_load_store, 2, &run_model<cas_load_store_t<2>>,
      bound_search (4, 50000000) },
    { model_holder_cas_load_store, 2, &run_model<cas_load_store_t<2>>,
      random_search (2000000) },
    { model_holder_three_loaders, 0, &run_model<three_loaders_store_t<0>>,
      bound_search (2, 50000000) },
    { model_holder_three_loaders, 0, &run_model<three_loaders_store_t<0>>,
      random_search (2000000) },
    { model_holder_three_loaders, 2, &run_model<three_loaders_store_t<2>>,
      bound_search (2, 50000000) },
    { model_holder_three_loaders, 2, &run_model<three_loaders_store_t<2>>,
      random_search (2000000) },
  };
  return main_driver (argc, argv, entries,
                      int (sizeof (entries) / sizeof (entries[0])));
}
