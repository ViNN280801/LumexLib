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

// The simulated-scheduler explorer of the split-count engine (package V1).
//
// The real engine (`split_count_cell`) runs under `sim_policy_t`, a policy
// whose hooks are the scheduling points of a cooperative scheduler: one real
// thread per scenario thread, a baton (one mutex, one condition variable, an
// integer `turn`) that lets exactly one thread run, so every execution is
// sequentially consistent and fully determined by the list of decisions. The
// explorer enumerates the interleavings by a stateless depth-first search
// with a preemption bound (CHESS), runs a ghost model of the protocol (tick
// tags, deposits, debts, argument lifetimes) beside the engine and evaluates
// the oracles of the plan at every boundary, the moment all threads are
// paused: the safety checks, the word, the counter identities (C-ID, E-ID,
// BAL), the guarantees G1 and G2 of the use count, the bounds on the deposit,
// the path of every step, the linearizability of the loads and of the
// compare-exchanges, and the quiescent invariants at the end of a schedule.
// On the first failing boundary it prints the report and the decision string
// (`LUMEX_SIM_REPLAY` replays it) and aborts.
//
// What the explorer cannot see (state this plainly):
// - Weak memory. The simulation is sequentially consistent: one thread runs at
//   a time and the baton's mutex is a full fence between steps. Every
//   `memory_order` argument of the engine is invisible here. On x86-64 the
//   asm and MSVC backends are locked instructions anyway, and the built-in
//   backend runs the same code. Store buffering, load reordering and missing
//   acquire/release edges are checked only by ThreadSanitizer (built-in
//   backend) and by the Relacy model; the orders of L3, L5, L7, L8, L9 and
//   S3-S6 are argued, not tested, here.
// - Interleavings inside a step. The explorer interleaves only at the hook
//   points of the policy. The owner mirror and the holder read-modify-writes
//   in `deposit`, `withdraw`, `pay` and the holder release, the guess, the
//   acquire re-read and the `add` of L6 in `count_and_settle`, `take_image`
//   with the holder creation, `adopt` and the wrapper's assignment, the
//   destruction of by-value arguments and every counter-internal CAS loop are
//   atomic here; a mutation that only reorders code inside such a stretch is
//   invisible unless it crashes within the step.
// - Torn guesses: `guess` is exact here (`torn_policy_t` has them).
// - Constructor, destructor, `wait` and `notify` are not interleaved.
// - Lock-freedom and starvation are seen only as PROGRESS (an operation that
//   does not finish alone within a bounded number of points).
#ifndef LUMEX_TESTS_CORE_ATOMIC_SMART_PTR_SPLIT_COUNT_SIM_HPP
#define LUMEX_TESTS_CORE_ATOMIC_SMART_PTR_SPLIT_COUNT_SIM_HPP

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <new>
#include <set>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <unistd.h>

#include "lumex/tests/core/atomic/smart_ptr/split_count/LumexSplitCountSimScenarios.hpp"
#include "lumex/tests/core/atomic/smart_ptr/split_count/LumexSplitCountTestSupport.hpp"

#if LUMEX_ATOMIC_SMART_PTR_HAS_SPLIT_COUNT

#if LUMEX_TEST_HAS_ASAN
// The entry points of <sanitizer/asan_interface.h> and
// <sanitizer/common_interface_defs.h> that the explorer uses, declared here so
// that no platform header follows the project ones.
extern "C" void __asan_poison_memory_region (void const volatile *address,
                                             std::size_t size);
extern "C" void __asan_unpoison_memory_region (void const volatile *address,
                                               std::size_t size);
extern "C" void __sanitizer_set_death_callback (void (*callback) (void));
#define LUMEX_SIM_ASAN_POISON(address, size)                                  \
  __asan_poison_memory_region ((address), (size))
#define LUMEX_SIM_ASAN_UNPOISON(address, size)                                \
  __asan_unpoison_memory_region ((address), (size))
#endif

namespace split_sim
{
namespace sp = split_test::sp;
namespace asp = split_test::asp;
using split_test::split_event_t;
using split_test::split_point_t;
using split_test::split_value_t;
typedef asp::Detail::split_word_t word;
typedef sp::detail::ctl_base ctl_t;
typedef sp::detail::ctl_holder holder_t;
typedef sp::detail::ctl_weak_holder weak_holder_t;
typedef sp::detail::split_counter counter_t;

// -- The scheduling points --

/// The point codes: 1..12 are `split_point_t`, 101..106 are `split_event_t`
/// plus 100 (the yield after a note), the rest belongs to the wrapper.
enum point_t : unsigned
{
  pt_none = 0,
  pt_load_read = 1,
  pt_load_tick = 2,
  pt_load_count = 3,
  pt_settle_read = 4,
  pt_settle_untick = 5,
  pt_settle_ext = 6,
  pt_swap = 7,
  pt_cas_read = 8,
  pt_cas_pin = 9,
  pt_cas = 10,
  pt_transfer = 11,
  pt_release = 12,
  pt_note_tick = 101,
  pt_note_untick = 102,
  pt_note_consumed = 103,
  pt_note_transfer = 104,
  pt_note_untransfer = 105,
  pt_note_settle_ext = 106,
  pt_op_begin = 200,
  pt_back_off = 201,
  pt_finished = 202
};

static_assert (static_cast<unsigned> (split_point_t::load_read) == 1u
                   && static_cast<unsigned> (split_point_t::release) == 12u
                   && static_cast<unsigned> (split_event_t::tick) == 1u
                   && static_cast<unsigned> (split_event_t::settle_ext) == 6u,
               "the point codes follow the policy enumerations");

inline char const *
point_name (unsigned p)
{
  switch (p)
    {
    case pt_none:
      return "none";
    case pt_load_read:
      return "load_read";
    case pt_load_tick:
      return "load_tick";
    case pt_load_count:
      return "load_count";
    case pt_settle_read:
      return "settle_read";
    case pt_settle_untick:
      return "settle_untick";
    case pt_settle_ext:
      return "settle_ext";
    case pt_swap:
      return "swap";
    case pt_cas_read:
      return "cas_read";
    case pt_cas_pin:
      return "cas_pin";
    case pt_cas:
      return "cas";
    case pt_transfer:
      return "transfer";
    case pt_release:
      return "release";
    case pt_note_tick:
      return "note_tick";
    case pt_note_untick:
      return "note_untick";
    case pt_note_consumed:
      return "note_consumed";
    case pt_note_transfer:
      return "note_transfer";
    case pt_note_untransfer:
      return "note_untransfer";
    case pt_note_settle_ext:
      return "note_settle_ext";
    case pt_op_begin:
      return "op_begin";
    case pt_back_off:
      return "back_off";
    case pt_finished:
      return "finished";
    default:
      return "?";
    }
}

// -- Values and labels --

/// The identity of a pointer value: the block (slot and generation of the
/// arena), the flavor of the stored pointer and, for a holder word, the slot
/// of the holder. Two values are equivalent iff slot, generation, flavor and
/// the weak flag are equal (the holder slot does not take part).
struct val_t
{
  signed char slot; ///< -1: empty
  signed char gen;
  signed char flavor;
  signed char hslot; ///< the holder arena slot of a holder word, else -1
  bool weak;
};

inline val_t
empty_val (bool weak = false)
{
  val_t const v = { -1, 0, 0, -1, weak };
  return v;
}

inline bool
is_empty_val (val_t const &v)
{
  return v.slot < 0;
}

inline bool
veq (val_t const &a, val_t const &b)
{
  if (a.slot < 0 || b.slot < 0)
    return a.slot == b.slot && a.weak == b.weak;
  return a.slot == b.slot && a.gen == b.gen && a.flavor == b.flavor
         && a.weak == b.weak;
}

/// Same block (slot and generation), whatever the flavor.
inline bool
same_block (val_t const &a, val_t const &b)
{
  return a.slot >= 0 && a.slot == b.slot && a.gen == b.gen;
}

inline bool
names_slot (val_t const &v, int slot)
{
  return v.slot >= 0 && v.slot == slot;
}

inline std::string
label_text (val_t const &v)
{
  std::string text;
  if (v.weak)
    text += 'w';
  if (v.slot < 0)
    {
      text += '-';
      return text;
    }
  text += v.slot < 6 ? "ABDEFG"[v.slot] : '?';
  for (int i = 1; i < v.gen; ++i)
    text += '\'';
  if (v.flavor == fl_member)
    text += ".m";
  else if (v.flavor == fl_far)
    text += "~f";
  else if (v.flavor != fl_plain)
    text += "?";
  return text;
}

// -- The memory of the simulation --

/**
 * @brief A fixed set of equal slots in static storage. A freed slot is filled
 * with 0xA5 and, under AddressSanitizer, poisoned; an allocation unpoisons
 * it. The arenas are plain data used by one running thread at a time (the
 * baton orders every access), so they take no lock.
 */
template <std::size_t Bytes, int Slots> class sim_arena_t
{
public:
  enum
  {
    slot_count = Slots
  };

  sim_arena_t () { reset (); }

  /// Every slot free again; the counters and the log cleared.
  void
  reset ()
  {
    for (int i = 0; i < Slots; ++i)
      {
        live_[i] = false;
        generation_[i] = 0;
        label_[i] = 0;
        unpoison (i);
        std::memset (cells_[i].bytes, 0xA5, Bytes);
        poison (i);
      }
    target = -1;
    next_label_ = 0;
    live_count_ = 0;
    double_free = 0;
    busy_slot = 0;
    exhausted = 0;
    log_count = 0;
  }

  /// The slot @p slot as a block address.
  void *
  base (int slot)
  {
    return cells_[slot].bytes;
  }

  bool
  is_live (int slot) const
  {
    return live_[slot];
  }

  unsigned
  generation (int slot) const
  {
    return generation_[slot];
  }

  int
  label (int slot) const
  {
    return label_[slot];
  }

  int
  live_count () const
  {
    return live_count_;
  }

  /// The slot that holds @p p (-1: not in the arena).
  int
  slot_of (void const *p) const
  {
    std::uintptr_t const address = reinterpret_cast<std::uintptr_t> (p);
    std::uintptr_t const first
        = reinterpret_cast<std::uintptr_t> (cells_[0].bytes);
    if (address < first || address >= first + sizeof (cells_))
      return -1;
    return static_cast<int> ((address - first) / sizeof (cell_t));
  }

  /// A free slot (the one named by `target`, else the lowest) or null when
  /// there is none; the problem is counted.
  void *
  allocate (std::size_t bytes)
  {
    if (bytes > Bytes)
      {
        ++exhausted;
        return nullptr;
      }
    int slot = -1;
    if (target >= 0)
      {
        slot = target;
        target = -1;
        if (live_[slot])
          {
            ++busy_slot;
            return nullptr;
          }
      }
    else
      {
        for (int i = 0; i < Slots; ++i)
          if (!live_[i])
            {
              slot = i;
              break;
            }
      }
    if (slot < 0)
      {
        ++exhausted;
        return nullptr;
      }
    live_[slot] = true;
    ++generation_[slot];
    label_[slot] = ++next_label_;
    ++live_count_;
    unpoison (slot);
    if (log_count < 8)
      log_slot[log_count++] = slot;
    return cells_[slot].bytes;
  }

  /// Frees the slot of @p p. False when @p p is not in the arena.
  bool
  deallocate (void *p)
  {
    int const slot = slot_of (p);
    if (slot < 0)
      return false;
    if (!live_[slot])
      {
        ++double_free;
        return true;
      }
    live_[slot] = false;
    --live_count_;
    std::memset (cells_[slot].bytes, 0xA5, Bytes);
    poison (slot);
    return true;
  }

  /// The first free slot whose fill pattern was disturbed (-1: none). Not
  /// meaningful under AddressSanitizer (the poison reports by itself).
  int
  disturbed_free_slot () const
  {
#if LUMEX_TEST_HAS_ASAN
    return -1;
#else
    for (int i = 0; i < Slots; ++i)
      {
        if (live_[i])
          continue;
        for (std::size_t k = 0; k < Bytes; ++k)
          if (cells_[i].bytes[k] != 0xA5)
            return i;
      }
    return -1;
#endif
  }

  int target; ///< the slot the next allocation must take (-1: any)
  int double_free;
  int busy_slot;
  int exhausted;
  int log_count; ///< the slots allocated since the log was cleared
  int log_slot[8];

private:
  struct alignas (64) cell_t
  {
    unsigned char bytes[Bytes];
  };

  void
  poison (int slot)
  {
#if LUMEX_TEST_HAS_ASAN
    LUMEX_SIM_ASAN_POISON (cells_[slot].bytes, Bytes);
#else
    static_cast<void> (slot);
#endif
  }

  void
  unpoison (int slot)
  {
#if LUMEX_TEST_HAS_ASAN
    LUMEX_SIM_ASAN_UNPOISON (cells_[slot].bytes, Bytes);
#else
    static_cast<void> (slot);
#endif
  }

  cell_t cells_[static_cast<std::size_t> (Slots)];
  bool live_[static_cast<std::size_t> (Slots)];
  unsigned generation_[static_cast<std::size_t> (Slots)];
  int label_[static_cast<std::size_t> (Slots)];
  int next_label_;
  int live_count_;
};

static_assert (sizeof (holder_t) <= 64 && sizeof (weak_holder_t) <= 64,
               "a holder fits in a holder slot");

typedef sim_arena_t<128, 6> obj_arena_t;
typedef sim_arena_t<64, 6> hold_arena_t;

/// The managed object. `pad` starts at offset 12: its address is the member
/// alias.
struct sim_obj_t
{
  explicit sim_obj_t (int identity);
  ~sim_obj_t ();

