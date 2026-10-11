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
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

// The scenario table of the simulated-scheduler explorer
// (LumexSplitCountSim.hpp). Data only: the blocks, the cell, the threads with
// their operations, the outcomes the correct engine may produce, the policies
// and the coverage flag. The names of the scenarios and of the policies are
// constants of this table; the tests refer to them by the enumerators and the
// name accessors.
//
// Notation (the oracle note of package V1):
// - `L (n)`: n times `[ld 0, rs 0]`; `W [v1, v2, ...]`: `st 0, st 1, ...` with
//   `held[i]` prefilled with one reference to `vi` (`rec (slot)` instead of a
//   value: `rc i slot` first, the block is created again in that slot);
// - `K (X)`: `[rs 0]` with `held[0]` the keeper of `X`;
// - `C (e -> d)`: `[cas 0 1, rs 0]`; `X (d)`: `[xc 0 1, rs 1]`.
// Blocks: `A` (slot 0), `B` (slot 1), `D` (slot 2); `A'` is `A` created again
// in slot 0; `.m` is the member alias of a block (offset 12), `~f` an alias
// that the engine stores through a holder.
#ifndef LUMEX_TESTS_CORE_ATOMIC_SMART_PTR_SPLIT_COUNT_SIM_SCENARIOS_HPP
#define LUMEX_TESTS_CORE_ATOMIC_SMART_PTR_SPLIT_COUNT_SIM_SCENARIOS_HPP

#include <cstddef>
#include <cstring>
#include <set>
#include <string>
#include <vector>

#include "lumex/tests/core/atomic/smart_ptr/split_count/LumexSplitCountTestSupport.hpp"

#if LUMEX_ATOMIC_SMART_PTR_HAS_SPLIT_COUNT

