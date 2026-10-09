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

// The hazard pointer domain: the pool of hazard slots, the per-thread slot
// cache, the retired lists and the reclamation pass.
//
// Everything here is written from the ideas of the hazard pointer
// implementations that are public (Michael's protocol; the record, cache and
// retired-list structure of Folly and of the libc++ proposal); no source text
// of them is copied. The state is one constant-initialized object that is
// never destroyed, so a hazard pointer can be made, used and released, and an
// object can be retired, from a static constructor or destructor.
//
// Slots (records) are 64-byte cache lines that live in blocks and are never
// freed. A free slot is either in the cache of the thread (up to
// kCacheCapacity slots, used without any atomic operation) or in the shared
// pool, a lock-free stack in which bit 0 of the head word locks pops out
// (pushes stay lock-free), which removes the ABA problem of a plain Treiber
// stack. The cache of a thread is created on first use and handed back by a
// hook the operating system calls when the thread exits (a pthread key
// destructor, a fiber local storage callback); a thread that is already past
// its hook uses the pool directly, so a late release from another thread-exit
// hook works.
//
// Retired nodes sit in kShards lists, pushed lock-free. A pass (at most one at
// a time) takes all of them, issues a sequentially consistent fence, reads
// every slot into a hash set, reclaims what no slot holds and pushes the
// protected rest back. A pass runs on the thread whose retire crosses the
// threshold (twice the number of slots, at least 1000, or two seconds since
// the last pass); deleters run on that thread. There are no background
// threads.

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <new>
#include <thread>

#if defined(_WIN32)
// Fiber local storage (FlsAlloc) is Windows Vista and later; older MinGW
// headers default to Windows XP.
#if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0600
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif
#if !defined(NOMINMAX)
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <pthread.h>
#endif

#include "lumex/core/hazard_pointer/engine/LumexHazardPointerEngine.hpp"
#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#if __has_warning("-Wunsafe-buffer-usage-in-libc-call")
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-libc-call"
#endif
#pragma clang diagnostic ignored "-Wglobal-constructors"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wcast-align"
#pragma clang diagnostic ignored "-Wcast-qual"
#endif

#if defined(__SANITIZE_THREAD__)
#define LUMEX_HAZARD_POINTER_UNDER_TSAN 1
#elif defined(__has_feature)
#if __has_feature(thread_sanitizer)
#define LUMEX_HAZARD_POINTER_UNDER_TSAN 1
#endif
#endif

