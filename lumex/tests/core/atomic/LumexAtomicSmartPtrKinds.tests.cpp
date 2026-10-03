// Kinds of smart pointers held by atomic_shared_ptr and atomic_weak_ptr:
// empty ones, empty ones that still store a pointer, owners of a null
// pointer, aliases with the same stored pointer and different owners (and
// the reverse), pointer adjustment under multiple inheritance, polymorphic,
// const, void and incomplete element types, pointers from unique_ptr, custom
// deleters, allocate_shared, enable_shared_from_this, and the array forms of
// C++17 and C++20. The equivalence cases extend the aliasing test of the
// libc++ implementation of llvm-project pull request 194215.

#include <atomic>
#include <cstddef>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/atomic/LumexAtomic"
#include "lumex/tests/core/atomic/LumexAtomicTestSupport.hpp"

using namespace lumex_atomic_test;

namespace
{
struct Pair
{
  int a;
  int b;
};

struct Base
{
  virtual ~Base () {}
  virtual int
  id () const
  {
    return 0;
  }
};

struct Derived : Base
{
  explicit Derived (int v) : value (v) {}
  int
  id () const override
  {
    return value;
  }
  int value;
};

struct Left
{
  virtual ~Left () {}
  int left = 1;
};

struct Right
{
  virtual ~Right () {}
  int right = 2;
};

struct Both : Left, Right
{
  int both = 3;
};

struct SelfAware : std::enable_shared_from_this<SelfAware>
{
  int value = 9;
};

struct Incomplete;
std::shared_ptr<Incomplete> make_incomplete ();
int incomplete_value (std::shared_ptr<Incomplete> const &p);

int g_first = 5;
int g_second = 6;

/// A shared pointer with no owner that still stores @p p.
std::shared_ptr<int>
empty_storing (int *p)
{
  return std::shared_ptr<int> (std::shared_ptr<int> (), p);
}
} // namespace

// --- empty, empty-but-storing and null-owning shared pointers --------------

TEST (LumexAtomicSharedPtrKindsTest,
      GivenTwoDefaultEmptyPointers_WhenCompareExchange_ThenEquivalent)
{
  atomic_shared_ptr<int> empty_atom;
  std::shared_ptr<int> expected;
  std::shared_ptr<int> const desired = std::make_shared<int> (1);
  EXPECT_TRUE (empty_atom.compare_exchange_strong (expected, desired));
  EXPECT_TRUE (same_owner_and_pointer (empty_atom.load (), desired));
}

TEST (LumexAtomicSharedPtrKindsTest,
      GivenEmptyPointersStoringDifferentPointers_WhenCompare_ThenNotEquivalent)
{
  std::shared_ptr<int> const empty_first = empty_storing (&g_first);
  std::shared_ptr<int> const empty_second = empty_storing (&g_second);
  ASSERT_EQ (empty_first.use_count (), 0L);
  ASSERT_EQ (empty_first.get (), &g_first);

  atomic_shared_ptr<int> atom (empty_first);
  std::shared_ptr<int> expected = empty_second;
  EXPECT_FALSE (
      atom.compare_exchange_strong (expected, std::make_shared<int> (1)));
  EXPECT_EQ (expected.get (), &g_first)
      << "both empty, but the stored pointers differ";

  expected = empty_first;
  EXPECT_TRUE (
      atom.compare_exchange_strong (expected, std::make_shared<int> (2)));
  EXPECT_EQ (*atom.load (), 2);
}

TEST (LumexAtomicSharedPtrKindsTest,
      GivenAnEmptyPointerStoringAPointer_WhenStoredAndLoaded_ThenKeepsIt)
{
  atomic_shared_ptr<int> atom;
  atom.store (empty_storing (&g_first));
  std::shared_ptr<int> const loaded = atom.load ();
  EXPECT_EQ (loaded.get (), &g_first);
  EXPECT_EQ (loaded.use_count (), 0L);
  EXPECT_FALSE (atom.load ().owner_before (std::shared_ptr<int> ()));
}