  std::uint64_t magic;
  std::int32_t id;
  std::int32_t pad[3];
};

inline std::uint64_t
live_magic ()
{
  return 0x51D0B1EC7A11FE00ull;
}

inline std::uint64_t
dead_magic ()
{
  return 0x51D0DEADDEADDE00ull;
}

static_assert (sizeof (sim_obj_t) == 24, "pad starts at offset 12");

/// All the memory state of one simulation.
struct sim_mem_t
{
  obj_arena_t obj;
  hold_arena_t hold;
  int live_objects;
  int double_dispose;
  int unexpected_alloc; ///< the first unexpected allocation size, else 0
  int unexpected_count;
};

inline sim_mem_t &
mem ()
{
  static sim_mem_t instance;
  return instance;
}

/// The id of the scheduled thread the caller is (-1: not a scheduled thread).
inline int &
thread_id ()
{
  static thread_local int id = -1;
  return id;
}

/// Non-zero while the calling thread runs an engine call (set around every
/// operation of a worker and around the engine calls of setup and teardown).
inline int &
engine_scope ()
{
  static thread_local int scope = 0;
  return scope;
}

inline sim_obj_t::sim_obj_t (int identity)
    : magic (live_magic ()), id (identity)
{
  ++mem ().live_objects;
  pad[0] = 0;
  pad[1] = 0;
  pad[2] = 0;
}

inline sim_obj_t::~sim_obj_t ()
{
  sim_mem_t &m = mem ();
  if (magic != live_magic ())
    ++m.double_dispose;
  else
    {
      magic = dead_magic ();
      --m.live_objects;
    }
}

/// The allocator of the blocks of `allocate_shared`: one slot of the object
/// arena holds the control block and the object.
template <typename T> struct sim_alloc_t
{
  typedef T value_type;

  explicit sim_alloc_t (obj_arena_t *a) : arena (a) {}

  template <typename U>
  sim_alloc_t (sim_alloc_t<U> const &other) : arena (other.arena)
  {
  }

  T *
  allocate (std::size_t n)
  {
    void *const p = arena->allocate (n * sizeof (T));
    if (p == nullptr)
      {
        std::fprintf (stderr, "SIM-FAIL oracles=SAFETY the object arena has "
                              "no slot for the block\n");
        std::fflush (stderr);
        std::abort ();
      }
    return static_cast<T *> (p);
  }

  void
  deallocate (T *p, std::size_t)
  {
    arena->deallocate (p);
  }

  obj_arena_t *arena;
};

template <typename T, typename U>
inline bool
operator== (sim_alloc_t<T> const &a, sim_alloc_t<U> const &b)
{
  return a.arena == b.arena;
}

template <typename T, typename U>
inline bool
operator!= (sim_alloc_t<T> const &a, sim_alloc_t<U> const &b)
{
  return a.arena != b.arena;
}

/// Makes a block in the slot @p slot (the id of the object is the slot).
inline sp::shared_ptr<sim_obj_t>
make_block (int slot)
{
  sim_mem_t &m = mem ();
  m.obj.target = slot;
  ++engine_scope ();
  sp::shared_ptr<sim_obj_t> p = sp::allocate_shared<sim_obj_t> (
      sim_alloc_t<sim_obj_t> (&m.obj), static_cast<int> (slot));
  --engine_scope ();
  return p;
}

// -- The redirect of the holder allocations (the shared allocation hooks) --

inline void *
redirect_allocate (std::size_t size)
{
  if (engine_scope () <= 0)
    return nullptr;
  sim_mem_t &m = mem ();
  if (size == sizeof (holder_t) || size == sizeof (weak_holder_t))
    return m.hold.allocate (size);
  if (m.unexpected_count++ == 0)
    m.unexpected_alloc = static_cast<int> (size);
  return nullptr;
}

inline bool
redirect_release (void *block)
{
  return mem ().hold.deallocate (block);
}

/// The control block of the slot @p slot of the object arena.
inline ctl_t *
obj_block (int slot)
{
  return static_cast<ctl_t *> (mem ().obj.base (slot));
}

inline ctl_t *
hold_block (int slot)
{
  return static_cast<ctl_t *> (mem ().hold.base (slot));
}

/// The counters of a block, read raw (the signed halves).
struct counts_t
{
  long count;
  long ext;
  long weak_count;
  long weak_ext;
  bool strong_zero;
  bool weak_zero;
};

inline counts_t
counts_of (ctl_t *block)
{
  counter_t::word_type const strong = block->strong_counter ().load ();
  counter_t::word_type const weak = block->weak_counter ().load ();
  counts_t c;
  c.count = counter_t::count_of (strong);
  c.ext = counter_t::ext_of (strong);
  c.weak_count = counter_t::count_of (weak);
  c.weak_ext = counter_t::ext_of (weak);
  c.strong_zero = strong == 0;
  c.weak_zero = weak == 0;
  return c;
}

/// Classifies a pointer by its block and stored pointer.
inline val_t
classify (ctl_t *block, void const *stored, bool weak)
{
  if (block == nullptr)
    return empty_val (weak);
  sim_mem_t &m = mem ();
  val_t v;
  v.weak = weak;
  v.hslot = -1;
  int const raw_slot = m.obj.slot_of (block);
  if (raw_slot < 0)
    {
      v.slot = 7; // not a block of the arena
      v.gen = -1;
      v.flavor = 9;
      return v;
    }
  v.slot = static_cast<signed char> (raw_slot);
  if (!m.obj.is_live (raw_slot))
    {
      v.gen = -1; // a block that was freed
      v.flavor = 9;
      return v;
    }
  v.gen = static_cast<signed char> (m.obj.generation (v.slot));
  std::uintptr_t const anchor = block->anchor ();
  std::uintptr_t const address = reinterpret_cast<std::uintptr_t> (stored);
  if (address == anchor)
    v.flavor = fl_plain;
  else if (address == anchor + 12u)
    v.flavor = fl_member;
  else if (address == anchor + (std::uintptr_t (1) << 41))
    v.flavor = fl_far;
  else
    v.flavor = 9;
  return v;
}

inline val_t
value_of (sp::shared_ptr<sim_obj_t> const &p)
{
  return classify (sp::detail::access::control (p), p.get (), false);
}

inline val_t
value_of (sp::weak_ptr<sim_obj_t> const &p)
{
  return classify (sp::detail::access::control (p),
                   sp::detail::access::stored (p), true);
}

/// A pointer that names the value @p spec of the block in the base slot.
inline sim_obj_t *
alias_of (sim_obj_t *object, int flavor)
{
  std::uintptr_t const address = reinterpret_cast<std::uintptr_t> (object);
  if (flavor == fl_member)
    return reinterpret_cast<sim_obj_t *> (address + 12u);
  if (flavor == fl_far)
    return reinterpret_cast<sim_obj_t *> (address
                                          + (std::uintptr_t (1) << 41));
  return object;
}

// -- The oracles (the names printed after `oracles=`) --

enum oracle_t
{
  or_SAFETY,
  or_WORD,
  or_NONNEG,
  or_PATH,
  or_NOTE,
  or_W2BOUND,
  or_DEPSHORT,
  or_EID,
  or_CID,
  or_G1,
  or_G2,
  or_BAL,
  or_OPEND,
  or_LOCK,
  or_EXPIRED,
  or_LIN,
  or_I4,
  or_LEDGER,
  or_PEAK,
  or_OUTCOME,
  or_LEAK,
  or_PROGRESS,
  or_HANG,
  or_DEADLOCK,
  or_TIME,
  or_COVERAGE,
  or_NONDET,
  or_count
};

inline char const *
oracle_name (oracle_t o)
{
  static char const *const names[or_count]
      = { "SAFETY",    "WORD",     "NONNEG",  "PATH",     "NOTE", "W2BOUND",
          "DEP-SHORT", "E-ID",     "C-ID",    "G1",       "G2",   "BAL",
          "OP-END",    "LOCK",     "EXPIRED", "LIN",      "I4",   "LEDGER",
          "PEAK",      "OUTCOME",  "LEAK",    "PROGRESS", "HANG", "DEADLOCK",
          "TIME",      "COVERAGE", "NONDET" };
  return names[o];
}

// -- The explorer: baton, threads, depth-first search with a preemption bound
// --

enum
{
  sched_turn = -2, ///< the value of `turn` while the scheduler runs
  max_threads = 8,
  max_notes = 16,
  max_points_per_schedule = 4000,
  max_points_per_op = 300
};

/// A recorded note of a step: the event and the amount.
struct note_t
{
  unsigned event;
  unsigned n;
};

/// The decision log in the form the crash handler can print: the header line
/// and the run-length coded decisions (`0x4.1x9.0x3.2`).
struct crash_line_t
{
  char buffer[32768];
  std::size_t length;
  std::size_t header;
  std::size_t run_start;
  int last;
  int count;

  void
  start (char const *text)
  {
    std::size_t n = std::strlen (text);
    if (n > 600)
      n = 600;
    std::memcpy (buffer, text, n);
    header = n;
    length = n;
    run_start = n;
    last = -1;
    count = 0;
  }

  void
  append (int decision)
  {
    if (length + 40 >= sizeof (buffer))
      return;
    if (decision == last)
      {
        ++count;
        length = run_start;
      }
    else
      {
        if (last >= 0)
          buffer[length++] = '.';
        run_start = length;
        last = decision;
        count = 1;
      }
    buffer[length++] = static_cast<char> ('0' + decision);
    if (count > 1)
      {
        buffer[length++] = 'x';
        char digits[16];
        int k = 0;
        int value = count;
        while (value > 0)
          {
            digits[k++] = static_cast<char> ('0' + value % 10);
            value /= 10;
          }
        while (k > 0)
          buffer[length++] = digits[--k];
      }
    buffer[length] = '\0';
  }

  std::string
  decisions () const
  {
    return std::string (buffer + header, length - header);
  }
};

inline crash_line_t &
crash_line ()
{
  static crash_line_t line;
  return line;
}

extern "C" inline void
sim_crash_signal (int signal_number)
{
  crash_line_t &line = crash_line ();
  if (line.length > 0)
    {
      ssize_t const written = ::write (2, line.buffer, line.length);
      static_cast<void> (written);
      static_cast<void> (::write (2, "\n", 1));
    }
  ::signal (signal_number, SIG_DFL);
  ::raise (signal_number);
}

#if LUMEX_TEST_HAS_ASAN
inline void
sim_crash_death_callback ()
{
  crash_line_t &line = crash_line ();
  if (line.length > 0)
    {
      static_cast<void> (::write (2, line.buffer, line.length));
      static_cast<void> (::write (2, "\n", 1));
    }
}
#endif

/// Installs the crash handlers for the lifetime of the object.
class crash_guard_t
{
public:
  crash_guard_t ()
  {
    struct sigaction action;
    std::memset (&action, 0, sizeof (action));
    action.sa_handler = &sim_crash_signal;
    sigemptyset (&action.sa_mask);
    action.sa_flags = static_cast<int> (SA_RESETHAND);
    ::sigaction (SIGABRT, &action, &old_abort_);
    ::sigaction (SIGSEGV, &action, &old_segv_);
#if LUMEX_TEST_HAS_ASAN
    __sanitizer_set_death_callback (&sim_crash_death_callback);
#endif
  }

  ~crash_guard_t ()
  {
    ::sigaction (SIGABRT, &old_abort_, nullptr);
    ::sigaction (SIGSEGV, &old_segv_, nullptr);
  }

  crash_guard_t (crash_guard_t const &) = delete;
  crash_guard_t &operator= (crash_guard_t const &) = delete;

private:
  struct sigaction old_abort_;
  struct sigaction old_segv_;
};

/// What the explorer runs: the setup, the bodies of the threads, the
/// evaluation at every boundary and the checks at the end of a schedule.
class world_t
{
public:
  virtual ~world_t () {}
  virtual int thread_count () const = 0;
  /// Before the threads exist (the main thread): builds the state.
  virtual void begin () = 0;
  /// The body of the scheduled thread @p id (a worker thread).
  virtual void body (int id) = 0;
  /// Whether thread @p id, paused at @p point, may be resumed.
  virtual bool
  enabled (int id, unsigned point)
  {
    static_cast<void> (id);
    static_cast<void> (point);
    return true;
  }
  /// After a step of @p id from @p from to @p to: the boundary. Returns
  /// whether the step changed shared state (a step that only spins at a
  /// back-off does not).
  virtual bool step (int id, unsigned from, unsigned to, long stepno,
                     int decision)
      = 0;
  /// After the threads are joined: the checks of the end, the teardown.
  virtual void end () = 0;
  /// The hash of the schedule that just ended.
  virtual std::uint64_t
  hash () const
  {
    return 0;
  }
};

class explorer_t;

inline explorer_t *&
current_explorer ()
{
  static explorer_t *current = nullptr;
  return current;
}

/// The search modes.
enum search_mode_t
{
  smode_dfs,
  smode_random,
  smode_replay
};

/// A decision frame of the depth-first search.
struct frame_t
{
  int order[max_threads];
  int count;
  int cur;
  bool cur_enabled;
  int pre;
  int idx;
};

/// The statistics of a search.
struct search_stats_t
{
  long schedules;
  long steps;
  double seconds;
  bool capped;
  std::uint64_t hash;
};

class explorer_t
{
public:
  explorer_t (world_t &world, char const *header)
      : world_ (&world), header_ (header), turn_ (sched_turn),
        draining_ (false), record_ (false), failed_ (false), stop_ (false),
        trace_ (false), time_limit_ (1e18), step_counter_ (0), progress_ (0),
        schedule_no_ (0), bound_ (0)
  {
    current_explorer () = this;
    std::memset (&stats_, 0, sizeof (stats_));
  }

  ~explorer_t () { current_explorer () = nullptr; }

  explorer_t (explorer_t const &) = delete;
  explorer_t &operator= (explorer_t const &) = delete;

  /// Failures are stored instead of aborting (the toy self-test only).
  void
  set_record_mode (bool on)
  {
    record_ = on;
  }

  void
  set_time_limit (double seconds)
  {
    time_limit_ = seconds;
  }

  void
  set_trace (bool on)
  {
    trace_ = on;
  }

  void
  set_bound_text (int bound)
  {
    bound_ = bound;
  }

  bool
  failed () const
  {
    return failed_;
  }

  std::string const &
  failure_text () const
  {
    return failure_text_;
  }

  std::string const &
  failure_decisions () const
  {
    return failure_decisions_;
  }

  long
  schedule_number () const
  {
    return schedule_no_;
  }

  search_stats_t const &
  stats () const
  {
    return stats_;
  }

  bool
  tracing () const
  {
    return trace_;
  }

  int
  bound () const
  {
    return bound_;
  }

  /// The yield of the calling scheduled thread at @p point.
  void
  yield (unsigned point)
  {
    int const id = thread_id ();
    trec_t &r = rec_[id];
    std::unique_lock<std::mutex> lock (mutex_);
    if (draining_)
      return;
    r.point = point;
    turn_ = sched_turn;
    sched_cv_.notify_one ();
    r.cv.wait (lock, [this, id] { return turn_ == id; });
  }

  /// Records a note of the calling thread (before the yield that follows).
  void
  record_note (unsigned event, unsigned n)
  {
    trec_t &r = rec_[thread_id ()];
    if (r.note_count < max_notes)
      {
        r.notes[r.note_count].event = event;
        r.notes[r.note_count].n = n;
        ++r.note_count;
      }
    else
      r.note_overflow = true;
  }

  int
  note_count (int id) const
  {
    return rec_[id].note_count;
  }

  note_t const &
  note_at (int id, int index) const
  {
    return rec_[id].notes[index];
  }

  bool
  note_overflow (int id) const
  {
    return rec_[id].note_overflow;
  }

  /// Marks the schedule failed: prints the report and aborts, or in record
  /// mode stores the text and the decisions and lets the search end.
  void
  report_failure (std::string const &text)
  {
    if (record_)
      {
        if (!failed_)
          {
            failed_ = true;
            failure_text_ = text;
            failure_decisions_ = crash_line ().decisions ();
          }
        stop_ = true;
        return;
      }
    std::fputs (text.c_str (), stderr);
    std::fflush (stderr);
    std::abort ();
  }

  std::string
  header_text () const
  {
    return header_;
  }

  /// The depth-first search up to @p bound preemptions; stops after @p cap
  /// schedules (0: no cap) with `stats ().capped`.
  void
  run_dfs (int bound, long cap)
  {
    bound_ = bound;
    crash_guard_t guard;
    start_clock ();
    stack_.clear ();
    for (;;)
      {
        execute (smode_dfs, nullptr, 0);
        if (stop_)
          break;
        ++stats_.schedules;
        check_time ();
        if (cap > 0 && stats_.schedules >= cap && has_next (bound))
          {
            stats_.capped = true;
            break;
          }
        if (!backtrack (bound))
          break;
      }
    stop_clock ();
  }

  /// @p count random schedules (no bound), the n-th with the seed
  /// @p seed + n.
  void
  run_random (std::uint64_t seed, long count)
  {
    crash_guard_t guard;
    start_clock ();
    for (long i = 0; i < count && !stop_; ++i)
      {
        execute (smode_random, nullptr, seed + static_cast<std::uint64_t> (i));
        ++stats_.schedules;
        check_time ();
      }
    stop_clock ();
  }

  /// Exactly the schedule of the decision string @p text.
  void
  run_replay (std::string const &text)
  {
    crash_guard_t guard;
    start_clock ();
    std::vector<int> decisions = parse_decisions (text);
    execute (smode_replay, &decisions, 0);
    ++stats_.schedules;
    stop_clock ();
  }

  /// `0x4.1x9.0x3.2` to the list of thread ids.
  static std::vector<int>
  parse_decisions (std::string const &text)
  {
    std::vector<int> out;
    std::size_t i = 0;
    while (i < text.size ())
      {
        int const thread = text[i] - '0';
        ++i;
        long count = 1;
        if (i < text.size () && text[i] == 'x')
          {
            ++i;
            count = 0;
            while (i < text.size () && text[i] >= '0' && text[i] <= '9')
              {
                count = count * 10 + (text[i] - '0');
                ++i;
              }
          }
        for (long k = 0; k < count; ++k)
          out.push_back (thread);
        if (i < text.size () && text[i] == '.')
          ++i;
      }
    return out;
  }

private:
  struct trec_t
  {
    trec_t ()
        : point (pt_op_begin), finished (false), note_count (0),
          note_overflow (false), back_off_stamp (0)
    {
    }

    unsigned point;
    bool finished;
    note_t notes[max_notes];
    int note_count;
    bool note_overflow;
    long back_off_stamp;
    std::thread th;
    std::condition_variable cv;
  };

  void
  start_clock ()
  {
    started_ = std::chrono::steady_clock::now ();
  }

  void
  stop_clock ()
  {
    stats_.seconds = elapsed ();
  }

  double
  elapsed () const
  {
    return std::chrono::duration<double> (std::chrono::steady_clock::now ()
                                          - started_)
        .count ();
  }

  void
  check_time ()
  {
    if (elapsed () > time_limit_)
      {
        char text[240];
        std::snprintf (text, sizeof (text),
                       "SIM-FAIL oracles=TIME %s elapsed=%.1f limit=%.1f "
                       "schedules=%ld\n",
                       header_.c_str (), elapsed (), time_limit_,
                       stats_.schedules);
        report_failure (text);
      }
  }

  bool
  is_enabled (int i) const
  {
    if (rec_[i].finished)
      return false;
    if (rec_[i].point == pt_back_off && progress_ <= rec_[i].back_off_stamp)
      return false;
    return world_->enabled (i, rec_[i].point);
  }