namespace lumex
{
namespace core
{
namespace hazard_pointer
{
namespace engine
{
namespace
{
// One slot per cache line; the size of the line of the processors this
// library targets (128 on Apple arm64 and ppc64).
#if defined(__APPLE__) && defined(__aarch64__)
static LUMEX_CONSTEXPR std::size_t kLine = 128;
#elif defined(__powerpc64__)
static LUMEX_CONSTEXPR std::size_t kLine = 128;
#else
static LUMEX_CONSTEXPR std::size_t kLine = 64;
#endif

// Slots a thread keeps for itself.
static LUMEX_CONSTEXPR std::size_t kCacheCapacity = 8;

// Retired lists.
static LUMEX_CONSTEXPR std::size_t kShards = 8;

// Records per new block: as many as exist, within these bounds.
static LUMEX_CONSTEXPR std::size_t kMinBlock = 32;
static LUMEX_CONSTEXPR std::size_t kMaxBlock = 512;

// A pass starts when this many nodes are waiting, or twice the number of
// slots if that is more; or when kPassPeriodSeconds went by (checked on every
// kClockSample-th retire so the clock is not read each time).
static LUMEX_CONSTEXPR long kMinThreshold = 1000;
static LUMEX_CONSTEXPR long kPassPeriodSeconds = 2;
static LUMEX_CONSTEXPR unsigned long kClockSample = 64;

#if defined(LUMEX_HAZARD_POINTER_UNDER_TSAN)
// ThreadSanitizer does not model fences: acquire loads show it the ordering
// the fence provides.
static LUMEX_CONSTEXPR std::memory_order kSlotRead = std::memory_order_acquire;
#else
static LUMEX_CONSTEXPR std::memory_order kSlotRead = std::memory_order_relaxed;
#endif

struct alignas (kLine) record_t : slot_t
{
  record_t *next_free;
};

static_assert (sizeof (record_t) == kLine, "one slot per cache line");

struct block_t
{
  block_t *next;
  record_t *records;
  std::size_t count;
};

struct alignas (kLine) shard_t
{
  std::atomic<node_t *> head;
};

struct cache_t
{
  slot_t *items[kCacheCapacity];
  std::size_t count;
};

struct domain_t
{
  // Blocks of slots (a lock-free stack, only ever pushed), newest first.
  std::atomic<block_t *> blocks;
  // The pool of free slots: record address, bit 0 locks pops.
  std::atomic<std::uintptr_t> avail;
  std::atomic<std::size_t> records;
  std::atomic<std::size_t> pooled;
  std::atomic<std::size_t> retired;
  std::atomic<std::size_t> reclaimed;
  std::atomic<std::size_t> passes;
  // Nodes waiting in the retired lists (may dip below zero for a moment).
  std::atomic<long> pending;
  // When the next pass is due by the clock, in seconds of the steady clock; 0
  // before the first sample.
  std::atomic<long> due_seconds;
  // 1 while a pass runs.
  std::atomic<int> pass_lock;
  // 0 not tried, 1 being set up, 2 ready, 3 not available.
  std::atomic<int> hook_state;
  shard_t shards[kShards];
};

// Constant-initialized (all members are trivially constructible and zero
// before any code runs) and trivially destructible.
domain_t g_domain;

// Stands for the cache of a thread that has none (its hook ran, or none could
// be made).
cache_t g_no_cache;

thread_local cache_t *t_cache = nullptr;
thread_local bool t_in_pass = false;

#if defined(LUMEX_HAZARD_POINTER_TEST_HOOKS)
std::atomic<test_handler_t> g_test_handler;
std::atomic<bool> g_test_linear_scan;
#endif

// ---------------------------------------------------------------------------
// The pool of free slots
// ---------------------------------------------------------------------------

void
backoff (unsigned &spins) LUMEX_NOEXCEPT
{
  if (++spins > 16)
    {
      std::this_thread::yield ();
    }
}

// Pushes the chain first .. last (linked through next_free) of `count` slots.
void
push_chain (record_t *first, record_t *last, std::size_t count) LUMEX_NOEXCEPT
{
  std::uintptr_t head = g_domain.avail.load (std::memory_order_relaxed);
  do
    {
      last->next_free
          = reinterpret_cast<record_t *> (head & ~std::uintptr_t (1));
    }
  while (!g_domain.avail.compare_exchange_weak (
      head, reinterpret_cast<std::uintptr_t> (first) | (head & 1),
      std::memory_order_release, std::memory_order_relaxed));
  g_domain.pooled.fetch_add (count, std::memory_order_relaxed);
}

// Pops one slot, or returns null when the pool is empty. A pop takes bit 0
// first, so two pops never overlap and the head cannot be removed and put
// back under a popper; concurrent pushes only add in front.
record_t *
pop_pool () LUMEX_NOEXCEPT
{
  std::uintptr_t head = g_domain.avail.load (std::memory_order_relaxed);
  unsigned spins = 0;
  for (;;)
    {
      if (head == 0)
        {
          return nullptr;
        }
      if ((head & 1) != 0)
        {
          backoff (spins);
          head = g_domain.avail.load (std::memory_order_relaxed);
          continue;
        }
      if (g_domain.avail.compare_exchange_weak (head, head | 1,
                                                std::memory_order_acquire,
                                                std::memory_order_acquire))
        {
          head |= 1;
          break;
        }
    }
  for (;;)
    {
      record_t *top
          = reinterpret_cast<record_t *> (head & ~std::uintptr_t (1));
      record_t *next = top->next_free;
      // Unlock and pop in one step; a push that got in between changes the
      // head and the loop reads it again.
      if (g_domain.avail.compare_exchange_weak (
              head, reinterpret_cast<std::uintptr_t> (next),
              std::memory_order_acquire, std::memory_order_acquire))
        {
          g_domain.pooled.fetch_sub (1, std::memory_order_relaxed);
          return top;
        }
    }
}

// Makes a block of slots, publishes it, puts all but one slot in the pool and
// returns that one. The block is published before any slot of it is handed
// out, so a pass that starts after a reader's fence finds the reader's slot.
record_t *
take_from_new_block ()
{
  std::size_t const known = g_domain.records.load (std::memory_order_relaxed);
  std::size_t const count = known < kMinBlock
                                ? kMinBlock
                                : (known > kMaxBlock ? kMaxBlock : known);
  std::size_t const bytes
      = sizeof (block_t) + count * sizeof (record_t) + kLine;
  void *const raw = ::operator new (bytes);
  block_t *const block = static_cast<block_t *> (raw);
  std::uintptr_t const aligned
      = (reinterpret_cast<std::uintptr_t> (raw) + sizeof (block_t) + kLine - 1)
        & ~(std::uintptr_t (kLine) - 1);
  record_t *const records = reinterpret_cast<record_t *> (aligned);
  for (std::size_t i = 0; i < count; ++i)
    {
      new (&records[i]) record_t ();
      records[i].next_free = i + 1 < count ? &records[i + 1] : nullptr;
    }
  block->records = records;
  block->count = count;
  block_t *top = g_domain.blocks.load (std::memory_order_relaxed);
  do
    {
      block->next = top;
    }
  while (!g_domain.blocks.compare_exchange_weak (
      top, block, std::memory_order_release, std::memory_order_relaxed));
  g_domain.records.fetch_add (count, std::memory_order_relaxed);
  if (count > 1)
    {
      push_chain (&records[1], &records[count - 1], count - 1);
    }
  return &records[0];
}

// ---------------------------------------------------------------------------
// The per-thread cache and its exit hook
// ---------------------------------------------------------------------------

// Hands the slots of a cache back to the pool and retires the cache; the
// thread goes on without one.
void
evict_cache (cache_t *cache) LUMEX_NOEXCEPT
{
  LUMEX_HAZARD_POINTER_TEST_POINT (cache_evicting);
  if (t_cache == cache)
    {
      t_cache = &g_no_cache;
    }
  if (cache->count > 0)
    {
      record_t *first = nullptr;
      record_t *last = nullptr;
      for (std::size_t i = 0; i < cache->count; ++i)
        {
          record_t *rec = static_cast<record_t *> (cache->items[i]);
          rec->next_free = first;
          if (last == nullptr)
            {
              last = rec;
            }
          first = rec;
        }
      push_chain (first, last, cache->count);
    }
  delete cache;
}

#if defined(_WIN32)
// The module stays loaded: the callback below must exist whenever a thread of
// the process can still exit.
char const g_module_marker = 0;
DWORD g_fls_index = FLS_OUT_OF_INDEXES;

VOID WINAPI
fls_callback (PVOID data)
{
  if (data != nullptr)
    {
      evict_cache (static_cast<cache_t *> (data));
    }
}

bool
create_exit_hook () LUMEX_NOEXCEPT
{
  g_fls_index = FlsAlloc (&fls_callback);
  if (g_fls_index == FLS_OUT_OF_INDEXES)
    {
      return false;
    }
  HMODULE module = nullptr;
  GetModuleHandleExW (GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                          | GET_MODULE_HANDLE_EX_FLAG_PIN,
                      reinterpret_cast<LPCWSTR> (&g_module_marker), &module);
  return true;
}

bool
set_exit_hook_value (cache_t *cache) LUMEX_NOEXCEPT
{
  return FlsSetValue (g_fls_index, cache) != 0;
}
#else
pthread_key_t g_key;

void
pthread_exit_hook (void *data)
{
  if (data != nullptr)
    {
      evict_cache (static_cast<cache_t *> (data));
    }
}

bool
create_exit_hook () LUMEX_NOEXCEPT
{
  return pthread_key_create (&g_key, &pthread_exit_hook) == 0;
}

bool
set_exit_hook_value (cache_t *cache) LUMEX_NOEXCEPT
{
  return pthread_setspecific (g_key, cache) == 0;
}
#endif

// Sets the exit hook up once per process; false when the platform refuses.
bool
exit_hook_ready () LUMEX_NOEXCEPT
{
  int state = g_domain.hook_state.load (std::memory_order_acquire);
  if (state == 0)
    {
      int expected = 0;
      if (g_domain.hook_state.compare_exchange_strong (
              expected, 1, std::memory_order_acquire))
        {
          state = create_exit_hook () ? 2 : 3;
          g_domain.hook_state.store (state, std::memory_order_release);
        }
      else
        {
          state = expected;
        }
    }
  while (state == 1)
    {
      std::this_thread::yield ();
      state = g_domain.hook_state.load (std::memory_order_acquire);
    }
  return state == 2;
}

cache_t *
create_cache () LUMEX_NOEXCEPT
{
  cache_t *cache = new (std::nothrow) cache_t ();
  if (cache != nullptr && exit_hook_ready () && set_exit_hook_value (cache))
    {
      t_cache = cache;
      return cache;
    }
  delete cache;
  t_cache = &g_no_cache;
  return &g_no_cache;
}

// The cache of the calling thread, or null when it has none.
cache_t *
get_cache () LUMEX_NOEXCEPT
{
  cache_t *cache = t_cache;
  if (cache == nullptr)
    {
      cache = create_cache ();
    }
  return cache == &g_no_cache ? nullptr : cache;
}

slot_t *
take_one ()
{
  cache_t *cache = get_cache ();
  if (LUMEX_ATTRIBUTE_LIKELY_COND (cache != nullptr && cache->count > 0))
    {
      --cache->count;
      return cache->items[cache->count];
    }
  record_t *rec = pop_pool ();
  if (rec != nullptr)
    {
      return rec;
    }
  return take_from_new_block ();
}

// ---------------------------------------------------------------------------
// Retired nodes and the pass
// ---------------------------------------------------------------------------

std::size_t
shard_of (node_t const *node) LUMEX_NOEXCEPT
{
  return (reinterpret_cast<std::uintptr_t> (node) >> 6) & (kShards - 1);
}

void
push_node (node_t *node) LUMEX_NOEXCEPT
{
  std::atomic<node_t *> &head = g_domain.shards[shard_of (node)].head;
  node_t *top = head.load (std::memory_order_relaxed);
  do
    {
      node->next = top;
    }
  while (!head.compare_exchange_weak (top, node, std::memory_order_release,
                                      std::memory_order_relaxed));
}

// Takes every waiting node; returns how many.
std::size_t
take_all (node_t *&list) LUMEX_NOEXCEPT
{
  list = nullptr;
  std::size_t count = 0;
  for (std::size_t s = 0; s < kShards; ++s)
    {
      node_t *chain = g_domain.shards[s].head.exchange (
          nullptr, std::memory_order_acquire);
      while (chain != nullptr)
        {
          node_t *next = chain->next;
          chain->next = list;
          list = chain;
          ++count;
          chain = next;
        }
    }
  return count;
}

// The addresses the slots hold, for the pass. Allocation failure leaves the
// set unusable and the pass matches node by node against the slots instead.
class address_set_t
{
public:
  address_set_t () LUMEX_NOEXCEPT : table_ (nullptr), mask_ (0) {}
  address_set_t (address_set_t const &) = delete;
  address_set_t &operator= (address_set_t const &) = delete;
  ~address_set_t () { delete[] table_; }

