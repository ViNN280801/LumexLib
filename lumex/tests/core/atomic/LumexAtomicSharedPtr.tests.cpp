// Per-member tests of atomic_shared_ptr<T>, typed over six value types.
// Started from the libc++ tests util.smartptr/atomic/shared/*.pass.cpp of
// llvm-project pull request 194215 (the author's own libc++ implementation of
// P0718R2) and extended to the whole public surface: every constructor and
// assignment, every overload of every member with every valid memory order
// on both outcomes, use_count around each operation, the members available
// on a const object, and misuse such as storing the value already held.

#include <atomic>
#include <cstddef>
#include <memory>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/atomic/LumexAtomic"
#include "lumex/tests/core/atomic/LumexAtomicTestSupport.hpp"

using namespace lumex_atomic_test;

template <typename T> class LumexAtomicSharedPtrTest : public ::testing::Test
{
};

TYPED_TEST_SUITE (LumexAtomicSharedPtrTest, ValueTypes);

// --- type surface
// ------------------------------------------------------------

TYPED_TEST (LumexAtomicSharedPtrTest,
            GivenTheType_WhenInspected_ThenMatchesTheStdInterface)
{
  typedef TypeParam T;
  typedef atomic_shared_ptr<T> A;

  static_assert (
      std::is_same<typename A::value_type, std::shared_ptr<T>>::value,
      "value_type");
  static_assert (
      std::is_same<decltype (A::is_always_lock_free), bool const>::value,
      "is_always_lock_free is a static constexpr bool");
  static_assert (!std::is_copy_constructible<A>::value, "no copy");
  static_assert (!std::is_copy_assignable<A>::value, "no copy assignment");
  static_assert (!std::is_move_constructible<A>::value, "no move");
  static_assert (!std::is_move_assignable<A>::value, "no move assignment");
  static_assert (std::is_nothrow_default_constructible<A>::value, "");
  static_assert (std::is_nothrow_constructible<A, std::nullptr_t>::value,
                 "LWG 3661");
  static_assert (std::is_nothrow_constructible<A, std::shared_ptr<T>>::value,
                 "");
  static_assert (
      std::is_nothrow_constructible<A, std::shared_ptr<T> const &>::value, "");
  static_assert (std::is_nothrow_destructible<A>::value, "");
  static_assert (!std::is_constructible<A, std::weak_ptr<T>>::value,
                 "no construction from weak_ptr");
  static_assert (std::is_assignable<A &, std::nullptr_t>::value, "LWG 3893");
  static_assert (std::is_assignable<A &, std::shared_ptr<T>>::value, "");
  static_assert (std::is_convertible<A const &, std::shared_ptr<T>>::value,
                 "implicit conversion to shared_ptr");
  static_assert (
      std::is_same<
          A, lumex::core::atomic::smart_ptr::atomic_shared_ptr<T>>::value,
      "the global name is the namespace class");
  SUCCEED ();
}

TYPED_TEST (LumexAtomicSharedPtrTest,
            GivenAConstObject_WhenCallingMembers_ThenOnlyObserversCompile)
{
  typedef atomic_shared_ptr<TypeParam> A;
  typedef A const C;

  static_assert (can_load<C>::value && can_load<A>::value, "load is const");
  static_assert (can_wait<C>::value && can_wait<A>::value, "wait is const");
  static_assert (can_is_lock_free<C>::value, "is_lock_free is const");
  static_assert (!can_store<C>::value && can_store<A>::value,
                 "store needs a non-const object");
  static_assert (!can_exchange<C>::value && can_exchange<A>::value, "");
  static_assert (!can_compare_exchange_strong<C>::value
                     && can_compare_exchange_strong<A>::value,
                 "");
  static_assert (!can_compare_exchange_weak<C>::value
                     && can_compare_exchange_weak<A>::value,
                 "");
  static_assert (!can_notify_one<C>::value && can_notify_one<A>::value, "");
  static_assert (!can_notify_all<C>::value && can_notify_all<A>::value, "");
  static_assert (!can_assign_value<C>::value && can_assign_value<A>::value,
                 "");
  static_assert (!can_compare_exchange_with_rvalue_expected<A>::value,
                 "expected must be an lvalue: it receives the stored value");
  SUCCEED ();
}