TEST (LumexAtomicSharedPtrKindsTest,
      GivenAnOwnerOfNullAndAnEmptyPointer_WhenCompare_ThenNotEquivalent)
{
  std::shared_ptr<int> const null_owner (static_cast<int *> (nullptr));
  ASSERT_EQ (null_owner.use_count (), 1L);
  ASSERT_EQ (null_owner.get (), nullptr);

  atomic_shared_ptr<int> atom (null_owner);
  std::shared_ptr<int> expected;
  EXPECT_FALSE (
      atom.compare_exchange_strong (expected, std::make_shared<int> (1)));
  EXPECT_TRUE (same_owner_and_pointer (expected, null_owner));
  EXPECT_EQ (null_owner.use_count (), 3L);

  atomic_shared_ptr<int> empty_atom;
  std::shared_ptr<int> expected_null_owner = null_owner;
  EXPECT_FALSE (empty_atom.compare_exchange_strong (
      expected_null_owner, std::make_shared<int> (2)));
  EXPECT_EQ (expected_null_owner.use_count (), 0L);
}

TEST (LumexAtomicSharedPtrKindsTest,
      GivenAnOwnerOfNullWithADeleter_WhenReleasedByStore_ThenDeleterRunsOnce)
{
  int deletions = 0;
  struct CountingNullDeleter
  {
    int *count;
    void
    operator() (int *p) const
    {
      EXPECT_EQ (p, nullptr);
      ++*count;
    }
  };
  {
    CountingNullDeleter const deleter = { &deletions };
    atomic_shared_ptr<int> atom (
        std::shared_ptr<int> (static_cast<int *> (nullptr), deleter));
    EXPECT_EQ (deletions, 0);
    atom.store (nullptr);
    EXPECT_EQ (deletions, 1);
  }
  EXPECT_EQ (deletions, 1);
}

// --- aliasing
// ----------------------------------------------------------------

TEST (
    LumexAtomicSharedPtrKindsTest,
    GivenAliasesOfOneOwnerWithDifferentPointers_WhenCompare_ThenNotEquivalent)
{
  std::shared_ptr<Pair> const owner = std::make_shared<Pair> (Pair{ 10, 20 });
  std::shared_ptr<int> const view_a (owner, &owner->a);
  std::shared_ptr<int> const view_b (owner, &owner->b);
  ASSERT_EQ (view_a.use_count (), view_b.use_count ());

  atomic_shared_ptr<int> atom (view_a);
  std::shared_ptr<int> expected = view_b;
  EXPECT_FALSE (
      atom.compare_exchange_strong (expected, std::make_shared<int> (99)));
  EXPECT_EQ (expected.get (), view_a.get ());
  EXPECT_EQ (atom.load ().get (), view_a.get ());
}

TEST (LumexAtomicSharedPtrKindsTest,
      GivenAliasesOfOneOwnerWithTheSamePointer_WhenCompare_ThenEquivalent)
{
  std::shared_ptr<Pair> const owner = std::make_shared<Pair> (Pair{ 10, 20 });
  std::shared_ptr<int> const view_a (owner, &owner->a);
  std::shared_ptr<int> const view_a_again (owner, &owner->a);
  std::shared_ptr<int> const view_b (owner, &owner->b);
  atomic_shared_ptr<int> atom (view_a);
  std::shared_ptr<int> expected = view_a_again;
  EXPECT_TRUE (atom.compare_exchange_strong (expected, view_b));
  EXPECT_EQ (*atom.load (), 20);
  EXPECT_EQ (owner.use_count (), 6L)
      << "owner, three views, expected (not replaced) and the atomic";
}