  void
  worker (int id)
  {
    thread_id () = id;
    {
      std::unique_lock<std::mutex> lock (mutex_);
      rec_[id].cv.wait (lock, [this, id] { return turn_ == id; });
    }
    world_->body (id);
    {
      std::unique_lock<std::mutex> lock (mutex_);
      rec_[id].finished = true;
      rec_[id].point = pt_finished;
      turn_ = sched_turn;
      sched_cv_.notify_one ();
    }
  }

  /// Resumes thread @p x and waits until it paused or finished.
  void
  resume (int x)
  {
    rec_[x].note_count = 0;
    rec_[x].note_overflow = false;
    std::unique_lock<std::mutex> lock (mutex_);
    turn_ = x;
    rec_[x].cv.notify_one ();
    bool const ok = sched_cv_.wait_for (lock, std::chrono::seconds (10), [this]
                                          { return turn_ == sched_turn; });
    if (!ok)
      {
        char text[240];
        std::snprintf (text, sizeof (text),
                       "SIM-FAIL oracles=HANG %s thread=%d resumed at %s\n",
                       header_.c_str (), x, point_name (rec_[x].point));
        std::fputs (text, stderr);
        std::fflush (stderr);
        std::abort ();
      }
  }

  /// Lets every unfinished thread run to its end with the yields off.
  void
  drain (int n)
  {
    {
      std::unique_lock<std::mutex> lock (mutex_);
      draining_ = true;
    }
    for (int i = 0; i < n; ++i)
      if (!rec_[i].finished)
        resume (i);
  }

  /// One schedule from scratch.
  void
  execute (search_mode_t mode, std::vector<int> const *replay,
           std::uint64_t seed)
  {
    char header[300];
    std::snprintf (header, sizeof (header),
                   "SIM-CRASH %s bound=%d schedule=%ld decisions=",
                   header_.c_str (), bound_, stats_.schedules + 1);
    crash_line ().start (header);
    world_->begin ();
    int const n = world_->thread_count ();
    for (int i = 0; i < n; ++i)
      {
        rec_[i].point = pt_op_begin;
        rec_[i].finished = false;
        rec_[i].note_count = 0;
        rec_[i].note_overflow = false;
        rec_[i].back_off_stamp = 0;
      }
    {
      std::unique_lock<std::mutex> lock (mutex_);
      turn_ = sched_turn;
      draining_ = false;
    }
    for (int i = 0; i < n; ++i)
      rec_[i].th = std::thread (&explorer_t::worker, this, i);
    split_test::rng_t rng (seed + 1u);
    step_counter_ = 0;
    progress_ = 0;
    int last = -1;
    int preemptions = 0;
    std::size_t depth = 0;
    long points = 0;
    for (;;)
      {
        int enabled[max_threads];
        int enabled_count = 0;
        bool unfinished = false;
        bool last_enabled = false;
        for (int i = 0; i < n; ++i)
          {
            if (rec_[i].finished)
              continue;
            unfinished = true;
            if (is_enabled (i))
              {
                enabled[enabled_count++] = i;
                if (i == last)
                  last_enabled = true;
              }
          }
        if (!unfinished)
          break;
        if (enabled_count == 0)
          {
            char text[300];
            std::snprintf (text, sizeof (text),
                           "SIM-FAIL oracles=DEADLOCK %s no thread is "
                           "enabled after %ld steps decisions=%s\n",
                           header_.c_str (), step_counter_,
                           crash_line ().decisions ().c_str ());
            report_failure (text);
            drain (n);
            break;
          }
        int choice = -1;
        if (mode == smode_dfs)
          {
            frame_t frame;
            frame.count = 0;
            if (last_enabled)
              frame.order[frame.count++] = last;
            else
              frame.order[frame.count++] = enabled[0];
            for (int k = 0; k < enabled_count; ++k)
              if (enabled[k] != frame.order[0])
                frame.order[frame.count++] = enabled[k];
            frame.cur = last;
            frame.cur_enabled = last_enabled;
            frame.pre = preemptions;
            frame.idx = 0;
            if (depth < stack_.size ())
              {
                frame_t const &kept = stack_[depth];
                bool same = kept.count == frame.count && kept.cur == frame.cur
                            && kept.pre == frame.pre;
                for (int k = 0; same && k < frame.count; ++k)
                  same = kept.order[k] == frame.order[k];
                if (!same)
                  {
                    char text[300];
                    std::snprintf (text, sizeof (text),
                                   "SIM-FAIL oracles=NONDET %s the schedule "
                                   "diverged from its prefix at decision "
                                   "%zu\n",
                                   header_.c_str (), depth);
                    report_failure (text);
                    drain (n);
                    break;
                  }
              }
            else
              stack_.push_back (frame);
            frame_t const &use = stack_[depth];
            choice = use.order[use.idx];
            preemptions
                = use.pre + ((use.cur_enabled && choice != last) ? 1 : 0);
          }
        else if (mode == smode_random)
          choice = enabled[rng.next ()
                           % static_cast<std::uint32_t> (enabled_count)];
        else
          {
            if (depth >= replay->size ())
              {
                char text[300];
                std::snprintf (text, sizeof (text),
                               "SIM-FAIL oracles=NONDET %s replay ended "
                               "before the threads did\n",
                               header_.c_str ());
                report_failure (text);
                drain (n);
                break;
              }
            choice = (*replay)[depth];
            bool ok = false;
            for (int k = 0; k < enabled_count; ++k)
              ok = ok || enabled[k] == choice;
            if (!ok)
              {
                char text[300];
                std::snprintf (text, sizeof (text),
                               "SIM-FAIL oracles=NONDET %s replay diverged: "
                               "thread %d is not enabled at decision %zu\n",
                               header_.c_str (), choice, depth);
                report_failure (text);
                drain (n);
                break;
              }
          }
        ++depth;
        crash_line ().append (choice);
        unsigned const from = rec_[choice].point;
        resume (choice);
        unsigned const to = rec_[choice].point;
        ++step_counter_;
        ++stats_.steps;
        last = choice;
        ++points;
        // A step that changed no shared state only spins: it does not let
        // another backing-off thread go on (fairness: two threads that spin
        // for each other would starve the rest forever).
        if (world_->step (choice, from, to, step_counter_,
                          static_cast<int> (depth - 1)))
          ++progress_;
        if (to == pt_back_off)
          rec_[choice].back_off_stamp = progress_;
        if (stop_)
          {
            drain (n);
            break;
          }
        if (points > max_points_per_schedule)
          {
            char text[300];
            std::snprintf (text, sizeof (text),
                           "SIM-FAIL oracles=PROGRESS %s more than %d points "
                           "in one schedule decisions=%s\n",
                           header_.c_str (), max_points_per_schedule,
                           crash_line ().decisions ().c_str ());
            report_failure (text);
            drain (n);
            break;
          }
      }
    for (int i = 0; i < n; ++i)
      if (rec_[i].th.joinable ())
        rec_[i].th.join ();
    stats_.hash ^= world_->hash ();
    world_->end ();
  }

  /// Whether a feasible alternative remains in the stack.
  bool
  has_next (int bound) const
  {
    for (std::size_t i = stack_.size (); i > 0; --i)
      {
        frame_t const &f = stack_[i - 1];
        for (int j = f.idx + 1; j < f.count; ++j)
          {
            int const cost = (f.cur_enabled && f.order[j] != f.cur) ? 1 : 0;
            if (f.pre + cost <= bound)
              return true;
          }
      }
    return false;
  }

  /// Moves to the next unexplored schedule; false when the search is done.
  bool
  backtrack (int bound)
  {
    while (!stack_.empty ())
      {
        frame_t &f = stack_.back ();
        int j = f.idx + 1;
        for (; j < f.count; ++j)
          {
            int const cost = (f.cur_enabled && f.order[j] != f.cur) ? 1 : 0;
            if (f.pre + cost <= bound)
              break;
          }
        if (j < f.count)
          {
            f.idx = j;
            return true;
          }
        stack_.pop_back ();
      }
    return false;
  }

  world_t *world_;
  std::string header_;
  std::mutex mutex_;
  std::condition_variable sched_cv_;
  int turn_;
  bool draining_;
  bool record_;
  bool failed_;
  bool stop_;
  bool trace_;
  double time_limit_;
  long step_counter_;
  long progress_;
  long schedule_no_;
  int bound_;
  trec_t rec_[max_threads];
  std::vector<frame_t> stack_;
  search_stats_t stats_;
  std::string failure_text_;
  std::string failure_decisions_;
  std::chrono::steady_clock::time_point started_;
};

/// The yield of the calling scheduled thread; nothing elsewhere.
inline void
sim_yield (unsigned point)
{
  if (thread_id () < 0)
    return;
  current_explorer ()->yield (point);
}

// -- The policy: the hooks are the scheduling points --

/// The word-address probe: `guess` stores the address of the word it reads
/// here (once), so that the oracles can read the word of a cell without
/// knowing the layout of the cell.
inline ::lumex::core::atomic::dwcas::dwcas_word const **&
probe_destination ()
{
  static ::lumex::core::atomic::dwcas::dwcas_word const **destination
      = nullptr;
  return destination;
}

template <std::uint32_t Reserve, std::uint32_t Limit> struct sim_tag_t
{
};

/**
 * @brief The policy of the explorer: the ledger counters of the tag, and
 * every hook a scheduling point on a scheduled thread (nothing on any other
 * thread, where only the ledger counts).
 */
template <std::uint32_t Reserve, std::uint32_t Limit>
struct sim_policy_t
    : split_test::ledger_policy_t<sim_tag_t<Reserve, Limit>, Limit, Reserve>
{
  typedef split_test::ledger_policy_t<sim_tag_t<Reserve, Limit>, Limit,
                                      Reserve>
      base_type;

  static void
  at (split_point_t point) noexcept
  {
    sim_yield (static_cast<unsigned> (point));
  }

  static void
  note (split_event_t event, std::uint32_t n) noexcept
  {
    base_type::note (event, n);
    if (thread_id () < 0)
      return;
    current_explorer ()->record_note (static_cast<unsigned> (event), n);
    sim_yield (100u + static_cast<unsigned> (event));
  }

  static split_value_t
  guess (::lumex::core::atomic::dwcas::dwcas_word const &w) noexcept
  {
    ::lumex::core::atomic::dwcas::dwcas_word const **&destination
        = probe_destination ();
    if (destination != nullptr)
      {
        *destination = &w;
        destination = nullptr;
      }
    return w.speculative_load ();
  }

  static void
  back_off () noexcept
  {
    sim_yield (pt_back_off);
  }
};

typedef sim_policy_t<8u, 0xFFFFFFu> sim_r8_t;
typedef sim_policy_t<0u, 0xFFFFFFu> sim_r0_t;
typedef sim_policy_t<8u, 2u> sim_l2_t;

// -- The cells --

typedef sp::shared_ptr<sim_obj_t> strong_t;
typedef sp::weak_ptr<sim_obj_t> weak_t;

/// The cell as the scenarios use it: the operations of the op vocabulary.
class cell_ops_t
{
public:
  virtual ~cell_ops_t () {}
  /// Loads and discards (the word-address probe).
  virtual void probe () = 0;
  virtual void ld (strong_t &out) = 0;
  virtual void st (strong_t &in) = 0;
  virtual void xc (strong_t &in, strong_t &out) = 0;
  virtual bool cas (strong_t &expected, strong_t &desired) = 0;
  virtual void wld (weak_t &out) = 0;
  virtual void wst (weak_t &in) = 0;
};

template <typename Policy> class shared_cell_ops_t : public cell_ops_t
{
public:
  explicit shared_cell_ops_t (strong_t const &initial) : cell_ (initial) {}

  void
  probe () override
  {
    strong_t discarded = cell_.load (std::memory_order_seq_cst);
  }

  void
  ld (strong_t &out) override
  {
    out = cell_.load (std::memory_order_seq_cst);
  }

  void
  st (strong_t &in) override
  {
    cell_.store (std::move (in), std::memory_order_seq_cst);
  }

  void
  xc (strong_t &in, strong_t &out) override
  {
    out = cell_.exchange (std::move (in), std::memory_order_seq_cst);
  }

  bool
  cas (strong_t &expected, strong_t &desired) override
  {
    return cell_.compare_exchange_strong (expected, std::move (desired),
                                          std::memory_order_seq_cst,
                                          std::memory_order_seq_cst);
  }

  void
  wld (weak_t &) override
  {
    std::abort ();
  }

  void
  wst (weak_t &) override
  {
    std::abort ();
  }

private:
  asp::Detail::split_count_cell<strong_t, Policy> cell_;
};

template <typename Policy> class weak_cell_ops_t : public cell_ops_t
{
public:
  explicit weak_cell_ops_t (weak_t const &initial) : cell_ (initial) {}

  void
  probe () override
  {
    weak_t discarded = cell_.load (std::memory_order_seq_cst);
  }

  void
  ld (strong_t &) override
  {
    std::abort ();
  }

  void
  st (strong_t &) override
  {
    std::abort ();
  }

  void
  xc (strong_t &, strong_t &) override
  {
    std::abort ();
  }

  bool
  cas (strong_t &, strong_t &) override
  {
    std::abort ();
  }

  void
  wld (weak_t &out) override
  {
    out = cell_.load (std::memory_order_seq_cst);
  }

  void
  wst (weak_t &in) override
  {
    cell_.store (std::move (in), std::memory_order_seq_cst);
  }

private:
  asp::Detail::split_count_cell<weak_t, Policy> cell_;
};

/// What a policy contributes to a run.
struct policy_info_t
{
  policy_id_t id;
  std::uint32_t reserve;
  std::uint32_t limit;
  split_test::ledger_counters_t *counters;
  cell_ops_t *(*create_shared) (void *storage, strong_t const &initial);
  cell_ops_t *(*create_weak) (void *storage, weak_t const &initial);
};

template <typename Policy>
inline cell_ops_t *
create_shared_cell (void *storage, strong_t const &initial)
{
  return ::new (storage) shared_cell_ops_t<Policy> (initial);
}

template <typename Policy>
inline cell_ops_t *
create_weak_cell (void *storage, weak_t const &initial)
{
  return ::new (storage) weak_cell_ops_t<Policy> (initial);
}

template <typename Policy>
inline policy_info_t
make_policy_info (policy_id_t id, std::uint32_t reserve, std::uint32_t limit)
{
  policy_info_t info;
  info.id = id;
  info.reserve = reserve;
  info.limit = limit;
  info.counters = &Policy::counters ();
  info.create_shared = &create_shared_cell<Policy>;
  info.create_weak = &create_weak_cell<Policy>;
  return info;
}

inline policy_info_t
policy_info (policy_id_t id)
{
  switch (id)
    {
    case pol_R0:
      return make_policy_info<sim_r0_t> (pol_R0, 0u, 0xFFFFFFu);
    case pol_L2:
      return make_policy_info<sim_l2_t> (pol_L2, 8u, 2u);
    case pol_R8:
    case pol_count:
    default:
      return make_policy_info<sim_r8_t> (pol_R8, 8u, 0xFFFFFFu);
    }
}

// -- The ghost model --

enum role_t
{
  r_none,
  r_ld,  ///< a load
  r_wr,  ///< a store or an exchange (`swap_out`)
  r_cs,  ///< a compare-exchange with a block or empty `expected`
  r_ch,  ///< a compare-exchange with a holder-shaped `expected`
  r_user ///< reset, lock, expired, a new block
};

enum pin_t
{
  pin_none,
  pin_in,  ///< the tag is in the word
  pin_left ///< not in the word: its unit is owed (unpaid)
};

enum counted_t
{
  cnt_no,
  cnt_live,    ///< the count step of L6 is done
  cnt_combined ///< counted and paid in one step (L9)
};

/// The ghost of one scheduled thread (or of the main pseudo-thread).
struct gthread_t
{
  role_t role;
  bool nested; ///< a load that runs inside a compare-exchange
  bool exchange;
  bool ch_nomatch; ///< a holder compare-exchange settling its pin
  pin_t pin;
  val_t pinned;
  std::uint64_t pinned_lo;
  counted_t counted;
  bool in_iii;
  int dep;
  val_t dep_val;
  int max_need;
  int pending_untransfer;
  int pending_settle;
  val_t lag;
  val_t xres;
  bool xres_holder_live;
  int d_param;
  int d_unit;
  int d_img;
  val_t arg;
  val_t expected;
  bool built;
  val_t cas_current;
  val_t load_result;
  int swap_hist;
  long op_first_step;
  long load_first_step;
  int op_steps;

