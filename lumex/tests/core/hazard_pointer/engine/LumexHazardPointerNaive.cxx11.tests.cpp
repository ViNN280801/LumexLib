// A deliberately wrong hazard pointer, and the stress harness that is meant to
// find it. The naive implementation announces with a release store and
// re-reads with an acquire load, with no sequentially consistent fence
// between them: on x86 the store may sit in the store buffer while the load
// runs, a reclaimer then misses the announcement and deletes an object that a
// reader is about to use. The same harness runs against the real module,
// where it must find nothing.
//
// The failure of the naive version is a race, so it appears only
// probabilistically; the test reports a skip when this machine does not show
// it within the time budget. The deterministic check of the fence is in the
// test-hook fixture (consumer/hazard_pointer_hooks).

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/tests/core/hazard_pointer/LumexHazardPointerTestSupport.hpp"

namespace
{
namespace hp = lumex::core::hazard_pointer;

// ---------------------------------------------------------------------------
// The naive implementation: a fixed table of slots, release store, acquire
// load, no fence; retired objects wait in a vector under a mutex.
// ---------------------------------------------------------------------------

struct naive_domain
{
  static std::size_t const slot_count = 64;
  std::atomic<void *> slots[slot_count];
  std::mutex lock;
  bool taken[slot_count];
  std::vector<std::pair<void *, void (*) (void *)>> retired;

  naive_domain ()
  {
    for (std::size_t i = 0; i < slot_count; ++i)
      {
        slots[i].store (nullptr);
        taken[i] = false;
      }
  }

  static naive_domain &
  instance ()
  {
    static naive_domain domain;
    return domain;
  }

  std::size_t
  acquire ()
  {
    std::lock_guard<std::mutex> guard (lock);
    for (std::size_t i = 0; i < slot_count; ++i)
      {
        if (!taken[i])
          {
            taken[i] = true;
            return i;
          }
      }
    std::abort ();
  }

  void
  release (std::size_t index)
  {
    slots[index].store (nullptr, std::memory_order_release);
    std::lock_guard<std::mutex> guard (lock);
    taken[index] = false;
  }

  // Deletes everything that is waiting (after every reader has gone).
  void
  drain ()
  {
    std::vector<std::pair<void *, void (*) (void *)>> batch;
    {
      std::lock_guard<std::mutex> guard (lock);
      batch.swap (retired);
    }
    for (std::size_t i = 0; i < batch.size (); ++i)
      {
        batch[i].second (batch[i].first);
      }
  }

  void
  retire (void *object, void (*deleter) (void *))
  {
    std::vector<std::pair<void *, void (*) (void *)>> batch;
    {
      std::lock_guard<std::mutex> guard (lock);
      retired.push_back (std::make_pair (object, deleter));
      if (retired.size () < 1)
        {
          return;
        }
      batch.swap (retired);
    }
    // No fence here either: the scan reads the announcements as they are.
    std::vector<void *> announced;
    for (std::size_t i = 0; i < slot_count; ++i)
      {
        void *value = slots[i].load (std::memory_order_acquire);
        if (value != nullptr)
          {
            announced.push_back (value);
          }
      }
    std::vector<std::pair<void *, void (*) (void *)>> keep;
    for (std::size_t i = 0; i < batch.size (); ++i)
      {
        bool held = false;
        for (std::size_t j = 0; j < announced.size (); ++j)
          {
            held = held || announced[j] == batch[i].first;
          }
        if (held)
          {
            keep.push_back (batch[i]);
          }
        else
          {
            batch[i].second (batch[i].first);
          }
      }
    std::lock_guard<std::mutex> guard (lock);
    retired.insert (retired.end (), keep.begin (), keep.end ());
  }
};

struct naive_holder
{
  std::size_t index;

  naive_holder () : index (naive_domain::instance ().acquire ()) {}
  ~naive_holder () { naive_domain::instance ().release (index); }
  naive_holder (naive_holder const &) = delete;
  naive_holder &operator= (naive_holder const &) = delete;

  template <class T>
  T *
  protect (std::atomic<T *> const &source)
  {
    T *pointer = source.load (std::memory_order_relaxed);
    for (;;)
      {
        naive_domain::instance ().slots[index].store (
            pointer, std::memory_order_release);
        T *again = source.load (std::memory_order_acquire);
        if (again == pointer)
          {
            return pointer;
          }
        pointer = again;
      }
  }

  void
  reset ()
  {
    naive_domain::instance ().slots[index].store (nullptr,
                                                  std::memory_order_release);
  }
};

struct naive_node
{
  std::uint64_t first;
  std::uint64_t second;

  explicit naive_node (std::uint64_t v) : first (v), second (~v) {}

