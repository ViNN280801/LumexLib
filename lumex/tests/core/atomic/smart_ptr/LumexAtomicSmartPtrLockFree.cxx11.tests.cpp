// Tests of what is particular to the lock-free engine
// (atomic_shared_ptr_lock_free / atomic_weak_ptr_lock_free): the facts it
// reports, the two policies for the destruction of a replaced value
// (reclaim::immediate and reclaim::deferred), the null word that stands for
// the empty value, readers against a poisoned payload, the bound on the
// boxes that wait, thread churn and deleters that call the atomic.
//
// The behavior suites (SharedPtr, WeakPtr, Lifetime, Concurrency, ...) run on
// the common names, which are the lock-free engine in this build, and on the
// other engines through the suite variants; these tests name the engine, so
// they exist only where it does (the no_lock_free variant compiles them to
// nothing) and always see the lock-free engine whatever the common name is.

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/atomic/LumexAtomic"
#include "lumex/core/hazard_pointer/LumexHazardPointer"
#include "lumex/tests/core/atomic/LumexAtomicTestSupport.hpp"

using namespace lumex_atomic_test;

#if LUMEX_ATOMIC_SMART_PTR_HAS_LOCK_FREE

namespace
{
namespace sp = lumex::core::atomic::smart_ptr;
namespace hp = lumex::core::hazard_pointer;

typedef sp::reclaim::immediate immediate_t;
typedef sp::reclaim::deferred deferred_t;

template <typename T, typename Reclaim>
using shared_engine = sp::atomic_shared_ptr_lock_free<T, Reclaim>;
template <typename T, typename Reclaim>
using weak_engine = sp::atomic_weak_ptr_lock_free<T, Reclaim>;

/// Tracker pointers made without a control block of their own.
std::shared_ptr<Tracker>
make_tracker (int value)
{
  return std::make_shared<Tracker> (value);
}

/// Forces the reclamation passes the deferred policy leaves to the domain.
void
quiesce ()
{
  hp::clean_up ();
}

/// A deleter that records on which thread and how often it ran.
struct Recorder
{
  std::atomic<int> runs;
  std::thread::id thread;
  Recorder () : runs (0) {}
};

struct RecordingDeleter
{
  Recorder *recorder;

  void
  operator() (int *p) const
  {
    recorder->thread = std::this_thread::get_id ();
    recorder->runs.fetch_add (1);
    delete p;
  }
};

std::shared_ptr<int>
recorded (Recorder &recorder, int value)
{
  RecordingDeleter const deleter = { &recorder };
  return std::shared_ptr<int> (new int (value), deleter);
}
} // namespace

TEST (LumexAtomicSmartPtrLockFreeTest,
      GivenTheEngine_WhenAskedWhatItIs_ThenItIsLockFreeAndSmall)
{
  static_assert (sp::atomic_shared_ptr_lock_free<int>::is_always_lock_free,
                 "always lock-free");
  static_assert (sp::atomic_weak_ptr_lock_free<int>::is_always_lock_free,
                 "always lock-free");
  static_assert ((shared_engine<int, deferred_t>::is_always_lock_free),
                 "any policy");
  static_assert (ATOMIC_POINTER_LOCK_FREE == 2, "pointer atomics");
  sp::atomic_shared_ptr_lock_free<int> shared;
  sp::atomic_weak_ptr_lock_free<int> weak;
  EXPECT_TRUE (shared.is_lock_free ());
  EXPECT_TRUE (weak.is_lock_free ());
  // The word and the 32-bit wait counter.
  EXPECT_LE (sizeof (shared), 2 * sizeof (void *));
  EXPECT_LE (sizeof (weak), 2 * sizeof (void *));
  // The common names resolve to the engine in a build that has it.
#if LUMEX_ATOMIC_SMART_PTR_COMMON_IS_LOCK_FREE
  static_assert (std::is_same<sp::atomic_shared_ptr<int>,
                              sp::atomic_shared_ptr_lock_free<int>>::value,
                 "common name");
#endif
}