  bool
  reserve (std::size_t entries) LUMEX_NOEXCEPT
  {
    std::size_t capacity = 16;
    while (capacity < entries * 2)
      {
        capacity *= 2;
      }
    table_ = new (std::nothrow) std::uintptr_t[capacity];
    if (table_ == nullptr)
      {
        return false;
      }
    for (std::size_t i = 0; i < capacity; ++i)
      {
        table_[i] = 0;
      }
    mask_ = capacity - 1;
    return true;
  }

  bool
  usable () const LUMEX_NOEXCEPT
  {
    return table_ != nullptr;
  }

  void
  insert (std::uintptr_t value) LUMEX_NOEXCEPT
  {
    std::size_t at = index_of (value);
    while (table_[at] != 0 && table_[at] != value)
      {
        at = (at + 1) & mask_;
      }
    table_[at] = value;
  }

  bool
  contains (std::uintptr_t value) const LUMEX_NOEXCEPT
  {
    std::size_t at = index_of (value);
    while (table_[at] != 0)
      {
        if (table_[at] == value)
          {
            return true;
          }
        at = (at + 1) & mask_;
      }
    return false;
  }

private:
  std::size_t
  index_of (std::uintptr_t value) const LUMEX_NOEXCEPT
  {
    std::uintptr_t mixed = value >> 4;
    mixed ^= mixed >> 17;
    mixed *= static_cast<std::uintptr_t> (0x9E3779B1u);
    return static_cast<std::size_t> (mixed) & mask_;
  }