// --- constructors and destructor -------------------------------------------

TYPED_TEST (LumexAtomicSharedPtrTest,
            GivenDefaultConstruction_WhenLoaded_ThenEmpty)
{
  atomic_shared_ptr<TypeParam> const a;
  std::shared_ptr<TypeParam> const loaded = a.load ();
  EXPECT_FALSE (loaded);
  EXPECT_EQ (loaded.use_count (), 0L);
}

TYPED_TEST (LumexAtomicSharedPtrTest,
            GivenNullptrConstruction_WhenLoaded_ThenEmpty)
{
  atomic_shared_ptr<TypeParam> const a (nullptr);
  std::shared_ptr<TypeParam> const loaded = a.load ();
  EXPECT_FALSE (loaded);
  EXPECT_EQ (loaded.use_count (), 0L);
}

TYPED_TEST (LumexAtomicSharedPtrTest,
            GivenAnLvalue_WhenConstructedFromIt_ThenSharesOwnership)
{
  typedef TypeParam T;
  std::shared_ptr<T> const p = state_a<T> ();
  atomic_shared_ptr<T> const a (p);
  EXPECT_EQ (p.use_count (), 2L);
  EXPECT_TRUE (same_owner_and_pointer (a.load (), p));
  EXPECT_TRUE (*a.load () == SpValues<T>::a ());
}

TYPED_TEST (LumexAtomicSharedPtrTest,
            GivenAnRvalue_WhenConstructedFromIt_ThenTakesOverItsReference)
{
  typedef TypeParam T;
  std::shared_ptr<T> source = state_a<T> ();
  std::weak_ptr<T> const observer = source;
  T *const raw = source.get ();
  atomic_shared_ptr<T> const a (std::move (source));
  EXPECT_FALSE (source) << "the moved-from argument is empty";
  EXPECT_EQ (observer.use_count (), 1L) << "no extra reference was taken";
  EXPECT_EQ (a.load ().get (), raw);
}

TYPED_TEST (LumexAtomicSharedPtrTest,
            GivenAStoredOwner_WhenTheObjectIsDestroyed_ThenReleasesIt)
{
  typedef TypeParam T;
  std::shared_ptr<T> const p = state_a<T> ();
  {
    atomic_shared_ptr<T> const a (p);
    EXPECT_EQ (p.use_count (), 2L);
  }
  EXPECT_EQ (p.use_count (), 1L);

  std::weak_ptr<T> observer;
  {
    atomic_shared_ptr<T> const a (state_b<T> ());
    observer = a.load ();
    EXPECT_FALSE (observer.expired ());
  }
  EXPECT_TRUE (observer.expired ()) << "the last owner died with the atomic";
}

// --- is_lock_free / is_always_lock_free
// --------------------------------------

TYPED_TEST (LumexAtomicSharedPtrTest,
            GivenIsAlwaysLockFree_WhenBoundToAReference_ThenAConstantBool)
{
  typedef atomic_shared_ptr<TypeParam> A;
#if LUMEX_ATOMIC_SMART_PTR_USES_STD
  static_assert (
      A::is_always_lock_free
          == std::atomic<std::shared_ptr<TypeParam>>::is_always_lock_free,
      "same as the standard type");
#else
  static_assert (!A::is_always_lock_free, "the lock-based form is not");
#endif
  // Binding to a reference odr-uses the member, which needs its out-of-line
  // definition before C++17.
  bool const &bound = A::is_always_lock_free;
  EXPECT_EQ (bound, A::is_always_lock_free);
}

TYPED_TEST (LumexAtomicSharedPtrTest,
            GivenAnObject_WhenAskingIsLockFree_ThenConsistentWithTheType)
{
  typedef atomic_shared_ptr<TypeParam> A;
  A const a (state_a<TypeParam> ());
  static_assert (std::is_same<decltype (a.is_lock_free ()), bool>::value,
                 "returns bool");
  static_assert (noexcept (a.is_lock_free ()), "noexcept");
  bool const lock_free = a.is_lock_free ();
  if (A::is_always_lock_free)
    EXPECT_TRUE (lock_free);
#if LUMEX_ATOMIC_SMART_PTR_USES_STD
  std::atomic<std::shared_ptr<TypeParam>> const standard;
  EXPECT_EQ (lock_free, standard.is_lock_free ());
#else
  EXPECT_FALSE (lock_free);
#endif
  EXPECT_EQ (a.is_lock_free (), lock_free) << "stable across calls";
}