TEST (LumexAtomicSharedPtrKindsTest,
      GivenTheSameStoredPointerWithDifferentOwners_WhenCompare_ThenNotEquiv)
{
  std::shared_ptr<int> const owner1 = std::make_shared<int> (0);
  std::shared_ptr<int> const owner2 = std::make_shared<int> (0);
  std::shared_ptr<int> const v1 (owner1, &g_first);
  std::shared_ptr<int> const v2 (owner2, &g_first);
  ASSERT_EQ (v1.get (), v2.get ());

  atomic_shared_ptr<int> atom (v1);
  std::shared_ptr<int> expected = v2;
  EXPECT_FALSE (
      atom.compare_exchange_strong (expected, std::make_shared<int> (1)));
  EXPECT_TRUE (same_owner_and_pointer (expected, v1));
  EXPECT_EQ (owner2.use_count (), 2L) << "owner2 and v2";

  std::shared_ptr<int> expected_weak_form = v2;
  EXPECT_FALSE (atom.compare_exchange_weak (expected_weak_form,
                                            std::make_shared<int> (1)));
}

TEST (LumexAtomicSharedPtrKindsTest,
      GivenMultipleInheritanceViews_WhenCompare_ThenTheAdjustedPointerDecides)
{
  std::shared_ptr<Both> const both = std::make_shared<Both> ();
  std::shared_ptr<Right> const right = both; // adjusted to the Right base
  ASSERT_NE (static_cast<void *> (right.get ()),
             static_cast<void *> (both.get ()));

  atomic_shared_ptr<Right> atom (right);
  std::shared_ptr<Right> same_view = std::static_pointer_cast<Right> (both);
  EXPECT_TRUE (atom.compare_exchange_strong (same_view, same_view))
      << "the same owner and the same adjusted pointer";

  std::shared_ptr<Right> other_view (both, nullptr);
  EXPECT_FALSE (
      atom.compare_exchange_strong (other_view, std::shared_ptr<Right> ()));
  EXPECT_EQ (atom.load ()->right, 2);

  atomic_shared_ptr<Left> left_atom (both);
  EXPECT_EQ (left_atom.load ()->left, 1);
  EXPECT_EQ (both.use_count (), 6L)
      << "both, right, same_view, other_view and the two atomics";
}

// --- element types ----------------------------------------------------------

TEST (LumexAtomicSharedPtrKindsTest,
      GivenAPolymorphicObject_WhenLoadedThroughTheBase_ThenCallsTheDerived)
{
  std::shared_ptr<Derived> const first = std::make_shared<Derived> (7);
  std::shared_ptr<Derived> const second = std::make_shared<Derived> (8);
  atomic_shared_ptr<Base> atom (first);
  EXPECT_EQ (atom.load ()->id (), 7);

  std::shared_ptr<Base> expected = first;
  EXPECT_TRUE (atom.compare_exchange_strong (expected, second));
  EXPECT_EQ (atom.load ()->id (), 8);
  std::shared_ptr<Derived> const down
      = std::dynamic_pointer_cast<Derived> (atom.load ());
  ASSERT_TRUE (down);
  EXPECT_EQ (down.get (), second.get ());

  atomic_weak_ptr<Base> observer ((std::weak_ptr<Base> (second)));
  EXPECT_EQ (observer.load ().lock ()->id (), 8);
}

TEST (LumexAtomicSharedPtrKindsTest,
      GivenAConstElement_WhenStoredAndCompareExchanged_ThenWorks)
{
  typedef std::string const Text;
  std::shared_ptr<Text> const first = std::make_shared<Text> ("first");
  atomic_shared_ptr<Text> atom (first);
  std::shared_ptr<Text> expected = first;
  EXPECT_TRUE (atom.compare_exchange_strong (
      expected, std::make_shared<Text> ("second")));
  EXPECT_EQ (*atom.load (), "second");
  static_assert (std::is_same<decltype (*atom.load ()), Text &>::value,
                 "the element stays const");
}

TEST (LumexAtomicSharedPtrKindsTest,
      GivenVoidElements_WhenStoredAndCompared_ThenTheOwnerDecides)
{
  std::shared_ptr<int> const number = std::make_shared<int> (5);
  std::shared_ptr<double> const real = std::make_shared<double> (0.5);
  atomic_shared_ptr<void> atom (number);
  EXPECT_EQ (*static_cast<int *> (atom.load ().get ()), 5);

  std::shared_ptr<void> expected = real;
  EXPECT_FALSE (atom.compare_exchange_strong (expected, real));
  EXPECT_EQ (expected.get (), static_cast<void *> (number.get ()));
  EXPECT_TRUE (atom.compare_exchange_strong (expected, real));
  EXPECT_EQ (*static_cast<double *> (atom.load ().get ()), 0.5);
}