  void
  clear (bool weak)
  {
    role = r_none;
    nested = false;
    exchange = false;
    ch_nomatch = false;
    pin = pin_none;
    pinned = empty_val (weak);
    pinned_lo = 0;
    counted = cnt_no;
    in_iii = false;
    dep = 0;
    dep_val = empty_val (weak);
    max_need = 0;
    pending_untransfer = 0;
    pending_settle = 0;
    lag = empty_val (weak);
    xres = empty_val (weak);
    xres_holder_live = false;
    d_param = 0;
    d_unit = 0;
    d_img = 0;
    arg = empty_val (weak);
    expected = empty_val (weak);
    built = false;
    cas_current = empty_val (weak);
    load_result = empty_val (weak);
    swap_hist = -1;
    op_first_step = 0;
    load_first_step = 0;
    op_steps = 0;
  }
};

inline bool
ld_like (gthread_t const &g)
{
  return g.role == r_ld || g.nested;
}

/// The history of the values of a cell.
struct hist_t
{
  val_t value;
  long step;
};

enum
{
  max_hist = 48,
  max_lin = 96,
  max_fail = 12,
  ring_size = 40,
  main_thread = 7,
  main_slots = 4
};

struct gcell_t
{
  val_t value;
  int tags[16];
  int ntags;
  hist_t hist[max_hist];
  int nhist;
};

/// A load (or a failed compare-exchange) for the linearizability check.
struct lin_rec_t
{
  int thread;
  val_t value;
  long
      first; ///< the first step of the load: the boundary before it is first-1
  long last; ///< the last step: the boundary after it
};

/// A recorded result of an operation.
struct result_t
{
  char kind; ///< 'v' a value, 'c' a compare-exchange, 'e' expired
  val_t value;
  bool flag;
};

/// The run-time state of one thread: its smart pointers and its ghost.
struct thread_rt_t
{
  strong_t held[4];
  weak_t weak[4];
  int op_index;
  result_t results[8];
  int nresults;
  bool result_fault;
  gthread_t g;

  void
  reset (bool weak_engine)
  {
    for (int i = 0; i < 4; ++i)
      {
        held[i].reset ();
        weak[i].reset ();
      }
    op_index = 0;
    nresults = 0;
    result_fault = false;
    g.clear (weak_engine);
  }
};

/// What a thread could do next, sampled after each of its steps.
struct snapshot_t
{
  val_t held[4];
  val_t weak[4];
};

struct failure_t
{
  oracle_t oracle;
  char text[260];
};

/// One line of the step history kept for the report.
struct step_snap_t
{
  int decision;
  int thread;
  unsigned from;
  unsigned to;
  val_t value;
  int ticks;
  long count[3];
  long ext[3];
};

/// The decoded word of a cell.
struct wdesc_t
{
  bool empty;
  bool holder;
  bool valid;
  int ticks;
  val_t value;
};

/// The quantities of an object block (2.3 of the oracle note).
struct quant_t
{
  int U, S, D, X, CNT, CC, LEFTNC;
  int Wi, Wii, Wiii;
  int OWING;
  int LEFTW;
  int DEP;
  int weak_refs; ///< weak pointers, pins and deposits naming the block
  counts_t counts;
};

// -- The world of a scenario: threads, ghost, oracles --

/// The environment of a run (the knobs of the debug output).
inline bool
env_flag (char const *name)
{
  char const *value = std::getenv (name);
  return value != nullptr && value[0] != '\0' && value[0] != '0';
}

class scenario_world_t : public world_t
{
public:
  scenario_world_t (scenario_def_t const &def, policy_info_t const &info)
      : def_ (&def), info_ (info), weak_engine_ (def.engine == engine_weak),
        cell_ (nullptr), word_ (nullptr), hash_ (0),
        trace_all_ (env_flag ("LUMEX_SIM_TRACE")), explorer_ (nullptr)
  {
    static_assert (sizeof (shared_cell_ops_t<sim_r8_t>) <= storage_bytes
                       && sizeof (weak_cell_ops_t<sim_r8_t>) <= storage_bytes,
                   "the cell fits its storage");
  }

  ~scenario_world_t () override {}

  void
  set_explorer (explorer_t *explorer)
  {
    explorer_ = explorer;
  }

  void
  set_trace (bool on)
  {
    trace_all_ = on;
  }

  std::set<std::string> const &
  seen () const
  {
    return seen_;
  }

  int
  thread_count () const override
  {
    return def_->nthreads;
  }

  std::uint64_t
  hash () const override
  {
    return hash_;
  }

  // ---- the schedule: setup ----

  void
  begin () override
  {
    sim_mem_t &m = mem ();
    m.obj.reset ();
    m.hold.reset ();
    m.live_objects = 0;
    m.double_dispose = 0;
    m.unexpected_alloc = 0;
    m.unexpected_count = 0;
    for (int t = 0; t < max_threads; ++t)
      {
        rt_[t].reset (weak_engine_);
        snap_[t] = empty_snapshot ();
        executing_op_[t] = 0;
      }
    int const n = def_->nthreads;
    ++engine_scope ();
    strong_t base[6];
    for (int s = 0; s < 6; ++s)
      if (((def_->blocks >> s) & 1u) != 0u)
        base[s] = make_block (s);
    for (int t = 0; t < n; ++t)
      {
        thread_def_t const &td = def_->threads[t];
        for (int i = 0; i < 4; ++i)
          {
            if (td.held[i].slot >= 0)
              rt_[t].held[i] = alias_strong (base, td.held[i]);
            if (td.weak[i].slot >= 0)
              rt_[t].weak[i] = weak_t (alias_strong (base, td.weak[i]));
          }
      }
    {
      strong_t const initial = alias_strong (base, def_->cell);
      if (weak_engine_)
        cell_ = info_.create_weak (storage_, weak_t (initial));
      else
        cell_ = info_.create_shared (storage_, initial);
    }
    for (int s = 0; s < 6; ++s)
      {
        if (((def_->blocks >> s) & 1u) == 0u)
          continue;
        if (((def_->main_keeps >> s) & 1u) != 0u && s < main_slots)
          rt_[main_thread].held[s] = std::move (base[s]);
        else
          base[s].reset ();
      }
    // The word-address probe: layout-free, the cell tells where its word is.
    ::lumex::core::atomic::dwcas::dwcas_word const *address = nullptr;
    probe_destination () = &address;
    cell_->probe ();
    probe_destination () = nullptr;
    word_ = address;
    --engine_scope ();
    info_.counters->reset ();
    m.hold.log_count = 0;
    // The ghost starts from the actual state.
    wdesc_t const start = describe (read_word ());
    gc_.value = start.value;
    gc_.ntags = 0;
    gc_.nhist = 1;
    gc_.hist[0].value = start.value;
    gc_.hist[0].step = 0;
    word_prev_ = read_word ();
    signature_ = state_signature ();
    stepno_ = 0;
    nfail_ = 0;
    nlin_ = 0;
    ring_count_ = 0;
    hash_ = 1469598103934665603ull;
    for (int t = 0; t < n; ++t)
      snap_[t] = snapshot_of (t);
    for (int s = 0; s < 6; ++s)
      owners_[s] = 0;
    refresh_owners ();
    if (!veq (start.value, expected_initial_value ()))
      {
        fail (or_WORD, "the initial word decodes to %s, the scenario names %s",
              label_text (start.value).c_str (),
              label_text (expected_initial_value ()).c_str ());
        report ("setup", 0, 0, 0);
      }
  }

  // ---- the worker ----

  void
  body (int id) override
  {
    thread_rt_t &rt = rt_[id];
    thread_def_t const &td = def_->threads[id];
    for (int k = 0; k < td.nops; ++k)
      {
        rt.op_index = k;
        if (k > 0)
          sim_yield (pt_op_begin);
        ++engine_scope ();
        run_op (rt, td.ops[k]);
        --engine_scope ();
      }
  }

  bool
  enabled (int id, unsigned point) override
  {
    if (point != pt_op_begin)
      return true;
    op_t const &op = def_->threads[id].ops[rt_[id].op_index];
    if (op.kind == op_rc)
      return !mem ().obj.is_live (op.b);
    return true;
  }

  // ---- the boundary ----

  bool
  step (int id, unsigned from, unsigned to, long stepno, int decision) override
  {
    stepno_ = stepno;
    nfail_ = 0;
    split_value_t const wb = word_prev_;
    split_value_t const wa = read_word ();
    gthread_t &g = rt_[id].g;
    ++g.op_steps;
    if (g.op_steps > max_points_per_op)
      fail (or_PROGRESS, "thread %d took more than %d points in one operation",
            id, static_cast<int> (max_points_per_op));
    transition (id, from, to, wb, wa);
    check_boundary ();
    record_step (decision, id, from, to);
    word_prev_ = wa;
    snap_[id] = snapshot_of (id);
    mem ().hold.log_count = 0;
    mem ().obj.log_count = 0;
    if (trace_all_ || (explorer_ != nullptr && explorer_->tracing ()))
      print_step_line (decision, id, from, to);
    if (nfail_ > 0)
      report ("step", decision, from, to, id);
    std::uint64_t const signature = state_signature ();
    bool const changed = signature != signature_;
    signature_ = signature;
    return changed;
  }

  /// A hash of the shared state: the word, the counters of every live block
  /// and holder.
  std::uint64_t
  state_signature () const
  {
    std::uint64_t h = 1469598103934665603ull;
    std::uint64_t const parts[2] = { read_word ().lo, read_word ().hi };
    for (int i = 0; i < 2; ++i)
      {
        h ^= parts[i];
        h *= 1099511628211ull;
      }
    sim_mem_t &m = mem ();
    for (int s = 0; s < 6; ++s)
      if (m.obj.is_live (s))
        {
          h ^= obj_block (s)->strong_counter ().load () + 0x9E37u * (s + 1);
          h *= 1099511628211ull;
          h ^= obj_block (s)->weak_counter ().load ();
          h *= 1099511628211ull;
        }
    for (int s = 0; s < hold_arena_t::slot_count; ++s)
      if (m.hold.is_live (s))
        {
          h ^= hold_block (s)->strong_counter ().load () + 0x7F4Au * (s + 1);
          h *= 1099511628211ull;
        }
    return h;
  }

  void
  end () override
  {
    // The threads are joined: the checks of the end, then the teardown.
    if (nfail_ == 0 && !aborted_run ())
      {
        nfail_ = 0;
        check_end ();
        if (nfail_ > 0)
          report ("end", -1, 0, 0);
      }
    teardown ();
  }

private:
  enum
  {
    storage_bytes = 128
  };

  bool
  aborted_run () const
  {
    return explorer_ != nullptr && explorer_->failed ();
  }

  // ---- helpers: values, words ----

  static snapshot_t
  empty_snapshot ()
  {
    snapshot_t s;
    for (int i = 0; i < 4; ++i)
      {
        s.held[i] = empty_val (false);
        s.weak[i] = empty_val (true);
      }
    return s;
  }

  snapshot_t
  snapshot_of (int t) const
  {
    snapshot_t s;
    for (int i = 0; i < 4; ++i)
      {
        s.held[i] = value_of (rt_[t].held[i]);
        s.weak[i] = value_of (rt_[t].weak[i]);
      }
    return s;
  }

  static strong_t
  alias_strong (strong_t const *base, vspec_t const &spec)
  {
    strong_t const &b = base[spec.slot];
    if (spec.flavor == fl_plain)
      return b;
    return strong_t (b, alias_of (b.get (), spec.flavor));
  }

  val_t
  expected_initial_value () const
  {
    val_t v;
    v.slot = static_cast<signed char> (def_->cell.slot);
    v.gen = 1;
    v.flavor = static_cast<signed char> (def_->cell.flavor);
    v.hslot = -1;
    v.weak = weak_engine_;
    return v;
  }

  split_value_t
  read_word () const
  {
    return word_->speculative_load ();
  }

  static int
  ticks_of (split_value_t w)
  {
    return word::is_empty (w) ? 0 : static_cast<int> (word::ticks_of (w));
  }

  static bool
  same_word (split_value_t a, split_value_t b)
  {
    return a.lo == b.lo && a.hi == b.hi;
  }

  wdesc_t
  describe (split_value_t w) const
  {
    wdesc_t d;
    d.valid = true;
    d.empty = word::is_empty (w);
    d.holder = false;
    d.ticks = 0;
    d.value = empty_val (weak_engine_);
    if (d.empty)
      {
        d.valid = w.hi == 0u;
        return d;
      }
    d.ticks = static_cast<int> (word::ticks_of (w));
    sim_mem_t &m = mem ();
    ctl_t *const block = word::block_of (w);
    if (word::is_holder (w))
      {
        d.holder = true;
        int const hs = m.hold.slot_of (block);
        if (hs < 0 || !m.hold.is_live (hs)
            || (w.hi >> word::offset_shift ()) != 0u)
          {
            d.valid = false;
            return d;
          }
        ctl_t *owner = nullptr;
        void *stored = nullptr;
        if (weak_engine_)
          {
            weak_holder_t const *const h
                = static_cast<weak_holder_t const *> (block);
            owner = h->owner ();
            stored = h->pointer ();
          }
        else
          {
            holder_t const *const h = static_cast<holder_t const *> (block);
            owner = h->owner ();
            stored = h->pointer ();
          }
        val_t v = classify (owner, stored, weak_engine_);
        v.hslot = static_cast<signed char> (hs);
        d.value = v;
        if (v.flavor != fl_far || v.slot >= 6)
          d.valid = false;
        return d;
      }
    int const s = m.obj.slot_of (block);
    if (s < 0 || !m.obj.is_live (s))
      {
        d.valid = false;
        return d;
      }
    std::int64_t const offset
        = static_cast<std::int64_t> (word::offset_of (w));
    val_t v;
    v.slot = static_cast<signed char> (s);
    v.gen = static_cast<signed char> (m.obj.generation (s));
    v.hslot = -1;
    v.weak = weak_engine_;
    if (offset == 0)
      v.flavor = fl_plain;
    else if (offset == 12)
      v.flavor = fl_member;
    else
      {
        v.flavor = 9;
        d.valid = false;
      }
    d.value = v;
    return d;
  }

  // ---- failures ----

  void
  fail (oracle_t oracle, char const *format, ...)
  {
    if (nfail_ >= max_fail)
      return;
    failure_t &f = failures_[nfail_++];
    f.oracle = oracle;
    std::va_list args;
    va_start (args, format);
    std::vsnprintf (f.text, sizeof (f.text), format, args);
    va_end (args);
  }

  void
  path_fail (int id, unsigned from, unsigned to, char const *what)
  {
    fail (or_PATH, "thread %d: %s -> %s: %s", id, point_name (from),
          point_name (to), what);
  }

  void
  require (bool ok, int id, unsigned from, unsigned to, char const *what)
  {
    if (!ok)
      path_fail (id, from, to, what);
  }

  static bool
  is_end (unsigned e)
  {
    return e == pt_op_begin || e == pt_finished;
  }

  // ---- the operations of a worker ----

  void
  push_value (thread_rt_t &rt, val_t const &v)
  {
    if (rt.nresults >= 8)
      return;
    result_t &r = rt.results[rt.nresults++];
    r.kind = 'v';
    r.value = v;
    r.flag = false;
  }

  void
  push_flag (thread_rt_t &rt, char kind, bool flag, val_t const &v)
  {
    if (rt.nresults >= 8)
      return;
    result_t &r = rt.results[rt.nresults++];
    r.kind = kind;
    r.value = v;
    r.flag = flag;
  }

  /// SAFETY (c): a strong result names a live block and, when it is the object
  /// itself, an object with the live magic.
  void
  check_result (thread_rt_t &rt, strong_t const &p)
  {
    if (p.get () == nullptr && sp::detail::access::control (p) == nullptr)
      return;
    val_t const v = value_of (p);
    if (v.gen < 0 || v.slot >= 6)
      {
        rt.result_fault = true;
        return;
      }
    if (v.flavor == fl_plain && p.get ()->magic != live_magic ())
      rt.result_fault = true;
  }