// --- load
// ----------------------------------------------------------------------

TYPED_TEST (LumexAtomicSharedPtrTest,
            GivenEveryLoadOrder_WhenLoading_ThenReturnsANewOwner)
{
  typedef TypeParam T;
  std::shared_ptr<T> const p = state_a<T> ();
  atomic_shared_ptr<T> const a (p);
  static_assert (std::is_same<decltype (a.load ()), std::shared_ptr<T>>::value,
                 "returns shared_ptr<T>");
  static_assert (noexcept (a.load ()), "noexcept");
  static_assert (noexcept (a.load (std::memory_order_seq_cst)), "noexcept");

  std::vector<std::memory_order> const orders = load_orders ();
  for (std::size_t i = 0; i < orders.size (); ++i)
    {
      std::shared_ptr<T> const loaded = a.load (orders[i]);
      EXPECT_TRUE (same_owner_and_pointer (loaded, p));
      EXPECT_EQ (p.use_count (), 3L) << "the load is a new owner";
    }
  EXPECT_EQ (p.use_count (), 2L);
}

TYPED_TEST (LumexAtomicSharedPtrTest,
            GivenAnEmptyObject_WhenLoadingWithEveryOrder_ThenReturnsEmpty)
{
  atomic_shared_ptr<TypeParam> const a;
  std::vector<std::memory_order> const orders = load_orders ();
  for (std::size_t i = 0; i < orders.size (); ++i)
    {
      std::shared_ptr<TypeParam> const loaded = a.load (orders[i]);
      EXPECT_FALSE (loaded);
      EXPECT_EQ (loaded.use_count (), 0L);
    }
}

// --- store
// -----------------------------------------------------------------------

TYPED_TEST (LumexAtomicSharedPtrTest,
            GivenEveryStoreOrder_WhenStoring_ThenReplacesAndReleases)
{
  typedef TypeParam T;
  atomic_shared_ptr<T> a;
  std::shared_ptr<T> const p = state_a<T> ();
  a.store (p);
  EXPECT_TRUE (same_owner_and_pointer (a.load (), p));
  EXPECT_EQ (p.use_count (), 2L);

  std::vector<std::memory_order> const orders = store_orders ();
  std::shared_ptr<T> previous = p;
  for (std::size_t i = 0; i < orders.size (); ++i)
    {
      std::shared_ptr<T> const next = state_b<T> ();
      long const previous_count = previous.use_count ();
      a.store (next, orders[i]);
      EXPECT_TRUE (same_owner_and_pointer (a.load (), next));
      EXPECT_EQ (next.use_count (), 2L);
      EXPECT_EQ (previous.use_count (), previous_count - 1)
          << "store released the previous value";
      previous = next;
    }

  a.store (nullptr, std::memory_order_relaxed);
  EXPECT_FALSE (a.load ());
  EXPECT_EQ (previous.use_count (), 1L);

  static_assert (std::is_same<decltype (a.store (nullptr)), void>::value,
                 "returns void");
  static_assert (noexcept (a.store (nullptr)), "noexcept");
  static_assert (
      noexcept (a.store (std::shared_ptr<T> (), std::memory_order_seq_cst)),
      "noexcept");
}

TYPED_TEST (LumexAtomicSharedPtrTest,
            GivenAnRvalue_WhenStored_ThenTakesOverItsReference)
{
  typedef TypeParam T;
  atomic_shared_ptr<T> a;
  std::shared_ptr<T> source = state_a<T> ();
  std::weak_ptr<T> const observer = source;
  a.store (std::move (source));
  EXPECT_FALSE (source);
  EXPECT_EQ (observer.use_count (), 1L);
}

TYPED_TEST (LumexAtomicSharedPtrTest,
            GivenTheHeldValue_WhenStoredAgain_ThenCountsAreUnchanged)
{
  typedef TypeParam T;
  std::shared_ptr<T> const p = state_a<T> ();
  atomic_shared_ptr<T> a (p);
  for (int i = 0; i < 3; ++i)
    a.store (a.load ());
  EXPECT_EQ (p.use_count (), 2L);
  EXPECT_TRUE (same_owner_and_pointer (a.load (), p));
}