TEST (LumexAtomicSharedPtrKindsTest,
      GivenAnIncompleteType_WhenHeld_ThenItIsCompletedLater)
{
  atomic_shared_ptr<Incomplete> atom;
  atomic_weak_ptr<Incomplete> weak;
  EXPECT_FALSE (atom.load ());
  std::shared_ptr<Incomplete> const p = make_incomplete ();
  atom.store (p);
  weak.store (std::weak_ptr<Incomplete> (p));
  EXPECT_EQ (atom.load ().get (), p.get ());
  EXPECT_EQ (weak.load ().lock ().get (), p.get ());
  EXPECT_EQ (incomplete_value (atom.load ()), 42);
}

TEST (LumexAtomicSharedPtrKindsTest,
      GivenAUniquePointerWithADeleter_WhenStored_ThenOwnershipMovesOnce)
{
  DeletionLedger ledger (2);
  std::unique_ptr<int, DeletionLedger::Deleter> unique
      = ledger.make_unique_owner ();
  {
    atomic_shared_ptr<int> atom (std::shared_ptr<int> (std::move (unique)));
    EXPECT_FALSE (unique);
    EXPECT_EQ (ledger.deletions (0), 0);
    EXPECT_EQ (*atom.load (), ledger.payload (0));
  }
  EXPECT_EQ (ledger.deletions (0), 1) << "deleted once, by the atomic";
}

TEST (LumexAtomicSharedPtrKindsTest,
      GivenACustomDeleter_WhenTheLastOwnerIsReleasedByStore_ThenRunsOnce)
{
  DeletionLedger ledger (8);
  {
    atomic_shared_ptr<int> atom (ledger.make ());
    EXPECT_EQ (ledger.deletions (0), 0);
    atom.store (ledger.make ());
    EXPECT_EQ (ledger.deletions (0), 1);
    std::shared_ptr<int> const held = atom.exchange (ledger.make ());
    EXPECT_EQ (ledger.deletions (1), 0) << "held by the exchange result";
  }
  EXPECT_EQ (ledger.deleted_twice (), 0u);
  EXPECT_EQ (ledger.not_deleted (), 0u);
}

TEST (LumexAtomicSharedPtrKindsTest,
      GivenAllocateShared_WhenEveryOwnerIsGone_ThenTheBlockIsFreed)
{
  long const before = live_allocations ().load ();
  {
    atomic_shared_ptr<Tracker> atom (make_counted_tracker (1));
    atomic_weak_ptr<Tracker> weak (atom.load ());
    EXPECT_EQ (live_allocations ().load (), before + 1);
    atom.store (nullptr);
    EXPECT_TRUE (weak.load ().expired ());
    EXPECT_EQ (live_allocations ().load (), before + 1)
        << "the weak reference keeps the combined block";
  }
  EXPECT_EQ (live_allocations ().load (), before);
}

TEST (LumexAtomicSharedPtrKindsTest,
      GivenEnableSharedFromThis_WhenLoaded_ThenSharedFromThisIsEquivalent)
{
  std::shared_ptr<SelfAware> const object = std::make_shared<SelfAware> ();
  atomic_shared_ptr<SelfAware> atom (object);
  std::shared_ptr<SelfAware> expected = atom.load ()->shared_from_this ();
  EXPECT_TRUE (atom.compare_exchange_strong (expected, nullptr));
  EXPECT_FALSE (atom.load ());
  EXPECT_EQ (object.use_count (), 2L) << "object and expected";
}

