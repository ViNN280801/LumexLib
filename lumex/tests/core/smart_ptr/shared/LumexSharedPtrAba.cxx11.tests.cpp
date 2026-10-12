// ABA tests of the control block: the address of a block is reused at once by
// the next allocation of the same size (a pool that hands the most recently
// freed block back first), and a weak pointer must never promote the new
// object that lives at an old block's address. The invariant that makes it
// hold is that a weak pointer keeps the block allocated, so an address can be
// reused only when no pointer to the old block exists any more; the tests
// check that, deterministically (with the pool's hooks and a stall point) and
// at random with several threads, and prove with a negative control that the
// harness does see address reuse when the observers hold raw pointers
// instead.

#include <atomic>
#include <map>
#include <set>
#include <string>

#include <gtest/gtest.h>

#include "lumex/tests/core/smart_ptr/LumexSmartPtrTestScenarios.hpp"
#include "lumex/tests/support/LumexTestReplay.hpp"

namespace
{
using namespace smart_ptr_test;
using lumex_test::PoolEvent;
using lumex_test::ReuseAllocator;
using lumex_test::ReusePool;

typedef scenario::Generation Gen;

sp::shared_ptr<Gen>
make_generation (ReusePool &pool, std::uint64_t id)
{
  return sp::allocate_shared<Gen> (ReuseAllocator<Gen> (pool), id);
}

TEST (LumexSharedPtrAbaTest,
      GivenALastWeakPointer_WhenItIsReleased_ThenTheNextObjectReusesTheAddress)
{
  // The harness can produce address reuse at all: the precondition of every
  // other test in this file.
  ReusePool pool;
  Gen *first_address = nullptr;
  {
    sp::shared_ptr<Gen> first = make_generation (pool, 1);
    first_address = first.get ();
    sp::weak_ptr<Gen> weak = first;
    first.reset ();
    EXPECT_TRUE (weak.expired ());
  }
  EXPECT_EQ (pool.live (), 0);
  sp::shared_ptr<Gen> second = make_generation (pool, 2);
  EXPECT_EQ (second.get (), first_address)
      << "the pool must hand the freed block back first";
  EXPECT_EQ (pool.reuses (), 1);
  EXPECT_EQ (second->id, 2u);
}

TEST (
    LumexSharedPtrAbaTest,
    GivenAWeakPointerToAnExpiredObject_WhenNewObjectsAreMade_ThenTheBlockIsNeverReused)
{
  ReusePool pool;
  sp::shared_ptr<Gen> first = make_generation (pool, 1);
  Gen *const address = first.get ();
  sp::weak_ptr<Gen> stale = first;
  first.reset (); // the object is destroyed; the block stays for `stale`
  EXPECT_TRUE (stale.expired ());
  EXPECT_EQ (pool.live (), 1)
      << "the weak pointer keeps the single allocation";
  std::set<Gen *> addresses;
  std::vector<sp::shared_ptr<Gen>> fresh;
  for (std::uint64_t id = 2; id < 40; ++id)
    {
      fresh.push_back (make_generation (pool, id));
      EXPECT_NE (fresh.back ().get (), address)
          << "a live block's address was handed out again";
      addresses.insert (fresh.back ().get ());
      EXPECT_TRUE (stale.lock () == nullptr)
          << "a stale weak pointer promoted a new object";
      EXPECT_TRUE (stale.expired ());
      if (id % 3 == 0)
        fresh.erase (fresh.begin ());
    }
  long const reuses_before = pool.reuses ();
  stale.reset ();
  EXPECT_EQ (pool.live (), static_cast<long> (fresh.size ()));
  sp::shared_ptr<Gen> after = make_generation (pool, 100);
  EXPECT_EQ (after.get (), address)
      << "after the last weak pointer the block is reused at once";
  EXPECT_EQ (pool.reuses (), reuses_before + 1);
}

TEST (
    LumexSharedPtrAbaTest,
    GivenABlockBeingFreed_WhenTheReleaseIsStalled_ThenItIsNotReusedBeforeItIsReturned)
{
  ReusePool pool;
  lumex_test::StallPoint stall;
  std::atomic<void *> stalled_address (nullptr);
  pool.set_hook (
      [&] (PoolEvent event, void *address)
        {
          if (event == PoolEvent::before_release)
            {
              stalled_address.store (address);
              stall.park ();
            }
        });
  sp::shared_ptr<Gen> object = make_generation (pool, 1);
  sp::weak_ptr<Gen> weak = object;
  object.reset ();
  stall.arm (true);
  std::thread releasing (
      [&] { weak.reset (); }); // frees the block, parks inside the pool
  ASSERT_TRUE (stall.wait_until_parked (1));
  // The block is not in the pool yet: a new allocation cannot get it.
  ASSERT_NE (stalled_address.load (), nullptr);
  sp::shared_ptr<Gen> other = make_generation (pool, 2);
  EXPECT_EQ (pool.reuses (), 0)
      << "a block that is being freed was handed out";
  stall.release ();
  releasing.join ();
  pool.set_hook (nullptr);
  sp::shared_ptr<Gen> third = make_generation (pool, 3);
  EXPECT_EQ (pool.reuses (), 1) << "reused only after it was returned";
}

TEST (
    LumexSharedPtrAbaTest,
    GivenOwnerLessKeys_WhenObjectsComeAndGo_ThenTheOrderOfExpiredKeysIsStable)
{
  // A map keyed by weak pointers keeps the blocks of dead objects alive, so
  // their addresses cannot be reused and the order of the keys cannot change
  // under the map: new objects get new addresses, and the keys still find
  // themselves.
  ReusePool pool;
  std::map<sp::weak_ptr<Gen>, std::uint64_t, sp::owner_less<sp::weak_ptr<Gen>>>
      map;
  std::vector<sp::weak_ptr<Gen>> keys;
  for (std::uint64_t id = 1; id <= 60; ++id)
    {
      sp::shared_ptr<Gen> object = make_generation (pool, id);
      map[object] = id;
      keys.push_back (object);
      if (id % 2 == 0)
        object.reset ();
    }
  EXPECT_EQ (map.size (), 60u)
      << "every object has an owner of its own, expired or not";
  for (std::size_t i = 0; i < keys.size (); ++i)
    {
      std::map<sp::weak_ptr<Gen>, std::uint64_t,
               sp::owner_less<sp::weak_ptr<Gen>>>::iterator found
          = map.find (keys[i]);
      ASSERT_TRUE (found != map.end ());
      EXPECT_EQ (found->second, i + 1);
    }
  keys.clear ();
  map.clear ();
  EXPECT_EQ (pool.live (), 0);
}

TEST (
    LumexSharedPtrAbaTest,
    GivenGenerations_WhenManyThreadsReuseAddresses_ThenNoWeakPointerPromotesAStaleObject)
{
  lumex_test::TestWatchdog watchdog ("aba generations");
  for (int threads : lumex_test::thread_counts ())
    for (lumex_test::ScheduleKind kind : lumex_test::all_schedule_kinds ())
      {
        scenario::Params p;
        p.threads = std::max (2, threads);
        p.schedule = kind;
        p.iterations = std::max (
            8, lumex_test::scaled (1500)
                   / ((kind == lumex_test::ScheduleKind::tight
                       || kind == lumex_test::ScheduleKind::yielding)
                          ? 1
                          : 8));
        p.seed = lumex_test::derive_seed (lumex_test::base_seed (),
                                          static_cast<std::uint64_t> (threads),
                                          static_cast<std::uint64_t> (kind));
        lumex_test::Verdict verdict;
        scenario::address_reuse_generations (p, verdict);
        lumex_test::ReplayNote note ("address_reuse_generations", p.seed,
                                     p.threads);
        ASSERT_TRUE (verdict.ok ())
            << "threads=" << p.threads
            << " schedule=" << lumex_test::schedule_name (kind) << "\n"
            << verdict.text ();
      }
}

TEST (LumexSharedPtrAbaTest,
      GivenRawObservers_WhenAddressesAreReused_ThenTheHarnessSeesStaleReads)
{
  // The negative control: observers that do not keep the block (raw pointers)
  // read objects of other generations once the pool reuses an address, which
  // proves that the checkers above would notice a pointer that does not keep
  // its block alive. The raw reads are a race by design, so the control does
  // not run under ThreadSanitizer.
  if (lumex_test::instrumented_build ())
    GTEST_SKIP () << "the negative control reads freed memory on purpose";
  lumex_test::TestWatchdog watchdog ("aba negative control");
  scenario::Params p;
  p.threads = 4;
  p.schedule = lumex_test::ScheduleKind::tight;
  p.iterations = 4000;
  p.seed = lumex_test::base_seed ();
  int attempts_without_stale_read = 0;
  for (int attempt = 0; attempt < 5; ++attempt)
    {
      lumex_test::Verdict verdict;
      scenario::address_reuse_unsafe_control (p, verdict);
      if (verdict.ok ())
        return; // the control reported what it was built to report
      ++attempts_without_stale_read;
      p.seed += 1;
    }
  FAIL () << "the unsafe observers saw no stale read in "
          << attempts_without_stale_read
          << " attempts: the harness cannot see address reuse";
}
} // namespace