// --- assignment and conversion
// -----------------------------------------------

TYPED_TEST (LumexAtomicSharedPtrTest,
            GivenValuesAndNullptr_WhenAssigned_ThenStoredWithVoidResult)
{
  typedef TypeParam T;
  atomic_shared_ptr<T> a;
  std::shared_ptr<T> const p = state_a<T> ();
  a = p;
  EXPECT_TRUE (same_owner_and_pointer (a.load (), p));
  EXPECT_EQ (p.use_count (), 2L);

  std::shared_ptr<T> moved = state_b<T> ();
  std::weak_ptr<T> const observer = moved;
  a = std::move (moved);
  EXPECT_FALSE (moved);
  EXPECT_EQ (observer.use_count (), 1L);
  EXPECT_EQ (p.use_count (), 1L) << "assignment released the previous value";

  a = nullptr;
  EXPECT_FALSE (a.load ());
  EXPECT_TRUE (observer.expired ());

  static_assert (std::is_same<decltype (a = p), void>::value, "void");
  static_assert (std::is_same<decltype (a = nullptr), void>::value, "void");
  static_assert (noexcept (a = nullptr), "noexcept");
  static_assert (noexcept (a = std::shared_ptr<T> (p)), "noexcept");
}

TYPED_TEST (LumexAtomicSharedPtrTest,
            GivenAConstObject_WhenConverted_ThenLoadsANewOwner)
{
  typedef TypeParam T;
  std::shared_ptr<T> const p = state_a<T> ();
  atomic_shared_ptr<T> const a (p);

  std::shared_ptr<T> const implicit = a;
  EXPECT_TRUE (same_owner_and_pointer (implicit, p));
  std::shared_ptr<T> const explicit_cast = static_cast<std::shared_ptr<T>> (a);
  EXPECT_TRUE (same_owner_and_pointer (explicit_cast, p));
  EXPECT_EQ (p.use_count (), 4L);

  static_assert (noexcept (static_cast<std::shared_ptr<T>> (a)), "noexcept");
  static_assert (std::is_same<decltype (static_cast<std::shared_ptr<T>> (a)),
                              std::shared_ptr<T>>::value,
                 "");
}

// --- exchange
// ------------------------------------------------------------------

TYPED_TEST (LumexAtomicSharedPtrTest,
            GivenEveryOrder_WhenExchanging_ThenReturnsThePreviousOwner)
{
  typedef TypeParam T;
  std::shared_ptr<T> const p1 = state_a<T> ();
  std::shared_ptr<T> const p2 = state_b<T> ();
  atomic_shared_ptr<T> a (p1);

  std::shared_ptr<T> out = a.exchange (p2);
  EXPECT_TRUE (same_owner_and_pointer (out, p1));
  EXPECT_TRUE (same_owner_and_pointer (a.load (), p2));
  EXPECT_EQ (p1.use_count (), 2L) << "the returned owner, not a copy";
  EXPECT_EQ (p2.use_count (), 2L);

  std::shared_ptr<T> current = p2;
  std::vector<std::memory_order> const orders = all_orders ();
  for (std::size_t i = 0; i < orders.size (); ++i)
    {
      std::shared_ptr<T> const next = state_c<T> ();
      std::shared_ptr<T> const previous = a.exchange (next, orders[i]);
      EXPECT_TRUE (same_owner_and_pointer (previous, current));
      EXPECT_TRUE (same_owner_and_pointer (a.load (), next));
      current = next;
    }

  out = a.exchange (nullptr, std::memory_order_seq_cst);
  EXPECT_TRUE (same_owner_and_pointer (out, current));
  EXPECT_FALSE (a.load ());
  out = a.exchange (nullptr);
  EXPECT_FALSE (out) << "exchanging an empty value returns an empty one";

  static_assert (
      std::is_same<decltype (a.exchange (nullptr)), std::shared_ptr<T>>::value,
      "returns shared_ptr<T>");
  static_assert (noexcept (a.exchange (nullptr)), "noexcept");
}