  std::uintptr_t *table_;
  std::size_t mask_;
};

// True when a slot of the blocks holds `value`.
bool
slots_hold (block_t const *blocks, std::uintptr_t value) LUMEX_NOEXCEPT
{
  for (block_t const *block = blocks; block != nullptr; block = block->next)
    {
      for (std::size_t i = 0; i < block->count; ++i)
        {
          if (reinterpret_cast<std::uintptr_t> (
                  block->records[i].value.load (kSlotRead))
              == value)
            {
              return true;
            }
        }
    }
  return false;
}

// Reclaims or keeps the nodes of one batch; `residue` collects the kept ones.
void
sweep (node_t *batch, std::size_t taken, node_t *&residue,
       std::size_t &residue_count) LUMEX_NOEXCEPT
{
  // The pass's side of the fence: pairs with the fence of every reader
  // (reader_fence). Either a reader's announcement is visible below, or the
  // reader's validation sees the removal that preceded the retirement.
  std::atomic_thread_fence (std::memory_order_seq_cst);
  LUMEX_HAZARD_POINTER_TEST_POINT (pass_fenced);

  block_t const *blocks = g_domain.blocks.load (std::memory_order_acquire);
  address_set_t set;
  bool use_set = taken > 8;
#if defined(LUMEX_HAZARD_POINTER_TEST_HOOKS)
  if (g_test_linear_scan.load (std::memory_order_relaxed))
    {
      use_set = false;
    }
#endif
  if (use_set)
    {
      std::size_t slots = 0;
      for (block_t const *block = blocks; block != nullptr;
           block = block->next)
        {
          slots += block->count;
        }
      if (set.reserve (slots))
        {
          for (block_t const *block = blocks; block != nullptr;
               block = block->next)
            {
              for (std::size_t i = 0; i < block->count; ++i)
                {
                  void const *held = block->records[i].value.load (kSlotRead);
                  if (held != nullptr)
                    {
                      set.insert (reinterpret_cast<std::uintptr_t> (held));
                    }
                }
            }
        }
    }
  LUMEX_HAZARD_POINTER_TEST_POINT (pass_scanned);
  // The slots' release stores (a reader that finished) become visible here.
  std::atomic_thread_fence (std::memory_order_acquire);

  node_t *node = batch;
  while (node != nullptr)
    {
      node_t *next = node->next;
      std::uintptr_t const address = reinterpret_cast<std::uintptr_t> (node);
      bool const held = set.usable () ? set.contains (address)
                                      : slots_hold (blocks, address);
      if (held)
        {
          node->next = residue;
          residue = node;
          ++residue_count;
        }
      else
        {
          g_domain.reclaimed.fetch_add (1, std::memory_order_relaxed);
          node->reclaim (node);
        }
      node = next;
    }
}

// Runs passes until the retired lists are empty (a deleter may retire more),
// then puts the protected nodes back. The caller holds pass_lock.
void
run_pass_locked () LUMEX_NOEXCEPT
{
  t_in_pass = true;
  node_t *residue = nullptr;
  std::size_t residue_count = 0;
  for (;;)
    {
      node_t *batch = nullptr;
      std::size_t const taken = take_all (batch);
      if (taken == 0)
        {
          break;
        }
      g_domain.passes.fetch_add (1, std::memory_order_relaxed);
      // Settle the count: the taken nodes are no longer waiting. Without this
      // the next retire would start another pass at once.
      g_domain.pending.fetch_sub (static_cast<long> (taken),
                                  std::memory_order_relaxed);
      sweep (batch, taken, residue, residue_count);
    }
  while (residue != nullptr)
    {
      node_t *next = residue->next;
      push_node (residue);
      residue = next;
    }
  g_domain.pending.fetch_add (static_cast<long> (residue_count),
                              std::memory_order_relaxed);
  t_in_pass = false;
}

bool
try_lock_pass () LUMEX_NOEXCEPT
{
  return g_domain.pass_lock.exchange (1, std::memory_order_acquire) == 0;
}

void
unlock_pass () LUMEX_NOEXCEPT
{
  g_domain.pass_lock.store (0, std::memory_order_release);
}

long
threshold () LUMEX_NOEXCEPT
{
  long const doubled = 2
                       * static_cast<long> (
                           g_domain.records.load (std::memory_order_relaxed));
  return doubled < kMinThreshold ? kMinThreshold : doubled;
}

// True when the pass period has passed; the first sample only arms the clock.
bool
period_over () LUMEX_NOEXCEPT
{
  long const now = static_cast<long> (
      std::chrono::duration_cast<std::chrono::seconds> (
          std::chrono::steady_clock::now ().time_since_epoch ())
          .count ());
  long due = g_domain.due_seconds.load (std::memory_order_relaxed);
  if (now < due)
    {
      return false;
    }
  bool const armed = due != 0;
  if (!g_domain.due_seconds.compare_exchange_strong (
          due, now + kPassPeriodSeconds, std::memory_order_relaxed))
    {
      return false;
    }
  return armed;
}
} // namespace

slot_t *
acquire_slot ()
{
  return take_one ();
}

void
acquire_slots (slot_t **out, std::size_t count)
{
  std::size_t got = 0;
  try
    {
      for (; got < count; ++got)
        {
          out[got] = take_one ();
        }
    }
  catch (...)
    {
      for (std::size_t i = 0; i < got; ++i)
        {
          release_slot (out[i]);
        }
      throw;
    }
}

void
release_slot (slot_t *slot) LUMEX_NOEXCEPT
{
  // Ends the protection; the release store orders the reader's last use of
  // the object before the reclamation that sees null.
  slot->value.store (nullptr, std::memory_order_release);
  cache_t *cache = get_cache ();
  if (LUMEX_ATTRIBUTE_LIKELY_COND (cache != nullptr
                                   && cache->count < kCacheCapacity))
    {
      cache->items[cache->count] = slot;
      ++cache->count;
      return;
    }
  record_t *rec = static_cast<record_t *> (slot);
  push_chain (rec, rec, 1);
}

void
retire_node (node_t *node) LUMEX_NOEXCEPT
{
  LUMEX_ASSERT (node != nullptr);
  LUMEX_ASSERT (node->reclaim != nullptr);
  // An object is retired at most once: a fresh node points at itself.
  LUMEX_ASSERT (node->next == node);
  // Keeps the removal that preceded this call ahead of the pass's fence even
  // when the reader's fence is weakened to a compiler barrier later.
  std::atomic_thread_fence (std::memory_order_seq_cst);
  push_node (node);
  g_domain.retired.fetch_add (1, std::memory_order_relaxed);
  long const waiting
      = g_domain.pending.fetch_add (1, std::memory_order_acq_rel) + 1;
  LUMEX_HAZARD_POINTER_TEST_POINT (retire_pushed);
  bool due = waiting >= threshold ();
  if (!due && (static_cast<unsigned long> (waiting) % kClockSample) == 0)
    {
      due = period_over ();
    }
  if (due && try_lock_pass ())
    {
      run_pass_locked ();
      unlock_pass ();
    }
}

bool
reclaim_or_retire (node_t *node) LUMEX_NOEXCEPT
{
  LUMEX_ASSERT (node != nullptr);
  LUMEX_ASSERT (node->reclaim != nullptr);
  // An object is retired at most once: a fresh node points at itself.
  LUMEX_ASSERT (node->next == node);
  // The removal that preceded this call, then the scan: the pass's side of
  // the protocol (see sweep) applied to this one address. Either a reader's
  // announcement is visible below, or the reader's validation sees the
  // removal and does not use the object.
  std::atomic_thread_fence (std::memory_order_seq_cst);
  LUMEX_HAZARD_POINTER_TEST_POINT (pass_fenced);
  block_t const *blocks = g_domain.blocks.load (std::memory_order_acquire);
  bool const held
      = slots_hold (blocks, reinterpret_cast<std::uintptr_t> (node));
  LUMEX_HAZARD_POINTER_TEST_POINT (pass_scanned);
  if (held)
    {
      // Somebody names it (or a reader announced it and has not validated
      // yet): the ordinary path, which keeps it until nobody does.
      retire_node (node);
      return false;
    }
  // The slots' release stores (a reader that finished) become visible here,
  // before the object is reclaimed.
  std::atomic_thread_fence (std::memory_order_acquire);
  g_domain.retired.fetch_add (1, std::memory_order_relaxed);
  g_domain.reclaimed.fetch_add (1, std::memory_order_relaxed);
  node->reclaim (node);
  return true;
}

void
clean_up () LUMEX_NOEXCEPT
{
  if (t_in_pass)
    {
      return;
    }
  unsigned spins = 0;
  while (!try_lock_pass ())
    {
      backoff (spins);
    }
  run_pass_locked ();
  unlock_pass ();
}

statistics_t
statistics () LUMEX_NOEXCEPT
{
  statistics_t result;
  result.records = g_domain.records.load (std::memory_order_relaxed);
  result.pooled_records = g_domain.pooled.load (std::memory_order_relaxed);
  result.retired = g_domain.retired.load (std::memory_order_relaxed);
  result.reclaimed = g_domain.reclaimed.load (std::memory_order_relaxed);
  result.passes = g_domain.passes.load (std::memory_order_relaxed);
  return result;
}

#if defined(LUMEX_HAZARD_POINTER_TEST_HOOKS)
void
test_point (int id) LUMEX_NOEXCEPT
{
  test_handler_t handler = g_test_handler.load (std::memory_order_acquire);
  if (handler != nullptr)
    {
      handler (id);
    }
}

void
set_test_handler (test_handler_t handler) LUMEX_NOEXCEPT
{
  g_test_handler.store (handler, std::memory_order_release);
}

void
set_test_linear_scan (bool enabled) LUMEX_NOEXCEPT
{
  g_test_linear_scan.store (enabled, std::memory_order_relaxed);
}
#endif
} // namespace engine
} // namespace hazard_pointer
} // namespace core
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