namespace split_sim
{
/// The scenarios; the names are in the table.
enum scenario_id_t
{
  sc_S01,
  sc_S02,
  sc_S03,
  sc_S04,
  sc_S05,
  sc_S06,
  sc_S07,
  sc_S08,
  sc_S09,
  sc_S10,
  sc_S11,
  sc_S12,
  sc_S13,
  sc_S14,
  sc_S15,
  sc_S16,
  sc_H1,
  sc_H2,
  sc_H3,
  sc_H4,
  sc_H5,
  sc_W1,
  sc_W2,
  sc_W3,
  sc_LK,
  sc_EX,
  sc_RI,
  sc_RIC,
  sc_UW,
  sc_UW2,
  sc_GF,
  sc_TU,
  sc_TL,
  sc_SC,
  sc_SCL,
  sc_TB,
  sc_HC,
  sc_HM,
  sc_XB,
  sc_E1,
  sc_WX,
  sc_WC,
  sc_WCW,
  sc_SW,
  sc_count
};

/// The policies: reserve and tick limit.
enum policy_id_t
{
  pol_R8,
  pol_R0,
  pol_L2,
  pol_L1,
  pol_count
};

inline char const *
policy_name (policy_id_t policy)
{
  static char const *const names[pol_count] = { "R8", "R0", "L2", "L1" };
  return names[policy];
}

/// The operations of a scenario thread.
enum op_kind_t
{
  op_ld,  ///< held[a] = cell.load ()
  op_st,  ///< cell.store (move (held[a]))
  op_xc,  ///< held[b] = cell.exchange (move (held[a]))
  op_cas, ///< ok = cell.compare_exchange_strong (held[a], move (held[b]))
  op_rs,  ///< held[a].reset ()
  op_lk,  ///< held[b] = weak[a].lock ()
  op_ex,  ///< expired = weak[a].expired ()
  op_rc,  ///< held[a] = a new block in slot b (enabled while the slot is free)
  op_wld, ///< weak[a] = weak_cell.load ()
  op_wst, ///< weak_cell.store (move (weak[a]))
  op_wrs, ///< weak[a].reset ()
  op_casw, ///< ok = cell.compare_exchange_weak (held[a], move (held[b]))
  op_wxc,  ///< weak[b] = weak_cell.exchange (move (weak[a]))
  op_wcas, ///< weak_cell.compare_exchange_strong (weak[a], move (weak[b]))
  op_wcasw ///< weak_cell.compare_exchange_weak (weak[a], move (weak[b]))
};

struct op_t
{
  op_kind_t kind;
  int a;
  int b;
};

/// The flavors of a value: the stored pointer relative to the object.
enum flavor_t
{
  fl_plain = 0,   ///< the object itself (offset 0)
  fl_member = 1,  ///< the member alias (offset 12)
  fl_far = 2,     ///< an alias 2^41 bytes away: stored through a holder
  fl_recreate = 3 ///< only in `rec (slot)`: the block is made again
};

/// A value to put into a smart pointer: the slot of its block (-1: empty)
/// and the flavor.
struct vspec_t
{
  int slot; ///< -1: no value at all; -2: an empty pointer with a stored
            ///< pointer
  int flavor;
  int alt; ///< 1: the second far alias (+8 bytes); 2: the stored empty pointer
};

inline vspec_t
vs (int slot, int flavor = fl_plain, int alt = 0)
{
  vspec_t const v = { slot, flavor, alt };
  return v;
}

/// An empty smart pointer that stores a pointer (an alias of no owner).
inline vspec_t
raw_empty ()
{
  return vs (-2, fl_plain, 2);
}

inline vspec_t
rec (int slot)
{
  return vs (slot, fl_recreate);
}

enum engine_kind_t
{
  engine_shared,
  engine_weak
};

struct thread_def_t
{
  int nops;
  op_t ops[8];
  vspec_t held[4];
  vspec_t weak[4];
};

struct scenario_def_t
{
  scenario_id_t id;
  char const *name;
  engine_kind_t engine;
  vspec_t cell;
  unsigned blocks;     ///< mask of the slots created at setup
  unsigned main_keeps; ///< mask of the slots whose creating reference stays
  int nthreads;
  thread_def_t threads[6];
  unsigned policies; ///< mask of `1u << policy_id_t`
  std::vector<std::string> rows;
  int outcomes;     ///< the number of expected outcomes of the table
  bool cov;         ///< COVERAGE is asserted at the soak bound
  int quick_bound;  ///< the preemption bound of the quick run (0: by threads)
  bool bound1_pass; ///< the soak run adds a complete bound-1 pass first
  std::set<std::string> expected;
};

namespace detail
{
inline vspec_t
none_value ()
{
  return vs (-1);
}

inline thread_def_t &
new_thread (scenario_def_t &s)
{
  thread_def_t &t = s.threads[s.nthreads++];
  t.nops = 0;
  for (int i = 0; i < 4; ++i)
    {
      t.held[i] = none_value ();
      t.weak[i] = none_value ();
    }
  return t;
}

inline void
add_op (thread_def_t &t, op_kind_t kind, int a = 0, int b = 0)
{
  op_t const op = { kind, a, b };
  t.ops[t.nops++] = op;
}

/// `L (n)`.
inline void
th_L (scenario_def_t &s, int n)
{
  thread_def_t &t = new_thread (s);
  for (int i = 0; i < n; ++i)
    {
      add_op (t, op_ld, 0);
      add_op (t, op_rs, 0);
    }
}

/// `W [v...]` (up to three values).
inline void
th_W (scenario_def_t &s, vspec_t v0, vspec_t v1 = vs (-1),
      vspec_t v2 = vs (-1))
{
  thread_def_t &t = new_thread (s);
  vspec_t const values[3] = { v0, v1, v2 };
  for (int i = 0; i < 3; ++i)
    {
      if (values[i].slot == -1)
        break;
      if (values[i].flavor == fl_recreate)
        add_op (t, op_rc, i, values[i].slot);
      else
        t.held[i] = values[i];
      add_op (t, op_st, i);
    }
}

/// `K (slot)`.
inline void
th_K (scenario_def_t &s, int slot)
{
  thread_def_t &t = new_thread (s);
  t.held[0] = vs (slot);
  add_op (t, op_rs, 0);
}

/// `C (e -> d)`.
inline void
th_C (scenario_def_t &s, vspec_t e, vspec_t d)
{
  thread_def_t &t = new_thread (s);
  t.held[0] = e;
  t.held[1] = d;
  add_op (t, op_cas, 0, 1);
  add_op (t, op_rs, 0);
}

/// `X (d)`.
inline void
th_X (scenario_def_t &s, vspec_t d)
{
  thread_def_t &t = new_thread (s);
  t.held[0] = d;
  add_op (t, op_xc, 0, 1);
  add_op (t, op_rs, 1);
}

/// `Cw (e -> d)`: the weak form, `[casw 0 1, rs 0]`.
inline void
th_Cw (scenario_def_t &s, vspec_t e, vspec_t d)
{
  thread_def_t &t = new_thread (s);
  t.held[0] = e;
  t.held[1] = d;
  add_op (t, op_casw, 0, 1);
  add_op (t, op_rs, 0);
}

/// The weak exchange: `[wxc 0 1, wrs 1]` with `weak[0]` naming @p d.
inline void
th_WX (scenario_def_t &s, vspec_t d)
{
  thread_def_t &t = new_thread (s);
  t.weak[0] = d;
  add_op (t, op_wxc, 0, 1);
  add_op (t, op_wrs, 1);
}

/// The weak compare-exchange: `[wcas 0 1, wrs 0]` (`[wcasw 0 1, wrs 0]` when
/// @p weak_form).
inline void
th_WC (scenario_def_t &s, vspec_t e, vspec_t d, bool weak_form)
{
  thread_def_t &t = new_thread (s);
  t.weak[0] = e;
  t.weak[1] = d;
  add_op (t, weak_form ? op_wcasw : op_wcas, 0, 1);
  add_op (t, op_wrs, 0);
}

/// The weak loader of W1: `[wld 0, lk 0 0, rs 0, wrs 0]`.
inline void
th_WLK (scenario_def_t &s)
{
  thread_def_t &t = new_thread (s);
  add_op (t, op_wld, 0);
  add_op (t, op_lk, 0, 0);
  add_op (t, op_rs, 0);
  add_op (t, op_wrs, 0);
}

/// The weak loader of W2 and W3: `[wld 0, wrs 0]`.
inline void
th_WL (scenario_def_t &s)
{
  thread_def_t &t = new_thread (s);
  add_op (t, op_wld, 0);
  add_op (t, op_wrs, 0);
}

/// The weak writer: `[wst 0]` with `weak[0]` naming @p v.
inline void
th_WW (scenario_def_t &s, vspec_t v)
{
  thread_def_t &t = new_thread (s);
  t.weak[0] = v;
  add_op (t, op_wst, 0);
}

/// The `lock` thread of LK: `weak[0]` names slot 0, `[lk 0 0, rs 0]`.
inline void
th_lock (scenario_def_t &s, int slot)
{
  thread_def_t &t = new_thread (s);
  t.weak[0] = vs (slot);
  add_op (t, op_lk, 0, 0);
  add_op (t, op_rs, 0);
}

/// The `expired` thread of EX.
inline void
th_expired (scenario_def_t &s, int slot)
{
  thread_def_t &t = new_thread (s);
  t.weak[0] = vs (slot);
  add_op (t, op_ex, 0);
}

/// The outcomes of one row of the mini language:
/// `<thread>:<result>,<result>|<thread>:<result>|final=<value>` where a result
/// or the final value is a label or a set `{a,b}` of labels.
inline void
expand_row (std::string const &row, std::set<std::string> &out)
{
  // Pieces: literal text and alternative sets, in order.
  std::vector<std::vector<std::string>> pieces;
  std::string literal;
  std::size_t i = 0;
  while (i < row.size ())
    {
      if (row[i] == '{')
        {
          if (!literal.empty ())
            {
              pieces.push_back (std::vector<std::string> (1, literal));
              literal.clear ();
            }
          std::vector<std::string> alternatives;
          std::string current;
          ++i;
          while (row[i] != '}')
            {
              if (row[i] == ',')
                {
                  alternatives.push_back (current);
                  current.clear ();
                }
              else
                current += row[i];
              ++i;
            }
          alternatives.push_back (current);
          pieces.push_back (alternatives);
          ++i;
        }
      else
        {
          literal += row[i];
          ++i;
        }
    }
  if (!literal.empty ())
    pieces.push_back (std::vector<std::string> (1, literal));
  std::vector<std::string> results (1, std::string ());
  for (std::size_t p = 0; p < pieces.size (); ++p)
    {
      std::vector<std::string> next;
      for (std::size_t r = 0; r < results.size (); ++r)
        for (std::size_t a = 0; a < pieces[p].size (); ++a)
          next.push_back (results[r] + pieces[p][a]);
      results.swap (next);
    }
  for (std::size_t r = 0; r < results.size (); ++r)
    out.insert (results[r]);
}

inline scenario_def_t &
begin (std::vector<scenario_def_t> &table, scenario_id_t id, char const *name,
       engine_kind_t engine, vspec_t cell, unsigned blocks)
{
  table.push_back (scenario_def_t ());
  scenario_def_t &s = table.back ();
  s.id = id;
  s.name = name;
  s.engine = engine;
  s.cell = cell;
  s.blocks = blocks;
  s.main_keeps = blocks;
  s.nthreads = 0;
  s.policies = (1u << pol_R8) | (1u << pol_R0);
  s.outcomes = 0;
  s.cov = true;
  s.quick_bound = 0;
  s.bound1_pass = false;
  return s;
}

inline void
expect (scenario_def_t &s, int outcomes, char const *row1,
        char const *row2 = nullptr, char const *row3 = nullptr)
{
  s.outcomes = outcomes;
  s.rows.push_back (row1);
  if (row2 != nullptr)
    s.rows.push_back (row2);
  if (row3 != nullptr)
    s.rows.push_back (row3);
  for (std::size_t i = 0; i < s.rows.size (); ++i)
    expand_row (s.rows[i], s.expected);
}

/// The slots of the keeper threads leave the creating reference to the thread.
inline void
keepers_in_threads (scenario_def_t &s)
{
  for (int i = 0; i < s.nthreads; ++i)
    {
      thread_def_t const &t = s.threads[i];
      if (t.nops == 1 && t.ops[0].kind == op_rs && t.held[0].slot >= 0
          && t.held[0].flavor == fl_plain)
        s.main_keeps &= ~(1u << t.held[0].slot);
    }
}

inline std::vector<scenario_def_t>
build_table ()
{
  std::vector<scenario_def_t> t;
  t.reserve (64);
  vspec_t const A = vs (0);
  vspec_t const B = vs (1);
  vspec_t const D = vs (2);
  vspec_t const Am = vs (0, fl_member);
  vspec_t const Af = vs (0, fl_far);
  vspec_t const Bf = vs (1, fl_far);
  unsigned const blockA = 1u;
  unsigned const blockAB = 3u;
  unsigned const blockABD = 7u;
  scenario_def_t *s = nullptr;

  // -- The 16 scenarios of the model (shared engine, the cell holds A) --
  s = &begin (t, sc_S01, "S01", engine_shared, A, blockAB);
  th_L (*s, 1);
  th_W (*s, B, A);
  expect (*s, 2, "0:{A,B}|final=A");

  s = &begin (t, sc_S02, "S02", engine_shared, A, blockAB);
  th_L (*s, 1);
  th_L (*s, 1);
  th_W (*s, B, A);
  expect (*s, 4, "0:{A,B}|1:{A,B}|final=A");

  s = &begin (t, sc_S03, "S03", engine_shared, A, blockAB);
  th_L (*s, 2);
  th_W (*s, B, A, B);
  expect (*s, 4, "0:{A,B},{A,B}|final=B");

  s = &begin (t, sc_S04, "S04", engine_shared, A, blockAB);
  th_L (*s, 1);
  th_L (*s, 1);
  th_W (*s, B);
  th_W (*s, A);
  expect (*s, 8, "0:{A,B}|1:{A,B}|final={A,B}");

  s = &begin (t, sc_S05, "S05", engine_shared, A, blockAB);
  th_L (*s, 1);
  th_W (*s, B);
  th_K (*s, 0);
  keepers_in_threads (*s);
  expect (*s, 2, "0:{A,B}|final=B");

  s = &begin (t, sc_S06, "S06", engine_shared, A, blockAB);
  th_L (*s, 1);
  th_L (*s, 1);
  th_W (*s, B, A);
  th_K (*s, 0);
  th_K (*s, 1);
  keepers_in_threads (*s);
  expect (*s, 4, "0:{A,B}|1:{A,B}|final=A");

  s = &begin (t, sc_S07, "S07", engine_shared, A, blockAB);
  th_L (*s, 2);
  th_W (*s, B, A, B);
  th_K (*s, 0);
  keepers_in_threads (*s);
  expect (*s, 4, "0:{A,B},{A,B}|final=B");

  s = &begin (t, sc_S08, "S08", engine_shared, A, blockAB);
  th_L (*s, 1);
  th_L (*s, 1);
  th_W (*s, B);
  th_W (*s, A);
  th_K (*s, 0);
  th_K (*s, 1);
  keepers_in_threads (*s);
  s->cov = false;
  s->bound1_pass = true;
  expect (*s, 8, "0:{A,B}|1:{A,B}|final={A,B}");

  s = &begin (t, sc_S09, "S09", engine_shared, A, blockAB);
  th_L (*s, 1);
  th_W (*s, B, rec (0));
  th_K (*s, 0);
  keepers_in_threads (*s);
  expect (*s, 3, "0:{A,B,A'}|final=A'");

  s = &begin (t, sc_S10, "S10", engine_shared, A, blockAB);
  th_L (*s, 2);
  th_W (*s, B, rec (0));
  th_K (*s, 0);
  keepers_in_threads (*s);
  expect (*s, 6, "0:A,{A,B,A'}|final=A'", "0:B,{B,A'}|final=A'",
          "0:A',A'|final=A'");

  s = &begin (t, sc_S11, "S11", engine_shared, A, blockAB);
  th_L (*s, 1);
  th_L (*s, 1);
  th_W (*s, B, rec (0));
  th_K (*s, 0);
  keepers_in_threads (*s);
  expect (*s, 9, "0:{A,B,A'}|1:{A,B,A'}|final=A'");

  s = &begin (t, sc_S12, "S12", engine_shared, A, blockAB);
  th_L (*s, 1);
  th_C (*s, A, B);
  th_W (*s, A);
  th_K (*s, 1);
  keepers_in_threads (*s);
  expect (*s, 4, "0:{A,B}|1:T|final={A,B}");

  s = &begin (t, sc_S13, "S13", engine_shared, A, blockAB);
  th_L (*s, 1);
  th_L (*s, 1);
  th_C (*s, A, B);
  th_W (*s, A, B);
  expect (*s, 8, "0:{A,B}|1:{A,B}|2:{T,F/B}|final=B");

  s = &begin (t, sc_S14, "S14", engine_shared, A, blockAB);
  th_L (*s, 1);
  th_L (*s, 1);
  th_C (*s, A, B);
  th_W (*s, B, rec (0));
  th_K (*s, 0);
  keepers_in_threads (*s);
  s->cov = true;
  expect (*s, 18, "0:{A,B,A'}|1:{A,B,A'}|2:{T,F/B}|final=A'");

  s = &begin (t, sc_S15, "S15", engine_shared, A, blockAB);
  th_L (*s, 2);
  th_L (*s, 1);
  th_W (*s, B, A, B);
  th_K (*s, 0);
  th_K (*s, 1);
  keepers_in_threads (*s);
  s->cov = true;
  expect (*s, 8, "0:{A,B},{A,B}|1:{A,B}|final=B");

  s = &begin (t, sc_S16, "S16", engine_shared, A, blockAB);
  th_L (*s, 1);
  th_L (*s, 1);
  th_L (*s, 1);
  th_W (*s, B, A);
  th_K (*s, 0);
  keepers_in_threads (*s);
  s->cov = true;
  s->policies |= 1u << pol_L2;
  expect (*s, 8, "0:{A,B}|1:{A,B}|2:{A,B}|final=A");

  // -- Holders: the cell holds an alias that the engine stores through a
  // holder --
  s = &begin (t, sc_H1, "H1", engine_shared, Af, blockAB);
  th_L (*s, 1);
  th_W (*s, B);
  expect (*s, 2, "0:{A~f,B}|final=B");

  s = &begin (t, sc_H2, "H2", engine_shared, Af, blockAB);
  th_L (*s, 1);
  th_L (*s, 1);
  th_W (*s, Bf, Af);
  expect (*s, 4, "0:{A~f,B~f}|1:{A~f,B~f}|final=A~f");

  s = &begin (t, sc_H3, "H3", engine_shared, Af, blockAB);
  th_L (*s, 1);
  th_X (*s, B);
  expect (*s, 2, "0:{A~f,B}|1:A~f|final=B");

  s = &begin (t, sc_H4, "H4", engine_shared, Af, blockABD);
  th_L (*s, 1);
  th_C (*s, Af, B);
  th_W (*s, D);
  // The only other writer stores D, so the final value is always D: a
  // successful compare-exchange must come before that store (the oracle
  // note listed a row "C = T, final B" as well; no schedule can produce it).
  expect (*s, 5, "0:{A~f,B,D}|1:T|final=D", "0:{A~f,D}|1:F/D|final=D");

  s = &begin (t, sc_H5, "H5", engine_shared, Af, blockAB);
  th_L (*s, 1);
  th_C (*s, A, B);
  expect (*s, 1, "0:A~f|1:F/A~f|final=A~f");

  // -- The weak engine --
  s = &begin (t, sc_W1, "W1", engine_weak, A, blockAB);
  th_WLK (*s);
  th_WW (*s, vs (1));
  th_K (*s, 0);
  keepers_in_threads (*s);
  s->policies = (1u << pol_R8) | (1u << pol_R0);
  expect (*s, 3, "0:wA,{A,-}|final=wB", "0:wB,B|final=wB");

  s = &begin (t, sc_W2, "W2", engine_weak, Af, blockAB);
  th_WL (*s);
  th_WW (*s, vs (1));
  s->policies = (1u << pol_R8) | (1u << pol_R0);
  expect (*s, 2, "0:{wA~f,wB}|final=wB");

  s = &begin (t, sc_W3, "W3", engine_weak, A, blockABD);
  th_WL (*s);
  th_WW (*s, vs (1));
  th_WW (*s, vs (2));
  s->policies = (1u << pol_R8) | (1u << pol_R0);
  expect (*s, 6, "0:{wA,wD,wB}|final=wB", "0:{wA,wB,wD}|final=wD");

  // -- lock and expired racing the last release --
  s = &begin (t, sc_LK, "LK", engine_shared, A, blockAB);
  th_L (*s, 1);
  th_W (*s, B);
  th_lock (*s, 0);
  s->main_keeps = 2u;
  expect (*s, 4, "0:{A,B}|2:{A,-}|final=B");

  s = &begin (t, sc_EX, "EX", engine_shared, A, blockAB);
  th_L (*s, 1);
  th_W (*s, B);
  th_expired (*s, 0);
  s->main_keeps = 2u;
  expect (*s, 4, "0:{A,B}|2:{T,F}|final=B");

  // -- The same block with a different delta --
  s = &begin (t, sc_RI, "RI", engine_shared, A, blockA);
  th_L (*s, 1);
  th_W (*s, Am);
  th_L (*s, 1);
  expect (*s, 4, "0:{A,A.m}|2:{A,A.m}|final=A.m");

  s = &begin (t, sc_RIC, "RIC", engine_shared, A, blockAB);
  th_L (*s, 1);
  th_W (*s, Am);
  th_C (*s, A, B);
  expect (*s, 5, "0:{A,B,A.m}|2:T|final=A.m", "0:{A,A.m}|2:F/A.m|final=A.m");

  // -- Two writers --
  s = &begin (t, sc_UW, "UW", engine_shared, A, blockABD);
  th_L (*s, 1);
  th_W (*s, B);
  th_W (*s, D);
  expect (*s, 6, "0:{A,D,B}|final=B", "0:{A,B,D}|final=D");

  s = &begin (t, sc_UW2, "UW2", engine_shared, A, blockABD);
  th_L (*s, 1);
  th_W (*s, B);
  th_W (*s, D, A);
  expect (*s, 6, "0:{A,B,D}|final=A", "0:{A,D,B}|final=B");

  s = &begin (t, sc_GF, "GF", engine_shared, A, blockABD);
  th_L (*s, 1);
  th_W (*s, B);
  th_W (*s, D);
  s->main_keeps = 6u;
  expect (*s, 6, "0:{A,D,B}|final=B", "0:{A,B,D}|final=D");

  // -- Reserve 0 and the tick limit --
  s = &begin (t, sc_TU, "TU", engine_shared, A, blockAB);
  th_L (*s, 1);
  th_L (*s, 1);
  th_W (*s, B);
  s->policies = 1u << pol_R0;
  s->quick_bound = 3; // the top-up needs three preemptions (M36)
  expect (*s, 4, "0:{A,B}|1:{A,B}|final=B");

  s = &begin (t, sc_TL, "TL", engine_shared, A, blockAB);
  th_L (*s, 1);
  th_L (*s, 1);
  th_L (*s, 1);
  th_W (*s, B);
  s->policies = 1u << pol_L2;
  expect (*s, 8, "0:{A,B}|1:{A,B}|2:{A,B}|final=B");

  // -- A compare-exchange racing a store --
  s = &begin (t, sc_SC, "SC", engine_shared, A, blockABD);
  th_C (*s, A, D);
  th_W (*s, B, A);
  expect (*s, 3, "0:T|final=A", "0:T|final=D", "0:F/B|final=A");

  s = &begin (t, sc_SCL, "SCL", engine_shared, A, blockABD);
  th_L (*s, 1);
  th_C (*s, A, D);
  expect (*s, 2, "0:{A,D}|1:T|final=D");

  // -- The tick limit of 1: the loader and the writer both back off --
  s = &begin (t, sc_TB, "TB", engine_shared, A, blockAB);
  th_L (*s, 1);
  th_L (*s, 1);
  th_W (*s, B);
  s->policies = 1u << pol_L1;
  expect (*s, 4, "0:{A,B}|1:{A,B}|final=B");

  // -- A holder compare-exchange against another alias of the same owner
  // (the stored pointers differ: no match), and a matching one, with the
  // tick limit of 2 so that it backs off --
  vspec_t const Ag = vs (0, fl_far, 1);
  s = &begin (t, sc_HC, "HC", engine_shared, Af, blockAB);
  th_L (*s, 1);
  th_L (*s, 1);
  th_C (*s, Ag, B);
  s->policies = (1u << pol_R8) | (1u << pol_L2);
  expect (*s, 1, "0:A~f|1:A~f|2:F/A~f|final=A~f");

  s = &begin (t, sc_HM, "HM", engine_shared, Af, blockAB);
  th_L (*s, 1);
  th_L (*s, 1);
  th_C (*s, Af, B);
  s->policies = 1u << pol_L2;
  expect (*s, 4, "0:{A~f,B}|1:{A~f,B}|2:T|final=B");

  // -- The exchange of a block word with a member alias --
  s = &begin (t, sc_XB, "XB", engine_shared, Am, blockAB);
  th_L (*s, 1);
  th_X (*s, B);
  expect (*s, 2, "0:{A.m,B}|1:A.m|final=B");

  // -- An empty pointer with a stored pointer, stored and held --
  s = &begin (t, sc_E1, "E1", engine_shared, raw_empty (), blockAB);
  th_L (*s, 1);
  th_L (*s, 1);
  th_W (*s, B, raw_empty ());
  expect (*s, 4, "0:{-*,B}|1:{-*,B}|final=-*");

  // -- The weak engine: exchange and compare-exchange, both forms --
  vspec_t const wA = vs (0);
  vspec_t const wB = vs (1);
  vspec_t const wD = vs (2);
  s = &begin (t, sc_WX, "WX", engine_weak, wA, blockABD);
  th_WL (*s);
  th_WL (*s);
  th_WX (*s, wB);
  s->policies = (1u << pol_R8) | (1u << pol_R0) | (1u << pol_L2);
  expect (*s, 4, "0:{wA,wB}|1:{wA,wB}|2:wA|final=wB");

  s = &begin (t, sc_WC, "WC", engine_weak, wA, blockABD);
  th_WL (*s);
  th_WC (*s, wA, wB, false);
  th_WW (*s, wD);
  s->policies = (1u << pol_R8) | (1u << pol_R0) | (1u << pol_L2);
  expect (*s, 5, "0:{wA,wB,wD}|1:T|final=wD", "0:{wA,wD}|1:F/wD|final=wD");

  s = &begin (t, sc_WCW, "WCW", engine_weak, wA, blockABD);
  th_WL (*s);
  th_WC (*s, wA, wB, true);
  th_WW (*s, wD);
  s->policies = (1u << pol_R8) | (1u << pol_R0) | (1u << pol_L2);
  // A weak compare-exchange that fails although the value is equivalent
  // (a loader's tick changed the word) leaves that loader on A.
  expect (*s, 6, "0:{wA,wB,wD}|1:T|final=wD", "0:{wA,wD}|1:F/wD|final=wD",
          "0:wA|1:F/wA|final=wD");

  // -- The weak form on the shared engine --
  s = &begin (t, sc_SW, "SW", engine_shared, A, blockAB);
  th_L (*s, 1);
  th_L (*s, 1);
  th_Cw (*s, A, B);
  expect (*s, 5, "0:{A,B}|1:{A,B}|2:T|final=B", "0:A|1:A|2:F/A|final=A");
  return t;
}
} // namespace detail

/// The scenario table (built once).
inline std::vector<scenario_def_t> const &
scenario_table ()
{
  static std::vector<scenario_def_t> const table = detail::build_table ();
  return table;
}

/// The scenario with the id @p id.
inline scenario_def_t const &
scenario_of (scenario_id_t id)
{
  std::vector<scenario_def_t> const &table = scenario_table ();
  for (std::size_t i = 0; i < table.size (); ++i)
    if (table[i].id == id)
      return table[i];
  std::abort ();
}

/// The name of a scenario (the constant of the table).
inline char const *
scenario_name (scenario_id_t id)
{
  return scenario_of (id).name;
}

/// Whether the policy @p policy runs the scenario.
inline bool
scenario_runs_policy (scenario_def_t const &s, policy_id_t policy)
{
  return (s.policies & (1u << policy)) != 0u;
}
} // namespace split_sim