  void
  run_op (thread_rt_t &rt, op_t const &op)
  {
    switch (op.kind)
      {
      case op_ld:
        cell_->ld (rt.held[op.a]);
        check_result (rt, rt.held[op.a]);
        push_value (rt, value_of (rt.held[op.a]));
        break;
      case op_st:
        cell_->st (rt.held[op.a]);
        break;
      case op_xc:
        cell_->xc (rt.held[op.a], rt.held[op.b]);
        check_result (rt, rt.held[op.b]);
        push_value (rt, value_of (rt.held[op.b]));
        break;
      case op_cas:
        {
          bool const ok = cell_->cas (rt.held[op.a], rt.held[op.b]);
          check_result (rt, rt.held[op.a]);
          push_flag (rt, 'c', ok, value_of (rt.held[op.a]));
        }
        break;
      case op_rs:
        rt.held[op.a].reset ();
        break;
      case op_lk:
        rt.held[op.b] = rt.weak[op.a].lock ();
        check_result (rt, rt.held[op.b]);
        push_value (rt, value_of (rt.held[op.b]));
        break;
      case op_ex:
        {
          bool const e = rt.weak[op.a].expired ();
          push_flag (rt, 'e', e, empty_val (false));
        }
        break;
      case op_rc:
        rt.held[op.a] = make_block (op.b);
        break;
      case op_wld:
        cell_->wld (rt.weak[op.a]);
        push_value (rt, value_of (rt.weak[op.a]));
        break;
      case op_wst:
        cell_->wst (rt.weak[op.a]);
        break;
      case op_wrs:
        rt.weak[op.a].reset ();
        break;
      }
  }

  // ---- the ghost: tags ----

  int
  find_tag (int id) const
  {
    for (int i = 0; i < gc_.ntags; ++i)
      if (gc_.tags[i] == id)
        return i;
    return -1;
  }

  void
  add_tag (int id)
  {
    if (gc_.ntags < 16)
      gc_.tags[gc_.ntags++] = id;
  }

  void
  remove_tag_at (int index)
  {
    for (int i = index; i + 1 < gc_.ntags; ++i)
      gc_.tags[i] = gc_.tags[i + 1];
    --gc_.ntags;
  }

  // ---- the ghost: transitions ----

  /// The amount of the last note of the step, if it has the event @p event.
  int
  last_note (int id, split_event_t event) const
  {
    explorer_t const &ex = *current_explorer ();
    int const n = ex.note_count (id);
    if (n == 0)
      return -1;
    note_t const &note = ex.note_at (id, n - 1);
    if (note.event != static_cast<unsigned> (event))
      return -1;
    return static_cast<int> (note.n);
  }

  void
  add_lin (int id, val_t const &value, long first)
  {
    if (nlin_ >= max_lin)
      return;
    lin_rec_t &r = lin_[nlin_++];
    r.thread = id;
    r.value = value;
    r.first = first;
    r.last = stepno_;
  }

  /// The operation about to run starts: the role, the argument, its units.
  void
  begin_op (int id, unsigned to, int op_index)
  {
    thread_rt_t &rt = rt_[id];
    gthread_t &g = rt.g;
    op_t const &op = def_->threads[id].ops[op_index];
    snapshot_t const &sn = snap_[id];
    g.clear (weak_engine_);
    g.op_first_step = stepno_;
    g.op_steps = 1;
    sim_mem_t &m = mem ();
    switch (op.kind)
      {
      case op_ld:
      case op_wld:
        g.role = r_ld;
        require (to == pt_load_read, id, pt_op_begin, to,
                 "a load starts with a read");
        break;
      case op_st:
      case op_xc:
      case op_wst:
        g.role = r_wr;
        g.exchange = op.kind == op_xc;
        g.arg = op.kind == op_wst ? sn.weak[op.a] : sn.held[op.a];
        if (!is_empty_val (g.arg))
          {
            if (g.arg.flavor == fl_far)
              {
                g.d_param = 1;
                g.d_img = 1;
                if (m.hold.log_count >= 1)
                  g.arg.hslot = static_cast<signed char> (m.hold.log_slot[0]);
                else
                  fail (or_PATH,
                        "thread %d: a holder-shaped argument made no "
                        "holder",
                        id);
              }
            else
              g.d_unit = 1;
          }
        require (to == pt_swap, id, pt_op_begin, to,
                 "a store starts with the swap");
        break;
      case op_cas:
        g.expected = sn.held[op.a];
        g.arg = sn.held[op.b];
        g.role = g.expected.flavor == fl_far ? r_ch : r_cs;
        if (!is_empty_val (g.arg))
          {
            if (g.arg.flavor == fl_far)
              g.d_param = 1;
            else
              g.d_unit = 1;
          }
        require (to == pt_cas_read, id, pt_op_begin, to,
                 "a compare-exchange starts with a read");
        break;
      case op_rs:
      case op_lk:
      case op_ex:
      case op_rc:
      case op_wrs:
        g.role = r_user;
        break;
      }
  }

  /// A load returned @p value.
  void
  end_load (int id, unsigned from, unsigned to, val_t const &value)
  {
    gthread_t &g = rt_[id].g;
    if (!is_empty_val (value) && g.counted == cnt_no)
      path_fail (id, from, to, "a load returned a value without counting it");
    g.load_result = value;
    if (g.nested)
      add_lin (id, value, g.load_first_step);
    g.counted = cnt_no;
    g.in_iii = false;
    if (g.pin != pin_none)
      fail (or_OPEND, "thread %d: a load returned with its pin still %s", id,
            g.pin == pin_in ? "in the word" : "owed");
    if (!g.nested)
      {
        require (is_end (to), id, from, to, "a load ends its operation");
        return;
      }
    g.cas_current = value;
    g.nested = false;
    if (veq (value, g.expected))
      require (to == pt_cas_read, id, from, to,
               "an equivalent result retries the compare-exchange");
    else if (g.built)
      require (to == pt_release, id, from, to,
               "a failed compare-exchange drops its image");
    else
      require (is_end (to), id, from, to,
               "a failed compare-exchange without image ends");
  }

  /// A nested load begins (inside a compare-exchange).
  void
  begin_nested (int id)
  {
    gthread_t &g = rt_[id].g;
    g.nested = true;
    g.counted = cnt_no;
    g.load_first_step = stepno_;
  }

  /// The swap of thread @p id succeeded (the transition of 2.2).
  void
  swap_transition (int id, split_value_t wa)
  {
    gthread_t &g = rt_[id].g;
    bool const had_pin = g.pin != pin_none;
    val_t const old = gc_.value;
    val_t fresh = g.arg;
    wdesc_t const now = describe (wa);
    if (now.valid)
      fresh.hslot = now.value.hslot;
    int const index = gc_.nhist;
    if (index < max_hist)
      {
        gc_.hist[index].value = fresh;
        gc_.hist[index].step = stepno_;
        gc_.nhist = index + 1;
      }
    g.swap_hist = index;
    if ((g.role == r_cs || g.role == r_ch) && !veq (old, g.expected))
      fail (or_LIN,
            "thread %d: a compare-exchange replaced %s, not its "
            "expected %s",
            id, label_text (old).c_str (), label_text (g.expected).c_str ());
    int const lb = gc_.ntags;
    int const own = find_tag (id);
    if (own >= 0)
      remove_tag_at (own);
    g.pin = pin_none;
    for (int i = 0; i < gc_.ntags; ++i)
      {
        gthread_t &other = rt_[gc_.tags[i]].g;
        other.pin = pin_left;
        if (ld_like (other) && other.counted == cnt_live)
          other.in_iii = true;
      }
    gc_.ntags = 0;
    int const need = lb - (had_pin ? 1 : 0);
    if (g.dep < need)
      {
        fail (or_DEPSHORT,
              "thread %d swapped with %d ticks and a deposit of "
              "%d",
              id, need, g.dep);
        g.dep = 0;
      }
    else
      g.dep -= need;
    g.d_unit = 0;
    g.d_img = 0;
    gc_.value = fresh;
    if (!is_empty_val (old))
      {
        if (g.role == r_wr && g.exchange)
          {
            g.xres = old;
            g.xres_holder_live = old.flavor == fl_far;
          }
        else
          g.lag = old;
      }
  }

  /// The end of the operation of @p id: OP-END and the results.
  void
  finish_op (int id)
  {
    thread_rt_t &rt = rt_[id];
    gthread_t &g = rt.g;
    if (g.pin != pin_none || g.dep != 0 || !is_empty_val (g.lag)
        || g.pending_untransfer != 0 || g.pending_settle != 0
        || g.counted != cnt_no || g.nested || g.ch_nomatch
        || g.xres_holder_live)
      fail (or_OPEND,
            "thread %d ended its operation with pin %d dep %d lag %s "
            "pending %d/%d counted %d nested %d",
            id, static_cast<int> (g.pin), g.dep, label_text (g.lag).c_str (),
            g.pending_untransfer, g.pending_settle,
            static_cast<int> (g.counted), g.nested ? 1 : 0);
    if (g.role == r_ld)
      {
        if (rt.nresults == 0)
          fail (or_PATH, "thread %d: a load without a result", id);
        else
          {
            result_t const &result = rt.results[rt.nresults - 1];
            if (!veq (result.value, g.load_result))
              fail (or_LIN, "thread %d: a load returned %s but pinned %s", id,
                    label_text (result.value).c_str (),
                    label_text (g.load_result).c_str ());
            add_lin (id, result.value, g.op_first_step);
          }
      }
    if (g.role == r_cs || g.role == r_ch || g.exchange)
      {
        result_t const &result
            = rt.results[rt.nresults > 0 ? rt.nresults - 1 : 0];
        if (rt.nresults == 0)
          fail (or_PATH, "thread %d: an operation without a result", id);
        else if (g.role == r_cs || g.role == r_ch)
          {
            if (!result.flag && !veq (result.value, g.cas_current))
              fail (or_LIN,
                    "thread %d: a failed compare-exchange returned %s "
                    "but loaded %s",
                    id, label_text (result.value).c_str (),
                    label_text (g.cas_current).c_str ());
            if (!result.flag && veq (result.value, g.expected))
              fail (or_LIN,
                    "thread %d: CAS-STRONG: a strong compare-exchange "
                    "failed but returned %s, equivalent to its "
                    "expected value",
                    id, label_text (result.value).c_str ());
          }
        else if (g.swap_hist >= 1 && g.swap_hist < gc_.nhist
                 && !veq (result.value, gc_.hist[g.swap_hist - 1].value))
          fail (or_LIN,
                "thread %d: an exchange returned %s, the value it "
                "replaced was %s",
                id, label_text (result.value).c_str (),
                label_text (gc_.hist[g.swap_hist - 1].value).c_str ());
      }
    g.clear (weak_engine_);
  }

  /// The USER operations: LOCK and EXPIRED use the owners of the boundary
  /// before the step.
  void
  check_user_op (int id, int op_index)
  {
    thread_rt_t &rt = rt_[id];
    op_t const &op = def_->threads[id].ops[op_index];
    if (op.kind != op_lk && op.kind != op_ex)
      return;
    val_t const target = snap_[id].weak[op.a];
    if (target.slot < 0 || target.slot >= 6 || rt.nresults == 0)
      return;
    result_t const &result = rt.results[rt.nresults - 1];
    int const owners_before = owners_[target.slot];
    if (op.kind == op_lk && is_empty_val (result.value) && owners_before != 0)
      fail (or_LOCK, "thread %d: lock returned empty with %d owners of %s", id,
            owners_before, label_text (target).c_str ());
    if (op.kind == op_ex && result.flag && owners_before != 0)
      fail (or_EXPIRED,
            "thread %d: expired returned true with %d owners of %s", id,
            owners_before, label_text (target).c_str ());
  }

