// Deterministic tests of the lock-free atomic smart pointer's safety against
// reclamation (cmake.consumer_atomic_lock_free_hooks). The hazard pointer
// engine and the cell are built into this executable with test hooks, so a
// thread can be stalled right after a reader announced a box, right after a
// load protected one, or right between a compare-exchange finding the value
// equivalent and swapping the pointer. The global allocator hands the block
// of a freed box out again at once (a LIFO free list per size), so an engine
// that frees a box a thread still names would let the next box take its
// address: the compare-exchange then succeeds although the value changed (a
// lost update), and a load copies a value out of another object. Exit code 0
// means every check passed; the last check ends the process through the
// terminate handler, which proves that an allocation failure inside a
// noexcept operation calls std::terminate.

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <memory>
#include <mutex>
#include <new>
#include <thread>
#include <vector>

#include <unistd.h>

void atomic_test_point (int id);

#define LUMEX_ATOMIC_SMART_PTR_TEST_HOOKS
#define LUMEX_ATOMIC_SMART_PTR_TEST_POINT(id)                                 \
  atomic_test_point (static_cast<int> (id))

#include "lumex/core/atomic/LumexAtomic"
#include "lumex/core/hazard_pointer/LumexHazardPointer"

namespace sp = lumex::core::atomic::smart_ptr;
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

// --- the allocator: a freed block is the next block of its size -----------
//
// Every block carries a 16-byte header with its rounded size and, while it
// is free, the link of its list (an intrusive list: the allocator must not
// allocate), so the unsized operator delete (C++11 has no sized one) finds
// its list too.

namespace
{
constexpr std::size_t kHeader = 16;
constexpr std::size_t kClasses = 16; // sizes 16, 32, ... 256
std::mutex g_alloc_lock;
void *g_free[kClasses];           // blocks, header included
std::atomic<int> g_fail_size (0); // a request of exactly this size fails

std::size_t
rounded (std::size_t size)
{
  return size == 0 ? 16 : (size + 15) / 16 * 16;
}
} // namespace

void *
operator new (std::size_t size)
{
  if (size == static_cast<std::size_t> (g_fail_size.load ()))
    {
      throw std::bad_alloc ();
    }
  std::size_t const round = rounded (size);
  std::size_t const klass = round / 16 - 1;
  void *block = nullptr;
  if (klass < kClasses)
    {
      std::lock_guard<std::mutex> guard (g_alloc_lock);
      block = g_free[klass];
      if (block != nullptr)
        {
          g_free[klass] = static_cast<void **> (block)[1];
        }
    }
  if (block == nullptr)
    {
      block = std::malloc (round + kHeader);
      if (block == nullptr)
        {
          throw std::bad_alloc ();
        }
      *static_cast<std::size_t *> (block) = round;
    }
  return static_cast<char *> (block) + kHeader;
}

void
operator delete (void *memory) noexcept
{
  if (memory == nullptr)
    {
      return;
    }
  void *const block = static_cast<char *> (memory) - kHeader;
  std::size_t const round = *static_cast<std::size_t *> (block);
  std::size_t const klass = round / 16 - 1;
  if (klass < kClasses)
    {
      std::lock_guard<std::mutex> guard (g_alloc_lock);
      static_cast<void **> (block)[1] = g_free[klass];
      g_free[klass] = block;
      return;
    }
  std::free (block);
}

void
operator delete (void *memory, std::size_t) noexcept
{
  operator delete (memory);
}

// --- stalling threads at test points ---------------------------------------

namespace
{
std::atomic<int> g_stall_point (0);
std::atomic<bool> g_arrived (false);
std::atomic<bool> g_release (false);
thread_local bool t_stalls = false;

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

void
stall_here (int id)
{
  if (t_stalls && id == g_stall_point.load ())
    {
      g_arrived.store (true);
      while (!g_release.load ())
        {
          std::this_thread::yield ();
        }
    }
}
} // namespace

void
atomic_test_point (int id)
{
  // The cell's points share the stall slot with the hazard engine's, offset
  // so that the ids do not meet.
  stall_here (1000 + id);
}

