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

// The negative control of package V2 (the ABA scenarios of the explorer): a
// deliberately ABA-vulnerable toy, a lock-free stack of nodes over a pointer
// compare-and-swap whose freed nodes are reused at once (the classic Treiber
// stack), run by the same explorer as the engine. The explorer must find the
// ABA schedule at one preemption and print its decision string; the same stack
// with a version in the top word, or without the immediate reuse, must not
// fail. If the explorer could not find this schedule, the absence of a failure
// in the scenarios of the engine would say nothing about ABA.
//
// What the control proves and what it does not: the state space of the toy is
// three schedules (the schedule that finds the ABA needs one preemption, and
// bound one already exhausts the space), so it shows that the explorer
// preempts, that the oracle sees the corrupted stack, and that the printed
// decision string replays the failure. It does not show that the scenarios of
// the engine would find an ABA in the engine; that is the job of the
// mutations of the engine that the scenarios catch.
//
// The schedule: thread 0 pops (reads the top A and its next B, then is
// paused before its compare-and-swap); thread 1 pops A, pops B (and keeps it),
// pushes A again (the freed node is reused, A -> C). Thread 0 resumes: the top
// is A again, the compare-and-swap succeeds and installs B, a node that thread
// 1 owns. The oracle (`end`): no node is reachable from the top and owned at
// once, no node is reachable twice, the stack has no cycle.
#ifndef LUMEX_TESTS_CORE_ATOMIC_SMART_PTR_SPLIT_COUNT_SIM_ABA_HPP
#define LUMEX_TESTS_CORE_ATOMIC_SMART_PTR_SPLIT_COUNT_SIM_ABA_HPP

#include <cstdio>
#include <string>

#include "lumex/tests/core/atomic/smart_ptr/split_count/LumexSplitCountSim.hpp"
#include "lumex/tests/core/atomic/smart_ptr/split_count/LumexSplitCountTestSupport.hpp"

#if LUMEX_ATOMIC_SMART_PTR_HAS_SPLIT_COUNT

namespace split_sim
{
/// The variants of the toy stack.
enum toy_aba_mode_t
{
  toy_aba_reuse,     ///< immediate reuse of a freed node, bare pointer CAS
  toy_aba_versioned, ///< the same, with a version in the top word
  toy_aba_no_reuse   ///< bare pointer CAS, but a freed node is never reused
};

class toy_aba_world_t : public world_t
{
public:
  explicit toy_aba_world_t (toy_aba_mode_t mode) : mode_ (mode) {}

  int
  thread_count () const override
  {
    return 2;
  }

  void
  begin () override
  {
    for (int i = 0; i < pool_size; ++i)
      {
        next_[i] = none;
        owner_[i] = none;
      }
    // The stack A -> B -> C (nodes 0, 1, 2); node 3 is spare.
    next_[0] = 1;
    next_[1] = 2;
    top_ = 0;
    version_ = 0;
    freed_count_ = 0;
  }

  void
  body (int id) override
  {
    if (id == 0)
      {
        // The popper that is paused between its read and its compare-and-swap.
        for (;;)
          {
            int const old_top = top_;
            unsigned const old_version = version_;
            if (old_top == none)
              return;
            int const old_next = next_[old_top];
            sim_yield (pt_op_begin);
            if (cas (old_top, old_version, old_next))
              {
                owner_[old_top] = 0;
                return;
              }
          }
      }
    // The thread that makes A -> B -> C into A -> C while thread 0 sleeps.
    int const a = pop (1);
    int const b = pop (1);
    static_cast<void> (b); // kept: owned by thread 1 to the end
    release (a);
    push (alloc (), 1);
  }

  bool
  step (int, unsigned, unsigned, long, int) override
  {
    return true;
  }

  void
  end () override
  {
    // Reachable nodes: no repeat, none owned.
    bool seen[pool_size] = { false, false, false, false };
    int steps = 0;
    for (int n = top_; n != none && steps < pool_size + 1;
         n = next_[n], ++steps)
      {
        if (n < 0 || n >= pool_size || seen[n] || owner_[n] != none)
          {
            fail ("a node is reachable from the top and owned or repeated");
            return;
          }
        seen[n] = true;
      }
    if (steps > pool_size)
      fail ("the stack has a cycle");
  }

private:
  enum
  {
    none = -1,
    pool_size = 4
  };

  bool
  cas (int expected, unsigned expected_version, int desired)
  {
    if (top_ != expected
        || (mode_ == toy_aba_versioned && version_ != expected_version))
      return false;
    top_ = desired;
    ++version_;
    return true;
  }

  int
  pop (int who)
  {
    for (;;)
      {
        int const old_top = top_;
        unsigned const old_version = version_;
        int const old_next = next_[old_top];
        if (cas (old_top, old_version, old_next))
          {
            owner_[old_top] = who;
            return old_top;
          }
      }
  }

  void
  release (int node)
  {
    owner_[node] = none;
    freed_[freed_count_++] = node;
  }

  int
  alloc ()
  {
    if (mode_ != toy_aba_no_reuse && freed_count_ > 0)
      return freed_[--freed_count_];
    return 3;
  }

  void
  push (int node, int)
  {
    for (;;)
      {
        int const old_top = top_;
        unsigned const old_version = version_;
        next_[node] = old_top;
        if (cas (old_top, old_version, node))
          return;
      }
  }

  void
  fail (char const *what)
  {
    if (current_explorer () != nullptr)
      {
        std::string text = "SIM-FAIL oracles=TOY-ABA ";
        text += what;
        text += "\n";
        current_explorer ()->report_failure (text);
      }
  }

  toy_aba_mode_t mode_;
  int next_[pool_size];
  int owner_[pool_size];
  int freed_[pool_size];
  int freed_count_;
  int top_;
  unsigned version_;
};
} // namespace split_sim

#endif // LUMEX_ATOMIC_SMART_PTR_HAS_SPLIT_COUNT

#endif // !LUMEX_TESTS_CORE_ATOMIC_SMART_PTR_SPLIT_COUNT_SIM_ABA_HPP