TYPED_TEST (LumexAtomicSharedPtrTest,
            GivenTheHeldValue_WhenExchangedWithItself_ThenCountsAreUnchanged)
{
  typedef TypeParam T;
  std::shared_ptr<T> const p = state_a<T> ();
  atomic_shared_ptr<T> a (p);
  {
    std::shared_ptr<T> const previous = a.exchange (a.load ());
    EXPECT_TRUE (same_owner_and_pointer (previous, p));
    EXPECT_EQ (p.use_count (), 3L);
  }
  EXPECT_EQ (p.use_count (), 2L);
}

// --- compare_exchange_strong
// -----------------------------------------------------

TYPED_TEST (
    LumexAtomicSharedPtrTest,
    GivenEquivalentExpected_WhenCompareExchangeStrong_ThenStoresDesired)
{
  typedef TypeParam T;
  std::shared_ptr<T> const p1 = state_a<T> ();
  std::shared_ptr<T> const p2 = state_b<T> ();
  atomic_shared_ptr<T> a ((std::shared_ptr<T> (p1)));

  std::shared_ptr<T> expected = p1;
  bool const ok
      = a.compare_exchange_strong (expected, std::shared_ptr<T> (p2));
  EXPECT_TRUE (ok);
  EXPECT_TRUE (same_owner_and_pointer (expected, p1)) << "expected unchanged";
  EXPECT_TRUE (same_owner_and_pointer (a.load (), p2));
  EXPECT_EQ (p1.use_count (), 2L) << "p1 and expected";
  EXPECT_EQ (p2.use_count (), 2L) << "p2 and the atomic";

  static_assert (std::is_same<decltype (a.compare_exchange_strong (
                                  expected, std::shared_ptr<T> (p2))),
                              bool>::value,
                 "returns bool");
  static_assert (
      noexcept (a.compare_exchange_strong (expected, std::shared_ptr<T> (p2))),
      "noexcept");
  static_assert (
      noexcept (a.compare_exchange_strong (expected, std::shared_ptr<T> (p2),
                                           std::memory_order_seq_cst)),
      "noexcept");
  static_assert (noexcept (a.compare_exchange_strong (
                     expected, std::shared_ptr<T> (p2),
                     std::memory_order_seq_cst, std::memory_order_seq_cst)),
                 "noexcept");
}

TYPED_TEST (LumexAtomicSharedPtrTest,
            GivenNonEquivalentExpected_WhenCompareExchangeStrong_ThenLoadsIt)
{
  typedef TypeParam T;
  std::shared_ptr<T> const p1 = state_a<T> ();
  std::shared_ptr<T> const p2 = state_b<T> ();
  atomic_shared_ptr<T> a ((std::shared_ptr<T> (p2)));

  std::shared_ptr<T> expected = p1;
  std::shared_ptr<T> const desired = state_c<T> ();
  bool const ok = a.compare_exchange_strong (expected, desired);
  EXPECT_FALSE (ok);
  EXPECT_TRUE (same_owner_and_pointer (expected, p2));
  EXPECT_TRUE (*expected == SpValues<T>::b ());
  EXPECT_TRUE (same_owner_and_pointer (a.load (), p2));
  EXPECT_EQ (desired.use_count (), 1L) << "the desired copy was dropped";
  EXPECT_EQ (p1.use_count (), 1L) << "the old expected was released";
  EXPECT_EQ (p2.use_count (), 3L) << "p2, the atomic and expected";
}

TYPED_TEST (LumexAtomicSharedPtrTest,
            GivenEveryOrder_WhenCompareExchangeStrongWithOneOrder_ThenWorks)
{
  typedef TypeParam T;
  atomic_shared_ptr<T> a (state_a<T> ());
  std::vector<std::memory_order> const orders = all_orders ();
  for (std::size_t i = 0; i < orders.size (); ++i)
    {
      std::shared_ptr<T> expected = a.load ();
      std::shared_ptr<T> const next = state_b<T> ();
      EXPECT_TRUE (a.compare_exchange_strong (expected, next, orders[i]));
      EXPECT_TRUE (same_owner_and_pointer (a.load (), next));

      std::shared_ptr<T> stale = state_c<T> ();
      EXPECT_FALSE (
          a.compare_exchange_strong (stale, state_c<T> (), orders[i]));
      EXPECT_TRUE (same_owner_and_pointer (stale, next));
      EXPECT_EQ (next.use_count (), 3L) << "next, the atomic and stale";
    }
}