  /// The row of 2.2 for the step of @p id that resumed at @p P and stopped at
  /// @p E, with the word @p wb before and @p wa after.
  void
  transition (int id, unsigned P, unsigned E, split_value_t wb,
              split_value_t wa)
  {
    thread_rt_t &rt = rt_[id];
    gthread_t &g = rt.g;
    int const op_index = executing_op_[id];
    if (P == pt_op_begin)
      {
        begin_op (id, E, op_index);
        ++executing_op_[id];
      }
    bool const writer = g.role == r_wr || g.role == r_cs || g.role == r_ch;
    if (writer && !g.nested)
      {
        int const need = ticks_of (wb) - (g.pin == pin_in ? 1 : 0);
        if (need > g.max_need)
          g.max_need = need;
      }
    bool const changed = !same_word (wb, wa);
    bool const same_block_word = wb.lo == wa.lo && wb.lo != 0u;
    bool const tick_up = same_block_word && wa.hi == wb.hi + 1u;
    bool const tick_down = same_block_word && wa.hi + 1u == wb.hi;
    if (changed)
      {
        bool ok = false;
        if (P == pt_load_tick || P == pt_cas_pin)
          ok = tick_up;
        else if (P == pt_settle_untick)
          ok = tick_down;
        else if (P == pt_swap || P == pt_cas)
          ok = true;
        if (!ok)
          fail (or_WORD, "thread %d wrote the word at %s", id, point_name (P));
      }
    if (E == pt_note_tick
        && !((P == pt_load_tick || P == pt_cas_pin) && tick_up))
      fail (or_NOTE, "note_tick after %s, which did not tick", point_name (P));
    if (E == pt_note_untick && !(P == pt_settle_untick && tick_down))
      fail (or_NOTE, "note_untick after %s, which did not untick",
            point_name (P));
    if (E == pt_note_consumed && !(P == pt_swap || P == pt_cas))
      fail (or_NOTE, "note_consumed after %s, which is no pinned swap",
            point_name (P));
    if (E == pt_note_transfer && P != pt_transfer)
      fail (or_NOTE, "note_transfer after %s", point_name (P));
    // The notes that precede their read-modify-write are pending until it.
    if (E == pt_note_untransfer)
      {
        int const n = last_note (id, split_event_t::untransfer);
        if (n < 0)
          fail (or_NOTE, "note_untransfer without its note");
        else
          g.pending_untransfer += n;
      }
    if (E == pt_note_settle_ext)
      {
        if (last_note (id, split_event_t::settle_ext) < 0)
          fail (or_NOTE, "note_settle_ext without its note");
        ++g.pending_settle;
        if (ld_like (g) && g.pending_settle > 1)
          fail (or_NOTE, "thread %d: a load pays twice", id);
      }
    wdesc_t const before = describe (wb);
    bool const present = !word::is_empty (wb) && wb.lo == g.pinned_lo
                         && word::ticks_of (wb) > 0u;
    switch (P)
      {
      case pt_op_begin:
        if (g.role == r_user)
          {
            require (is_end (E), id, P, E, "an operation without hooks");
            check_user_op (id, op_index);
          }
        break;
      case pt_load_read:
        require (ld_like (g), id, P, E, "load_read outside a load");
        require (E == pt_load_tick || E == pt_back_off, id, P, E,
                 "a read goes on to the tick or backs off");
        break;
      case pt_back_off:
        if (ld_like (g))
          require (E == pt_load_read, id, P, E,
                   "a load reads after backing off");
        else if (g.role == r_wr)
          require (E == pt_swap, id, P, E, "a store reads after backing off");
        else
          require (E == pt_cas_read, id, P, E,
                   "a compare-exchange reads after backing off");
        break;
      case pt_load_tick:
        if (tick_up)
          {
            g.pin = pin_in;
            g.pinned = before.value;
            g.pinned_lo = wb.lo;
            add_tag (id);
            require (E == pt_note_tick, id, P, E, "a tick is noted");
          }
        else if (ld_like (g))
          {
            if (word::is_empty (wb))
              {
                if (E != pt_load_tick)
                  end_load (id, P, E, empty_val (weak_engine_));
              }
            else
              require (E == pt_load_tick || E == pt_back_off, id, P, E,
                       "a failed tick tries again");
          }
        else
          require (E == pt_swap || E == pt_back_off || E == pt_load_tick, id,
                   P, E, "a failed pin of a writer starts again");
        break;
      case pt_note_tick:
        if (ld_like (g))
          require (E == pt_load_count, id, P, E,
                   "a pinned load takes its reference next");
        else if (g.role == r_wr)
          require (E == pt_transfer || E == pt_swap, id, P, E,
                   "a pinned writer deposits or swaps");
        else
          {
            bool const match = veq (g.pinned, g.expected);
            if (match)
              {
                require (E == pt_transfer || E == pt_cas, id, P, E,
                         "a matching holder is swapped");
                if (!g.built)
                  {
                    g.built = true;
                    if (g.arg.flavor == fl_far)
                      {
                        g.d_img = 1;
                        sim_mem_t &m = mem ();
                        if (m.hold.log_count >= 1)
                          g.arg.hslot
                              = static_cast<signed char> (m.hold.log_slot[0]);
                      }
                  }
              }
            else
              {
                require (E == pt_settle_read, id, P, E,
                         "a holder that does not match is settled");
                g.ch_nomatch = true;
              }
          }
        break;
      case pt_load_count:
        if (g.role == r_wr)
          require (E == pt_release, id, P, E,
                   "the hand-over of a holder releases its unit");
        else
          {
            if (present)
              {
                if (g.pin == pin_none)
                  path_fail (id, P, E, "a load found a tick but owns none");
                g.counted = cnt_live;
                if (g.pin == pin_left)
                  g.in_iii = true;
                require (E == pt_settle_read, id, P, E,
                         "a counted load settles its tick");
              }
            else
              {
                if (g.pin == pin_in)
                  path_fail (id, P, E,
                             "the tag is in the word but the load "
                             "found it absent");
                require (E == pt_note_settle_ext, id, P, E,
                         "an absent tick is paid");
              }
          }
        break;
      case pt_settle_read:
        if (present)
          require (E == pt_settle_untick, id, P, E,
                   "a present tick is taken back");
        else
          require (E == pt_settle_read || E == pt_note_settle_ext, id, P, E,
                   "an absent tick is confirmed and paid");
        break;
      case pt_settle_untick:
        if (tick_down)
          {
            require (E == pt_note_untick, id, P, E, "an untick is noted");
            int const own = find_tag (id);
            if (g.pin == pin_in && own >= 0)
              remove_tag_at (own);
            else
              {
                int victim_index = -1;
                for (int i = 0; i < gc_.ntags; ++i)
                  if (gc_.tags[i] != id)
                    {
                      victim_index = i;
                      break;
                    }
                if (victim_index < 0)
                  fail (or_WORD, "thread %d took back a tick of an empty set",
                        id);
                else
                  {
                    int const victim = gc_.tags[victim_index];
                    remove_tag_at (victim_index);
                    gthread_t &other = rt_[victim].g;
                    other.pin = pin_left;
                    if (ld_like (other) && other.counted == cnt_live)
                      other.in_iii = true;
                  }
              }
            g.pin = pin_none;
            g.in_iii = false;
          }
        else
          require (E == pt_settle_untick || E == pt_settle_read, id, P, E,
                   "a failed untick looks again");
        break;
      case pt_note_untick:
        if (g.role == r_ch && g.ch_nomatch)
          {
            g.ch_nomatch = false;
            begin_nested (id);
            require (E == pt_load_read, id, P, E,
                     "a holder that did not match loads");
          }
        else
          end_load (id, P, E, g.pinned);
        break;
      case pt_note_settle_ext:
        require (E == pt_settle_ext, id, P, E, "a noted payment is made");
        break;
      case pt_settle_ext:
        if (ld_like (g))
          {
            if (g.pin != pin_left)
              path_fail (id, P, E, "a load paid without an owed tick");
            if (g.counted == cnt_no)
              g.counted = cnt_combined;
            g.pin = pin_none;
            g.in_iii = false;
            if (g.pending_settle != 1)
              fail (or_NOTE, "thread %d paid without its note", id);
            g.pending_settle = 0;
            end_load (id, P, E, g.pinned);
          }
        else if (g.role == r_ch && g.ch_nomatch)
          {
            g.pin = pin_none;
            g.pending_settle = 0;
            g.ch_nomatch = false;
            begin_nested (id);
            require (E == pt_load_read, id, P, E,
                     "a holder that did not match loads");
          }
        else
          {
            // pay_back: the own tick and the own deposit in one step.
            g.dep -= g.pending_untransfer;
            g.pending_untransfer = 0;
            if (g.dep != 0)
              fail (or_EID,
                    "thread %d paid back but %d units of its deposit "
                    "remain",
                    id, g.dep);
            if (g.pin != pin_left)
              path_fail (id, P, E, "a writer paid back without an owed tick");
            g.pin = pin_none;
            g.pending_settle = 0;
            if (g.role == r_wr)
              require (E == pt_swap || E == pt_load_tick || E == pt_back_off,
                       id, P, E, "a writer starts again after paying back");
            else
              require (E == pt_cas_pin || E == pt_back_off
                           || E == pt_load_read,
                       id, P, E, "a compare-exchange starts again");
          }
        break;
      case pt_note_untransfer:
        require (E == pt_transfer || E == pt_release
                     || E == pt_note_settle_ext,
                 id, P, E, "a noted withdrawal is made");
        break;
      case pt_transfer:
        if (E == pt_note_transfer)
          {
            int const n = last_note (id, split_event_t::transfer);
            if (n < 0)
              fail (or_NOTE, "a deposit without its note");
            else
              g.dep += n;
            g.dep_val = g.role == r_cs ? g.expected : g.pinned;
          }
        else
          {
            if (g.pending_untransfer <= 0)
              path_fail (id, P, E, "a transfer step that deposits nothing");
            g.dep -= g.pending_untransfer;
            g.pending_untransfer = 0;
            if (g.role == r_wr)
              require (is_end (E) || E == pt_load_count, id, P, E,
                       "an exchange hands over after the withdrawal");
            else
              require (E == pt_cas || E == pt_transfer || E == pt_load_read,
                       id, P, E, "a failed compare-exchange goes on");
          }
        break;
      case pt_note_transfer:
        if (g.role == r_wr)
          require (E == pt_swap, id, P, E,
                   "a deposit is followed by the swap");
        else
          require (E == pt_cas, id, P, E,
                   "a deposit is followed by the compare-exchange");
        break;
      case pt_swap:
        swap_row (id, P, E, wb, wa);
        break;
      case pt_cas_read:
        cas_read_row (id, P, E, wb, before);
        break;
      case pt_cas_pin:
        if (tick_up)
          {
            g.pin = pin_in;
            g.pinned = before.value;
            g.pinned_lo = wb.lo;
            add_tag (id);
            require (E == pt_note_tick, id, P, E, "a tick is noted");
          }
        else
          require (E == pt_cas_pin || E == pt_load_read || E == pt_back_off,
                   id, P, E, "a failed pin looks again");
        break;
      case pt_cas:
        cas_row (id, P, E, wb, wa);
        break;
      case pt_release:
        release_row (id, P, E);
        break;
      case pt_note_consumed:
        if (g.role == r_wr && g.exchange)
          require (E == pt_note_untransfer || is_end (E) || E == pt_load_count,
                   id, P, E, "an exchange withdraws or hands over");
        else
          require (E == pt_note_untransfer || E == pt_release, id, P, E,
                   "a swap drops the old unit");
        break;
      default:
        path_fail (id, P, E, "a step from an unknown point");
        break;
      }
    if ((g.role == r_cs || g.role == r_ch) && !g.nested && E == pt_load_read)
      begin_nested (id);
    if (is_end (E) && P != pt_op_begin)
      finish_op (id);
    else if (is_end (E) && P == pt_op_begin && g.role == r_user)
      finish_op (id);
  }

  /// The row of `swap` (the store and the exchange).
  void
  swap_row (int id, unsigned P, unsigned E, split_value_t wb, split_value_t wa)
  {
    gthread_t &g = rt_[id].g;
    require (g.role == r_wr, id, P, E, "a swap outside a store");
    bool const success = !same_word (wb, wa) || is_end (E)
                         || E == pt_note_consumed || E == pt_release
                         || E == pt_load_count;
    if (success)
      {
        bool const had_pin = g.pin != pin_none;
        val_t const old = gc_.value;
        swap_transition (id, wa);
        if (had_pin)
          require (E == pt_note_consumed, id, P, E,
                   "a pinned swap notes the consumed tick");
        else if (g.exchange)
          require (is_end (E) || (old.flavor == fl_far && E == pt_load_count),
                   id, P, E, "an exchange hands over");
        else if (is_empty_val (old))
          require (is_end (E), id, P, E, "the old value was empty");
        else
          require (E == pt_release, id, P, E, "a store drops the old unit");
        return;
      }
    if (g.pin == pin_none)
      {
        require (E == pt_swap || E == pt_back_off || E == pt_load_tick, id, P,
                 E, "a swap that did not happen looks again");
        return;
      }
    pinned_failure (id, P, E, wb, pt_swap);
  }

  /// A pinned swap that failed: the top-up or the undo (S5 and S12).
  void
  pinned_failure (int id, unsigned P, unsigned E, split_value_t wb,
                  unsigned again)
  {
    gthread_t &g = rt_[id].g;
    int const ticks = ticks_of (wb);
    if (wb.lo == g.pinned_lo && ticks >= 1)
      require (E == again || (E == pt_transfer && ticks - 1 > g.dep), id, P, E,
               "the same block with other ticks: top up or retry");
    else if (g.dep > 0)
      require (E == pt_note_untransfer, id, P, E,
               "the block is gone: pay back with the deposit");
    else
      require (E == pt_note_settle_ext, id, P, E,
               "the block is gone: pay back the tick");
  }

  /// The row of `cas_read`.
  void
  cas_read_row (int id, unsigned P, unsigned E, split_value_t wb,
                wdesc_t const &before)
  {
    gthread_t &g = rt_[id].g;
    g.cas_current = empty_val (weak_engine_);
    if (g.role == r_cs)
      {
        bool const match = veq (before.value, g.expected);
        if (match)
          {
            if (!g.built)
              {
                g.built = true;
                if (g.arg.flavor == fl_far)
                  {
                    g.d_img = 1;
                    sim_mem_t &m = mem ();
                    if (m.hold.log_count >= 1)
                      g.arg.hslot
                          = static_cast<signed char> (m.hold.log_slot[0]);
                  }
              }
            require (E == (ticks_of (wb) > 0 ? pt_transfer : pt_cas), id, P, E,
                     "a match deposits its ticks and swaps");
          }
        else
          {
            begin_nested (id);
            require (E == pt_load_read, id, P, E,
                     "no match: the failure is a load");
          }
      }
    else
      {
        if (before.holder)
          require (E == pt_cas_pin || E == pt_back_off, id, P, E,
                   "a holder word is pinned");
        else
          {
            begin_nested (id);
            require (E == pt_load_read, id, P, E,
                     "no holder: the failure is a load");
          }
      }
  }

  /// The row of `cas` (the swap of a compare-exchange).
  void
  cas_row (int id, unsigned P, unsigned E, split_value_t wb, split_value_t wa)
  {
    gthread_t &g = rt_[id].g;
    if (g.role == r_cs)
      {
        bool const success
            = !same_word (wb, wa) || E == pt_release || is_end (E);
        if (success)
          {
            val_t const old = gc_.value;
            swap_transition (id, wa);
            if (is_empty_val (old))
              require (is_end (E), id, P, E, "the old value was empty");
            else
              require (E == pt_release, id, P, E, "a swap drops the old unit");
          }
        else
          require (E == pt_note_untransfer || E == pt_cas || E == pt_transfer
                       || E == pt_load_read,
                   id, P, E, "a failed swap withdraws and looks again");
        return;
      }
    // The holder form.
    if (!same_word (wb, wa) || E == pt_note_consumed)
      {
        swap_transition (id, wa);
        require (E == pt_note_consumed, id, P, E,
                 "a pinned swap notes the consumed tick");
        return;
      }
    pinned_failure (id, P, E, wb, pt_cas);
  }

  /// The row of `release`.
  void
  release_row (int id, unsigned P, unsigned E)
  {
    gthread_t &g = rt_[id].g;
    if (g.role == r_wr && g.exchange)
      {
        g.xres_holder_live = false;
        require (is_end (E), id, P, E, "the hand-over ends the exchange");
        return;
      }
    if (g.role == r_wr || !is_empty_val (g.lag))
      {
        g.dep -= g.pending_untransfer;
        g.pending_untransfer = 0;
        g.lag = empty_val (weak_engine_);
        require (is_end (E), id, P, E, "the drop ends the operation");
        return;
      }
    if (g.role == r_cs || g.role == r_ch)
      {
        g.d_unit = 0;
        g.d_img = 0;
        g.built = false;
        require (is_end (E), id, P, E, "the dropped image ends the operation");
        return;
      }
    path_fail (id, P, E, "a release outside a writer");
  }

  // ---- the quantities of 2.3 ----

  void
  compute_quant (int slot, quant_t &q) const
  {
    std::memset (&q, 0, sizeof (q));
    ctl_t *const block = obj_block (slot);
    int const n = def_->nthreads;
    for (int ti = 0; ti <= n; ++ti)
      {
        int const t = ti == n ? main_thread : ti;
        thread_rt_t const &r = rt_[t];
        for (int i = 0; i < 4; ++i)
          {
            if (sp::detail::access::control (r.held[i]) == block)
              ++q.U;
            if (sp::detail::access::control (r.weak[i]) == block)
              ++q.weak_refs;
          }
        if (t == main_thread)
          continue;
        gthread_t const &g = r.g;
        if (g.role == r_none)
          continue;
        bool const ldl = ld_like (g);
        bool const counted_here = !weak_engine_ || g.pinned.flavor != fl_far;
        if (names_slot (g.arg, slot))
          {
            q.D += g.d_param + g.d_unit + g.d_img;
            if (weak_engine_)
              ++q.weak_refs;
          }
        if (names_slot (g.xres, slot))
          ++q.X;
        if (names_slot (g.lag, slot))
          ++q.Wi;
        if (names_slot (g.cas_current, slot))
          ++q.CC;
        if (ldl && g.counted != cnt_no && names_slot (g.pinned, slot))
          ++q.CNT;
        if (g.pin != pin_none && names_slot (g.pinned, slot))
          ++q.weak_refs;
        if (g.pin == pin_left && names_slot (g.pinned, slot) && counted_here)
          {
            ++q.OWING;
            if (ldl)
              {
                if (g.counted == cnt_no)
                  ++q.LEFTNC;
              }
            else
              ++q.LEFTW;
          }
        if (g.in_iii && names_slot (g.pinned, slot))
          ++q.Wiii;
        if ((g.role == r_wr || g.role == r_cs || g.role == r_ch)
            && names_slot (g.dep_val, slot)
            && (!weak_engine_ || g.dep_val.flavor != fl_far))
          q.DEP += g.dep;
        if (g.dep > 0 && names_slot (g.dep_val, slot))
          ++q.weak_refs;
      }
    if (names_slot (gc_.value, slot))
      {
        if (weak_engine_)
          ++q.weak_refs;
        else
          q.S = 1;
      }
    q.Wii = q.DEP + q.LEFTW;
    if (mem ().obj.is_live (slot))
      q.counts = counts_of (block);
    else
      std::memset (&q.counts, 0, sizeof (q.counts));
  }

  /// The strong owners of a block. The weak engine never changes the strong
  /// group: its owners are the strong pointers that are held.
  int
  owners_of (quant_t const &q) const
  {
    if (weak_engine_)
      return q.U;
    return q.U + q.S + q.D + q.X + q.CNT + q.CC + q.LEFTNC;
  }

  void
  refresh_owners ()
  {
    for (int s = 0; s < 6; ++s)
      {
        if (mem ().obj.generation (s) == 0u)
          {
            owners_[s] = 0;
            continue;
          }
        quant_t q;
        compute_quant (s, q);
        owners_[s] = owners_of (q);
      }
  }

  // ---- the boundary checks ----

  void
  check_names_live ()
  {
    sim_mem_t &m = mem ();
    int const n = def_->nthreads;
    for (int ti = 0; ti <= n; ++ti)
      {
        int const t = ti == n ? main_thread : ti;
        thread_rt_t const &r = rt_[t];
        for (int i = 0; i < 4; ++i)
          {
            ctl_t *const sc = sp::detail::access::control (r.held[i]);
            if (sc != nullptr)
              {
                int const s = m.obj.slot_of (sc);
                if (s < 0 || !m.obj.is_live (s))
                  fail (or_SAFETY, "thread %d holds a freed block in held[%d]",
                        t, i);
              }
            ctl_t *const wc = sp::detail::access::control (r.weak[i]);
            if (wc != nullptr)
              {
                int const s = m.obj.slot_of (wc);
                if (s < 0 || !m.obj.is_live (s))
                  fail (or_SAFETY, "thread %d holds a freed block in weak[%d]",
                        t, i);
              }
          }
        if (r.result_fault)
          fail (or_SAFETY, "thread %d returned a value of a dead object", t);
        if (t == main_thread)
          continue;
        gthread_t const &g = r.g;
        val_t const values[3] = { g.cas_current, g.xres, g.lag };
        for (int k = 0; k < 3; ++k)
          if (values[k].slot >= 0
              && (values[k].slot >= 6 || !m.obj.is_live (values[k].slot)))
            fail (or_SAFETY, "thread %d keeps a freed block (ghost value %d)",
                  t, k);
      }
    if (gc_.value.slot >= 0
        && (gc_.value.slot >= 6 || !m.obj.is_live (gc_.value.slot)))
      fail (or_SAFETY, "the cell names a freed block");
  }

