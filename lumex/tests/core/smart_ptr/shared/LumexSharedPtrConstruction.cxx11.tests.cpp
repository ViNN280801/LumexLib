// Tests of the constructors of shared_ptr ([util.smartptr.shared.const]):
// default, nullptr, a raw pointer (with a deleter, with an allocator),
// nullptr with a deleter, the aliasing forms, copy, move, conversions, from a
// weak_ptr and from a unique_ptr, the exceptions of the allocating forms, and
// the constraints that keep wrong calls out of overload resolution. Every
// behavior that the standard fixes is also run on std::shared_ptr and the two
// traces are compared (the differential oracle).

#include <atomic>
#include <cstddef>
#include <memory>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

#include "lumex/tests/core/smart_ptr/LumexSmartPtrTestSupport.hpp"

namespace
{
using namespace smart_ptr_test;

// A base without a virtual destructor: shared_ptr<Plain> (new PlainDerived)
// must still run the derived destructor, because the deleter keeps the
// static type of the pointer it was given.
class Plain
{
public:
  explicit Plain (std::atomic<int> *destroyed) : destroyed_ (destroyed) {}
  ~Plain () {}

protected:
  std::atomic<int> *destroyed_;
};

class PlainDerived : public Plain
{
public:
  explicit PlainDerived (std::atomic<int> *destroyed) : Plain (destroyed) {}
  ~PlainDerived () { destroyed_->fetch_add (1); }
};

template <class F>
void
scenario_empty (Trace &t)
{
  shared_of<F, int> a;
  shared_of<F, int> b (nullptr);
  shared_of<F, void> c;
  for (shared_of<F, int> const *p : { &a, &b })
    {
      t.note (p->use_count ());
      t.note_bool (p->get () == nullptr);
      t.note_bool (static_cast<bool> (*p));
      t.note_bool (*p == nullptr);
    }
  t.note (c.use_count ());
  t.note_bool (c.get () == nullptr);
}

template <class F>
void
scenario_raw_pointer (Trace &t)
{
  lumex_test::ObjectLedger ledger (4);
  {
    shared_of<F, Probe> p (new Probe (ledger, 41));
    t.note (p.use_count ());
    t.note (p->value);
    t.note (static_cast<long> (ledger.alive ()));
    shared_of<F, Probe> q = p;
    t.note (p.use_count ());
    p.reset ();
    t.note (q.use_count ());
    t.note (static_cast<long> (ledger.alive ()));
  }
  t.note (static_cast<long> (ledger.alive ()));
  t.note_bool (ledger.balanced ());
}

template <class F>
void
scenario_raw_pointer_of_derived (Trace &t)
{
  std::atomic<int> destroyed (0);
  {
    shared_of<F, Plain> p (new PlainDerived (&destroyed));
    t.note (p.use_count ());
  }
  t.note (destroyed.load ());
  std::atomic<int> live (0);
  {
    shared_of<F, Animal> a (new Dog (&live));
    shared_of<F, Animal const> c = a;
    t.note (live.load ());
    t.note (a->legs ());
    t.note (c.use_count ());
  }
  t.note (live.load ());
}

template <class F>
void
scenario_deleter (Trace &t)
{
  std::atomic<int> calls (0);
  lumex_test::ObjectLedger ledger (4);
  {
    shared_of<F, Probe> p (new Probe (ledger),
                           CountingDeleter<Probe> (&calls));
    t.note (calls.load ());
    shared_of<F, Probe> q (p);
    p.reset ();
    t.note (calls.load ());
  }
  t.note (calls.load ());
  t.note (static_cast<long> (ledger.alive ()));
  // A function-pointer deleter and a lambda.
  int lambda_calls = 0;
  {
    shared_of<F, int> a (new int (5),
                         [&lambda_calls] (int *x)
                           {
                             ++lambda_calls;
                             delete x;
                           });
    t.note (*a);
  }
  t.note (lambda_calls);
}

template <class F>
void
scenario_deleter_receives_the_pointer (Trace &t)
{
  int calls = 0;
  void *last = nullptr;
  int object = 3;
  {
    shared_of<F, int> p (&object, RecordingDeleter (&calls, &last));
    t.note_bool (p.get () == &object);
  }
  t.note (calls);
  t.note_bool (last == &object);
}

template <class F>
void
scenario_nullptr_with_deleter (Trace &t)
{
  int calls = 0;
  void *last = &calls;
  {
    shared_of<F, int> p (nullptr, RecordingDeleter (&calls, &last));
    t.note (p.use_count ());
    t.note_bool (p.get () == nullptr);
    t.note_bool (static_cast<bool> (p));
    shared_of<F, int> q = p;
    t.note (p.use_count ());
  }
  // The standard calls d (p) with the null pointer when the last owner goes.
  t.note (calls);
  t.note_bool (last == nullptr);
}

template <class F>
void
scenario_allocator (Trace &t)
{
  AllocStats stats;
  std::atomic<int> calls (0);
  {
    shared_of<F, int> p (new int (8), CountingDeleter<int> (&calls),
                         CountingAllocator<int> (&stats));
    t.note (stats.allocations.load ());
    shared_of<F, int> q = p;
    shared_of<F, int> r;
    r = q;
    t.note (stats.allocations.load ());
    t.note (stats.deallocations.load ());
  }
  t.note (calls.load ());
  t.note (stats.deallocations.load ());
  {
    shared_of<F, int> n (nullptr, CountingDeleter<int> (&calls),
                         CountingAllocator<int> (&stats));
    t.note (n.use_count ());
  }
  t.note (stats.allocations.load ());
  t.note (stats.deallocations.load ());
}

template <class F>
void
scenario_allocation_failure (Trace &t)
{
  AllocStats stats;
  stats.fail_at.store (1);
  std::atomic<int> calls (0);
  lumex_test::ObjectLedger ledger (4);
  bool threw = false;
  try
    {
      shared_of<F, Probe> p (new Probe (ledger),
                             CountingDeleter<Probe> (&calls),
                             CountingAllocator<Probe> (&stats));
    }
  catch (std::bad_alloc const &)
    {
      threw = true;
    }
  // The deleter ran once on the pointer; nothing leaked.
  t.note_bool (threw);
  t.note (calls.load ());
  t.note (static_cast<long> (ledger.alive ()));
  t.note (stats.live_blocks ());
  // The null form: d (nullptr) runs.
  stats.allocations.store (0);
  stats.deallocations.store (0);
  stats.fail_at.store (1);
  int null_calls = 0;
  void *last = &null_calls;
  threw = false;
  try
    {
      shared_of<F, int> q (nullptr, RecordingDeleter (&null_calls, &last),
                           CountingAllocator<int> (&stats));
    }
  catch (std::bad_alloc const &)
    {
      threw = true;
    }
  t.note_bool (threw);
  t.note (null_calls);
}

template <class F>
void
scenario_aliasing (Trace &t)
{
  struct Pair
  {
    int first;
    int second;
  };
  lumex_test::ObjectLedger ledger (2);
  {
    shared_of<F, Pair> owner (new Pair{ 1, 2 });
    shared_of<F, int> alias (owner, &owner->second);
    t.note (owner.use_count ());
    t.note (*alias);
    t.note_bool (alias.get () == &owner->second);
    owner.reset ();
    t.note (alias.use_count ());
    t.note (*alias);
    shared_of<F, int> moved_from_alias (alias, alias.get ());
    t.note (alias.use_count ());
  }
  // The aliasing constructor with an empty owner: empty, but get () is the
  // given pointer.
  int stored = 4;
  shared_of<F, int> empty_owner;
  shared_of<F, int> alias_of_empty (empty_owner, &stored);
  t.note (alias_of_empty.use_count ());
  t.note_bool (alias_of_empty.get () == &stored);
  t.note_bool (static_cast<bool> (alias_of_empty));
  // The same with an owner of another type.
  shared_of<F, Probe> probe (new Probe (ledger, 9));
  shared_of<F, int> into_probe (probe, &probe->value);
  t.note (probe.use_count ());
  probe.reset ();
  t.note (static_cast<long> (ledger.alive ()));
  t.note (*into_probe);
  into_probe.reset ();
  t.note (static_cast<long> (ledger.alive ()));
}

template <class F>
void
scenario_copy_and_move (Trace &t)
{
  lumex_test::ObjectLedger ledger (2);
  shared_of<F, Probe> a (new Probe (ledger, 5));
  shared_of<F, Probe> b (a);
  t.note (a.use_count ());
  shared_of<F, Probe> c (std::move (a));
  t.note (b.use_count ());
  t.note (a.use_count ());
  t.note_bool (a.get () == nullptr);
  t.note_bool (c.get () == b.get ());
  shared_of<F, Probe const> d (std::move (c));
  t.note (c.use_count ());
  t.note (d.use_count ());
  t.note (static_cast<long> (ledger.alive ()));
}

template <class F>
void
scenario_conversion (Trace &t)
{
  std::atomic<int> live (0);
  {
    shared_of<F, Dog> dog (new Dog (&live));
    shared_of<F, Animal> animal (dog);
    t.note (dog.use_count ());
    t.note (animal->legs ());
    shared_of<F, Animal const> constant (std::move (dog));
    t.note (dog.use_count ());
    t.note (constant.use_count ());
    shared_of<F, void> erased (animal);
    t.note (animal.use_count ());
    t.note_bool (erased.get () == static_cast<void *> (animal.get ()));
  }
  t.note (live.load ());
  // A base at a non-zero offset: the stored pointer changes, the block is the
  // same.
  std::atomic<int> both_live (0);
  {
    shared_of<F, Both> both (new Both (&both_live));
    shared_of<F, Right> right = both;
    t.note_bool (static_cast<void *> (right.get ())
                 != static_cast<void *> (both.get ()));
    t.note_bool (static_cast<Right *> (both.get ()) == right.get ());
    t.note (both.use_count ());
    both.reset ();
    t.note (both_live.load ());
  }
  t.note (both_live.load ());
}

template <class F>
void
scenario_from_weak (Trace &t)
{
  lumex_test::ObjectLedger ledger (2);
  weak_of<F, Probe> w;
  {
    shared_of<F, Probe> owner (new Probe (ledger, 1));
    w = owner;
    shared_of<F, Probe> promoted (w);
    t.note (owner.use_count ());
    t.note_bool (promoted == owner);
    shared_of<F, Probe const> constant (w);
    t.note (owner.use_count ());
  }
  bool threw = false;
  try
    {
      shared_of<F, Probe> late (w);
    }
  catch (typename F::bad_weak const &)
    {
      threw = true;
    }
  t.note_bool (threw);
  weak_of<F, Probe> empty;
  threw = false;
  try
    {
      shared_of<F, Probe> none (empty);
    }
  catch (std::bad_weak_ptr const &)
    {
      // Both families throw something that is a std::bad_weak_ptr.
      threw = true;
    }
  t.note_bool (threw);
  t.note (static_cast<long> (ledger.alive ()));
}

template <class F>
void
scenario_from_unique (Trace &t)
{
  lumex_test::ObjectLedger ledger (4);
  {
    std::unique_ptr<Probe> u (new Probe (ledger, 6));
    shared_of<F, Probe> s (std::move (u));
    t.note_bool (u == nullptr);
    t.note (s.use_count ());
    t.note (s->value);
  }
  t.note (static_cast<long> (ledger.alive ()));
  std::unique_ptr<Probe> none;
  shared_of<F, Probe> from_none (std::move (none));
  t.note (from_none.use_count ());
  t.note_bool (from_none.get () == nullptr);
  // A derived unique_ptr into a base shared_ptr.
  std::atomic<int> live (0);
  {
    std::unique_ptr<Dog> dog (new Dog (&live));
    shared_of<F, Animal> animal (std::move (dog));
    t.note (animal->legs ());
    t.note (live.load ());
  }
  t.note (live.load ());
  // A deleter object is moved in, a deleter reference is referred to.
  std::atomic<int> calls (0);
  {
    std::unique_ptr<Probe, CountingDeleter<Probe>> u (
        new Probe (ledger), CountingDeleter<Probe> (&calls));
    shared_of<F, Probe> s (std::move (u));
    t.note (calls.load ());
  }
  t.note (calls.load ());
  CountingDeleter<Probe> external (&calls);
  {
    std::unique_ptr<Probe, CountingDeleter<Probe> &> u (new Probe (ledger),
                                                        external);
    shared_of<F, Probe> s (std::move (u));
    t.note_bool (u == nullptr);
  }
  t.note (calls.load ());
  t.note (static_cast<long> (ledger.alive ()));
}

template <class F>
void
scenario_move_only_deleter (Trace &t)
{
  std::atomic<int> calls (0);
  {
    shared_of<F, int> p (new int (1), MoveOnlyDeleter (&calls));
    t.note (p.use_count ());
  }
  t.note (calls.load ());
}

TEST (LumexSharedPtrConstructionTest,
      GivenTheDefaultAndTheNullptrForms_WhenObserved_ThenTheyAreEmptyLikeStd)
{
  EXPECT_TRACES_EQUAL (scenario_empty);
  sp::shared_ptr<int> a;
  EXPECT_EQ (a.use_count (), 0);
  EXPECT_EQ (a.get (), nullptr);
  EXPECT_FALSE (a);
}

TEST (LumexSharedPtrConstructionTest,
      GivenARawPointer_WhenOwnedAndReleased_ThenTheObjectIsDestroyedOnce)
{
  EXPECT_TRACES_EQUAL (scenario_raw_pointer);
  lumex_test::ObjectLedger ledger (2);
  {
    sp::shared_ptr<Probe> p (new Probe (ledger, 3));
    EXPECT_EQ (p.use_count (), 1);
    EXPECT_EQ (ledger.alive (), 1u);
  }
  EXPECT_TRUE (ledger.balanced ()) << ledger.report ();
}

TEST (
    LumexSharedPtrConstructionTest,
    GivenADerivedPointer_WhenOwnedByABaseSharedPtr_ThenTheDerivedDestructorRuns)
{
  EXPECT_TRACES_EQUAL (scenario_raw_pointer_of_derived);
  std::atomic<int> destroyed (0);
  {
    sp::shared_ptr<Plain> p (new PlainDerived (&destroyed));
  }
  EXPECT_EQ (destroyed.load (), 1);
}

TEST (LumexSharedPtrConstructionTest,
      GivenADeleter_WhenTheLastOwnerGoes_ThenItRunsOnce)
{
  EXPECT_TRACES_EQUAL (scenario_deleter);
  std::atomic<int> calls (0);
  {
    sp::shared_ptr<int> p (new int (1), CountingDeleter<int> (&calls));
    sp::shared_ptr<int> q = p;
    p.reset ();
    EXPECT_EQ (calls.load (), 0);
  }
  EXPECT_EQ (calls.load (), 1);
}

TEST (LumexSharedPtrConstructionTest,
      GivenADeleter_WhenItIsCalled_ThenItReceivesTheOwnedPointer)
{
  EXPECT_TRACES_EQUAL (scenario_deleter_receives_the_pointer);
}

TEST (LumexSharedPtrConstructionTest,
      GivenANullptrAndADeleter_WhenTheLastOwnerGoes_ThenTheDeleterGetsNullptr)
{
  EXPECT_TRACES_EQUAL (scenario_nullptr_with_deleter);
}

TEST (LumexSharedPtrConstructionTest,
      GivenAnAllocator_WhenOwning_ThenOneBlockIsAllocatedAndReturned)
{
  EXPECT_TRACES_EQUAL (scenario_allocator);
  AllocStats stats;
  {
    sp::shared_ptr<int> p (new int (8), CountingDeleter<int> (),
                           CountingAllocator<int> (&stats));
    sp::shared_ptr<int> q = p;
    EXPECT_EQ (stats.allocations.load (), 1);
    EXPECT_EQ (stats.deallocations.load (), 0);
  }
  EXPECT_EQ (stats.deallocations.load (), 1);
}

TEST (
    LumexSharedPtrConstructionTest,
    GivenAFailingAllocation_WhenConstructing_ThenTheDeleterRunsAndNothingLeaks)
{
  EXPECT_TRACES_EQUAL (scenario_allocation_failure);
  AllocStats stats;
  stats.fail_at.store (1);
  std::atomic<int> calls (0);
  lumex_test::ObjectLedger ledger (2);
  EXPECT_THROW (sp::shared_ptr<Probe> (new Probe (ledger),
                                       CountingDeleter<Probe> (&calls),
                                       CountingAllocator<Probe> (&stats)),
                std::bad_alloc);
  EXPECT_EQ (calls.load (), 1);
  EXPECT_TRUE (ledger.balanced ()) << ledger.report ();
}

TEST (LumexSharedPtrConstructionTest,
      GivenTheAliasingConstructors_WhenObserved_ThenTheyShareTheOwnerLikeStd)
{
  EXPECT_TRACES_EQUAL (scenario_aliasing);
}

TEST (LumexSharedPtrConstructionTest,
      GivenAnRvalueAlias_WhenConstructed_ThenTheOwnerIsMovedNotCopied)
{
  lumex_test::ObjectLedger ledger (2);
  sp::shared_ptr<Probe> owner (new Probe (ledger, 5));
  Probe *const raw = owner.get ();
  sp::shared_ptr<int> alias (std::move (owner), &raw->value);
  EXPECT_EQ (owner.use_count (), 0);
  EXPECT_EQ (owner.get (), nullptr);
  EXPECT_EQ (alias.use_count (), 1);
  EXPECT_EQ (*alias, 5);
  alias.reset ();
  EXPECT_TRUE (ledger.balanced ()) << ledger.report ();
}

TEST (LumexSharedPtrConstructionTest,
      GivenCopyAndMove_WhenObserved_ThenCountsAndMovedFromStateMatchStd)
{
  EXPECT_TRACES_EQUAL (scenario_copy_and_move);
}

TEST (LumexSharedPtrConstructionTest,
      GivenConversions_WhenObserved_ThenTheyMatchStd)
{
  EXPECT_TRACES_EQUAL (scenario_conversion);
}

TEST (
    LumexSharedPtrConstructionTest,
    GivenAWeakPtr_WhenPromotingByConstructor_ThenAnExpiredOneThrowsBadWeakPtr)
{
  EXPECT_TRACES_EQUAL (scenario_from_weak);
  sp::weak_ptr<int> expired;
  EXPECT_THROW (sp::shared_ptr<int> unused (expired), sp::bad_weak_ptr);
  // The module's exception is a std::bad_weak_ptr, so a handler for the
  // standard type catches it.
  EXPECT_THROW (sp::shared_ptr<int> unused (expired), std::bad_weak_ptr);
  EXPECT_THROW (sp::shared_ptr<int> unused (expired), std::exception);
}

TEST (LumexSharedPtrConstructionTest,
      GivenAUniquePtr_WhenConverting_ThenOwnershipMovesLikeStd)
{
  EXPECT_TRACES_EQUAL (scenario_from_unique);
}

TEST (LumexSharedPtrConstructionTest,
      GivenAMoveOnlyDeleter_WhenConstructing_ThenItIsMovedIn)
{
  EXPECT_TRACES_EQUAL (scenario_move_only_deleter);
}

TEST (LumexSharedPtrConstructionTest,
      GivenAUniquePtr_WhenTheAllocationFails_ThenTheUniquePtrKeepsTheObject)
{
  // The unique_ptr constructor has no allocator argument, so the failure is
  // reproduced by the module's rule: the pointer is released only after the
  // control block exists. A successful conversion leaves the source null
  // (checked above); here the source of a conversion into the same type with
  // a throwing deleter move shows that nothing was released.
  struct ThrowingMove
  {
    ThrowingMove (std::atomic<int> *counter, bool const *flag)
        : calls (counter), armed (flag)
    {
    }
    ThrowingMove (ThrowingMove const &other)
        : calls (other.calls), armed (other.armed)
    {
      if (*armed)
        throw std::runtime_error ("copy");
    }
    ThrowingMove (ThrowingMove &&other)
        : calls (other.calls), armed (other.armed)
    {
      if (*armed)
        throw std::runtime_error ("move");
    }
    void
    operator() (Probe *p) const
    {
      calls->fetch_add (1);
      delete p;
    }
    std::atomic<int> *calls;
    bool const *armed;
  };
  lumex_test::ObjectLedger ledger (2);
  std::atomic<int> calls (0);
  bool armed = false;
  {
    std::unique_ptr<Probe, ThrowingMove> u (new Probe (ledger),
                                            ThrowingMove (&calls, &armed));
    armed = true;
    EXPECT_THROW (sp::shared_ptr<Probe> converted (std::move (u)),
                  std::runtime_error);
    armed = false;
    EXPECT_NE (u.get (), nullptr) << "the failed conversion took the object";
    EXPECT_EQ (calls.load (), 0);
  }
  EXPECT_EQ (calls.load (), 1);
  EXPECT_TRUE (ledger.balanced ()) << ledger.report ();
}

// --- Constraints
// --------------------------------------------------------------

TEST (LumexSharedPtrConstructionTest,
      GivenTheConstraints_WhenCheckingTraits_ThenWrongConversionsAreRejected)
{
  // Implicit conversions that exist.
  static_assert (
      std::is_convertible<sp::shared_ptr<Dog>, sp::shared_ptr<Animal>>::value,
      "");
  static_assert (std::is_convertible<sp::shared_ptr<int>,
                                     sp::shared_ptr<int const>>::value,
                 "");
  static_assert (
      std::is_convertible<sp::shared_ptr<int>, sp::shared_ptr<void>>::value,
      "");
  static_assert (
      std::is_convertible<std::nullptr_t, sp::shared_ptr<int>>::value, "");
  // And those that must not.
  static_assert (
      !std::is_convertible<sp::shared_ptr<Animal>, sp::shared_ptr<Dog>>::value,
      "");
  static_assert (!std::is_convertible<sp::shared_ptr<int const>,
                                      sp::shared_ptr<int>>::value,
                 "");
  static_assert (
      !std::is_convertible<sp::shared_ptr<int>, sp::shared_ptr<double>>::value,
      "");
  static_assert (
      !std::is_convertible<sp::shared_ptr<void>, sp::shared_ptr<int>>::value,
      "");
  // The pointer constructor is explicit and checks convertibility.
  static_assert (!std::is_convertible<int *, sp::shared_ptr<int>>::value, "");
  static_assert (std::is_constructible<sp::shared_ptr<int>, int *>::value, "");
  static_assert (std::is_constructible<sp::shared_ptr<Animal>, Dog *>::value,
                 "");
  static_assert (!std::is_constructible<sp::shared_ptr<Dog>, Animal *>::value,
                 "");
  static_assert (!std::is_constructible<sp::shared_ptr<int>, double *>::value,
                 "");
  static_assert (
      !std::is_constructible<sp::shared_ptr<int>, int const *>::value, "");
  // A deleter must be callable with the pointer and be movable.
  static_assert (
      !std::is_constructible<sp::shared_ptr<int>, int *, int>::value,
      "an int is not a deleter");
  static_assert (std::is_constructible<sp::shared_ptr<int>, int *,
                                       CountingDeleter<int>>::value,
                 "");
  // A weak_ptr converts explicitly only.
  static_assert (
      !std::is_convertible<sp::weak_ptr<int>, sp::shared_ptr<int>>::value, "");
  static_assert (std::is_constructible<sp::shared_ptr<int>,
                                       sp::weak_ptr<int> const &>::value,
                 "");
  static_assert (!std::is_constructible<sp::shared_ptr<Dog>,
                                        sp::weak_ptr<Animal> const &>::value,
                 "");
  // unique_ptr converts by rvalue only.
  static_assert (std::is_constructible<sp::shared_ptr<int>,
                                       std::unique_ptr<int> &&>::value,
                 "");
  static_assert (!std::is_constructible<sp::shared_ptr<int>,
                                        std::unique_ptr<int> &>::value,
                 "");
  static_assert (!std::is_constructible<sp::shared_ptr<Dog>,
                                        std::unique_ptr<Animal> &&>::value,
                 "");
  // Not convertible from and to the standard class.
  static_assert (
      !std::is_convertible<std::shared_ptr<int>, sp::shared_ptr<int>>::value,
      "");
  static_assert (
      !std::is_constructible<sp::shared_ptr<int>, std::shared_ptr<int>>::value,
      "");
  static_assert (
      !std::is_convertible<sp::shared_ptr<int>, std::shared_ptr<int>>::value,
      "");
  static_assert (
      !std::is_constructible<std::shared_ptr<int>, sp::shared_ptr<int>>::value,
      "");
  SUCCEED ();
}

TEST (LumexSharedPtrConstructionTest,
      GivenTheNoexceptRules_WhenCheckingTraits_ThenTheyMatchTheStandard)
{
  typedef sp::shared_ptr<int> ptr;
  static_assert (std::is_nothrow_default_constructible<ptr>::value, "");
  static_assert (std::is_nothrow_constructible<ptr, std::nullptr_t>::value,
                 "");
  static_assert (std::is_nothrow_copy_constructible<ptr>::value, "");
  static_assert (std::is_nothrow_move_constructible<ptr>::value, "");
  static_assert (std::is_nothrow_copy_assignable<ptr>::value, "");
  static_assert (std::is_nothrow_move_assignable<ptr>::value, "");
  static_assert (std::is_nothrow_destructible<ptr>::value, "");
  static_assert (noexcept (ptr (std::declval<ptr const &> (),
                                static_cast<int *> (nullptr))),
                 "the aliasing constructor is noexcept");
  static_assert (!noexcept (ptr (static_cast<int *> (nullptr))),
                 "the pointer constructor allocates");
  static_assert (std::is_nothrow_constructible<sp::shared_ptr<Animal>,
                                               sp::shared_ptr<Dog>>::value,
                 "converting move");
  static_assert (
      std::is_nothrow_constructible<sp::shared_ptr<Animal>,
                                    sp::shared_ptr<Dog> const &>::value,
      "converting copy");
  SUCCEED ();
}

TEST (LumexSharedPtrConstructionTest,
      GivenTheLayout_WhenMeasured_ThenTheClassIsTwoPointers)
{
  static_assert (sizeof (sp::shared_ptr<int>) == 2 * sizeof (void *), "");
  static_assert (sizeof (sp::shared_ptr<void>) == 2 * sizeof (void *), "");
  static_assert (sizeof (sp::weak_ptr<int>) == 2 * sizeof (void *), "");
  static_assert (alignof (sp::shared_ptr<int>) == alignof (void *), "");
  SUCCEED ();
}

TEST (LumexSharedPtrConstructionTest,
      GivenAConstantInitializedGlobal_WhenUsedBeforeMain_ThenItIsEmpty)
{
  // The default constructor is constexpr, so a namespace-scope pointer is
  // constant-initialized and safe to use during static initialization.
  static sp::shared_ptr<int> global;
  EXPECT_EQ (global.use_count (), 0);
  static sp::shared_ptr<int> global_null (nullptr);
  EXPECT_EQ (global_null.get (), nullptr);
}
} // namespace