TYPED_TEST (LumexAtomicSharedPtrTest,
            GivenEveryOrderPair_WhenCompareExchangeStrongWithTwo_ThenWorks)
{
  typedef TypeParam T;
  atomic_shared_ptr<T> a (state_a<T> ());
  std::vector<std::memory_order> const successes = all_orders ();
  std::vector<std::memory_order> const failures = load_orders ();
  for (std::size_t s = 0; s < successes.size (); ++s)
    for (std::size_t f = 0; f < failures.size (); ++f)
      {
        std::shared_ptr<T> expected = a.load ();
        std::shared_ptr<T> const next = state_b<T> ();
        EXPECT_TRUE (a.compare_exchange_strong (expected, next, successes[s],
                                                failures[f]));
        EXPECT_TRUE (same_owner_and_pointer (a.load (), next));

        std::shared_ptr<T> stale = state_c<T> ();
        EXPECT_FALSE (a.compare_exchange_strong (stale, state_c<T> (),
                                                 successes[s], failures[f]));
        EXPECT_TRUE (same_owner_and_pointer (stale, next));
      }
}

// --- compare_exchange_weak
// -------------------------------------------------------

TYPED_TEST (LumexAtomicSharedPtrTest,
            GivenEquivalentExpected_WhenCompareExchangeWeakLoops_ThenStores)
{
  typedef TypeParam T;
  std::shared_ptr<T> const p1 = state_a<T> ();
  std::shared_ptr<T> const p2 = state_b<T> ();
  atomic_shared_ptr<T> a ((std::shared_ptr<T> (p1)));

  std::shared_ptr<T> expected = p1;
  bool ok = false;
  while (!ok)
    ok = a.compare_exchange_weak (expected, std::shared_ptr<T> (p2));
  EXPECT_TRUE (same_owner_and_pointer (a.load (), p2));
  EXPECT_EQ (p1.use_count (), 2L);
  EXPECT_EQ (p2.use_count (), 2L);

  static_assert (std::is_same<decltype (a.compare_exchange_weak (
                                  expected, std::shared_ptr<T> (p2))),
                              bool>::value,
                 "returns bool");
  static_assert (
      noexcept (a.compare_exchange_weak (expected, std::shared_ptr<T> (p2))),
      "noexcept");
  static_assert (
      noexcept (a.compare_exchange_weak (expected, std::shared_ptr<T> (p2),
                                         std::memory_order_acq_rel)),
      "noexcept");
  static_assert (noexcept (a.compare_exchange_weak (
                     expected, std::shared_ptr<T> (p2),
                     std::memory_order_acq_rel, std::memory_order_acquire)),
                 "noexcept");
}

TYPED_TEST (LumexAtomicSharedPtrTest,
            GivenNonEquivalentExpected_WhenCompareExchangeWeak_ThenLoadsIt)
{
  typedef TypeParam T;
  std::shared_ptr<T> const p1 = state_a<T> ();
  std::shared_ptr<T> const p2 = state_b<T> ();
  atomic_shared_ptr<T> a ((std::shared_ptr<T> (p2)));

  std::shared_ptr<T> expected = p1;
  std::shared_ptr<T> const desired = state_c<T> ();
  EXPECT_FALSE (a.compare_exchange_weak (expected, desired));
  EXPECT_TRUE (same_owner_and_pointer (expected, p2));
  EXPECT_TRUE (same_owner_and_pointer (a.load (), p2));
  EXPECT_EQ (desired.use_count (), 1L);
  EXPECT_EQ (p1.use_count (), 1L);
}

