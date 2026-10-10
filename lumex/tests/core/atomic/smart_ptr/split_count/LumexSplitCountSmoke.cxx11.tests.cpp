// Smoke test of the split-count engine: the four operations on both
// pointers, a holder round trip, the size, and the build without the engine
// (the variant no_split_count), where the class templates do not exist.

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <thread>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

#include "lumex/tests/core/atomic/smart_ptr/split_count/LumexSplitCountTestSupport.hpp"

#if defined(LUMEX_ATOMIC_SMART_PTR_DISABLE_SPLIT_COUNT)

static_assert (LUMEX_ATOMIC_SMART_PTR_HAS_SPLIT_COUNT == 0,
               "the switch removes the split-count engine");

TEST (LumexSplitCountSmokeTest,
      GivenTheDisableSwitch_WhenCompiled_ThenNoEngine)
{
  EXPECT_EQ (LUMEX_ATOMIC_SMART_PTR_HAS_SPLIT_COUNT, 0);
}

#elif LUMEX_ATOMIC_SMART_PTR_HAS_SPLIT_COUNT

namespace
{
using namespace split_test;

typedef asp::atomic_shared_ptr_lock_free_split_count<Obj> shared_atomic;
typedef asp::atomic_weak_ptr_lock_free_split_count<Obj> weak_atomic;

static_assert (sizeof (shared_atomic) == 32, "a 16-byte word and the epoch");
static_assert (sizeof (weak_atomic) == 32, "a 16-byte word and the epoch");
static_assert (alignof (shared_atomic) == 16, "the word is 16-byte aligned");
static_assert (shared_atomic::is_always_lock_free, "lock-free by contract");
static_assert (weak_atomic::is_always_lock_free, "lock-free by contract");
static_assert (
    std::is_same<shared_atomic::value_type, sp::shared_ptr<Obj>>::value,
    "the value type is the module's own shared_ptr");
static_assert (std::is_same<weak_atomic::value_type, sp::weak_ptr<Obj>>::value,
               "the value type is the module's own weak_ptr");
static_assert (std::is_nothrow_default_constructible<shared_atomic>::value,
               "the default constructor is noexcept");
static_assert (
    std::is_nothrow_constructible<shared_atomic, std::nullptr_t>::value,
    "construction from nullptr is noexcept (LWG 3661)");

TEST (LumexSplitCountSmokeTest,
      GivenAValue_WhenLoaded_ThenTheSameObjectIsReturned)
{
  sp::shared_ptr<Obj> p = sp::make_shared<Obj> (7);
  shared_atomic a (p);
  EXPECT_TRUE (a.is_lock_free ());
  EXPECT_EQ (p.use_count (), 2);
  sp::shared_ptr<Obj> q = a.load ();
  EXPECT_EQ (q, p);
  EXPECT_EQ (p.use_count (), 3);
  q.reset ();
  EXPECT_EQ (p.use_count (), 2);
}

TEST (LumexSplitCountSmokeTest, GivenAValue_WhenStored_ThenTheOldOneIsReleased)
{
  long const before = alive_objects ().load ();
  sp::shared_ptr<Obj> first = sp::make_shared<Obj> (1);
  {
    shared_atomic a (first);
    sp::shared_ptr<Obj> second = sp::make_shared<Obj> (2);
    a.store (second);
    EXPECT_EQ (first.use_count (), 1);
    EXPECT_EQ (second.use_count (), 2);
    EXPECT_EQ (a.load ()->v, 2);
  }
  first.reset ();
  EXPECT_EQ (alive_objects ().load (), before);
}

TEST (LumexSplitCountSmokeTest,
      GivenAValue_WhenExchanged_ThenTheOldOneIsReturned)
{
  sp::shared_ptr<Obj> first = sp::make_shared<Obj> (1);
  sp::shared_ptr<Obj> second = sp::make_shared<Obj> (2);
  shared_atomic a (first);
  sp::shared_ptr<Obj> old = a.exchange (second);
  EXPECT_EQ (old, first);
  EXPECT_EQ (first.use_count (), 2);
  EXPECT_EQ (second.use_count (), 2);
  EXPECT_EQ (a.load (), second);
}

TEST (LumexSplitCountSmokeTest,
      GivenAMatchingExpected_WhenCompareExchange_ThenItSwaps)
{
  sp::shared_ptr<Obj> first = sp::make_shared<Obj> (1);
  sp::shared_ptr<Obj> second = sp::make_shared<Obj> (2);
  shared_atomic a (first);
  sp::shared_ptr<Obj> expected = first;
  EXPECT_TRUE (a.compare_exchange_strong (expected, second));
  EXPECT_EQ (a.load (), second);
  EXPECT_EQ (first.use_count (), 2) << "first, expected";
}

TEST (LumexSplitCountSmokeTest,
      GivenAnotherExpected_WhenCompareExchange_ThenItLoads)
{
  sp::shared_ptr<Obj> first = sp::make_shared<Obj> (1);
  sp::shared_ptr<Obj> second = sp::make_shared<Obj> (2);
  sp::shared_ptr<Obj> third = sp::make_shared<Obj> (3);
  shared_atomic a (first);
  sp::shared_ptr<Obj> expected = second;
  EXPECT_FALSE (a.compare_exchange_strong (expected, third));
  EXPECT_EQ (expected, first) << "expected receives the current value";
  EXPECT_EQ (a.load (), first);
}

TEST (LumexSplitCountSmokeTest,
      GivenAFarAlias_WhenRoundTripped_ThenTheHolderIsInvisible)
{
  sp::shared_ptr<Obj> owner = sp::make_shared<Obj> (5);
  int *const far = far_of (owner.get ());
  sp::shared_ptr<int> alias (owner, far);
  asp::atomic_shared_ptr_lock_free_split_count<int> a (alias);
  EXPECT_EQ (owner.use_count (), 3)
      << "owner, alias, and the holder's reference";
  sp::shared_ptr<int> loaded = a.load ();
  EXPECT_EQ (loaded.get (), far);
  EXPECT_EQ (owner.use_count (), 4)
      << "the loaded alias owns the owner, not the holder";
  sp::shared_ptr<int> expected = alias;
  EXPECT_TRUE (a.compare_exchange_strong (expected, sp::shared_ptr<int> ()));
  EXPECT_EQ (a.load (), sp::shared_ptr<int> ());
  loaded.reset ();
  alias.reset ();
  expected.reset ();
  EXPECT_EQ (owner.use_count (), 1) << "the holder is gone";
  sp::shared_ptr<int> again (owner, far);
  a.store (again);
  sp::shared_ptr<int> old = a.exchange (sp::shared_ptr<int> ());
  EXPECT_EQ (old.get (), far);
  EXPECT_EQ (owner.use_count (), 3) << "owner, again, old";
}

TEST (LumexSplitCountSmokeTest,
      GivenAWeakPointer_WhenLoaded_ThenItLocksTheObject)
{
  sp::shared_ptr<Obj> p = sp::make_shared<Obj> (9);
  weak_atomic a ((sp::weak_ptr<Obj> (p)));
  sp::weak_ptr<Obj> w = a.load ();
  EXPECT_EQ (w.lock (), p);
  EXPECT_EQ (p.use_count (), 1) << "weak references are not owners";
  a.store (sp::weak_ptr<Obj> ());
  EXPECT_TRUE (a.load ().expired ());
}

struct ledger_tag
{
};
struct torn_tag
{
};
struct gate_tag
{
};

template <typename Policy>
void
run_every_operation_once ()
{
  reset_ledger<Policy> ();
  sp::shared_ptr<Obj> first = sp::make_shared<Obj> (1);
  sp::shared_ptr<Obj> second = sp::make_shared<Obj> (2);
  sp::shared_ptr<Obj> owner = sp::make_shared<Obj> (3);
  sp::shared_ptr<int> far (owner, far_of (owner.get ()));
  {
    shared_cell<Obj, Policy> a (first);
    sp::shared_ptr<Obj> p = a.load ();
    a.store (second);
    sp::shared_ptr<Obj> q = a.exchange (first);
    sp::shared_ptr<Obj> expected = first;
    EXPECT_TRUE (a.compare_exchange_strong (expected, second));
    EXPECT_FALSE (a.compare_exchange_strong (expected, second));
    EXPECT_EQ (expected, second);
    shared_cell<int, Policy> h (far);
    sp::shared_ptr<int> r = h.load ();
    EXPECT_EQ (r.get (), far.get ());
    sp::shared_ptr<int> hexpected = far;
    EXPECT_TRUE (
        h.compare_exchange_strong (hexpected, sp::shared_ptr<int> ()));
  }
  EXPECT_EQ (first.use_count (), 1);
  EXPECT_EQ (second.use_count (), 1);
  check_balanced<Policy> ("every operation");
}

TEST (LumexSplitCountSmokeTest,
      GivenTheLedgerPolicy_WhenEveryOperationRuns_ThenItBalances)
{
  typedef ledger_policy_t<ledger_tag> policy;
  run_every_operation_once<policy> ();
  EXPECT_GT (policy::counters ().ticks.load (), 0u) << "loads pin the word";
  EXPECT_EQ (policy::counters ().transfers.load (), 0u)
      << "nothing raced, so no writer found a tick to deposit";
}

TEST (LumexSplitCountSmokeTest,
      GivenTheTornPolicy_WhenEveryOperationRuns_ThenItBalances)
{
  typedef torn_policy_t<torn_tag> policy;
  policy::set_period (2);
  run_every_operation_once<policy> ();
  policy::set_period (0);
}

TEST (LumexSplitCountSmokeTest,
      GivenAHeldLoad_WhenTheValueIsReplaced_ThenUseCountIsNeverBelow)
{
  typedef gate_policy_t<gate_tag> policy;
  reset_ledger<policy> ();
  policy::arrived ().store (0);
  sp::shared_ptr<Obj> p = sp::make_shared<Obj> (7);
  std::atomic<bool> gate (false);
  sp::shared_ptr<Obj> got;
  {
    shared_cell<Obj, policy> a (p);
    std::thread loader (
        [&]
          {
            this_thread_gate () = &gate;
            got = a.load ();
            this_thread_gate () = nullptr;
          });
    while (policy::arrived ().load () < 1)
      std::this_thread::yield ();
    // The load has ticked and not counted yet: the owners are p, the slot
    // and (linearized) the load.
    a.store (sp::make_shared<Obj> (8));
    EXPECT_EQ (p.use_count (), 2)
        << "p and the load; the slot's unit is dropped";
    gate.store (true);
    loader.join ();
    EXPECT_EQ (got, p);
    EXPECT_EQ (p.use_count (), 2);
  }
  got.reset ();
  EXPECT_EQ (p.use_count (), 1);
  check_balanced<policy> ("held load");
}

struct alias_tag
{
};
struct limit_tag
{
};

TEST (LumexSplitCountSmokeTest,
      GivenAnAliasBelowTheAnchor_WhenLoaded_ThenTheSamePointerComesBack)
{
  sp::shared_ptr<Obj> owner = sp::make_shared<Obj> (4);
  int *const below = reinterpret_cast<int *> (
      reinterpret_cast<std::uintptr_t> (owner.get ()) - 64u);
  sp::shared_ptr<int> alias (owner, below);
  asp::atomic_shared_ptr_lock_free_split_count<int> a (alias);
  sp::shared_ptr<int> loaded = a.load ();
  EXPECT_EQ (loaded.get (), below) << "a negative offset is sign-extended";
  EXPECT_EQ (owner.use_count (), 4) << "owner, alias, slot, loaded: no holder";
}

TEST (LumexSplitCountSmokeTest,
      GivenTwoAliasesOfOneOwner_WhenCompareExchangeWithTheOther_ThenItFails)
{
  sp::shared_ptr<std::pair<int, int>> owner (new std::pair<int, int> (1, 2));
  sp::shared_ptr<int> first (owner, &owner->first);
  sp::shared_ptr<int> second (owner, &owner->second);
  asp::atomic_shared_ptr_lock_free_split_count<int> a (first);
  sp::shared_ptr<int> expected = second;
  EXPECT_FALSE (a.compare_exchange_strong (expected, sp::shared_ptr<int> ()));
  EXPECT_EQ (expected.get (), &owner->first)
      << "the same owner with another stored pointer is not equivalent";
  EXPECT_EQ (a.load ().get (), &owner->first);
}

TEST (LumexSplitCountSmokeTest,
      GivenAHeldLoad_WhenStrongCompareExchange_ThenItSucceedsAtOnce)
{
  typedef gate_policy_t<alias_tag> policy;
  reset_ledger<policy> ();
  policy::arrived ().store (0);
  sp::shared_ptr<Obj> p = sp::make_shared<Obj> (1);
  sp::shared_ptr<Obj> q = sp::make_shared<Obj> (2);
  std::atomic<bool> gate (false);
  std::atomic<bool> swapped (false);
  shared_cell<Obj, policy> a (p);
  sp::shared_ptr<Obj> got;
  std::thread loader (
      [&]
        {
          this_thread_gate () = &gate;
          got = a.load ();
          this_thread_gate () = nullptr;
        });
  while (policy::arrived ().load () < 1)
    std::this_thread::yield ();
  std::thread comparer (
      [&]
        {
          sp::shared_ptr<Obj> expected = p;
          EXPECT_TRUE (a.compare_exchange_strong (expected, q));
          swapped.store (true);
        });
  for (int i = 0; i < 200 && !swapped.load (); ++i)
    std::this_thread::sleep_for (std::chrono::milliseconds (5));
  EXPECT_TRUE (swapped.load ())
      << "the tick of a held load is not part of the value compared";
  gate.store (true);
  loader.join ();
  comparer.join ();
  EXPECT_EQ (got, p);
  EXPECT_EQ (a.load (), q);
}

TEST (LumexSplitCountSmokeTest,
      GivenTheTickLimit_WhenMoreLoadsComeThanItAllows_ThenTheExtraOneBacksOff)
{
  typedef gate_policy_t<limit_tag, 2u> policy;
  reset_ledger<policy> ();
  policy::arrived ().store (0);
  sp::shared_ptr<Obj> p = sp::make_shared<Obj> (1);
  std::atomic<bool> gate (false);
  shared_cell<Obj, policy> a (p);
  sp::shared_ptr<Obj> got[3];
  std::thread loaders[3];
  for (int i = 0; i < 3; ++i)
    loaders[i] = std::thread (
        [&, i]
          {
            this_thread_gate () = &gate;
            got[i] = a.load ();
            this_thread_gate () = nullptr;
          });
  while (policy::arrived ().load () < 2)
    std::this_thread::yield ();
  std::this_thread::sleep_for (std::chrono::milliseconds (100));
  EXPECT_EQ (policy::arrived ().load (), 2)
      << "the third load backs off while two ticks are in the word";
  EXPECT_EQ (policy::counters ().peak.load (), 2u);
  gate.store (true);
  for (int i = 0; i < 3; ++i)
    loaders[i].join ();
  EXPECT_EQ (policy::arrived ().load (), 3);
  EXPECT_LE (policy::counters ().peak.load (), 2u);
  for (int i = 0; i < 3; ++i)
    EXPECT_EQ (got[i], p);
  got[0].reset ();
  got[1].reset ();
  got[2].reset ();
  check_balanced<policy> ("tick limit");
}

struct empty_alias_tag
{
};

TEST (LumexSplitCountSmokeTest,
      GivenTornGuesses_WhenEmptyAliasesAreLoaded_ThenTheCurrentOneIsReturned)
{
  typedef torn_policy_t<empty_alias_tag> policy;
  policy::set_period (1);
  int first = 1;
  int second = 2;
  sp::shared_ptr<int> const none;
  sp::shared_ptr<int> const e1 (none, &first);
  sp::shared_ptr<int> const e2 (none, &second);
  {
    shared_cell<int, policy> a (e1);
    EXPECT_EQ (a.load ().get (), &first);
    a.store (e2);
    EXPECT_EQ (a.load ().get (), &second)
        << "an empty word is confirmed by a compare-and-swap, not guessed";
    sp::shared_ptr<int> expected = e2;
    EXPECT_TRUE (a.compare_exchange_strong (expected, e1));
    EXPECT_EQ (a.exchange (sp::shared_ptr<int> ()).get (), &first);
    EXPECT_EQ (a.load ().get (), static_cast<int *> (nullptr));
  }
  policy::set_period (0);
}
} // namespace

#else

TEST (LumexSplitCountSmokeTest,
      GivenAPlatformWithoutTheEngine_WhenCompiled_ThenSkipped)
{
  GTEST_SKIP () << "the split-count engine needs x86-64 and lumex::smart_ptr";
}

#endif