/// Every (scenario, policy) pair that runs: R8 for every scenario but TU and
/// TL, R0 for the shared-engine scenarios but TL, L2 for TL and S16.
#define LUMEX_SIM_ALL_RUNS(X)                                                 \
  X (S01, R8)                                                                 \
  X (S02, R8)                                                                 \
  X (S03, R8)                                                                 \
  X (S04, R8)                                                                 \
  X (S05, R8)                                                                 \
  X (S06, R8)                                                                 \
  X (S07, R8)                                                                 \
  X (S08, R8)                                                                 \
  X (S09, R8)                                                                 \
  X (S10, R8)                                                                 \
  X (S11, R8)                                                                 \
  X (S12, R8)                                                                 \
  X (S13, R8)                                                                 \
  X (S14, R8)                                                                 \
  X (S15, R8)                                                                 \
  X (S16, R8)                                                                 \
  X (H1, R8)                                                                  \
  X (H2, R8)                                                                  \
  X (H3, R8)                                                                  \
  X (H4, R8)                                                                  \
  X (H5, R8)                                                                  \
  X (W1, R8)                                                                  \
  X (W2, R8)                                                                  \
  X (W3, R8)                                                                  \
  X (LK, R8)                                                                  \
  X (EX, R8)                                                                  \
  X (RI, R8)                                                                  \
  X (RIC, R8)                                                                 \
  X (UW, R8)                                                                  \
  X (UW2, R8)                                                                 \
  X (GF, R8)                                                                  \
  X (SC, R8)                                                                  \
  X (SCL, R8)                                                                 \
  X (S01, R0)                                                                 \
  X (S02, R0)                                                                 \
  X (S03, R0)                                                                 \
  X (S04, R0)                                                                 \
  X (S05, R0)                                                                 \
  X (S06, R0)                                                                 \
  X (S07, R0)                                                                 \
  X (S08, R0)                                                                 \
  X (S09, R0)                                                                 \
  X (S10, R0)                                                                 \
  X (S11, R0)                                                                 \
  X (S12, R0)                                                                 \
  X (S13, R0)                                                                 \
  X (S14, R0)                                                                 \
  X (S15, R0)                                                                 \
  X (S16, R0)                                                                 \
  X (H1, R0)                                                                  \
  X (H2, R0)                                                                  \
  X (H3, R0)                                                                  \
  X (H4, R0)                                                                  \
  X (H5, R0)                                                                  \
  X (LK, R0)                                                                  \
  X (EX, R0)                                                                  \
  X (RI, R0)                                                                  \
  X (RIC, R0)                                                                 \
  X (UW, R0)                                                                  \
  X (UW2, R0)                                                                 \
  X (GF, R0)                                                                  \
  X (TU, R0)                                                                  \
  X (SC, R0)                                                                  \
  X (SCL, R0)                                                                 \
  X (TL, L2)                                                                  \
  X (S16, L2)                                                                 \
  X (TB, L1)                                                                  \
  X (HC, R8)                                                                  \
  X (HC, L2)                                                                  \
  X (HM, L2)                                                                  \
  X (XB, R8)                                                                  \
  X (XB, R0)                                                                  \
  X (E1, R8)                                                                  \
  X (E1, R0)                                                                  \
  X (W1, R0)                                                                  \
  X (W2, R0)                                                                  \
  X (W3, R0)                                                                  \
  X (WX, R8)                                                                  \
  X (WX, R0)                                                                  \
  X (WX, L2)                                                                  \
  X (WC, R8)                                                                  \
  X (WC, R0)                                                                  \
  X (WC, L2)                                                                  \
  X (WCW, R8)                                                                 \
  X (WCW, R0)                                                                 \
  X (WCW, L2)                                                                 \
  X (SW, R8)                                                                  \
  X (SW, R0)

#endif // LUMEX_ATOMIC_SMART_PTR_HAS_SPLIT_COUNT

#endif // !LUMEX_TESTS_CORE_ATOMIC_SMART_PTR_SPLIT_COUNT_SIM_SCENARIOS_HPP
