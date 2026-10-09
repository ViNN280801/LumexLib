// Tests of the assignment operators, swap and the four reset forms of
// shared_ptr ([util.smartptr.shared.assign], [util.smartptr.shared.mod]):
// counts, the order in which the old and the new object are released, self
// assignment, the state of a moved-from pointer, and the guarantee that a
// failed reset leaves the pointer untouched. Each scenario also runs on
// std::shared_ptr and the traces are compared.

#include <atomic>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

#include "lumex/tests/core/smart_ptr/LumexSmartPtrTestSupport.hpp"

namespace
{
using namespace smart_ptr_test;

template <class F>
void
scenario_copy_assignment (Trace &t)
{
  lumex_test::ObjectLedger ledger (8);
  shared_of<F, Probe> a (new Probe (ledger, 1));
  shared_of<F, Probe> b (new Probe (ledger, 2));
  shared_of<F, Probe> c = a;
  t.note (a.use_count ());
  b = a;
  t.note (a.use_count ());
  t.note (static_cast<long> (ledger.alive ()));
  t.note (b->value);
  // Self assignment changes nothing.
  shared_of<F, Probe> &self = b;
  b = self;
  t.note (b.use_count ());
  t.note (static_cast<long> (ledger.alive ()));
  // Assigning an empty pointer releases.
  shared_of<F, Probe> empty;
  c = empty;
  t.note (a.use_count ());
  t.note_bool (c.get () == nullptr);
  a = empty;
  b = empty;
  t.note (static_cast<long> (ledger.alive ()));
  t.note_bool (ledger.balanced ());
}

template <class F>
void
scenario_move_assignment (Trace &t)
{
  lumex_test::ObjectLedger ledger (8);
  shared_of<F, Probe> a (new Probe (ledger, 1));
  shared_of<F, Probe> b (new Probe (ledger, 2));
  b = std::move (a);
  t.note (b.use_count ());
  t.note (b->value);
  t.note (a.use_count ());
  t.note_bool (a.get () == nullptr);
  t.note (static_cast<long> (ledger.alive ()));
  // Self move leaves a valid pointer (the specified result is that it is
  // either unchanged or empty; both families keep it unchanged here).
  shared_of<F, Probe> &self = b;
  b = std::move (self);
  t.note_bool (b.get () != nullptr);
  t.note (b.use_count ());
  // Moving into an empty one and back.
  shared_of<F, Probe> c;
  c = std::move (b);
  t.note (c.use_count ());
  t.note_bool (b.get () == nullptr);
  b = std::move (c);
  t.note (b.use_count ());
}

template <class F>
void
scenario_converting_assignment (Trace &t)
{
  std::atomic<int> live (0);
  {
    shared_of<F, Dog> dog (new Dog (&live));
    shared_of<F, Animal> animal (new Bird (&live));
    t.note (live.load ());
    animal = dog;
    t.note (live.load ());
    t.note (dog.use_count ());
    t.note (animal->legs ());
    shared_of<F, Animal> second (new Bird (&live));
    second = std::move (dog);
    t.note (dog.use_count ());
    t.note (live.load ());
    t.note (second.use_count ());
    shared_of<F, Animal const> constant;
    constant = second;
    t.note (second.use_count ());
    shared_of<F, void> erased;
    erased = std::move (second);
    t.note (erased.use_count ());
  }
  t.note (live.load ());
}

template <class F>
void
scenario_unique_assignment (Trace &t)
{
  lumex_test::ObjectLedger ledger (8);
  shared_of<F, Probe> s (new Probe (ledger, 1));
  std::unique_ptr<Probe> u (new Probe (ledger, 2));
  s = std::move (u);
  t.note_bool (u == nullptr);
  t.note (s->value);
  t.note (static_cast<long> (ledger.alive ()));
}

template <class F>
void
scenario_swap (Trace &t)
{
  lumex_test::ObjectLedger ledger (8);
  shared_of<F, Probe> a (new Probe (ledger, 1));
  shared_of<F, Probe> a2 = a;
  shared_of<F, Probe> b (new Probe (ledger, 2));
  Probe *const pa = a.get ();
  Probe *const pb = b.get ();
  a.swap (b);
  t.note_bool (a.get () == pb);
  t.note_bool (b.get () == pa);
  t.note (a.use_count ());
  t.note (b.use_count ());
  swap (a, b);
  t.note_bool (a.get () == pa);
  t.note (a.use_count ());
  shared_of<F, Probe> empty;
  a.swap (empty);
  t.note_bool (a.get () == nullptr);
  t.note (empty.use_count ());
  empty.swap (a);
  t.note (a.use_count ());
  a.swap (a);
  t.note (a.use_count ());
  t.note (static_cast<long> (ledger.alive ()));
  // An alias swaps both parts.
  int x = 3;
  shared_of<F, int> alias (a, &x);
  shared_of<F, int> other (new int (9));
  alias.swap (other);
  t.note (*alias);
  t.note (a.use_count ());
  t.note_bool (other.get () == &x);
}

template <class F>
void
scenario_reset (Trace &t)
{
  lumex_test::ObjectLedger ledger (16);
  std::atomic<int> calls (0);
  AllocStats stats;
  shared_of<F, Probe> p (new Probe (ledger, 1));
  p.reset ();
  t.note (p.use_count ());
  t.note (static_cast<long> (ledger.alive ()));
  p.reset (new Probe (ledger, 2));
  t.note (p->value);
  t.note (p.use_count ());
  // The old object is released after the new one took over.
  p.reset (new Probe (ledger, 3));
  t.note (p->value);
  t.note (static_cast<long> (ledger.alive ()));
  p.reset (new Probe (ledger, 4), CountingDeleter<Probe> (&calls));
  t.note (calls.load ());
  p.reset (new Probe (ledger, 5), CountingDeleter<Probe> (&calls),
           CountingAllocator<Probe> (&stats));
  t.note (calls.load ());
  t.note (stats.allocations.load ());
  p.reset ();
  t.note (calls.load ());
  t.note (stats.deallocations.load ());
  t.note (static_cast<long> (ledger.alive ()));
  // reset to a derived pointer
  std::atomic<int> live (0);
  shared_of<F, Animal> a;
  a.reset (new Dog (&live));
  t.note (a->legs ());
  a.reset (new Bird (&live));
  t.note (a->legs ());
  t.note (live.load ());
  a.reset ();
  t.note (live.load ());
}

template <class F>
void
scenario_reset_shared_owner (Trace &t)
{
  // reset () of one of several owners leaves the others.
  lumex_test::ObjectLedger ledger (4);
  shared_of<F, Probe> a (new Probe (ledger, 1));
  shared_of<F, Probe> b = a;
  shared_of<F, Probe> c = a;
  a.reset ();
  t.note (b.use_count ());
  b.reset (new Probe (ledger, 2));
  t.note (c.use_count ());
  t.note (static_cast<long> (ledger.alive ()));
  c.reset ();
  t.note (static_cast<long> (ledger.alive ()));
}

template <class F>
void
scenario_reset_failure (Trace &t)
{
  lumex_test::ObjectLedger ledger (8);
  std::atomic<int> calls (0);
  AllocStats stats;
  shared_of<F, Probe> p (new Probe (ledger, 1));
  stats.fail_at.store (1);
  bool threw = false;
  try
    {
      p.reset (new Probe (ledger, 2), CountingDeleter<Probe> (&calls),
               CountingAllocator<Probe> (&stats));
    }
  catch (std::bad_alloc const &)
    {
      threw = true;
    }
  t.note_bool (threw);
  // The pointer is as before; the new object was deleted by the deleter.
  t.note (p->value);
  t.note (p.use_count ());
  t.note (calls.load ());
  t.note (static_cast<long> (ledger.alive ()));
}

TEST (LumexSharedPtrModifiersTest,
      GivenCopyAssignment_WhenObserved_ThenCountsMatchStd)
{
  EXPECT_TRACES_EQUAL (scenario_copy_assignment);
}

TEST (LumexSharedPtrModifiersTest,
      GivenMoveAssignment_WhenObserved_ThenSourceIsEmptyLikeStd)
{
  EXPECT_TRACES_EQUAL (scenario_move_assignment);
}

TEST (LumexSharedPtrModifiersTest,
      GivenConvertingAssignment_WhenObserved_ThenItMatchesStd)
{
  EXPECT_TRACES_EQUAL (scenario_converting_assignment);
}

TEST (LumexSharedPtrModifiersTest,
      GivenAssignmentFromUniquePtr_WhenObserved_ThenItMatchesStd)
{
  EXPECT_TRACES_EQUAL (scenario_unique_assignment);
}

TEST (LumexSharedPtrModifiersTest, GivenSwap_WhenObserved_ThenItMatchesStd)
{
  EXPECT_TRACES_EQUAL (scenario_swap);
}

TEST (LumexSharedPtrModifiersTest,
      GivenTheFourResetForms_WhenObserved_ThenTheyMatchStd)
{
  EXPECT_TRACES_EQUAL (scenario_reset);
}

TEST (LumexSharedPtrModifiersTest,
      GivenSeveralOwners_WhenOneResets_ThenTheOthersKeepTheObject)
{
  EXPECT_TRACES_EQUAL (scenario_reset_shared_owner);
}

TEST (LumexSharedPtrModifiersTest,
      GivenAFailingReset_WhenAllocationThrows_ThenThePointerIsUntouched)
{
  EXPECT_TRACES_EQUAL (scenario_reset_failure);
}

TEST (LumexSharedPtrModifiersTest,
      GivenSelfAssignmentOfTheLastOwner_WhenAssigning_ThenTheObjectSurvives)
{
  lumex_test::ObjectLedger ledger (2);
  sp::shared_ptr<Probe> p (new Probe (ledger, 7));
  sp::shared_ptr<Probe> &alias = p;
  p = alias;
  EXPECT_EQ (p.use_count (), 1);
  EXPECT_TRUE (p->intact ());
  EXPECT_EQ (ledger.alive (), 1u);
  p = std::move (alias);
  EXPECT_EQ (p.use_count (), 1);
  EXPECT_TRUE (p->intact ());
}

TEST (
    LumexSharedPtrModifiersTest,
    GivenADeleterThatInspectsTheOwner_WhenTheOwnerResets_ThenItIsAlreadyEmpty)
{
  struct Reentrant
  {
    sp::shared_ptr<int> *target;
    void
    operator() (int *p) const
    {
      // The control block is finishing; resetting the owner that is being
      // destroyed must not touch it again (the pointer is already empty).
      EXPECT_EQ (target->use_count (), 0);
      delete p;
    }
  };
  sp::shared_ptr<int> holder;
  holder = sp::shared_ptr<int> (new int (5), Reentrant{ &holder });
  holder.reset ();
  EXPECT_EQ (holder.use_count (), 0);
}

TEST (LumexSharedPtrModifiersTest,
      GivenTheModifiers_WhenCheckingTraits_ThenTheyAreNoexceptWhereSpecified)
{
  typedef sp::shared_ptr<int> ptr;
  static_assert (noexcept (std::declval<ptr &> ().reset ()), "");
  static_assert (
      noexcept (std::declval<ptr &> ().swap (std::declval<ptr &> ())), "");
  static_assert (
      noexcept (swap (std::declval<ptr &> (), std::declval<ptr &> ())), "");
  static_assert (
      !noexcept (std::declval<ptr &> ().reset (static_cast<int *> (nullptr))),
      "reset (p) allocates");
  static_assert (
      std::is_same<decltype (std::declval<ptr &> () = std::declval<ptr &> ()),
                   ptr &>::value,
      "");
  SUCCEED ();
}
} // namespace