#if defined(__cpp_lib_enable_shared_from_this)
TEST (LumexAtomicSharedPtrKindsTest,
      GivenWeakFromThis_WhenStoredInAnAtomicWeakPtr_ThenRefersToTheObject)
{
  std::shared_ptr<SelfAware> const object = std::make_shared<SelfAware> ();
  atomic_weak_ptr<SelfAware> atom (object->weak_from_this ());
  EXPECT_TRUE (refers_to (atom.load (), object));
  std::weak_ptr<SelfAware> expected = object->weak_from_this ();
  EXPECT_TRUE (
      atom.compare_exchange_strong (expected, std::weak_ptr<SelfAware> ()));
}
#endif

#if defined(__cpp_lib_shared_ptr_arrays)
TEST (LumexAtomicSharedPtrKindsTest,
      GivenAnArrayElementType_WhenStoredAndCompareExchanged_ThenWorks)
{
  std::shared_ptr<int[]> const arr (new int[3]{ 1, 2, 3 });
  atomic_shared_ptr<int[]> atom (arr);
  EXPECT_EQ (atom.load ()[2], 3);
  atomic_weak_ptr<int[]> weak ((std::weak_ptr<int[]> (arr)));
  EXPECT_EQ (weak.load ().lock ()[1], 2);
  std::shared_ptr<int[]> expected = arr;
  EXPECT_TRUE (atom.compare_exchange_strong (expected, nullptr));
  EXPECT_FALSE (atom.load ());
  EXPECT_EQ (arr.use_count (), 2L) << "arr and expected";
}
#endif

#if defined(__cpp_lib_shared_ptr_arrays)                                      \
    && __cpp_lib_shared_ptr_arrays >= 201707L
TEST (LumexAtomicSharedPtrKindsTest,
      GivenMakeSharedForAnArray_WhenStored_ThenElementsAreReachable)
{
  atomic_shared_ptr<double[]> atom (std::make_shared<double[]> (4, 0.25));
  EXPECT_EQ (atom.load ()[3], 0.25);
  std::shared_ptr<double[]> const previous
      = atom.exchange (std::make_shared<double[]> (2));
  EXPECT_EQ (previous[0], 0.25);
  EXPECT_EQ (atom.load ()[1], 0.0);
}
#endif

// --- weak pointers
// -------------------------------------------------------------

TEST (LumexAtomicWeakPtrKindsTest,
      GivenLiveAliasesOfOneOwner_WhenCompareExchange_ThenThePointerDecides)
{
  std::shared_ptr<Pair> const owner = std::make_shared<Pair> (Pair{ 10, 20 });
  std::shared_ptr<int> const view_a (owner, &owner->a);
  std::shared_ptr<int> const view_b (owner, &owner->b);
  std::weak_ptr<int> const wa = view_a;
  std::weak_ptr<int> const wb = view_b;

  atomic_weak_ptr<int> atom (wa);
  std::weak_ptr<int> expected = wb;
  EXPECT_FALSE (atom.compare_exchange_strong (expected, wb));
  EXPECT_EQ (expected.lock ().get (), view_a.get ());

  expected = wa;
  EXPECT_TRUE (atom.compare_exchange_strong (expected, wb));
  EXPECT_EQ (atom.load ().lock ().get (), &owner->b);
  EXPECT_EQ (owner.use_count (), 3L) << "the comparison left no owner behind";
}

TEST (LumexAtomicWeakPtrKindsTest,
      GivenALiveOwnerStoringNull_WhenCompareExchange_ThenNotEquivalent)
{
  // expected locks to a live owner that stores a null pointer: the stored
  // pointers still have to be compared.
  std::shared_ptr<Pair> const owner = std::make_shared<Pair> (Pair{ 10, 20 });
  std::shared_ptr<int> const view_null (owner, static_cast<int *> (nullptr));
  std::shared_ptr<int> const view_a (owner, &owner->a);
  atomic_weak_ptr<int> atom ((std::weak_ptr<int> (view_a)));

  std::weak_ptr<int> expected = view_null;
  EXPECT_FALSE (
      atom.compare_exchange_strong (expected, std::weak_ptr<int> (view_null)));
  EXPECT_EQ (expected.lock ().get (), &owner->a);
  EXPECT_EQ (atom.load ().lock ().get (), &owner->a);
}