TYPED_TEST (LumexAtomicSharedPtrTest,
            GivenEveryOrder_WhenCompareExchangeWeakWithOneOrder_ThenWorks)
{
  typedef TypeParam T;
  atomic_shared_ptr<T> a (state_a<T> ());
  std::vector<std::memory_order> const orders = all_orders ();
  for (std::size_t i = 0; i < orders.size (); ++i)
    {
      std::shared_ptr<T> expected = a.load ();
      std::shared_ptr<T> const next = state_b<T> ();
      bool ok = false;
      while (!ok)
        ok = a.compare_exchange_weak (expected, next, orders[i]);
      EXPECT_TRUE (same_owner_and_pointer (a.load (), next));

      std::shared_ptr<T> stale = state_c<T> ();
      EXPECT_FALSE (a.compare_exchange_weak (stale, state_c<T> (), orders[i]));
      EXPECT_TRUE (same_owner_and_pointer (stale, next));
    }
}

TYPED_TEST (LumexAtomicSharedPtrTest,
            GivenEveryOrderPair_WhenCompareExchangeWeakWithTwo_ThenWorks)
{
  typedef TypeParam T;
  atomic_shared_ptr<T> a (state_a<T> ());
  std::vector<std::memory_order> const successes = all_orders ();
  std::vector<std::memory_order> const failures = load_orders ();
  for (std::size_t s = 0; s < successes.size (); ++s)
    for (std::size_t f = 0; f < failures.size (); ++f)
      {
        std::shared_ptr<T> expected = a.load ();
        std::shared_ptr<T> const next = state_b<T> ();
        bool ok = false;
        while (!ok)
          ok = a.compare_exchange_weak (expected, next, successes[s],
                                        failures[f]);
        EXPECT_TRUE (same_owner_and_pointer (a.load (), next));

        std::shared_ptr<T> stale = state_c<T> ();
        EXPECT_FALSE (a.compare_exchange_weak (stale, state_c<T> (),
                                               successes[s], failures[f]));
        EXPECT_TRUE (same_owner_and_pointer (stale, next));
      }
}

TYPED_TEST (LumexAtomicSharedPtrTest,
            GivenExpectedThatIsAlsoDesired_WhenCompareExchange_ThenBalanced)
{
  typedef TypeParam T;
  std::shared_ptr<T> const p = state_a<T> ();
  atomic_shared_ptr<T> a (p);
  std::shared_ptr<T> expected = p;
  // desired is a copy of expected taken before the call: storing the value
  // already held must leave every count where it was.
  EXPECT_TRUE (a.compare_exchange_strong (expected, expected));
  EXPECT_TRUE (same_owner_and_pointer (a.load (), p));
  EXPECT_EQ (p.use_count (), 3L) << "p, the atomic and expected";

  std::shared_ptr<T> other = state_b<T> ();
  EXPECT_FALSE (a.compare_exchange_strong (other, other));
  EXPECT_TRUE (same_owner_and_pointer (other, p));
  EXPECT_EQ (p.use_count (), 4L);
}

TYPED_TEST (LumexAtomicSharedPtrTest,
            GivenAnEqualValueInAnotherObject_WhenCompareExchange_ThenFails)
{
  typedef TypeParam T;
  std::shared_ptr<T> const held = state_a<T> ();
  atomic_shared_ptr<T> a (held);
  std::shared_ptr<T> equal_value = state_a<T> ();
  ASSERT_TRUE (*equal_value == *held);
  EXPECT_FALSE (a.compare_exchange_strong (equal_value, state_b<T> ()));
  EXPECT_TRUE (same_owner_and_pointer (equal_value, held))
      << "equivalence is by stored pointer and owner, not by value";
  std::shared_ptr<T> equal_again = state_a<T> ();
  EXPECT_FALSE (a.compare_exchange_weak (equal_again, state_b<T> ()));
  EXPECT_TRUE (same_owner_and_pointer (a.load (), held));
}

// --- wait / notify_one / notify_all
// ------------------------------------------------

TYPED_TEST (LumexAtomicSharedPtrTest,
            GivenANonEquivalentValue_WhenWaitingWithEveryOrder_ThenReturns)
{
  typedef TypeParam T;
  Watchdog const dog ("GivenANonEquivalentValue_WhenWaiting");
  std::shared_ptr<T> const p = state_a<T> ();
  atomic_shared_ptr<T> const a (p);
  a.wait (nullptr);
  a.wait (state_a<T> ());
  std::vector<std::memory_order> const orders = load_orders ();
  for (std::size_t i = 0; i < orders.size (); ++i)
    a.wait (std::shared_ptr<T> (), orders[i]);
  EXPECT_EQ (p.use_count (), 2L) << "waiting keeps no extra owner";

  static_assert (
      std::is_same<decltype (a.wait (std::shared_ptr<T> ())), void>::value,
      "returns void");
  static_assert (
      noexcept (a.wait (std::shared_ptr<T> (), std::memory_order_seq_cst)),
      "noexcept");
}

