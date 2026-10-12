/*
 * Copyright (c) 2026 Vladislav Semykin <vladislav.semykin@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
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

/**
 * @file LumexTestHistory.hpp
 * @brief Records a concurrent history and checks it for linearizability
 * with the Wing and Gong search.
 * @details Each thread brackets an operation with `begin ()` and `end ()`;
 * both take a ticket from one global counter, so the order of tickets is a
 * real-time order that is only too coarse (never wrong): an operation that
 * responded before another one was invoked precedes it. The recorded
 * operations are plain numbers (`HistoryOp`), so the structure under test
 * decides what they mean: the models below read them as the calls of a
 * register with compare-and-swap, a stack and a queue.
 *
 * `is_linearizable (history, model)` searches for a total order of the
 * operations that respects the real-time order and in which a sequential
 * execution of the model reproduces every recorded result (Wing and Gong,
 * "Testing and verifying concurrent objects", with the memoization of seen
 * (set of linearized operations, model state) pairs from Lowe). It handles
 * up to 62 operations; a history is meant to be short (2 or 3 threads with
 * 3 to 5 operations each), run many times with different schedules.
 */
#ifndef LUMEX_TESTS_SUPPORT_TEST_HISTORY_HPP
#define LUMEX_TESTS_SUPPORT_TEST_HISTORY_HPP

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace lumex_test
{
/// One completed operation of a history.
struct HistoryOp
{
  int thread;
  int kind;
  std::int64_t arg0;
  std::int64_t arg1;
  std::int64_t res0;
  std::int64_t res1;
  std::uint64_t invoke;
  std::uint64_t respond;
};

/// Collects the operations of N threads; one thread writes only its own slot.
class HistoryRecorder
{
public:
  explicit HistoryRecorder (int threads)
      : clock_ (1), per_thread_ (static_cast<std::size_t> (threads))
  {
  }

  HistoryRecorder (HistoryRecorder const &) = delete;
  HistoryRecorder &operator= (HistoryRecorder const &) = delete;

  /// Takes the invocation ticket; call it just before the operation.
  std::uint64_t
  begin ()
  {
    return clock_.fetch_add (1, std::memory_order_seq_cst);
  }

  /// Takes the response ticket and stores the operation; call it just after.
  void
  end (int thread, int kind, std::uint64_t invoke, std::int64_t arg0,
       std::int64_t arg1, std::int64_t res0, std::int64_t res1)
  {
    HistoryOp op;
    op.thread = thread;
    op.kind = kind;
    op.arg0 = arg0;
    op.arg1 = arg1;
    op.res0 = res0;
    op.res1 = res1;
    op.invoke = invoke;
    op.respond = clock_.fetch_add (1, std::memory_order_seq_cst);
    per_thread_[static_cast<std::size_t> (thread)].push_back (op);
  }

  /// Every recorded operation, in the order of invocation. Call after join.
  std::vector<HistoryOp>
  history () const
  {
    std::vector<HistoryOp> all;
    for (std::size_t t = 0; t < per_thread_.size (); ++t)
      all.insert (all.end (), per_thread_[t].begin (), per_thread_[t].end ());
    std::sort (all.begin (), all.end (),
               [] (HistoryOp const &a, HistoryOp const &b)
                 { return a.invoke < b.invoke; });
    return all;
  }

private:
  std::atomic<std::uint64_t> clock_;
  std::vector<std::vector<HistoryOp>> per_thread_;
};

/// Result of the linearizability search.
enum class Linearizability
{
  linearizable,
  not_linearizable,
  too_long
};

namespace history_detail
{
template <typename Model> class Search
{
public:
  typedef typename Model::state_type state_type;

  explicit Search (std::vector<HistoryOp> const &ops)
      : ops_ (ops), full_ ((std::uint64_t (1) << ops.size ()) - 1)
  {
  }

  bool
  run (state_type const &initial)
  {
    return search (0, initial);
  }

private:
  bool
  search (std::uint64_t done, state_type const &state)
  {
    if (done == full_)
      return true;
    if (seen_.count (std::make_pair (done, state)) != 0)
      return false;
    std::size_t const n = ops_.size ();
    for (std::size_t i = 0; i < n; ++i)
      {
        if ((done >> i) & 1u)
          continue;
        // Minimal: no other pending operation responded before this one was
        // invoked.
        bool minimal = true;
        for (std::size_t j = 0; j < n && minimal; ++j)
          if (j != i && !((done >> j) & 1u)
              && ops_[j].respond < ops_[i].invoke)
            minimal = false;
        if (!minimal)
          continue;
        state_type next = state;
        if (Model::apply (next, ops_[i])
            && search (done | (std::uint64_t (1) << i), next))
          return true;
      }
    seen_.insert (std::make_pair (done, state));
    return false;
  }

  std::vector<HistoryOp> const &ops_;
  std::uint64_t full_;
  std::set<std::pair<std::uint64_t, state_type>> seen_;
};
} // namespace history_detail

/**
 * @brief Tells whether @p ops can be ordered into a legal sequential
 * execution of @p Model starting from @p initial.
 */
template <typename Model>
Linearizability
is_linearizable (std::vector<HistoryOp> const &ops,
                 typename Model::state_type const &initial)
{
  if (ops.size () > 62)
    return Linearizability::too_long;
  history_detail::Search<Model> search (ops);
  return search.run (initial) ? Linearizability::linearizable
                              : Linearizability::not_linearizable;
}

/// A readable list of a history, for failure messages.
template <typename Model>
std::string
describe_history (std::vector<HistoryOp> const &ops)
{
  std::ostringstream out;
  for (std::size_t i = 0; i < ops.size (); ++i)
    out << "\n    t" << ops[i].thread << " [" << ops[i].invoke << ","
        << ops[i].respond << "] " << Model::describe (ops[i]);
  return out.str ();
}

// --- Models -----------------------------------------------------------------

/// Calls of a register with exchange and compare-and-swap. Values are small
/// integers (ids of the equivalence classes); `-1` is "never stored".
enum RegisterKind
{
  register_load = 0,
  register_store,
  register_exchange,
  /// arg0 = expected, arg1 = desired, res0 = 1 on success, res1 = the value
  /// found on failure.
  register_cas_strong,
  /// As the strong one, but a failure with an equal value is allowed.
  register_cas_weak
};

struct RegisterModel
{
  typedef std::int64_t state_type;

  static bool
  apply (state_type &state, HistoryOp const &op)
  {
    switch (op.kind)
      {
      case register_load:
        return op.res0 == state;
      case register_store:
        state = op.arg0;
        return true;
      case register_exchange:
        if (op.res0 != state)
          return false;
        state = op.arg0;
        return true;
      case register_cas_strong:
        if (op.res0 != 0)
          {
            if (state != op.arg0)
              return false;
            state = op.arg1;
            return true;
          }
        return state != op.arg0 && op.res1 == state;
      case register_cas_weak:
        if (op.res0 != 0)
          {
            if (state != op.arg0)
              return false;
            state = op.arg1;
            return true;
          }
        return op.res1 == state;
      }
    return false;
  }

  static std::string
  describe (HistoryOp const &op)
  {
    std::ostringstream out;
    switch (op.kind)
      {
      case register_load:
        out << "load -> " << op.res0;
        break;
      case register_store:
        out << "store " << op.arg0;
        break;
      case register_exchange:
        out << "exchange " << op.arg0 << " -> " << op.res0;
        break;
      case register_cas_strong:
      case register_cas_weak:
        out << (op.kind == register_cas_strong ? "cas_strong(" : "cas_weak(")
            << op.arg0 << " -> " << op.arg1 << ") = "
            << (op.res0 != 0 ? "true"
                             : "false, found " + std::to_string (op.res1));
        break;
      default:
        out << "?";
      }
    return out.str ();
  }
};

/// Calls of a stack: push (arg0) and pop (res0 = value, -1 for "empty").
enum StackKind
{
  stack_push = 0,
  stack_pop
};

struct StackModel
{
  typedef std::vector<std::int64_t> state_type;

  static bool
  apply (state_type &state, HistoryOp const &op)
  {
    if (op.kind == stack_push)
      {
        state.push_back (op.arg0);
        return true;
      }
    if (state.empty ())
      return op.res0 == -1;
    if (state.back () != op.res0)
      return false;
    state.pop_back ();
    return true;
  }

  static std::string
  describe (HistoryOp const &op)
  {
    return op.kind == stack_push ? "push " + std::to_string (op.arg0)
                                 : "pop -> " + std::to_string (op.res0);
  }
};

/// Calls of a FIFO queue: enqueue (arg0) and dequeue (res0 = value, -1 for
/// "empty").
enum QueueKind
{
  queue_enqueue = 0,
  queue_dequeue
};

struct QueueModel
{
  typedef std::vector<std::int64_t> state_type;

  static bool
  apply (state_type &state, HistoryOp const &op)
  {
    if (op.kind == queue_enqueue)
      {
        state.push_back (op.arg0);
        return true;
      }
    if (state.empty ())
      return op.res0 == -1;
    if (state.front () != op.res0)
      return false;
    state.erase (state.begin ());
    return true;
  }

  static std::string
  describe (HistoryOp const &op)
  {
    return op.kind == queue_enqueue ? "enqueue " + std::to_string (op.arg0)
                                    : "dequeue -> " + std::to_string (op.res0);
  }
};

/// Calls of a set of small non-negative keys (below 62): insert, remove and
/// contains, each with `res0` = 1 when it reported success (inserted,
/// removed, found).
enum SetKind
{
  set_insert = 0,
  set_remove,
  set_contains
};

struct SetModel
{
  /// The members as a bit mask.
  typedef std::int64_t state_type;

  static bool
  apply (state_type &state, HistoryOp const &op)
  {
    std::int64_t const bit = std::int64_t (1) << op.arg0;
    bool const member = (state & bit) != 0;
    switch (op.kind)
      {
      case set_insert:
        if ((op.res0 != 0) == member)
          return false;
        state |= bit;
        return true;
      case set_remove:
        if ((op.res0 != 0) != member)
          return false;
        state &= ~bit;
        return true;
      case set_contains:
        return (op.res0 != 0) == member;
      }
    return false;
  }

  static std::string
  describe (HistoryOp const &op)
  {
    char const *const names[] = { "insert ", "remove ", "contains " };
    return std::string (names[op.kind]) + std::to_string (op.arg0) + " -> "
           + (op.res0 != 0 ? "true" : "false");
  }
};
} // namespace lumex_test

#endif // !LUMEX_TESTS_SUPPORT_TEST_HISTORY_HPP
