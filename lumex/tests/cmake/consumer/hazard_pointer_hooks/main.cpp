// Deterministic tests of the hazard pointer protocol. The engine in this
// executable is built with test hooks, so a thread can be stalled at a named
// point: right after a reader announced and fenced, or inside a reclamation
// pass after it read the slots. With the two threads held there, the outcome
// of the race between "the reader announced" and "the object was removed" is
// not a matter of timing. Exit code 0 means every check passed.

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <new>
#include <thread>
#include <vector>

#include "lumex/core/hazard_pointer/LumexHazardPointer"

namespace hp = lumex::core::hazard_pointer;
namespace engine = lumex::core::hazard_pointer::engine;

static int g_failures = 0;

#define CHECK(condition)                                                      \
  do                                                                          \
    {                                                                         \
      if (!(condition))                                                       \
        {                                                                     \
          std::fprintf (stderr, "%s:%d: CHECK failed: %s\n", __FILE__,        \
                        __LINE__, #condition);                                \
          ++g_failures;                                                       \
        }                                                                     \
    }                                                                         \
  while (false)

// --- injected allocation failure
// -----------------------------------------------

namespace
{
// -1: off; n >= 0: the allocation that finds the counter at 0 throws.
std::atomic<int> g_fail_after (-1);
}

void *
operator new (std::size_t size)
{
  int const left = g_fail_after.load ();
  if (left >= 0 && g_fail_after.fetch_sub (1) == 0)
    {
      throw std::bad_alloc ();
    }
  void *memory = std::malloc (size != 0 ? size : 1);
  if (memory == nullptr)
    {
      throw std::bad_alloc ();
    }
  return memory;
}

void
operator delete (void *memory) noexcept
{
  std::free (memory);
}

void
operator delete (void *memory, std::size_t) noexcept
{
  std::free (memory);
}

namespace
{
struct node : hp::hazard_pointer_obj_base<node>
{
  explicit node (std::atomic<int> &the_deleted)
      : deleted (the_deleted), alive_marker (0x600D)
  {
  }
  ~node ()
  {
    alive_marker = 0xDEAD;
    deleted.fetch_add (1);
  }

  std::atomic<int> &deleted;
  int alive_marker;
};

// --- stalling threads at test points ------------------------------------

std::atomic<int> g_stall_point (0);
std::atomic<bool> g_arrived (false);
std::atomic<bool> g_release (false);
thread_local bool t_stalls = false;
thread_local std::vector<int> t_events;
thread_local bool t_records = false;

void
handler (int id)
{
  if (t_records)
    {
      t_events.push_back (id);
    }
  if (t_stalls && id == g_stall_point.load ())
    {
      g_arrived.store (true);
      while (!g_release.load ())
        {
          std::this_thread::yield ();
        }
    }
}

void
arm (int point)
{
  g_stall_point.store (point);
  g_arrived.store (false);
  g_release.store (false);
}

void
wait_until_arrived ()
{
  while (!g_arrived.load ())
    {
      std::this_thread::yield ();
    }
}

void
release ()
{
  g_release.store (true);
}

// --- the checks
// --------------------------------------------------------------

// A try_protect issues the reader's fence before it re-reads the source, and
// the announcement is complete when the test point after the fence is hit.
void
check_fence_comes_before_the_reload ()
{
  std::atomic<int> deleted (0);
  std::atomic<node *> source (new node (deleted));
  hp::hazard_pointer holder = hp::make_hazard_pointer ();
  t_records = true;
  t_events.clear ();
  node *ptr = source.load ();
  bool const ok = holder.try_protect (ptr, source);
  t_records = false;
  CHECK (ok);
  CHECK (t_events.size () == 2);
  if (t_events.size () == 2)
    {
      CHECK (t_events[0] == engine::reader_fenced);
      CHECK (t_events[1] == engine::reader_announced);
    }
  holder.reset_protection ();
  node *old = source.exchange (nullptr);
  old->retire ();
  hp::clean_up ();
  CHECK (deleted.load () == 1);
}

// A reader announced and waits before it re-reads. The object is removed and
// retired meanwhile and a pass runs: it must see the announcement and keep the
// object. When the reader goes on, its validation fails.
void
check_pass_sees_a_stalled_announcement ()
{
  std::atomic<int> deleted (0);
  node *const a = new node (deleted);
  node *const b = new node (deleted);
  std::atomic<node *> source (a);
  std::atomic<bool> reader_result (true);
  std::atomic<node *> reader_ptr (nullptr);
  arm (engine::reader_announced);
  std::thread reader (
      [&]
        {
          t_stalls = true;
          hp::hazard_pointer holder = hp::make_hazard_pointer ();
          node *ptr = source.load ();
          reader_result.store (holder.try_protect (ptr, source));
          reader_ptr.store (ptr);
          // The holder is destroyed here and ends whatever protection is left.
        });
  wait_until_arrived ();
  source.store (b);
  a->retire ();
  hp::clean_up ();
  CHECK (deleted.load ()
         == 0); // the announcement of the stalled reader holds a
  release ();
  reader.join ();
  CHECK (!reader_result.load ());  // validation saw the removal
  CHECK (reader_ptr.load () == b); // and delivered the new value
  hp::clean_up ();
  CHECK (deleted.load () == 1); // a is free now
  source.store (nullptr);
  b->retire ();
  hp::clean_up ();
  CHECK (deleted.load () == 2);
}

// A pass is stalled after it read the (empty) slots. A reader that still
// holds a stale pointer to the removed object then announces it: its
// validation must fail, so it never touches the object the pass is about to
// delete.
void
check_a_reader_after_the_scan_fails_validation ()
{
  std::atomic<int> deleted (0);
  node *const a = new node (deleted);
  node *const b = new node (deleted);
  std::atomic<node *> source (a);
  node *stale = source.load ();
  source.store (b);
  a->retire ();
  arm (engine::pass_scanned);
  std::thread scanner (
      [&]
        {
          t_stalls = true;
          hp::clean_up ();
        });
  wait_until_arrived ();
  CHECK (deleted.load () == 0); // not yet deleted: the pass waits
  hp::hazard_pointer holder = hp::make_hazard_pointer ();
  bool const ok = holder.try_protect (stale, source);
  CHECK (!ok);
  CHECK (stale == b);
  release ();
  scanner.join ();
  CHECK (deleted.load () == 1); // the pass deleted a, and nobody used it
  holder.reset_protection ();
  source.store (nullptr);
  b->retire ();
  hp::clean_up ();
  CHECK (deleted.load () == 2);
}

// A pass is stalled after its fence; the slot it will read was released and
// taken again by another thread in the meantime. The new owner announces the
// still-linked object: the pass must read that announcement.
void
check_a_recycled_slot_is_read_by_the_pass ()
{
  std::atomic<int> deleted (0);
  node *const a = new node (deleted);
  std::atomic<node *> source (a);
  // A holder takes a slot and gives it back at once; the next make gets it.
  {
    hp::hazard_pointer first = hp::make_hazard_pointer ();
  }
  node *const spare = new node (deleted);
  spare->retire (); // something for the pass to take
  arm (engine::pass_fenced);
  std::thread scanner (
      [&]
        {
          t_stalls = true;
          hp::clean_up ();
        });
  wait_until_arrived ();
  // The pass fenced and has not read the slots. Another thread announces a,
  // which is still linked.
  std::thread announcer (
      [&]
        {
          hp::hazard_pointer reused = hp::make_hazard_pointer ();
          node *ptr = source.load ();
          CHECK (reused.try_protect (ptr, source));
          // Wait for the pass to finish while holding a.
          release ();
          scanner.join ();
          CHECK (deleted.load () == 1); // spare only
          CHECK (a->alive_marker == 0x600D);
        });
  announcer.join ();
  source.store (nullptr);
  a->retire ();
  hp::clean_up ();
  CHECK (deleted.load () == 2);
}

// clean_up () waits for a pass that is running: when it returns, every
// unprotected object retired before the call is reclaimed, also the ones the
// running pass has taken out of the lists.
void
check_clean_up_waits_for_a_running_pass ()
{
  std::atomic<int> deleted (0);
  node *const a = new node (deleted);
  a->retire ();
  arm (engine::pass_scanned);
  std::thread scanner (
      [&]
        {
          t_stalls = true;
          hp::clean_up (); // takes a, reads the slots, stalls
        });
  wait_until_arrived ();
  std::atomic<bool> returned (false);
  std::thread waiter (
      [&]
        {
          hp::clean_up ();
          returned.store (true);
        });
  std::this_thread::sleep_for (std::chrono::milliseconds (300));
  CHECK (!returned.load ()); // it waits for the pass of the scanner
  CHECK (deleted.load () == 0);
  release ();
  scanner.join ();
  waiter.join ();
  CHECK (returned.load ());
  CHECK (deleted.load () == 1);
}

// A thread that used the engine gives its cache back when it exits: one
// eviction per thread.
void
check_thread_exit_evicts_the_cache ()
{
  std::atomic<int> evictions (0);
  static std::atomic<int> *counter = nullptr;
  counter = &evictions;
  engine::set_test_handler (
      [] (int id)
        {
          if (id == engine::cache_evicting)
            {
              counter->fetch_add (1);
            }
        });
  std::vector<std::thread> threads;
  for (int i = 0; i < 5; ++i)
    {
      threads.emplace_back (
          []
            {
              hp::hazard_pointer holder = hp::make_hazard_pointer ();
              (void)holder;
            });
    }
  for (std::thread &thread : threads)
    {
      thread.join ();
    }
  engine::set_test_handler (&handler);
  CHECK (evictions.load () == 5);
}

// The pass that matches node by node gives the same answers as the set.
void
check_linear_scan_agrees ()
{
  for (int linear = 0; linear < 2; ++linear)
    {
      engine::set_test_linear_scan (linear != 0);
      std::atomic<int> deleted (0);
      std::vector<node *> nodes;
      std::vector<hp::hazard_pointer> holders;
      for (int i = 0; i < 40; ++i)
        {
          nodes.push_back (new node (deleted));
          holders.push_back (hp::make_hazard_pointer ());
        }
      for (int i = 0; i < 40; ++i)
        {
          if (i % 2 == 0)
            {
              holders[static_cast<std::size_t> (i)].reset_protection (
                  nodes[static_cast<std::size_t> (i)]);
            }
        }
      for (int i = 0; i < 40; ++i)
        {
          nodes[static_cast<std::size_t> (i)]->retire ();
        }
      hp::clean_up ();
      CHECK (deleted.load () == 20);
      holders.clear ();
      hp::clean_up ();
      CHECK (deleted.load () == 40);
    }
  engine::set_test_linear_scan (false);
}

// A batch that cannot get its slots has no effect: every holder stays empty
// and the slots it had taken are back.
void
check_a_failed_batch_has_no_effect ()
{
  // Make sure the pool holds some slots and then ask for far more than there
  // are, so that the batch needs a new block.
  engine::statistics_t const start = engine::statistics ();
  std::size_t const wanted = start.records + 3000;
  bool failed_once = false;
  for (int fail_at = 0; fail_at < 6; ++fail_at)
    {
      std::vector<hp::hazard_pointer> holders (wanted);
      engine::statistics_t const before = engine::statistics ();
      bool thrown = false;
      g_fail_after.store (fail_at);
      try
        {
          hp::make_hazard_pointer_batch (
              lumex::core::span::view::span<hp::hazard_pointer> (
                  holders.data (), holders.size ()));
        }
      catch (std::bad_alloc const &)
        {
          thrown = true;
        }
      g_fail_after.store (-1);
      if (thrown)
        {
          failed_once = true;
          for (hp::hazard_pointer const &holder : holders)
            {
              CHECK (holder.empty ());
            }
          engine::statistics_t const after = engine::statistics ();
          // Slots taken before the failure came back (up to one cache of them
          // may sit in the thread's cache instead of the pool).
          std::size_t const free_before = before.pooled_records;
          std::size_t const free_after = after.pooled_records;
          CHECK (free_after + 8 >= free_before);
          CHECK (free_after
                 <= free_before + 8 + (after.records - before.records));
        }
      else
        {
          for (hp::hazard_pointer const &holder : holders)
            {
              CHECK (!holder.empty ());
            }
        }
      hp::clear_hazard_pointer_batch (
          lumex::core::span::view::span<hp::hazard_pointer> (holders.data (),
                                                             holders.size ()));
    }
  CHECK (failed_once);
}
} // namespace

int
main ()
{
  engine::set_test_handler (&handler);
  t_records = false;
  check_fence_comes_before_the_reload ();
  check_pass_sees_a_stalled_announcement ();
  check_a_reader_after_the_scan_fails_validation ();
  check_a_recycled_slot_is_read_by_the_pass ();
  check_clean_up_waits_for_a_running_pass ();
  check_thread_exit_evicts_the_cache ();
  check_linear_scan_agrees ();
  check_a_failed_batch_has_no_effect ();
  if (g_failures != 0)
    {
      std::fprintf (stderr, "%d check(s) failed\n", g_failures);
      return 1;
    }
  std::printf ("hazard_pointer_hooks: all checks passed\n");
  return 0;
}