  bool
  group_alive (val_t const &pinned) const
  {
    sim_mem_t &m = mem ();
    if (pinned.flavor == fl_far && pinned.hslot >= 0)
      {
        if (!m.hold.is_live (pinned.hslot))
          return false;
        return !counts_of (hold_block (pinned.hslot)).strong_zero;
      }
    if (pinned.slot < 0 || pinned.slot >= 6 || !m.obj.is_live (pinned.slot))
      return false;
    counts_t const c = counts_of (obj_block (pinned.slot));
    return weak_engine_ ? !c.weak_zero : !c.strong_zero;
  }

  void
  check_boundary ()
  {
    sim_mem_t &m = mem ();
    // SAFETY (a), (b)
    int const dirty = m.obj.disturbed_free_slot ();
    if (dirty >= 0)
      fail (or_SAFETY, "a freed object slot %d was written", dirty);
    int const dirty_holder = m.hold.disturbed_free_slot ();
    if (dirty_holder >= 0)
      fail (or_SAFETY, "a freed holder slot %d was written", dirty_holder);
    if (m.double_dispose > 0)
      fail (or_SAFETY, "DOUBLE-DISPOSE: an object was destroyed twice");
    if (m.obj.double_free > 0 || m.hold.double_free > 0)
      fail (or_SAFETY, "DOUBLE-FREE of a block");
    if (m.obj.busy_slot > 0)
      fail (or_SAFETY, "a block was made in a slot that is not free");
    if (m.hold.exhausted > 0)
      fail (or_SAFETY, "the holder arena is exhausted");
    if (m.unexpected_count > 0)
      fail (or_SAFETY, "unexpected allocation of %d bytes",
            m.unexpected_alloc);
    check_names_live ();
    // WORD
    split_value_t const w = read_word ();
    wdesc_t const d = describe (w);
    if (!d.valid)
      fail (or_WORD, "the word %016llx:%016llx does not decode",
            static_cast<unsigned long long> (w.hi),
            static_cast<unsigned long long> (w.lo));
    else
      {
        if (!veq (d.value, gc_.value)
            || (!d.empty && d.holder != (gc_.value.flavor == fl_far))
            || d.value.hslot != gc_.value.hslot)
          fail (or_WORD,
                "the word decodes to %s (holder %d), the ghost %s "
                "(holder %d)",
                label_text (d.value).c_str (), d.value.hslot,
                label_text (gc_.value).c_str (), gc_.value.hslot);
        if (d.ticks != gc_.ntags)
          fail (or_WORD, "L is %d but the ghost has %d tags", d.ticks,
                gc_.ntags);
        if (static_cast<std::uint32_t> (d.ticks) > info_.limit)
          fail (or_WORD, "L %d exceeds the tick limit", d.ticks);
      }
    // The pins: SAFETY (e)
    int const n = def_->nthreads;
    for (int t = 0; t < n; ++t)
      {
        gthread_t const &g = rt_[t].g;
        if (g.pin != pin_none && !group_alive (g.pinned))
          fail (or_SAFETY,
                "thread %d has a pin on %s, whose block is freed or "
                "has a zero counter",
                t, label_text (g.pinned).c_str ());
      }
    // W2BOUND
    for (int t = 0; t < n; ++t)
      {
        gthread_t const &g = rt_[t].g;
        if (g.role == r_wr
            && g.dep > g.max_need + static_cast<int> (info_.reserve))
          fail (or_W2BOUND, "thread %d deposited %d, the bound is %d + %d", t,
                g.dep, g.max_need, static_cast<int> (info_.reserve));
        else if ((g.role == r_cs || g.role == r_ch) && g.dep > g.max_need)
          fail (or_W2BOUND, "thread %d deposited %d, the bound is %d", t,
                g.dep, g.max_need);
      }
    // The object blocks
    for (int s = 0; s < 6; ++s)
      {
        if (m.obj.generation (s) == 0u)
          continue;
        quant_t q;
        compute_quant (s, q);
        int const owners = owners_of (q);
        owners_[s] = owners;
        if (!m.obj.is_live (s))
          {
            if (owners > 0 || q.weak_refs > 0)
              fail (or_SAFETY,
                    "block %s is freed but still owned (%d owners, "
                    "%d weak)",
                    label_text (block_val (s)).c_str (), owners, q.weak_refs);
            continue;
          }
        counts_t const &c = q.counts;
        if (c.count < 0 || c.ext < 0 || c.weak_count < 0 || c.weak_ext < 0)
          fail (or_NONNEG, "block %s: count %ld ext %ld, weak %ld/%ld",
                label_text (block_val (s)).c_str (), c.count, c.ext,
                c.weak_count, c.weak_ext);
        long const use = c.count + c.ext;
        sim_obj_t const *const object
            = reinterpret_cast<sim_obj_t const *> (obj_block (s)->anchor ());
        bool const alive = object->magic == live_magic ();
        if (owners > 0 && alive != (use > 0))
          fail (or_SAFETY, "block %s: object %s with count+ext %ld",
                label_text (block_val (s)).c_str (),
                alive ? "alive" : "destroyed", use);
        if (weak_engine_)
          {
            if (owners > 0 || q.weak_refs > 0)
              if (c.weak_zero)
                fail (or_SAFETY,
                      "block %s is referenced but its weak ledger "
                      "is zero",
                      label_text (block_val (s)).c_str ());
            if (c.count != q.U)
              fail (or_CID,
                    "block %s: the weak engine touched the strong "
                    "count %ld (%d owners)",
                    label_text (block_val (s)).c_str (), c.count, q.U);
            if (c.ext != 0)
              fail (or_EID, "block %s: the weak engine left strong ext %ld",
                    label_text (block_val (s)).c_str (), c.ext);
            if (c.weak_ext != q.DEP + q.OWING)
              fail (or_EID, "block %s: weak ext %ld but deposits %d + owed %d",
                    label_text (block_val (s)).c_str (), c.weak_ext, q.DEP,
                    q.OWING);
            continue;
          }
        int const a = q.Wi + q.Wii + q.Wiii;
        long const count_sum = q.U + q.S + q.D + q.X + q.CNT + q.CC + q.Wi;
        if (c.count != count_sum)
          fail (or_CID,
                "block %s: count %ld, U%d S%d D%d X%d CNT%d CC%d Wi%d "
                "sum %ld",
                label_text (block_val (s)).c_str (), c.count, q.U, q.S, q.D,
                q.X, q.CNT, q.CC, q.Wi, count_sum);
        if (c.ext != q.DEP + q.OWING)
          fail (or_EID, "block %s: ext %ld, deposits %d + owed %d",
                label_text (block_val (s)).c_str (), c.ext, q.DEP, q.OWING);
        if (use < owners)
          fail (or_G1, "block %s: use_count %ld below %d owners",
                label_text (block_val (s)).c_str (), use, owners);
        if (a == 0 && use != owners)
          fail (or_G2,
                "block %s: use_count %ld with %d owners and no window "
                "open",
                label_text (block_val (s)).c_str (), use, owners);
        if (use != owners + a)
          fail (or_BAL, "block %s: use_count %ld, owners %d + windows %d",
                label_text (block_val (s)).c_str (), use, owners, a);
      }
    check_holders ();
  }

  val_t
  block_val (int slot) const
  {
    val_t v;
    v.slot = static_cast<signed char> (slot);
    v.gen = static_cast<signed char> (mem ().obj.generation (slot));
    v.flavor = fl_plain;
    v.hslot = -1;
    v.weak = weak_engine_;
    return v;
  }

  /// The counters of every holder against the ghost (both engines).
  void
  check_holders ()
  {
    sim_mem_t &m = mem ();
    int const n = def_->nthreads;
    for (int hs = 0; hs < hold_arena_t::slot_count; ++hs)
      {
        if (!m.hold.is_live (hs))
          continue;
        counts_t const c = counts_of (hold_block (hs));
        if (c.count < 0 || c.ext < 0)
          fail (or_NONNEG, "holder H%d: count %ld ext %ld", m.hold.label (hs),
                c.count, c.ext);
        int expect_count = 0;
        int expect_ext = 0;
        if (gc_.value.slot >= 0 && gc_.value.flavor == fl_far
            && gc_.value.hslot == hs)
          ++expect_count;
        for (int t = 0; t < n; ++t)
          {
            gthread_t const &g = rt_[t].g;
            if (g.role == r_none)
              continue;
            if (g.d_img > 0 && g.arg.hslot == hs)
              ++expect_count;
            if (g.lag.flavor == fl_far && g.lag.hslot == hs && g.lag.slot >= 0)
              ++expect_count;
            if (g.xres_holder_live && g.xres.hslot == hs)
              ++expect_count;
            if ((g.role == r_wr || g.role == r_cs || g.role == r_ch)
                && g.dep_val.flavor == fl_far && g.dep_val.hslot == hs)
              expect_ext += g.dep;
            if (g.pin == pin_left && g.pinned.flavor == fl_far
                && g.pinned.hslot == hs)
              ++expect_ext;
          }
        if (c.count != expect_count)
          fail (or_CID, "holder H%d: count %ld, the ghost expects %d",
                m.hold.label (hs), c.count, expect_count);
        if (c.ext != expect_ext)
          fail (or_EID, "holder H%d: ext %ld, the ghost expects %d",
                m.hold.label (hs), c.ext, expect_ext);
      }
    // A holder the ghost names must be live.
    if (gc_.value.flavor == fl_far && gc_.value.hslot >= 0
        && !m.hold.is_live (gc_.value.hslot))
      fail (or_SAFETY, "the cell names a freed holder");
  }

  // ---- the end of a schedule ----

  std::string
  result_text (result_t const &r) const
  {
    if (r.kind == 'c')
      return r.flag ? std::string ("T") : "F/" + label_text (r.value);
    if (r.kind == 'e')
      return r.flag ? "T" : "F";
    return label_text (r.value);
  }

  std::string
  outcome_string () const
  {
    std::string s;
    for (int t = 0; t < def_->nthreads; ++t)
      {
        thread_rt_t const &r = rt_[t];
        if (r.nresults == 0)
          continue;
        if (!s.empty ())
          s += '|';
        s += static_cast<char> ('0' + t);
        s += ':';
        for (int k = 0; k < r.nresults; ++k)
          {
            if (k > 0)
              s += ',';
            s += result_text (r.results[k]);
          }
      }
    if (!s.empty ())
      s += '|';
    s += "final=" + label_text (gc_.value);
    return s;
  }

  void
  check_lin ()
  {
    int previous[max_threads];
    for (int t = 0; t < max_threads; ++t)
      previous[t] = 0;
    for (int k = 0; k < nlin_; ++k)
      {
        lin_rec_t const &r = lin_[k];
        int found = -1;
        for (int i = previous[r.thread]; i < gc_.nhist; ++i)
          {
            if (!veq (gc_.hist[i].value, r.value))
              continue;
            long const start = gc_.hist[i].step;
            long const stop
                = i + 1 < gc_.nhist ? gc_.hist[i + 1].step - 1 : 0x7FFFFFFFL;
            if (start <= r.last && stop >= r.first - 1)
              {
                found = i;
                break;
              }
          }
        if (found < 0)
          fail (or_LIN,
                "thread %d read %s in steps %ld..%ld, which the cell "
                "never held then (or out of order)",
                r.thread, label_text (r.value).c_str (), r.first, r.last);
        else
          previous[r.thread] = found;
      }
  }

  void
  check_end ()
  {
    sim_mem_t &m = mem ();
    // I4
    wdesc_t const d = describe (read_word ());
    if (!d.valid || d.ticks != 0 || gc_.ntags != 0)
      fail (or_I4, "the word is not quiescent: L %d, %d tags", d.ticks,
            gc_.ntags);
    for (int s = 0; s < 6; ++s)
      {
        if (m.obj.generation (s) == 0u || !m.obj.is_live (s))
          continue;
        quant_t q;
        compute_quant (s, q);
        counts_t const &c = q.counts;
        int uw = 0;
        int const n = def_->nthreads;
        for (int ti = 0; ti <= n; ++ti)
          {
            int const t = ti == n ? main_thread : ti;
            for (int i = 0; i < 4; ++i)
              if (sp::detail::access::control (rt_[t].weak[i])
                  == obj_block (s))
                ++uw;
          }
        int const sw = weak_engine_ && names_slot (gc_.value, s) ? 1 : 0;
        int const ss = weak_engine_ ? 0 : q.S;
        if (c.ext != 0 || c.weak_ext != 0)
          fail (or_I4, "block %s: ext %ld/%ld at quiescence",
                label_text (block_val (s)).c_str (), c.ext, c.weak_ext);
        if (c.count != q.U + ss)
          fail (or_I4, "block %s: strong count %ld, owners %d + slot %d",
                label_text (block_val (s)).c_str (), c.count, q.U, ss);
        if (c.weak_count != uw + sw + (c.strong_zero ? 0 : 1))
          fail (or_I4,
                "block %s: weak count %ld, weak owners %d + slot %d + "
                "the strong group %d",
                label_text (block_val (s)).c_str (), c.weak_count, uw, sw,
                c.strong_zero ? 0 : 1);
      }
    int live_holders = 0;
    for (int hs = 0; hs < hold_arena_t::slot_count; ++hs)
      {
        if (!m.hold.is_live (hs))
          continue;
        ++live_holders;
        counts_t const c = counts_of (hold_block (hs));
        bool const installed
            = gc_.value.flavor == fl_far && gc_.value.hslot == hs;
        if (!installed)
          fail (or_I4, "holder H%d is live but installed nowhere",
                m.hold.label (hs));
        else if (c.count != 1 || c.ext != 0)
          fail (or_I4, "holder H%d: count %ld ext %ld, expected 1/0",
                m.hold.label (hs), c.count, c.ext);
      }
    if (gc_.value.flavor == fl_far && live_holders != 1)
      fail (or_I4, "%d live holders for one installed holder", live_holders);
    // LEDGER and PEAK
    split_test::ledger_counters_t const &lc = *info_.counters;
    if (lc.ticks.load ()
        != lc.unticks.load () + lc.settles.load () + lc.consumed.load ())
      fail (or_LEDGER, "ticks %lu, unticks %lu + settles %lu + consumed %lu",
            lc.ticks.load (), lc.unticks.load (), lc.settles.load (),
            lc.consumed.load ());
    if (lc.transfers.load () - lc.untransfers.load () != lc.settles.load ())
      fail (or_LEDGER, "transfers %lu - untransfers %lu != settles %lu",
            lc.transfers.load (), lc.untransfers.load (), lc.settles.load ());
    if (lc.peak.load () > info_.limit)
      fail (or_PEAK, "the peak of L is %lu, the limit %u", lc.peak.load (),
            static_cast<unsigned> (info_.limit));
    // LIN and OUTCOME
    check_lin ();
    std::string const outcome = outcome_string ();
    if (def_->expected.count (outcome) == 0)
      fail (or_OUTCOME, "the outcome %s is not one of the %d expected",
            outcome.c_str (), def_->outcomes);
    else
      seen_.insert (outcome);
  }

  void
  teardown ()
  {
    sim_mem_t &m = mem ();
    ++engine_scope ();
    if (cell_ != nullptr)
      {
        cell_->~cell_ops_t ();
        cell_ = nullptr;
      }
    for (int t = 0; t < max_threads; ++t)
      rt_[t].reset (weak_engine_);
    --engine_scope ();
    nfail_ = 0;
    if (!aborted_run ())
      {
        if (m.obj.live_count () != 0 || m.live_objects != 0
            || m.hold.live_count () != 0)
          fail (or_LEAK,
                "after teardown %d object blocks, %d objects and %d "
                "holders are still alive",
                m.obj.live_count (), m.live_objects, m.hold.live_count ());
        if (m.double_dispose > 0 || m.obj.double_free > 0
            || m.hold.double_free > 0 || m.unexpected_count > 0)
          fail (or_SAFETY,
                "teardown: double dispose %d, double free %d/%d, "
                "unexpected allocations %d",
                m.double_dispose, m.obj.double_free, m.hold.double_free,
                m.unexpected_count);
        if (nfail_ > 0)
          report ("teardown", -1, 0, 0);
      }
  }

  // ---- the report ----