TEST (LumexAtomicWeakPtrKindsTest,
      GivenExpiredAliasesWithTheSamePointer_WhenCompareExchange_ThenEquivalent)
{
  std::weak_ptr<int> wa;
  {
    std::shared_ptr<Pair> const owner
        = std::make_shared<Pair> (Pair{ 10, 20 });
    wa = std::shared_ptr<int> (owner, &owner->a);
  }
  ASSERT_TRUE (wa.expired ());
  atomic_weak_ptr<int> atom (wa);
  std::weak_ptr<int> expected = wa;
  EXPECT_TRUE (atom.compare_exchange_strong (expected, std::weak_ptr<int> ()));
  EXPECT_TRUE (atom.load ().expired ());
}

TEST (
    LumexAtomicWeakPtrKindsTest,
    GivenExpiredAliasesWithDifferentPointers_WhenCompare_ThenPerImplementation)
{
  std::weak_ptr<int> wa;
  std::weak_ptr<int> wb;
  {
    std::shared_ptr<Pair> const owner
        = std::make_shared<Pair> (Pair{ 10, 20 });
    wa = std::shared_ptr<int> (owner, &owner->a);
    wb = std::shared_ptr<int> (owner, &owner->b);
  }
  ASSERT_TRUE (wa.expired ());
  atomic_weak_ptr<int> atom (wa);
  std::weak_ptr<int> expected = wb;
  bool const exchanged
      = atom.compare_exchange_strong (expected, std::weak_ptr<int> ());
#if LUMEX_ATOMIC_SMART_PTR_USES_STD
  // The standard types still see the stored pointer of an expired weak
  // pointer, so the two differ.
  EXPECT_FALSE (exchanged);
#else
  // std::weak_ptr does not expose the stored pointer once the object is
  // gone; the lock-based implementation then compares ownership only.
  EXPECT_TRUE (exchanged);
#endif
}

TEST (LumexAtomicWeakPtrKindsTest,
      GivenExpiredPointersOfDifferentOwners_WhenCompare_ThenNotEquivalent)
{
  std::weak_ptr<int> w1;
  std::weak_ptr<int> w2;
  {
    std::shared_ptr<int> const p1 = std::make_shared<int> (1);
    std::shared_ptr<int> const p2 = std::make_shared<int> (2);
    w1 = p1;
    w2 = p2;
  }
  atomic_weak_ptr<int> atom (w1);
  std::weak_ptr<int> expected = w2;
  EXPECT_FALSE (
      atom.compare_exchange_strong (expected, std::weak_ptr<int> ()));
  EXPECT_TRUE (same_owner (expected, w1));
}

TEST (LumexAtomicWeakPtrKindsTest,
      GivenAnEmptyAndAnExpiredPointer_WhenCompareExchange_ThenNotEquivalent)
{
  std::weak_ptr<int> expired;
  {
    std::shared_ptr<int> const p = std::make_shared<int> (1);
    expired = p;
  }
  atomic_weak_ptr<int> atom (expired);
  std::weak_ptr<int> expected;
  EXPECT_FALSE (
      atom.compare_exchange_strong (expected, std::weak_ptr<int> ()));
  EXPECT_TRUE (same_owner (expected, expired));

  atomic_weak_ptr<int> empty_atom;
  std::weak_ptr<int> expected_expired = expired;
  EXPECT_FALSE (empty_atom.compare_exchange_strong (expected_expired,
                                                    std::weak_ptr<int> ()));
  EXPECT_TRUE (same_owner (expected_expired, std::weak_ptr<int> ()));
}

namespace
{
struct Incomplete
{
  int value;
};

std::shared_ptr<Incomplete>
make_incomplete ()
{
  std::shared_ptr<Incomplete> p = std::make_shared<Incomplete> ();
  p->value = 42;
  return p;
}

int
incomplete_value (std::shared_ptr<Incomplete> const &p)
{
  return p->value;
}
} // namespace