TEST (
    LumexAtomicSmartPtrLockFreeTest,
    GivenImmediateReclaim_WhenAValueIsReplaced_ThenItIsDestroyedInsideTheCall)
{
  Recorder recorder;
  sp::atomic_shared_ptr_lock_free<int> atom (recorded (recorder, 1));
  atom.store (std::shared_ptr<int> (new int (2)));
  EXPECT_EQ (recorder.runs.load (), 1) << "store";
  EXPECT_EQ (recorder.thread, std::this_thread::get_id ());

  atom.store (recorded (recorder, 3));
  std::shared_ptr<int> previous = atom.exchange (std::shared_ptr<int> ());
  EXPECT_EQ (recorder.runs.load (), 1) << "the exchange hands the value back";
  EXPECT_EQ (previous.use_count (), 1)
      << "the box of the replaced value is already gone";
  previous.reset ();
  EXPECT_EQ (recorder.runs.load (), 2);

  atom.store (recorded (recorder, 4));
  std::shared_ptr<int> expected = atom.load ();
  EXPECT_EQ (expected.use_count (), 2);
  EXPECT_TRUE (atom.compare_exchange_strong (
      expected, std::shared_ptr<int> (new int (5))));
  EXPECT_EQ (expected.use_count (), 1) << "compare_exchange";
  expected.reset ();
  EXPECT_EQ (recorder.runs.load (), 3);
}

TEST (LumexAtomicSmartPtrLockFreeTest,
      GivenDeferredReclaim_WhenAValueIsReplaced_ThenItIsDestroyedByAPassLater)
{
  quiesce ();
  Recorder recorder;
  shared_engine<int, deferred_t> atom (recorded (recorder, 1));
  atom.store (std::shared_ptr<int> (new int (2)));
  EXPECT_EQ (recorder.runs.load (), 0) << "the replaced value outlives store";
  quiesce ();
  EXPECT_EQ (recorder.runs.load (), 1) << "a pass destroys it";
  EXPECT_EQ (recorder.thread, std::this_thread::get_id ());
}

TEST (
    LumexAtomicSmartPtrLockFreeTest,
    GivenTheNullWord_WhenEmptyAndNonEmptyValuesAreStored_ThenOnlyBoxesAreRetired)
{
  quiesce ();
  hp::engine::statistics_t const start = hp::engine::statistics ();
  sp::atomic_shared_ptr_lock_free<int> atom;
  std::shared_ptr<int> const owner = std::make_shared<int> (1);
  // empty -> value: the replaced word is null, nothing to dispose of.
  atom.store (owner);
  EXPECT_EQ (hp::engine::statistics ().retired - start.retired, 0u);
  // value -> empty: the box goes, and the empty value needs none.
  atom.store (std::shared_ptr<int> ());
  EXPECT_EQ (hp::engine::statistics ().retired - start.retired, 1u);
  // empty -> empty: nothing.
  atom.store (std::shared_ptr<int> ());
  EXPECT_EQ (hp::engine::statistics ().retired - start.retired, 1u);
  // An empty pointer that stores a non-null pointer is not the empty value.
  int storage = 0;
  std::shared_ptr<int> const aliasing_empty (std::shared_ptr<int> (),
                                             &storage);
  atom.store (aliasing_empty);
  EXPECT_EQ (atom.load ().get (), &storage);
  atom.store (std::shared_ptr<int> ());
  EXPECT_EQ (hp::engine::statistics ().retired - start.retired, 2u);
  EXPECT_EQ (atom.load ().get (), nullptr);
  // A pointer that owns a null pointer is not the empty value either.
  std::shared_ptr<int> const owns_null (static_cast<int *> (nullptr));
  atom.store (owns_null);
  EXPECT_EQ (atom.load ().use_count (), 3) << "owns_null, the box, the copy";
  std::shared_ptr<int> expected;
  EXPECT_FALSE (atom.compare_exchange_strong (expected, owner))
      << "the empty value is not what the cell holds";
  EXPECT_EQ (expected.use_count (), 3);
  std::shared_ptr<int> expected_null = owns_null;
  EXPECT_TRUE (atom.compare_exchange_strong (expected_null, owner));
}

TEST (LumexAtomicSmartPtrLockFreeTest,
      GivenTheWeakEngine_WhenEmptyAndExpiredValuesAreStored_ThenTheyStayApart)
{
  weak_engine<int, immediate_t> atom;
  std::weak_ptr<int> expired;
  {
    std::shared_ptr<int> const gone = std::make_shared<int> (1);
    expired = gone;
  }
  atom.store (expired);
  std::weak_ptr<int> expected;
  EXPECT_FALSE (atom.compare_exchange_strong (expected, std::weak_ptr<int> ()))
      << "an expired pointer is not the empty one";
  EXPECT_TRUE (same_owner (expected, expired));
  EXPECT_TRUE (atom.compare_exchange_strong (expected, std::weak_ptr<int> ()));
  EXPECT_TRUE (atom.load ().expired ());
  std::weak_ptr<int> none;
  EXPECT_TRUE (atom.compare_exchange_strong (none, expired));
}