TYPED_TEST (LumexAtomicSharedPtrTest,
            GivenAnEquivalentValue_WhenWaiting_ThenBlocksUntilNotifiedChange)
{
  typedef TypeParam T;
  Watchdog const dog ("GivenAnEquivalentValue_WhenWaiting");
  atomic_shared_ptr<T> a (state_a<T> ());
  std::atomic<bool> started (false);
  std::atomic<bool> woke (false);
  std::thread waiter (
      [&]
        {
          std::shared_ptr<T> const old = a.load ();
          started.store (true, std::memory_order_release);
          a.wait (old);
          woke.store (true, std::memory_order_release);
        });

  wait_for_flag (started);
  sleep_ms (20);
  EXPECT_FALSE (woke.load ()) << "wait returned while the value was equal";
  a.store (state_c<T> ());
  a.notify_all ();
  waiter.join ();
  EXPECT_TRUE (woke.load ());
  EXPECT_TRUE (*a.load () == SpValues<T>::c ());
}

TYPED_TEST (LumexAtomicSharedPtrTest, GivenOneWaiter_WhenNotifyOne_ThenItWakes)
{
  typedef TypeParam T;
  Watchdog const dog ("GivenOneWaiter_WhenNotifyOne_ThenItWakes");
  atomic_shared_ptr<T> a (state_a<T> ());
  static_assert (noexcept (a.notify_one ()), "noexcept");
  static_assert (std::is_same<decltype (a.notify_one ()), void>::value,
                 "returns void");

  std::atomic<bool> started (false);
  std::thread waiter (
      [&]
        {
          std::shared_ptr<T> const old = a.load ();
          started.store (true, std::memory_order_release);
          a.wait (old);
        });
  wait_for_flag (started);
  sleep_ms (10);
  a.store (state_b<T> ());
  a.notify_one ();
  waiter.join ();
  EXPECT_TRUE (*a.load () == SpValues<T>::b ());
}

TYPED_TEST (LumexAtomicSharedPtrTest,
            GivenSeveralWaiters_WhenNotifyAll_ThenEveryOneWakes)
{
  typedef TypeParam T;
  Watchdog const dog ("GivenSeveralWaiters_WhenNotifyAll_ThenEveryOneWakes");
  atomic_shared_ptr<T> a (state_a<T> ());
  static_assert (noexcept (a.notify_all ()), "noexcept");
  static_assert (std::is_same<decltype (a.notify_all ()), void>::value,
                 "returns void");

  int const waiter_count = 4;
  std::atomic<int> ready (0);
  std::atomic<int> woke (0);
  std::vector<std::thread> waiters;
  for (int i = 0; i < waiter_count; ++i)
    waiters.push_back (std::thread (
        [&]
          {
            std::shared_ptr<T> const old = a.load ();
            ready.fetch_add (1, std::memory_order_acq_rel);
            a.wait (old);
            woke.fetch_add (1, std::memory_order_acq_rel);
          }));
  wait_for_count (ready, waiter_count);
  sleep_ms (10);
  EXPECT_EQ (woke.load (), 0);
  a.store (state_b<T> ());
  a.notify_all ();
  join_all (waiters);
  EXPECT_EQ (woke.load (), waiter_count);
}

TYPED_TEST (LumexAtomicSharedPtrTest,
            GivenNoWaiter_WhenNotifying_ThenTheValueIsUntouched)
{
  typedef TypeParam T;
  std::shared_ptr<T> const p = state_a<T> ();
  atomic_shared_ptr<T> a (p);
  for (int i = 0; i < 10; ++i)
    {
      a.notify_one ();
      a.notify_all ();
    }
  EXPECT_TRUE (same_owner_and_pointer (a.load (), p));
  EXPECT_EQ (p.use_count (), 2L);
}