  void
  record_step (int decision, int id, unsigned from, unsigned to)
  {
    sim_mem_t &m = mem ();
    step_snap_t &s = ring_[ring_count_ % ring_size];
    ++ring_count_;
    wdesc_t const d = describe (read_word ());
    s.decision = decision;
    s.thread = id;
    s.from = from;
    s.to = to;
    s.value = d.value;
    s.ticks = d.ticks;
    mix (static_cast<std::uint64_t> (decision));
    mix ((static_cast<std::uint64_t> (id) << 32)
         | (static_cast<std::uint64_t> (from) << 16)
         | static_cast<std::uint64_t> (to));
    mix (
        (static_cast<std::uint64_t> (static_cast<unsigned char> (d.value.slot))
         << 24)
        | (static_cast<std::uint64_t> (
               static_cast<unsigned char> (d.value.gen))
           << 16)
        | (static_cast<std::uint64_t> (
               static_cast<unsigned char> (d.value.flavor))
           << 8)
        | static_cast<std::uint64_t> (d.ticks));
    for (int k = 0; k < 3; ++k)
      {
        if (m.obj.is_live (k))
          {
            counts_t const c = counts_of (obj_block (k));
            s.count[k] = c.count;
            s.ext[k] = c.ext;
            mix (static_cast<std::uint64_t> (c.count));
            mix (static_cast<std::uint64_t> (c.ext));
          }
        else
          {
            s.count[k] = -1000;
            s.ext[k] = -1000;
            mix (0xFFFFull);
          }
      }
  }

  void
  mix (std::uint64_t value)
  {
    for (int i = 0; i < 8; ++i)
      {
        hash_ ^= (value >> (8 * i)) & 0xFFu;
        hash_ *= 1099511628211ull;
      }
  }

  std::string
  describe_thread (int t) const
  {
    gthread_t const &g = rt_[t].g;
    char const *const roles[] = { "-", "LD", "WR", "CS", "CH", "USER" };
    char const *const pins[] = { "none", "IN", "LEFT" };
    char buffer[300];
    std::snprintf (buffer, sizeof (buffer),
                   "thread %d: %s%s pin %s(%s) counted %d dep %d(%s) max_need "
                   "%d lag %s x %s cur %s D %d/%d/%d pend %d/%d iii %d built "
                   "%d",
                   t, roles[g.role], g.nested ? "+nested" : "", pins[g.pin],
                   label_text (g.pinned).c_str (),
                   static_cast<int> (g.counted), g.dep,
                   label_text (g.dep_val).c_str (), g.max_need,
                   label_text (g.lag).c_str (), label_text (g.xres).c_str (),
                   label_text (g.cas_current).c_str (), g.d_param, g.d_unit,
                   g.d_img, g.pending_untransfer, g.pending_settle,
                   g.in_iii ? 1 : 0, g.built ? 1 : 0);
    return buffer;
  }

  void
  print_step_line (int decision, int id, unsigned from, unsigned to)
  {
    step_snap_t const &s = ring_[(ring_count_ - 1) % ring_size];
    std::string line;
    char buffer[400];
    std::snprintf (buffer, sizeof (buffer),
                   "  %s decision %d: thread %d %s -> %s | cell %s L=%d | A "
                   "%ld/%ld B %ld/%ld D %ld/%ld",
                   explorer_ != nullptr ? explorer_->header_text ().c_str ()
                                        : "",
                   decision, id, point_name (from), point_name (to),
                   label_text (s.value).c_str (), s.ticks, s.count[0],
                   s.ext[0], s.count[1], s.ext[1], s.count[2], s.ext[2]);
    std::fputs (buffer, stdout);
    for (int t = 0; t < def_->nthreads; ++t)
      {
        gthread_t const &g = rt_[t].g;
        std::printf (" | t%d %d/%d/%d", t, static_cast<int> (g.pin), g.dep,
                     g.counted == cnt_no ? 0 : 1);
      }
    std::fputc ('\n', stdout);
    std::fflush (stdout);
  }

  void
  report (char const *when, int, unsigned from, unsigned to, int id = -1)
  {
    std::string names;
    bool listed[or_count];
    for (int o = 0; o < or_count; ++o)
      listed[o] = false;
    for (int i = 0; i < nfail_; ++i)
      listed[failures_[i].oracle] = true;
    for (int o = 0; o < or_count; ++o)
      if (listed[o])
        {
          if (!names.empty ())
            names += ',';
          names += oracle_name (static_cast<oracle_t> (o));
        }
    std::string text = "SIM-FAIL oracles=" + names;
    char buffer[600];
    explorer_t *const ex = explorer_;
    std::snprintf (buffer, sizeof (buffer),
                   " %s bound=%d schedule=%ld "
                   "decisions=%s\n",
                   ex != nullptr ? ex->header_text ().c_str () : "",
                   ex != nullptr ? ex->bound () : 0,
                   ex != nullptr ? ex->stats ().schedules + 1 : 0L,
                   crash_line ().decisions ().c_str ());
    text += buffer;
    std::snprintf (buffer, sizeof (buffer),
                   "  %s: step %ld: thread %d resumed at %s, stopped at %s\n",
                   when, stepno_, id, point_name (from), point_name (to));
    text += buffer;
    for (int i = 0; i < nfail_; ++i)
      {
        text += "  [";
        text += oracle_name (failures_[i].oracle);
        text += "] ";
        text += failures_[i].text;
        text += '\n';
      }
    std::snprintf (
        buffer, sizeof (buffer), "  cell 0: ghost %s hslot %d L=%d\n",
        label_text (gc_.value).c_str (), gc_.value.hslot, gc_.ntags);
    text += buffer;
    sim_mem_t &m = mem ();
    for (int s = 0; s < 6; ++s)
      {
        if (m.obj.generation (s) == 0u)
          continue;
        quant_t q;
        compute_quant (s, q);
        std::snprintf (buffer, sizeof (buffer),
                       "  block %s%s: count %ld ext %ld weak %ld/%ld | owners "
                       "%d = U%d S%d D%d X%d CNT%d CC%d LEFTNC%d | windows "
                       "i%d ii%d iii%d | dep %d owing %d\n",
                       label_text (block_val (s)).c_str (),
                       m.obj.is_live (s) ? "" : " (freed)", q.counts.count,
                       q.counts.ext, q.counts.weak_count, q.counts.weak_ext,
                       owners_of (q), q.U, q.S, q.D, q.X, q.CNT, q.CC,
                       q.LEFTNC, q.Wi, q.Wii, q.Wiii, q.DEP, q.OWING);
        text += buffer;
      }
    for (int t = 0; t < def_->nthreads; ++t)
      text += "  " + describe_thread (t) + '\n';
    text += "  last steps:\n";
    int const shown = ring_count_ < ring_size ? ring_count_ : ring_size;
    for (int k = ring_count_ - shown; k < ring_count_; ++k)
      {
        step_snap_t const &s = ring_[k % ring_size];
        std::snprintf (buffer, sizeof (buffer),
                       "    %d: thread %d %s -> %s | cell %s L=%d | A %ld/%ld "
                       "B %ld/%ld D %ld/%ld\n",
                       s.decision, s.thread, point_name (s.from),
                       point_name (s.to), label_text (s.value).c_str (),
                       s.ticks, s.count[0], s.ext[0], s.count[1], s.ext[1],
                       s.count[2], s.ext[2]);
        text += buffer;
      }
    if (ex != nullptr)
      ex->report_failure (text);
    else
      {
        std::fputs (text.c_str (), stderr);
        std::abort ();
      }
  }

  // ---- state ----

  scenario_def_t const *def_;
  policy_info_t info_;
  bool weak_engine_;
  cell_ops_t *cell_;
  ::lumex::core::atomic::dwcas::dwcas_word const *word_;
  alignas (64) unsigned char storage_[storage_bytes];
  thread_rt_t rt_[max_threads];
  snapshot_t snap_[max_threads];
  gcell_t gc_;
  split_value_t word_prev_;
  std::uint64_t signature_;
  lin_rec_t lin_[max_lin];
  int nlin_;
  failure_t failures_[max_fail];
  int nfail_;
  step_snap_t ring_[ring_size];
  int ring_count_;
  int owners_[6];
  int executing_op_[max_threads]; ///< the operation the next step from
                                  ///< op_begin runs
  long stepno_;
  std::uint64_t hash_;
  bool trace_all_;
  explorer_t *explorer_;
  std::set<std::string> seen_;
};

// -- Toy threads: the self-test of the explorer --

/// Threads that only yield (counts of schedules), or that lose an update.
class toy_world_t : public world_t
{
public:
  explicit toy_world_t (std::vector<int> const &yields,
                        bool lost_update = false)
      : yields_ (yields), lost_update_ (lost_update), shared_ (0)
  {
  }

  int
  thread_count () const override
  {
    return static_cast<int> (yields_.size ());
  }

  void
  begin () override
  {
    shared_ = 0;
  }

  void
  body (int id) override
  {
    if (lost_update_)
      {
        int const seen = shared_;
        sim_yield (pt_op_begin);
        shared_ = seen + 1;
        return;
      }
    for (int k = 0; k < yields_[static_cast<std::size_t> (id)]; ++k)
      sim_yield (pt_op_begin);
  }

  bool
  step (int, unsigned, unsigned, long, int) override
  {
    return true;
  }

  void
  end () override
  {
    if (lost_update_ && shared_ != 2 && current_explorer () != nullptr)
      current_explorer ()->report_failure (
          "SIM-FAIL oracles=TOY a lost update: shared is not 2\n");
  }

private:
  std::vector<int> yields_;
  bool lost_update_;
  int shared_;
};

// -- Budgets and runs --

/// What one search may spend.
struct budget_t
{
  int bound;
  long cap; ///< schedules of the depth-first search (0: no cap)
  long random_count;
  std::uint64_t seed;
  double seconds;
  bool assert_coverage;
};

inline double
time_scale ()
{
  char const *const text = std::getenv ("LUMEX_SIM_TIME_SCALE");
  if (text == nullptr)
    return 1.0;
  double const value = std::atof (text);
  return value > 0.0 ? value : 1.0;
}

inline std::uint64_t
sim_seed ()
{
  return 20261010u;
}

/// The quick budget by the number of threads of the scenario.
inline budget_t
quick_budget (int threads)
{
  budget_t b;
  b.bound = threads <= 3 ? 2 : 1;
  b.cap = threads >= 5 ? 10000 : 0;
  b.random_count = threads >= 5 ? 200 : 0;
  b.seed = sim_seed ();
  b.seconds = 120.0 * time_scale ();
  b.assert_coverage = false;
  return b;
}

/// The soak budget by the number of threads of the scenario.
inline budget_t
soak_budget (int threads)
{
  budget_t b;
  b.bound = threads <= 3 ? 3 : 2;
  b.cap = threads >= 5 ? 600000 : 0;
  b.random_count = threads >= 5 ? 5000 : 0;
  b.seed = sim_seed ();
  b.seconds = 1800.0 * time_scale ();
  b.assert_coverage = true;
  return b;
}

/// Installs the holder redirect of the allocation hooks for a run.
class redirect_guard_t
{
public:
  redirect_guard_t ()
  {
    split_test::sim_alloc_redirect_t &r = split_test::sim_alloc_redirect ();
    r.allocate = &redirect_allocate;
    r.release = &redirect_release;
  }

  ~redirect_guard_t ()
  {
    split_test::sim_alloc_redirect_t &r = split_test::sim_alloc_redirect ();
    r.allocate = nullptr;
    r.release = nullptr;
  }

  redirect_guard_t (redirect_guard_t const &) = delete;
  redirect_guard_t &operator= (redirect_guard_t const &) = delete;
};

/// What `LUMEX_SIM_ONLY` and `LUMEX_SIM_REPLAY` ask for.
struct selection_t
{
  bool only;
  bool replay;
  std::string scenario;
  std::string policy;
  int bound;
  std::string decisions;
};

inline selection_t
selection_from_environment ()
{
  selection_t s;
  s.only = false;
  s.replay = false;
  s.bound = 0;
  char const *const only = std::getenv ("LUMEX_SIM_ONLY");
  if (only != nullptr && only[0] != '\0')
    {
      std::string const text = only;
      std::size_t const a = text.find ('@');
      std::size_t const b = text.find ('@', a + 1);
      if (a != std::string::npos && b != std::string::npos)
        {
          s.only = true;
          s.scenario = text.substr (0, a);
          s.policy = text.substr (a + 1, b - a - 1);
          s.bound = std::atoi (text.c_str () + b + 1);
        }
    }
  char const *const replay = std::getenv ("LUMEX_SIM_REPLAY");
  if (replay != nullptr && replay[0] != '\0')
    {
      std::string const text = replay;
      std::size_t const a = text.find ('@');
      std::size_t const b = text.find (':');
      if (a != std::string::npos && b != std::string::npos && a < b)
        {
          s.replay = true;
          s.scenario = text.substr (0, a);
          s.policy = text.substr (a + 1, b - a - 1);
          s.decisions = text.substr (b + 1);
        }
    }
  return s;
}

/// Why a test of (@p id, @p policy) must be skipped; null: it runs.
inline char const *
skip_reason (scenario_id_t id, policy_id_t policy)
{
  if (!split_test::alloc_hooks_active ())
    return "the simulator needs the allocation hooks";
  scenario_def_t const &def = scenario_of (id);
  if (!scenario_runs_policy (def, policy))
    return "the scenario does not run with this policy";
  selection_t const s = selection_from_environment ();
  if ((s.only || s.replay)
      && (s.scenario != def.name || s.policy != policy_name (policy)))
    return "narrowed to another scenario by LUMEX_SIM_ONLY or "
           "LUMEX_SIM_REPLAY";
  return nullptr;
}

/// The result of one run.
struct run_result_t
{
  search_stats_t dfs;
  search_stats_t random;
  int seen;
  int expected;
  bool covered;
  int bound;
};

/// Explores the scenario @p id with the policy @p policy within @p budget.
/// The environment may narrow the bound (`LUMEX_SIM_ONLY`) or replay one
/// schedule (`LUMEX_SIM_REPLAY`).
inline run_result_t
run_scenario (scenario_id_t id, policy_id_t policy, budget_t budget)
{
  scenario_def_t const &def = scenario_of (id);
  selection_t const choice = selection_from_environment ();
  redirect_guard_t guard;
  scenario_world_t world (def, policy_info (policy));
  char header[120];
  std::snprintf (header, sizeof (header), "scenario=%s policy=%s", def.name,
                 policy_name (policy));
  explorer_t explorer (world, header);
  world.set_explorer (&explorer);
  run_result_t result;
  std::memset (&result, 0, sizeof (result));
  if (choice.replay)
    {
      explorer.set_trace (true);
      explorer.run_replay (choice.decisions);
      result.dfs = explorer.stats ();
      result.seen = static_cast<int> (world.seen ().size ());
      result.expected = static_cast<int> (def.expected.size ());
      return result;
    }
  if (choice.only)
    {
      budget.bound = choice.bound;
      budget.cap = 600000;
      budget.random_count = 0;
      budget.seconds = 1800.0 * time_scale ();
    }
  explorer.set_time_limit (budget.seconds);
  explorer.set_trace (env_flag ("LUMEX_SIM_TRACE"));
  explorer.run_dfs (budget.bound, budget.cap);
  result.dfs = explorer.stats ();
  result.bound = budget.bound;
  if (budget.random_count > 0)
    {
      explorer_t randomizer (world, header);
      world.set_explorer (&randomizer);
      randomizer.set_bound_text (-1);
      randomizer.set_time_limit (budget.seconds);
      randomizer.run_random (budget.seed, budget.random_count);
      result.random = randomizer.stats ();
      world.set_explorer (&explorer);
    }
  result.seen = static_cast<int> (world.seen ().size ());
  result.expected = static_cast<int> (def.expected.size ());
  result.covered = result.seen == result.expected;
  if (!result.covered)
    for (std::set<std::string>::const_iterator it = def.expected.begin ();
         it != def.expected.end (); ++it)
      if (world.seen ().count (*it) == 0)
        std::printf ("SIM-UNSEEN scenario=%s policy=%s outcome=%s\n", def.name,
                     policy_name (policy), it->c_str ());
  std::printf ("SIM-STATS scenario=%s policy=%s bound=%d schedules=%ld "
               "random=%ld steps=%ld seconds=%.2f outcomes=%d/%d capped=%d\n",
               def.name, policy_name (policy), budget.bound,
               result.dfs.schedules, result.random.schedules,
               result.dfs.steps + result.random.steps,
               result.dfs.seconds + result.random.seconds, result.seen,
               result.expected, result.dfs.capped ? 1 : 0);
  std::fflush (stdout);
  return result;
}
} // namespace split_sim

#endif // LUMEX_ATOMIC_SMART_PTR_HAS_SPLIT_COUNT

#endif // !LUMEX_TESTS_CORE_ATOMIC_SMART_PTR_SPLIT_COUNT_SIM_HPP