namespace
{
/// Readers load and check the canary of every object while writers replace
/// the value with store, exchange and compare-exchange; every object that
/// was made is destroyed exactly once.
template <typename Engine>
void
run_poison_stress (int readers, int writers, int rounds, std::uint32_t seed,
                   long &broken)
{
  int const alive_before = Tracker::alive ().load ();
  Engine atom (make_tracker (0));
  std::atomic<bool> stop (false);
  std::atomic<long> bad (0);
  std::atomic<int> writers_left (writers);
  std::vector<std::thread> threads;
  for (int r = 0; r < readers; ++r)
    threads.emplace_back (
        [&]
          {
            while (!stop.load ())
              {
                std::shared_ptr<Tracker> const seen = atom.load ();
                if (seen && !seen->intact ())
                  bad.fetch_add (1);
              }
          });
  for (int w = 0; w < writers; ++w)
    threads.emplace_back (
        [&, w]
          {
            Random random (seed + static_cast<std::uint32_t> (w));
            for (int i = 0; i < rounds; ++i)
              {
                switch (random.next () % 3)
                  {
                  case 0:
                    atom.store (make_tracker (i));
                    break;
                  case 1:
                    {
                      std::shared_ptr<Tracker> const old
                          = atom.exchange (make_tracker (i));
                      if (old && !old->intact ())
                        bad.fetch_add (1);
                    }
                    break;
                  default:
                    {
                      std::shared_ptr<Tracker> expected = atom.load ();
                      atom.compare_exchange_strong (expected,
                                                    make_tracker (i));
                      if (expected && !expected->intact ())
                        bad.fetch_add (1);
                    }
                    break;
                  }
              }
            if (writers_left.fetch_sub (1) == 1)
              stop.store (true);
          });
  join_all (threads);
  atom.store (std::shared_ptr<Tracker> ());
  quiesce ();
  broken = bad.load ();
  EXPECT_EQ (Tracker::alive ().load (), alive_before)
      << "every object destroyed exactly once, none leaked";
}
} // namespace

TEST (
    LumexAtomicSmartPtrLockFreeTest,
    Stress_GivenReadersAgainstWriters_WhenImmediateReclaim_ThenNoReaderSeesAPoisonedObject)
{
  for (int threads : stress_thread_counts ())
    {
      long broken = 0;
      run_poison_stress<shared_engine<Tracker, immediate_t>> (
          threads, 2, stress_iterations (3000), stress_seed (), broken);
      EXPECT_EQ (broken, 0) << threads << " readers, seed " << stress_seed ();
    }
}

TEST (
    LumexAtomicSmartPtrLockFreeTest,
    Stress_GivenReadersAgainstWriters_WhenDeferredReclaim_ThenNoReaderSeesAPoisonedObject)
{
  for (int threads : stress_thread_counts ())
    {
      long broken = 0;
      run_poison_stress<shared_engine<Tracker, deferred_t>> (
          threads, 2, stress_iterations (3000), stress_seed () + 1, broken);
      EXPECT_EQ (broken, 0) << threads << " readers, seed " << stress_seed ();
    }
}

TEST (
    LumexAtomicSmartPtrLockFreeTest,
    Stress_GivenManyWritersAndOneReader_WhenReplacingConstantly_ThenEveryObjectDiesOnce)
{
  long broken = 0;
  run_poison_stress<shared_engine<Tracker, immediate_t>> (
      1, std::max (4, stress_thread_counts ().back ()),
      stress_iterations (1500), stress_seed () + 2, broken);
  EXPECT_EQ (broken, 0);
}

TEST (
    LumexAtomicSmartPtrLockFreeTest,
    Stress_GivenDeferredReclaim_WhenManyValuesAreReplaced_ThenTheBoxesThatWaitStayBounded)
{
  quiesce ();
  shared_engine<int, deferred_t> atom;
  std::size_t worst = 0;
  hp::engine::statistics_t const start = hp::engine::statistics ();
  for (int i = 0; i < stress_iterations (50000); ++i)
    {
      atom.store (std::shared_ptr<int> (new int (i)));
      hp::engine::statistics_t const now = hp::engine::statistics ();
      std::size_t const waiting
          = (now.retired - start.retired) - (now.reclaimed - start.reclaimed);
      if (waiting > worst)
        worst = waiting;
    }
  hp::engine::statistics_t const end = hp::engine::statistics ();
  // The documented bound of the hazard module: max (1000, 2 * R) - 1 + R + n
  // with R the slots ever created and n the retires of other threads during a
  // pass (none here).
  std::size_t const records = end.records;
  std::size_t const threshold = std::max<std::size_t> (1000, 2 * records);
  EXPECT_LE (worst, threshold - 1 + records);
  EXPECT_GT (worst, 0u) << "the deferred policy does keep boxes waiting";
  quiesce ();
}

