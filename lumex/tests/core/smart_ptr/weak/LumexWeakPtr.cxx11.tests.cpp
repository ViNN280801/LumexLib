// Tests of weak_ptr ([util.smartptr.weak]): construction from shared and weak
// pointers, assignment, swap, reset, use_count, expired, lock, owner_before
// and owner_less, aliasing, and the lifetime of the object against the
// lifetime of the control block (the object dies with the last owner, the
// block with the last weak pointer, observed through a counting allocator).
// Each behavior the standard fixes is also run on std::weak_ptr.

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/tests/core/smart_ptr/LumexSmartPtrTestSupport.hpp"

namespace
{
using namespace smart_ptr_test;

template <class F>
void
scenario_construct_and_observe (Trace &t)
{
  lumex_test::ObjectLedger ledger (4);
  weak_of<F, Probe> empty;
  t.note (empty.use_count ());
  t.note_bool (empty.expired ());
  t.note_bool (empty.lock () == nullptr);
  shared_of<F, Probe> owner (new Probe (ledger, 3));
  weak_of<F, Probe> w (owner);
  t.note (w.use_count ());
  t.note_bool (w.expired ());
  t.note (owner.use_count ());
  {
    shared_of<F, Probe> locked = w.lock ();
    t.note_bool (locked == owner);
    t.note (owner.use_count ());
    t.note (w.use_count ());
  }
  weak_of<F, Probe> copy (w);
  weak_of<F, Probe> moved (std::move (copy));
  t.note (w.use_count ());
  t.note (moved.use_count ());
  t.note (copy.use_count ());
  t.note_bool (copy.expired ());
  owner.reset ();
  t.note (w.use_count ());
  t.note_bool (w.expired ());
  t.note_bool (w.lock () == nullptr);
  t.note (static_cast<long> (ledger.alive ()));
}

template <class F>
void
scenario_assignment (Trace &t)
{
  lumex_test::ObjectLedger ledger (4);
  shared_of<F, Probe> a (new Probe (ledger, 1));
  shared_of<F, Probe> b (new Probe (ledger, 2));
  weak_of<F, Probe> w;
  w = a;
  t.note (w.lock ()->value);
  w = b;
  t.note (w.lock ()->value);
  weak_of<F, Probe> other (a);
  w = other;
  t.note (w.lock ()->value);
  w = std::move (other);
  t.note (w.lock ()->value);
  t.note_bool (other.expired ());
  // Self assignment.
  weak_of<F, Probe> &self = w;
  w = self;
  t.note (w.use_count ());
  // Conversions.
  std::atomic<int> live (0);
  shared_of<F, Dog> dog (new Dog (&live));
  weak_of<F, Animal> base;
  base = dog;
  t.note (base.lock ()->legs ());
  weak_of<F, Dog> weak_dog (dog);
  weak_of<F, Animal> from_weak;
  from_weak = weak_dog;
  t.note (from_weak.lock ()->legs ());
  weak_of<F, Animal> from_moved;
  from_moved = std::move (weak_dog);
  t.note_bool (weak_dog.expired ());
  t.note (from_moved.use_count ());
  weak_of<F, Animal const> constant (from_moved);
  t.note (constant.use_count ());
  weak_of<F, void> erased (dog);
  t.note (erased.use_count ());
}

template <class F>
void
scenario_swap_and_reset (Trace &t)
{
  lumex_test::ObjectLedger ledger (4);
  shared_of<F, Probe> a (new Probe (ledger, 1));
  weak_of<F, Probe> wa (a);
  weak_of<F, Probe> wb;
  wa.swap (wb);
  t.note_bool (wa.expired ());
  t.note (wb.use_count ());
  swap (wa, wb);
  t.note (wa.use_count ());
  wa.reset ();
  t.note_bool (wa.expired ());
  t.note (a.use_count ());
  wb.reset ();
  t.note (static_cast<long> (ledger.alive ()));
}

template <class F>
void
scenario_aliasing (Trace &t)
{
  lumex_test::ObjectLedger ledger (4);
  shared_of<F, Probe> owner (new Probe (ledger, 8));
  shared_of<F, int> alias (owner, &owner->value);
  weak_of<F, int> w (alias);
  t.note_bool (w.lock ().get () == &owner->value);
  t.note (*w.lock ());
  owner.reset ();
  t.note (w.use_count ());
  alias.reset ();
  t.note_bool (w.expired ());
  t.note (static_cast<long> (ledger.alive ()));
  // A weak pointer to an alias of an empty owner is expired.
  int stored = 1;
  shared_of<F, int> alias_of_empty (shared_of<F, int> (), &stored);
  weak_of<F, int> weak_of_empty (alias_of_empty);
  t.note_bool (weak_of_empty.expired ());
  t.note_bool (weak_of_empty.lock () == nullptr);
}

template <class F>
void
scenario_lock_races_nothing_single_thread (Trace &t)
{
  // lock () on an expired pointer must not resurrect the object even when a
  // new owner was made elsewhere.
  lumex_test::ObjectLedger ledger (4);
  weak_of<F, Probe> w;
  {
    shared_of<F, Probe> a (new Probe (ledger, 1));
    w = a;
  }
  shared_of<F, Probe> b (new Probe (ledger, 2));
  t.note_bool (w.lock () == nullptr);
  weak_of<F, Probe> wb = b;
  t.note (wb.lock ()->value);
  t.note (static_cast<long> (ledger.alive ()));
}

template <class F>
void
scenario_owner_order (Trace &t)
{
  shared_of<F, int> a (new int (1));
  shared_of<F, int> b (new int (2));
  weak_of<F, int> wa = a;
  weak_of<F, int> wb = b;
  weak_of<F, int> wa2 = a;
  t.note_bool (wa.owner_before (wa));
  t.note_bool (wa.owner_before (wb) != wb.owner_before (wa));
  t.note_bool (wa.owner_before (wa2) || wa2.owner_before (wa));
  t.note_bool (wa.owner_before (a) || a.owner_before (wa));
  t.note_bool (wa.owner_before (b) == wa.owner_before (wb));
  weak_of<F, int> empty1;
  weak_of<F, int> empty2;
  t.note_bool (empty1.owner_before (empty2) || empty2.owner_before (empty1));
}

TEST (LumexWeakPtrTest, GivenConstructionAndLock_WhenObserved_ThenItMatchesStd)
{
  EXPECT_TRACES_EQUAL (scenario_construct_and_observe);
}

TEST (LumexWeakPtrTest, GivenAssignments_WhenObserved_ThenTheyMatchStd)
{
  EXPECT_TRACES_EQUAL (scenario_assignment);
}

TEST (LumexWeakPtrTest, GivenSwapAndReset_WhenObserved_ThenTheyMatchStd)
{
  EXPECT_TRACES_EQUAL (scenario_swap_and_reset);
}

TEST (LumexWeakPtrTest, GivenAliases_WhenObserved_ThenTheyMatchStd)
{
  EXPECT_TRACES_EQUAL (scenario_aliasing);
}

TEST (LumexWeakPtrTest,
      GivenAnExpiredPointer_WhenLocking_ThenNothingIsResurrected)
{
  EXPECT_TRACES_EQUAL (scenario_lock_races_nothing_single_thread);
}

TEST (LumexWeakPtrTest, GivenOwnerBefore_WhenOrdering_ThenItIsAnOrderOfOwners)
{
  EXPECT_TRACES_EQUAL (scenario_owner_order);
}

TEST (LumexWeakPtrTest,
      GivenAWeakPtr_WhenTheOwnersGo_ThenTheObjectDiesBeforeTheBlock)
{
  AllocStats stats;
  lumex_test::ObjectLedger ledger (2);
  sp::weak_ptr<Probe> w;
  {
    sp::shared_ptr<Probe> p = sp::allocate_shared<Probe> (
        CountingAllocator<Probe> (&stats), std::ref (ledger), 5);
    w = p;
    EXPECT_EQ (stats.live_blocks (), 1);
  }
  // The object is gone, the single allocation (block and object) stays for
  // the weak pointer.
  EXPECT_TRUE (ledger.balanced ()) << ledger.report ();
  EXPECT_EQ (stats.destroys.load (), 1);
  EXPECT_EQ (stats.live_blocks (), 1);
  EXPECT_TRUE (w.expired ());
  w.reset ();
  EXPECT_EQ (stats.live_blocks (), 0);
}

TEST (LumexWeakPtrTest,
      GivenManyWeakPointers_WhenTheLastGoes_ThenTheBlockIsFreedOnce)
{
  AllocStats stats;
  lumex_test::ObjectLedger ledger (2);
  std::vector<sp::weak_ptr<Probe>> weaks;
  {
    sp::shared_ptr<Probe> p = sp::allocate_shared<Probe> (
        CountingAllocator<Probe> (&stats), std::ref (ledger));
    for (int i = 0; i < 10; ++i)
      weaks.push_back (p);
    EXPECT_EQ (sp::detail::access::weak_count (p), 10);
  }
  EXPECT_EQ (stats.live_blocks (), 1);
  while (weaks.size () > 1)
    weaks.pop_back ();
  EXPECT_EQ (stats.live_blocks (), 1);
  weaks.clear ();
  EXPECT_EQ (stats.live_blocks (), 0);
  EXPECT_EQ (stats.deallocations.load (), 1);
}

TEST (LumexWeakPtrTest,
      GivenTheWeakCount_WhenOwnersAndWeaksChange_ThenItCountsWeaksOnly)
{
  sp::shared_ptr<int> p (new int (1));
  EXPECT_EQ (sp::detail::access::weak_count (p), 0);
  sp::weak_ptr<int> a (p);
  sp::weak_ptr<int> b (p);
  EXPECT_EQ (sp::detail::access::weak_count (p), 2);
  EXPECT_EQ (sp::detail::access::weak_count (a), 2);
  sp::shared_ptr<int> q = p;
  EXPECT_EQ (sp::detail::access::weak_count (p), 2) << "owners are not weak";
  b.reset ();
  EXPECT_EQ (sp::detail::access::weak_count (p), 1);
  sp::weak_ptr<int> empty;
  EXPECT_EQ (sp::detail::access::weak_count (empty), 0);
}

TEST (LumexWeakPtrTest, GivenOwnerLess_WhenKeyingContainers_ThenOwnersGroup)
{
  sp::shared_ptr<int> a (new int (1));
  sp::shared_ptr<int> b (new int (2));
  sp::shared_ptr<int> alias (a, b.get ());
  std::set<sp::shared_ptr<int>, sp::owner_less<sp::shared_ptr<int>>>
      shared_set;
  shared_set.insert (a);
  shared_set.insert (alias);
  shared_set.insert (b);
  EXPECT_EQ (shared_set.size (), 2u);

  std::set<sp::weak_ptr<int>, sp::owner_less<sp::weak_ptr<int>>> weak_set;
  weak_set.insert (a);
  weak_set.insert (alias);
  weak_set.insert (b);
  EXPECT_EQ (weak_set.size (), 2u);

  // The transparent form orders shared and weak pointers together.
  std::set<sp::weak_ptr<int>, sp::owner_less<>> transparent;
  transparent.insert (a);
  transparent.insert (b);
  EXPECT_EQ (transparent.count (a), 1u);
  EXPECT_EQ (transparent.count (alias), 1u);
  EXPECT_EQ (transparent.count (sp::shared_ptr<int> (new int (3))), 0u);

  // A weak key survives the expiry of its object and still finds itself.
  std::map<sp::weak_ptr<int>, int, sp::owner_less<sp::weak_ptr<int>>> map;
  map[a] = 1;
  map[b] = 2;
  sp::weak_ptr<int> key (a);
  shared_set.clear (); // the set holds an owner of a
  a.reset ();
  alias.reset ();
  EXPECT_TRUE (key.expired ());
  EXPECT_EQ (map.count (key), 1u);
  EXPECT_EQ (map[key], 1);
}

TEST (LumexWeakPtrTest, GivenTheTraits_WhenChecked_ThenTheSignaturesAreRight)
{
  typedef sp::weak_ptr<int> weak;
  static_assert (std::is_nothrow_default_constructible<weak>::value, "");
  static_assert (std::is_nothrow_copy_constructible<weak>::value, "");
  static_assert (std::is_nothrow_move_constructible<weak>::value, "");
  static_assert (std::is_nothrow_copy_assignable<weak>::value, "");
  static_assert (std::is_nothrow_move_assignable<weak>::value, "");
  static_assert (
      std::is_nothrow_constructible<weak, sp::shared_ptr<int> const &>::value,
      "");
  static_assert (noexcept (std::declval<weak const &> ().lock ()), "");
  static_assert (noexcept (std::declval<weak const &> ().expired ()), "");
  static_assert (noexcept (std::declval<weak const &> ().use_count ()), "");
  static_assert (std::is_same<decltype (std::declval<weak const &> ().lock ()),
                              sp::shared_ptr<int>>::value,
                 "");
  static_assert (
      std::is_same<decltype (std::declval<weak const &> ().use_count ()),
                   long>::value,
      "");
  static_assert (std::is_convertible<sp::shared_ptr<int>, weak>::value, "");
  static_assert (
      std::is_convertible<sp::weak_ptr<Dog>, sp::weak_ptr<Animal>>::value, "");
  static_assert (
      !std::is_convertible<sp::weak_ptr<Animal>, sp::weak_ptr<Dog>>::value,
      "");
  static_assert (!std::is_constructible<weak, std::weak_ptr<int>>::value,
                 "no conversion from the standard weak_ptr");
  static_assert (!std::is_constructible<weak, std::shared_ptr<int>>::value,
                 "");
  static_assert (std::is_same<sp::shared_ptr<int>::weak_type, weak>::value,
                 "");
  static_assert (sizeof (weak) == 2 * sizeof (void *), "");
  SUCCEED ();
}
} // namespace