  ~naive_node ()
  {
    first = 0xDEADBEEFDEADBEEFull;
    second = 0xDEADBEEFDEADBEEFull;
  }

  bool
  intact () const
  {
    return second == ~first && first != 0xDEADBEEFDEADBEEFull;
  }

  void
  retire ()
  {
    naive_domain::instance ().retire (this, &naive_node::destroy);
  }

  static void
  destroy (void *object)
  {
    delete static_cast<naive_node *> (object);
  }
};

// ---------------------------------------------------------------------------
// The harness: the same code against either implementation.
// ---------------------------------------------------------------------------

struct naive_adapter
{
  typedef naive_node node_type;

  struct holder_type
  {
    naive_holder holder;

    node_type *
    protect (std::atomic<node_type *> const &source)
    {
      return holder.protect (source);
    }

    void
    reset_protection ()
    {
      holder.reset ();
    }
  };

  static void
  settle ()
  {
    naive_domain::instance ().drain ();
  }
};

struct real_adapter
{
  typedef lumex_hp_test::poisoned_node node_type;

  struct holder_type
  {
    hp::hazard_pointer holder;

    holder_type () : holder (hp::make_hazard_pointer ()) {}

    node_type *
    protect (std::atomic<node_type *> const &source)
    {
      return holder.protect (source);
    }

    void
    reset_protection ()
    {
      holder.reset_protection ();
    }
  };

  static void
  settle ()
  {
    hp::clean_up ();
  }
};

// Two writers replace the shared node and retire the old one; readers
// protect, check and release in a tight loop until a deleted node is seen or
// the budget is over. Returns the number of reads that saw a deleted node.
template <class Adapter>
long
hunt (std::chrono::milliseconds budget)
{
  typedef typename Adapter::node_type node_type;
  typedef typename Adapter::holder_type holder_type;
  std::atomic<node_type *> shared (new node_type (1));
  std::atomic<bool> stop (false);
  std::atomic<long> violations (0);
  std::vector<std::thread> threads;
  for (unsigned w = 0; w < 2; ++w)
    {
      threads.emplace_back (
          [&, w]
            {
              std::uint64_t value = 2 + w * 1000000000ull;
              while (!stop.load (std::memory_order_relaxed))
                {
                  node_type *old = shared.exchange (new node_type (value++));
                  old->retire ();
                }
            });
    }
  for (unsigned r = 0; r < 4; ++r)
    {
      threads.emplace_back (
          [&]
            {
              holder_type holder;
              while (!stop.load (std::memory_order_relaxed))
                {
                  node_type *node = holder.protect (shared);
                  if (!node->intact ())
                    {
                      violations.fetch_add (1);
                      stop.store (true);
                    }
                  holder.reset_protection ();
                }
            });
    }
  std::chrono::steady_clock::time_point const end
      = std::chrono::steady_clock::now () + budget;
  while (std::chrono::steady_clock::now () < end && !stop.load ())
    {
      std::this_thread::sleep_for (std::chrono::milliseconds (5));
    }
  stop.store (true);
  for (std::thread &thread : threads)
    {
      thread.join ();
    }
  // The last shared node goes the same way as the others.
  shared.exchange (nullptr)->retire ();
  Adapter::settle ();
  return violations.load ();
}
} // namespace

TEST (
    LumexHazardPointerNaiveTest,
    GivenTheNaiveProtocolWithoutAFence_WhenTheStressHarnessRuns_ThenItMayFindAUseAfterFree)
{
#if LUMEX_HP_TEST_SANITIZED
  GTEST_SKIP () << "a sanitizer slows the race down; the harness is checked "
                   "in the other build";
#else
  long found = 0;
  for (int attempt = 0; attempt < 3 && found == 0; ++attempt)
    {
      found = hunt<naive_adapter> (std::chrono::milliseconds (1500));
    }
  if (found == 0)
    {
      GTEST_SKIP () << "the naive protocol did not fail on this machine "
                       "within the budget (the race is a matter of timing)";
    }
  SUCCEED () << "the harness found " << found
             << " use after free in the naive protocol";
#endif
}

TEST (LumexHazardPointerNaiveTest,
      GivenTheSameHarness_WhenItRunsAgainstTheModule_ThenItFindsNothing)
{
  EXPECT_EQ (hunt<real_adapter> (std::chrono::milliseconds (
                 LUMEX_HP_TEST_SANITIZED ? 1000 : 1500)),
             0)
      << "the module's protocol failed the harness that breaks the naive one";
  EXPECT_EQ (lumex_hp_test::poisoned_node::alive ().load (), 0)
      << "every node, the last shared one included, was deleted";
}