TEST (
    LumexAtomicSmartPtrLockFreeTest,
    Stress_GivenThreadsThatLoadOnceAndExit_WhenWritersReplace_ThenNothingLeaks)
{
  int const alive_before = Tracker::alive ().load ();
  {
    shared_engine<Tracker, immediate_t> atom (make_tracker (0));
    std::atomic<bool> stop (false);
    std::thread writer (
        [&]
          {
            int i = 0;
            while (!stop.load ())
              atom.store (make_tracker (++i));
          });
    long bad = 0;
    for (int round = 0; round < stress_iterations (150); ++round)
      {
        std::vector<std::thread> threads;
        std::atomic<long> local_bad (0);
        for (int t = 0; t < 4; ++t)
          threads.emplace_back (
              [&]
                {
                  // A thread's first operation takes a hazard record, and its
                  // exit hands the record back, while the writer scans.
                  std::shared_ptr<Tracker> const seen = atom.load ();
                  if (seen && !seen->intact ())
                    local_bad.fetch_add (1);
                });
        join_all (threads);
        bad += local_bad.load ();
      }
    stop.store (true);
    writer.join ();
    EXPECT_EQ (bad, 0);
  }
  quiesce ();
  EXPECT_EQ (Tracker::alive ().load (), alive_before);
}

namespace
{
/// A deleter that calls every operation on the atomic that held the value.
template <typename Engine> struct Reentrant
{
  static Engine *&
  target ()
  {
    static Engine *value = nullptr;
    return value;
  }

  static std::atomic<int> &
  runs ()
  {
    static std::atomic<int> value (0);
    return value;
  }

  void
  operator() (int *p) const
  {
    Engine *const atom = target ();
    runs ().fetch_add (1);
    if (atom != nullptr)
      {
        std::shared_ptr<int> const seen = atom->load ();
        std::shared_ptr<int> expected = seen;
        atom->compare_exchange_strong (expected, seen);
        atom->exchange (seen);
        atom->store (seen);
        atom->notify_all ();
      }
    delete p;
  }
};

template <typename Engine>
void
run_reentrant ()
{
  typedef Reentrant<Engine> deleter_t;
  deleter_t::runs ().store (0);
  {
    Engine atom (std::shared_ptr<int> (new int (0), deleter_t ()));
    deleter_t::target () = &atom;
    for (int i = 1; i <= 50; ++i)
      {
        switch (i % 3)
          {
          case 0:
            atom.store (std::shared_ptr<int> (new int (i), deleter_t ()));
            break;
          case 1:
            atom.exchange (std::shared_ptr<int> (new int (i), deleter_t ()));
            break;
          default:
            {
              std::shared_ptr<int> expected = atom.load ();
              atom.compare_exchange_strong (
                  expected, std::shared_ptr<int> (new int (i), deleter_t ()));
            }
            break;
          }
      }
    deleter_t::target () = nullptr;
  }
  quiesce ();
  EXPECT_EQ (deleter_t::runs ().load (), 51) << "the first value and 50 more";
}
} // namespace

TEST (
    LumexAtomicSmartPtrLockFreeTest,
    GivenADeleterThatUsesTheAtomic_WhenValuesAreReplaced_ThenNothingDeadlocksOrLeaks)
{
  run_reentrant<shared_engine<int, immediate_t>> ();
  run_reentrant<shared_engine<int, deferred_t>> ();
}

#else // !LUMEX_ATOMIC_SMART_PTR_HAS_LOCK_FREE

TEST (LumexAtomicSmartPtrLockFreeTest,
      GivenABuildWithoutTheEngine_WhenAsked_ThenTheMacroSaysSo)
{
  EXPECT_EQ (LUMEX_ATOMIC_SMART_PTR_HAS_LOCK_FREE, 0);
  EXPECT_EQ (LUMEX_ATOMIC_SMART_PTR_COMMON_IS_LOCK_FREE, 0);
  EXPECT_FALSE (atomic_shared_ptr<int> ().is_lock_free ());
  EXPECT_FALSE (atomic_weak_ptr<int> ().is_lock_free ());
  static_assert (!atomic_shared_ptr<int>::is_always_lock_free,
                 "the common name is the lock-based engine");
  static_assert (std::is_same<atomic_shared_ptr<int>,
                              lumex::core::atomic::smart_ptr::
                                  atomic_shared_ptr_lock_based<int>>::value,
                 "the common name is the lock-based engine");
}

#endif // LUMEX_ATOMIC_SMART_PTR_HAS_LOCK_FREE