namespace
{
typedef sp::atomic_shared_ptr_lock_free<int> atom_t;

void
hazard_handler (int id)
{
  stall_here (id);
}

std::shared_ptr<int>
value (int v)
{
  return std::make_shared<int> (v);
}

// A compare-exchange found the value equivalent and is about to swap. Other
// threads replace the value three times, which with an engine that frees a
// box under a thread that names it would give the next box the address the
// stalled thread compares against. The swap must fail.
void
check_cas_does_not_succeed_on_a_recycled_box ()
{
  std::shared_ptr<int> const v1 = value (1);
  std::shared_ptr<int> const v3 = value (3);
  std::shared_ptr<int> const mine = value (99);
  atom_t atom (v1);
  std::atomic<bool> result (true);
  std::shared_ptr<int> observed;
  arm (1000 + sp::Detail::cas_checked);
  std::thread cas (
      [&]
        {
          t_stalls = true;
          std::shared_ptr<int> expected = v1;
          result.store (atom.compare_exchange_strong (expected, mine));
          observed = expected;
        });
  wait_until_arrived ();
  CHECK (v1.use_count () == 3); // v1, the box, the expected of the thread
  atom.store (value (2));
  CHECK (v1.use_count () == 3); // the box is named: retired, not destroyed
  atom.store (v3);              // the box of 2 goes at once
  atom.store (value (4));
  atom.store (v3); // would reuse the first box's block
  release ();
  cas.join ();
  CHECK (!result.load ());
  CHECK (observed == v3);
  CHECK (atom.load () == v3);
  hp::clean_up ();
  CHECK (v1.use_count () == 1); // the retired box is gone after the pass
}

// A load protected the box and stalls before it copies the value. The value
// is replaced meanwhile; the copy must still be the value the box holds.
void
check_a_load_copies_from_a_live_box ()
{
  std::shared_ptr<int> const v1 = value (11);
  atom_t atom (v1);
  std::shared_ptr<int> got;
  arm (1000 + sp::Detail::load_protected);
  std::thread loader (
      [&]
        {
          t_stalls = true;
          got = atom.load ();
        });
  wait_until_arrived ();
  atom.store (value (22));
  atom.store (value (33));      // the second store reuses blocks at once
  CHECK (v1.use_count () == 2); // v1 and the retired box
  release ();
  loader.join ();
  CHECK (got == v1);
  CHECK (*got == 11);
  got.reset ();
  hp::clean_up ();
  CHECK (v1.use_count () == 1);
}

// A reader announced the box and stalls before it validates. The scan of the
// replacing store finds the announcement and does not destroy the box; the
// validation then fails, and the load returns the new value.
void
check_a_stalled_announcement_keeps_the_box ()
{
  std::shared_ptr<int> const v1 = value (5);
  std::shared_ptr<int> const v2 = value (6);
  atom_t atom (v1);
  std::shared_ptr<int> got;
  arm (engine::reader_announced);
  std::thread loader (
      [&]
        {
          t_stalls = true;
          got = atom.load ();
        });
  wait_until_arrived ();
  atom.store (v2);
  CHECK (v1.use_count () == 2); // main and the retired box
  release ();
  loader.join ();
  CHECK (got == v2);
  hp::clean_up ();
  CHECK (v1.use_count () == 1);
}

// With nobody named, the replacing call destroys the box before it returns:
// the immediate policy's promise, with the recycling allocator in place.
void
check_immediate_policy_destroys_inside_the_call ()
{
  std::shared_ptr<int> const v1 = value (7);
  atom_t atom (v1);
  CHECK (v1.use_count () == 2);
  atom.store (value (8));
  CHECK (v1.use_count () == 1);
}

[[noreturn]] void
on_terminate ()
{
  std::printf ("atomic_lock_free_hooks: all checks passed; the allocation "
               "failure ended in std::terminate\n");
  std::fflush (stdout);
  _exit (g_failures != 0 ? 1 : 0);
}

// The box is 32 bytes: a failing allocation of that size inside a noexcept
// operation must end the program through std::terminate, not throw out.
void
check_allocation_failure_terminates ()
{
  atom_t atom;
  std::shared_ptr<int> const v = value (1);
  std::set_terminate (&on_terminate);
  g_fail_size.store (
      static_cast<int> (sizeof (sp::Detail::box_t<std::shared_ptr<int>>)));
  atom.store (v); // make_box: operator new throws, std::terminate runs
  g_fail_size.store (0);
  std::fprintf (stderr, "the allocation failure did not terminate\n");
  ++g_failures;
}
} // namespace

int
main ()
{
  engine::set_test_handler (&hazard_handler);
  check_cas_does_not_succeed_on_a_recycled_box ();
  check_a_load_copies_from_a_live_box ();
  check_a_stalled_announcement_keeps_the_box ();
  check_immediate_policy_destroys_inside_the_call ();
  if (g_failures != 0)
    {
      std::fprintf (stderr, "%d check(s) failed\n", g_failures);
      return 1;
    }
  check_allocation_failure_terminates ();
  return 2;
}
