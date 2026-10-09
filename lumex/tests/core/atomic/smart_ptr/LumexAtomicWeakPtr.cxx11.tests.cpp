// Per-member tests of atomic_weak_ptr<T>, typed over six value types.
// Started from the libc++ tests util.smartptr/atomic/weak/*.pass.cpp of
// llvm-project pull request 194215 (the author's own libc++ implementation of
// P0718R2) and extended to the whole public surface: every constructor and
// assignment, every overload of every member with every valid memory order
// on both outcomes, the weak references held around each operation, and the
// members available on a const object.

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

template <typename T> class LumexAtomicWeakPtrTest : public ::testing::Test
{
};

TYPED_TEST_SUITE (LumexAtomicWeakPtrTest, ValueTypes);

// --- type surface
// ------------------------------------------------------------

TYPED_TEST (LumexAtomicWeakPtrTest,
            GivenTheType_WhenInspected_ThenMatchesTheStdInterface)
{
  typedef TypeParam T;
  typedef atomic_weak_ptr<T> A;

  static_assert (std::is_same<typename A::value_type, std::weak_ptr<T>>::value,
                 "value_type");
  static_assert (
      std::is_same<decltype (A::is_always_lock_free), bool const>::value,
      "is_always_lock_free is a static constexpr bool");
  static_assert (!std::is_copy_constructible<A>::value, "no copy");
  static_assert (!std::is_copy_assignable<A>::value, "no copy assignment");
  static_assert (!std::is_move_constructible<A>::value, "no move");
  static_assert (!std::is_move_assignable<A>::value, "no move assignment");
  static_assert (std::is_nothrow_default_constructible<A>::value, "");
  static_assert (std::is_nothrow_constructible<A, std::weak_ptr<T>>::value,
                 "");
  static_assert (std::is_nothrow_constructible<A, std::shared_ptr<T>>::value,
                 "a shared_ptr converts to the weak_ptr argument");
  static_assert (std::is_nothrow_destructible<A>::value, "");
  // Unlike atomic<shared_ptr<T>>: no nullptr constructor or assignment.
  static_assert (!std::is_constructible<A, std::nullptr_t>::value, "");
  static_assert (!std::is_assignable<A &, std::nullptr_t>::value, "");
  static_assert (std::is_assignable<A &, std::weak_ptr<T>>::value, "");
  static_assert (std::is_convertible<A const &, std::weak_ptr<T>>::value,
                 "implicit conversion to weak_ptr");
  static_assert (
      std::is_same<A,
                   lumex::core::atomic::smart_ptr::atomic_weak_ptr<T>>::value,
      "the short name is the namespace class");
  SUCCEED ();
}

TYPED_TEST (LumexAtomicWeakPtrTest,
            GivenAConstObject_WhenCallingMembers_ThenOnlyObserversCompile)
{
  typedef atomic_weak_ptr<TypeParam> A;
  typedef A const C;

  static_assert (can_load<C>::value && can_load<A>::value, "load is const");
  static_assert (can_wait<C>::value && can_wait<A>::value, "wait is const");
  static_assert (can_is_lock_free<C>::value, "is_lock_free is const");
  static_assert (!can_store<C>::value && can_store<A>::value, "");
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
  static_assert (!can_compare_exchange_with_rvalue_expected<A>::value, "");
  SUCCEED ();
}

// --- constructors and destructor -------------------------------------------

TYPED_TEST (LumexAtomicWeakPtrTest,
            GivenDefaultConstruction_WhenLoaded_ThenEmptyAndExpired)
{
  atomic_weak_ptr<TypeParam> const a;
  std::weak_ptr<TypeParam> const loaded = a.load ();
  EXPECT_TRUE (loaded.expired ());
  EXPECT_EQ (loaded.use_count (), 0L);
  EXPECT_TRUE (same_owner (loaded, std::weak_ptr<TypeParam> ()))
      << "empty, not merely expired";
}

TYPED_TEST (LumexAtomicWeakPtrTest,
            GivenAWeakPointer_WhenConstructedFromIt_ThenRefersToTheObject)
{
  typedef TypeParam T;
  std::shared_ptr<T> const sp = state_a<T> ();
  std::weak_ptr<T> const wp = sp;
  atomic_weak_ptr<T> const a (wp);
  EXPECT_TRUE (refers_to (a.load (), sp));
  EXPECT_TRUE (*a.load ().lock () == SpValues<T>::a ());
  EXPECT_EQ (sp.use_count (), 1L) << "a weak pointer is no owner";

  atomic_weak_ptr<T> const from_shared (sp);
  EXPECT_TRUE (refers_to (from_shared.load (), sp));
}

TYPED_TEST (LumexAtomicWeakPtrTest,
            GivenAnExpiredWeakPointer_WhenConstructedFromIt_ThenStaysExpired)
{
  typedef TypeParam T;
  std::weak_ptr<T> expired;
  {
    std::shared_ptr<T> const sp = state_a<T> ();
    expired = sp;
  }
  atomic_weak_ptr<T> const a (expired);
  std::weak_ptr<T> const loaded = a.load ();
  EXPECT_TRUE (loaded.expired ());
  EXPECT_TRUE (same_owner (loaded, expired)) << "keeps the control block";
}

// --- is_lock_free / is_always_lock_free
// --------------------------------------

TYPED_TEST (LumexAtomicWeakPtrTest,
            GivenIsAlwaysLockFree_WhenBoundToAReference_ThenAConstantBool)
{
  typedef atomic_weak_ptr<TypeParam> A;
  // The lock-free engine always is; the lock-based one never; the wrapper
  // has the standard type's value.
  if (EngineFacts::std_backed ())
    {
#if LUMEX_ATOMIC_SMART_PTR_HAS_STD_BACKED
      EXPECT_EQ (A::is_always_lock_free,
                 std::atomic<std::weak_ptr<TypeParam>>::is_always_lock_free);
#endif
    }
  else
    {
      EXPECT_EQ (A::is_always_lock_free, EngineFacts::lock_free ());
    }
  bool const &bound = A::is_always_lock_free;
  EXPECT_EQ (bound, A::is_always_lock_free);
}

TYPED_TEST (LumexAtomicWeakPtrTest,
            GivenAnObject_WhenAskingIsLockFree_ThenConsistentWithTheType)
{
  typedef atomic_weak_ptr<TypeParam> A;
  A const a;
  static_assert (std::is_same<decltype (a.is_lock_free ()), bool>::value,
                 "returns bool");
  static_assert (noexcept (a.is_lock_free ()), "noexcept");
  bool const lock_free = a.is_lock_free ();
  if (A::is_always_lock_free)
    EXPECT_TRUE (lock_free);
#if LUMEX_ATOMIC_SMART_PTR_HAS_STD_BACKED
  if (EngineFacts::std_backed ())
    {
      std::atomic<std::weak_ptr<TypeParam>> const standard;
      EXPECT_EQ (lock_free, standard.is_lock_free ());
    }
  else
#endif
    EXPECT_EQ (lock_free, EngineFacts::lock_free ());
}

// --- load
// ----------------------------------------------------------------------

TYPED_TEST (LumexAtomicWeakPtrTest,
            GivenEveryLoadOrder_WhenLoading_ThenReturnsACopy)
{
  typedef TypeParam T;
  std::shared_ptr<T> const sp = state_a<T> ();
  atomic_weak_ptr<T> const a ((std::weak_ptr<T> (sp)));
  static_assert (std::is_same<decltype (a.load ()), std::weak_ptr<T>>::value,
                 "returns weak_ptr<T>");
  static_assert (noexcept (a.load ()), "noexcept");
  static_assert (noexcept (a.load (std::memory_order_acquire)), "noexcept");

  std::vector<std::memory_order> const orders = load_orders ();
  for (std::size_t i = 0; i < orders.size (); ++i)
    {
      std::weak_ptr<T> const loaded = a.load (orders[i]);
      EXPECT_TRUE (refers_to (loaded, sp));
    }
  EXPECT_EQ (sp.use_count (), 1L) << "loads take weak references only";
}

TYPED_TEST (LumexAtomicWeakPtrTest,
            GivenAnEmptyObject_WhenLoadingWithEveryOrder_ThenReturnsEmpty)
{
  atomic_weak_ptr<TypeParam> const a;
  std::vector<std::memory_order> const orders = load_orders ();
  for (std::size_t i = 0; i < orders.size (); ++i)
    {
      std::weak_ptr<TypeParam> const loaded = a.load (orders[i]);
      EXPECT_TRUE (loaded.expired ());
      EXPECT_FALSE (loaded.lock ());
    }
}

// --- store
// -----------------------------------------------------------------------

TYPED_TEST (LumexAtomicWeakPtrTest,
            GivenEveryStoreOrder_WhenStoring_ThenReplacesTheValue)
{
  typedef TypeParam T;
  std::shared_ptr<T> const sp1 = state_a<T> ();
  atomic_weak_ptr<T> a;
  a.store (std::weak_ptr<T> (sp1));
  EXPECT_TRUE (refers_to (a.load (), sp1));

  std::vector<std::memory_order> const orders = store_orders ();
  std::vector<std::shared_ptr<T>> keep;
  for (std::size_t i = 0; i < orders.size (); ++i)
    {
      keep.push_back (state_b<T> ());
      a.store (std::weak_ptr<T> (keep.back ()), orders[i]);
      EXPECT_TRUE (refers_to (a.load (), keep.back ()));
      EXPECT_EQ (keep.back ().use_count (), 1L);
    }

  a.store (std::weak_ptr<T> (), std::memory_order_relaxed);
  EXPECT_TRUE (a.load ().expired ());

  static_assert (
      std::is_same<decltype (a.store (std::weak_ptr<T> ())), void>::value,
      "returns void");
  static_assert (noexcept (a.store (std::weak_ptr<T> ())), "noexcept");
  static_assert (
      noexcept (a.store (std::weak_ptr<T> (), std::memory_order_release)),
      "noexcept");
}

TYPED_TEST (LumexAtomicWeakPtrTest,
            GivenTheHeldValue_WhenStoredAgain_ThenStillRefersToTheObject)
{
  typedef TypeParam T;
  std::shared_ptr<T> const sp = state_a<T> ();
  atomic_weak_ptr<T> a ((std::weak_ptr<T> (sp)));
  for (int i = 0; i < 3; ++i)
    a.store (a.load ());
  EXPECT_TRUE (refers_to (a.load (), sp));
  EXPECT_EQ (sp.use_count (), 1L);
}

// --- assignment and conversion
// -----------------------------------------------

TYPED_TEST (LumexAtomicWeakPtrTest,
            GivenWeakPointers_WhenAssigned_ThenStoredWithVoidResult)
{
  typedef TypeParam T;
  std::shared_ptr<T> const sp1 = state_a<T> ();
  std::shared_ptr<T> const sp2 = state_b<T> ();
  std::weak_ptr<T> const wp1 = sp1;

  atomic_weak_ptr<T> a;
  a = wp1;
  EXPECT_TRUE (refers_to (a.load (), sp1));
  a = std::weak_ptr<T> (sp2);
  EXPECT_TRUE (refers_to (a.load (), sp2));
  EXPECT_TRUE (*a.load ().lock () == SpValues<T>::b ());
  a = std::weak_ptr<T> ();
  EXPECT_TRUE (a.load ().expired ());

  static_assert (std::is_same<decltype (a = wp1), void>::value, "void");
  static_assert (noexcept (a = std::weak_ptr<T> (wp1)), "noexcept");
}

TYPED_TEST (LumexAtomicWeakPtrTest,
            GivenAConstObject_WhenConverted_ThenReturnsACopy)
{
  typedef TypeParam T;
  std::shared_ptr<T> const sp = state_a<T> ();
  atomic_weak_ptr<T> const a ((std::weak_ptr<T> (sp)));

  std::weak_ptr<T> const implicit = a;
  EXPECT_TRUE (refers_to (implicit, sp));
  std::weak_ptr<T> const explicit_cast = static_cast<std::weak_ptr<T>> (a);
  EXPECT_TRUE (refers_to (explicit_cast, sp));

  static_assert (noexcept (static_cast<std::weak_ptr<T>> (a)), "noexcept");
}

// --- exchange
// ------------------------------------------------------------------

TYPED_TEST (LumexAtomicWeakPtrTest,
            GivenEveryOrder_WhenExchanging_ThenReturnsThePreviousValue)
{
  typedef TypeParam T;
  std::shared_ptr<T> const sp1 = state_a<T> ();
  std::shared_ptr<T> const sp2 = state_b<T> ();
  atomic_weak_ptr<T> a ((std::weak_ptr<T> (sp1)));

  std::weak_ptr<T> out = a.exchange (std::weak_ptr<T> (sp2));
  EXPECT_TRUE (refers_to (out, sp1));
  EXPECT_TRUE (refers_to (a.load (), sp2));

  std::shared_ptr<T> current = sp2;
  std::vector<std::shared_ptr<T>> keep;
  std::vector<std::memory_order> const orders = all_orders ();
  for (std::size_t i = 0; i < orders.size (); ++i)
    {
      keep.push_back (state_c<T> ());
      std::weak_ptr<T> const previous
          = a.exchange (std::weak_ptr<T> (keep.back ()), orders[i]);
      EXPECT_TRUE (refers_to (previous, current));
      EXPECT_TRUE (refers_to (a.load (), keep.back ()));
      current = keep.back ();
    }

  out = a.exchange (std::weak_ptr<T> (), std::memory_order_seq_cst);
  EXPECT_TRUE (refers_to (out, current));
  EXPECT_TRUE (a.load ().expired ());

  static_assert (std::is_same<decltype (a.exchange (std::weak_ptr<T> ())),
                              std::weak_ptr<T>>::value,
                 "returns weak_ptr<T>");
  static_assert (noexcept (a.exchange (std::weak_ptr<T> ())), "noexcept");
}

// --- compare_exchange_strong
// -----------------------------------------------------

TYPED_TEST (
    LumexAtomicWeakPtrTest,
    GivenEquivalentExpected_WhenCompareExchangeStrong_ThenStoresDesired)
{
  typedef TypeParam T;
  std::shared_ptr<T> const sp1 = state_a<T> ();
  std::shared_ptr<T> const sp2 = state_b<T> ();
  std::weak_ptr<T> const wp1 = sp1;
  std::weak_ptr<T> const wp2 = sp2;
  atomic_weak_ptr<T> a ((std::weak_ptr<T> (wp1)));

  std::weak_ptr<T> expected = wp1;
  EXPECT_TRUE (a.compare_exchange_strong (expected, std::weak_ptr<T> (wp2)));
  EXPECT_TRUE (refers_to (expected, sp1)) << "expected unchanged";
  EXPECT_TRUE (refers_to (a.load (), sp2));
  EXPECT_EQ (sp1.use_count (), 1L) << "the comparison keeps no owner";
  EXPECT_EQ (sp2.use_count (), 1L);

  static_assert (std::is_same<decltype (a.compare_exchange_strong (
                                  expected, std::weak_ptr<T> (wp2))),
                              bool>::value,
                 "returns bool");
  static_assert (
      noexcept (a.compare_exchange_strong (expected, std::weak_ptr<T> (wp2))),
      "noexcept");
  static_assert (
      noexcept (a.compare_exchange_strong (expected, std::weak_ptr<T> (wp2),
                                           std::memory_order_seq_cst)),
      "noexcept");
  static_assert (noexcept (a.compare_exchange_strong (
                     expected, std::weak_ptr<T> (wp2),
                     std::memory_order_seq_cst, std::memory_order_seq_cst)),
                 "noexcept");
}

TYPED_TEST (LumexAtomicWeakPtrTest,
            GivenNonEquivalentExpected_WhenCompareExchangeStrong_ThenLoadsIt)
{
  typedef TypeParam T;
  std::shared_ptr<T> const sp1 = state_a<T> ();
  std::shared_ptr<T> const sp2 = state_b<T> ();
  atomic_weak_ptr<T> a ((std::weak_ptr<T> (sp2)));

  std::weak_ptr<T> expected = sp1;
  EXPECT_FALSE (a.compare_exchange_strong (expected, std::weak_ptr<T> (sp1)));
  EXPECT_TRUE (refers_to (expected, sp2));
  EXPECT_TRUE (*expected.lock () == SpValues<T>::b ());
  EXPECT_TRUE (refers_to (a.load (), sp2));

  std::weak_ptr<T> empty;
  EXPECT_FALSE (a.compare_exchange_strong (empty, std::weak_ptr<T> (sp1)))
      << "an empty expected value is not equivalent to a live one";
  EXPECT_TRUE (refers_to (empty, sp2));
}

TYPED_TEST (LumexAtomicWeakPtrTest,
            GivenEveryOrder_WhenCompareExchangeStrongWithOneOrder_ThenWorks)
{
  typedef TypeParam T;
  std::vector<std::shared_ptr<T>> keep;
  keep.push_back (state_a<T> ());
  atomic_weak_ptr<T> a ((std::weak_ptr<T> (keep.back ())));
  std::vector<std::memory_order> const orders = all_orders ();
  for (std::size_t i = 0; i < orders.size (); ++i)
    {
      std::weak_ptr<T> expected = a.load ();
      keep.push_back (state_b<T> ());
      std::shared_ptr<T> const next = keep.back ();
      EXPECT_TRUE (a.compare_exchange_strong (
          expected, std::weak_ptr<T> (next), orders[i]));
      EXPECT_TRUE (refers_to (a.load (), next));

      std::shared_ptr<T> const other = state_c<T> ();
      std::weak_ptr<T> stale = other;
      EXPECT_FALSE (a.compare_exchange_strong (stale, std::weak_ptr<T> (other),
                                               orders[i]));
      EXPECT_TRUE (refers_to (stale, next));
    }
}

TYPED_TEST (LumexAtomicWeakPtrTest,
            GivenEveryOrderPair_WhenCompareExchangeStrongWithTwo_ThenWorks)
{
  typedef TypeParam T;
  std::vector<std::shared_ptr<T>> keep;
  keep.push_back (state_a<T> ());
  atomic_weak_ptr<T> a ((std::weak_ptr<T> (keep.back ())));
  std::vector<std::memory_order> const successes = all_orders ();
  std::vector<std::memory_order> const failures = load_orders ();
  for (std::size_t s = 0; s < successes.size (); ++s)
    for (std::size_t f = 0; f < failures.size (); ++f)
      {
        std::weak_ptr<T> expected = a.load ();
        keep.push_back (state_b<T> ());
        std::shared_ptr<T> const next = keep.back ();
        EXPECT_TRUE (a.compare_exchange_strong (
            expected, std::weak_ptr<T> (next), successes[s], failures[f]));
        EXPECT_TRUE (refers_to (a.load (), next));

        std::shared_ptr<T> const other = state_c<T> ();
        std::weak_ptr<T> stale = other;
        EXPECT_FALSE (a.compare_exchange_strong (
            stale, std::weak_ptr<T> (other), successes[s], failures[f]));
        EXPECT_TRUE (refers_to (stale, next));
      }
}

// --- compare_exchange_weak
// -------------------------------------------------------

TYPED_TEST (LumexAtomicWeakPtrTest,
            GivenEquivalentExpected_WhenCompareExchangeWeakLoops_ThenStores)
{
  typedef TypeParam T;
  std::shared_ptr<T> const sp1 = state_a<T> ();
  std::shared_ptr<T> const sp2 = state_b<T> ();
  std::shared_ptr<T> const sp3 = state_c<T> ();
  atomic_weak_ptr<T> a ((std::weak_ptr<T> (sp1)));

  std::weak_ptr<T> expected = sp1;
  bool ok = false;
  while (!ok)
    ok = a.compare_exchange_weak (expected, std::weak_ptr<T> (sp2));
  EXPECT_TRUE (refers_to (a.load (), sp2));

  expected = sp2;
  ok = false;
  while (!ok)
    ok = a.compare_exchange_weak (expected, std::weak_ptr<T> (sp3),
                                  std::memory_order_release,
                                  std::memory_order_relaxed);
  EXPECT_TRUE (refers_to (a.load (), sp3));

  static_assert (std::is_same<decltype (a.compare_exchange_weak (
                                  expected, std::weak_ptr<T> (sp2))),
                              bool>::value,
                 "returns bool");
  static_assert (
      noexcept (a.compare_exchange_weak (expected, std::weak_ptr<T> (sp2))),
      "noexcept");
  static_assert (
      noexcept (a.compare_exchange_weak (expected, std::weak_ptr<T> (sp2),
                                         std::memory_order_acq_rel)),
      "noexcept");
  static_assert (noexcept (a.compare_exchange_weak (
                     expected, std::weak_ptr<T> (sp2),
                     std::memory_order_acq_rel, std::memory_order_acquire)),
                 "noexcept");
}

TYPED_TEST (LumexAtomicWeakPtrTest,
            GivenNonEquivalentExpected_WhenCompareExchangeWeak_ThenLoadsIt)
{
  typedef TypeParam T;
  std::shared_ptr<T> const sp1 = state_a<T> ();
  std::shared_ptr<T> const sp2 = state_b<T> ();
  atomic_weak_ptr<T> a ((std::weak_ptr<T> (sp2)));

  std::weak_ptr<T> expected = sp1;
  EXPECT_FALSE (a.compare_exchange_weak (expected, std::weak_ptr<T> (sp1)));
  EXPECT_TRUE (refers_to (expected, sp2));
  EXPECT_TRUE (refers_to (a.load (), sp2));
}

TYPED_TEST (LumexAtomicWeakPtrTest,
            GivenEveryOrder_WhenCompareExchangeWeakWithOneOrder_ThenWorks)
{
  typedef TypeParam T;
  std::vector<std::shared_ptr<T>> keep;
  keep.push_back (state_a<T> ());
  atomic_weak_ptr<T> a ((std::weak_ptr<T> (keep.back ())));
  std::vector<std::memory_order> const orders = all_orders ();
  for (std::size_t i = 0; i < orders.size (); ++i)
    {
      std::weak_ptr<T> expected = a.load ();
      keep.push_back (state_b<T> ());
      std::shared_ptr<T> const next = keep.back ();
      bool ok = false;
      while (!ok)
        ok = a.compare_exchange_weak (expected, std::weak_ptr<T> (next),
                                      orders[i]);
      EXPECT_TRUE (refers_to (a.load (), next));

      std::shared_ptr<T> const other = state_c<T> ();
      std::weak_ptr<T> stale = other;
      EXPECT_FALSE (a.compare_exchange_weak (stale, std::weak_ptr<T> (other),
                                             orders[i]));
      EXPECT_TRUE (refers_to (stale, next));
    }
}

TYPED_TEST (LumexAtomicWeakPtrTest,
            GivenEveryOrderPair_WhenCompareExchangeWeakWithTwo_ThenWorks)
{
  typedef TypeParam T;
  std::vector<std::shared_ptr<T>> keep;
  keep.push_back (state_a<T> ());
  atomic_weak_ptr<T> a ((std::weak_ptr<T> (keep.back ())));
  std::vector<std::memory_order> const successes = all_orders ();
  std::vector<std::memory_order> const failures = load_orders ();
  for (std::size_t s = 0; s < successes.size (); ++s)
    for (std::size_t f = 0; f < failures.size (); ++f)
      {
        std::weak_ptr<T> expected = a.load ();
        keep.push_back (state_b<T> ());
        std::shared_ptr<T> const next = keep.back ();
        bool ok = false;
        while (!ok)
          ok = a.compare_exchange_weak (expected, std::weak_ptr<T> (next),
                                        successes[s], failures[f]);
        EXPECT_TRUE (refers_to (a.load (), next));

        std::shared_ptr<T> const other = state_c<T> ();
        std::weak_ptr<T> stale = other;
        EXPECT_FALSE (a.compare_exchange_weak (stale, std::weak_ptr<T> (other),
                                               successes[s], failures[f]));
        EXPECT_TRUE (refers_to (stale, next));
      }
}

// --- wait / notify_one / notify_all
// ------------------------------------------------

TYPED_TEST (LumexAtomicWeakPtrTest,
            GivenANonEquivalentValue_WhenWaitingWithEveryOrder_ThenReturns)
{
  typedef TypeParam T;
  Watchdog const dog ("GivenANonEquivalentValue_WhenWaiting");
  std::shared_ptr<T> const sp = state_a<T> ();
  std::shared_ptr<T> const other = state_a<T> ();
  atomic_weak_ptr<T> const a ((std::weak_ptr<T> (sp)));
  a.wait (std::weak_ptr<T> ());
  a.wait (std::weak_ptr<T> (other));
  std::vector<std::memory_order> const orders = load_orders ();
  for (std::size_t i = 0; i < orders.size (); ++i)
    a.wait (std::weak_ptr<T> (other), orders[i]);
  EXPECT_EQ (sp.use_count (), 1L);

  static_assert (
      std::is_same<decltype (a.wait (std::weak_ptr<T> ())), void>::value,
      "returns void");
  static_assert (
      noexcept (a.wait (std::weak_ptr<T> (), std::memory_order_seq_cst)),
      "noexcept");
}

TYPED_TEST (LumexAtomicWeakPtrTest,
            GivenAnEquivalentValue_WhenWaiting_ThenBlocksUntilNotifiedChange)
{
  typedef TypeParam T;
  Watchdog const dog ("GivenAnEquivalentValue_WhenWaiting");
  std::shared_ptr<T> const sp_for_wait = state_a<T> ();
  std::shared_ptr<T> const sp1 = state_b<T> ();
  atomic_weak_ptr<T> a ((std::weak_ptr<T> (sp_for_wait)));
  std::atomic<bool> started (false);
  std::atomic<bool> woke (false);
  std::thread waiter (
      [&]
        {
          std::weak_ptr<T> const old = a.load ();
          started.store (true, std::memory_order_release);
          a.wait (old);
          woke.store (true, std::memory_order_release);
        });

  wait_for_flag (started);
  sleep_ms (20);
  EXPECT_FALSE (woke.load ()) << "wait returned while the value was equal";
  EXPECT_EQ (sp_for_wait.use_count (), 1L) << "a sleeping wait keeps no owner";
  a.store (std::weak_ptr<T> (sp1));
  a.notify_all ();
  waiter.join ();
  EXPECT_TRUE (woke.load ());
  EXPECT_TRUE (refers_to (a.load (), sp1));
}

TYPED_TEST (LumexAtomicWeakPtrTest, GivenOneWaiter_WhenNotifyOne_ThenItWakes)
{
  typedef TypeParam T;
  Watchdog const dog ("GivenOneWaiter_WhenNotifyOne_ThenItWakes");
  std::shared_ptr<T> const sp1 = state_a<T> ();
  std::shared_ptr<T> const sp2 = state_b<T> ();
  atomic_weak_ptr<T> a ((std::weak_ptr<T> (sp1)));
  static_assert (noexcept (a.notify_one ()), "noexcept");

  std::atomic<bool> started (false);
  std::thread waiter (
      [&]
        {
          std::weak_ptr<T> const old = a.load ();
          started.store (true, std::memory_order_release);
          a.wait (old);
        });
  wait_for_flag (started);
  sleep_ms (10);
  a.store (std::weak_ptr<T> (sp2));
  a.notify_one ();
  waiter.join ();
  EXPECT_TRUE (refers_to (a.load (), sp2));
}

TYPED_TEST (LumexAtomicWeakPtrTest,
            GivenSeveralWaiters_WhenNotifyAll_ThenEveryOneWakes)
{
  typedef TypeParam T;
  Watchdog const dog ("GivenSeveralWaiters_WhenNotifyAll_ThenEveryOneWakes");
  std::shared_ptr<T> const sp1 = state_a<T> ();
  std::shared_ptr<T> const sp2 = state_b<T> ();
  atomic_weak_ptr<T> a ((std::weak_ptr<T> (sp1)));
  static_assert (noexcept (a.notify_all ()), "noexcept");

  int const waiter_count = 4;
  std::atomic<int> ready (0);
  std::atomic<int> woke (0);
  std::vector<std::thread> waiters;
  for (int i = 0; i < waiter_count; ++i)
    waiters.push_back (std::thread (
        [&]
          {
            std::weak_ptr<T> const old = a.load ();
            ready.fetch_add (1, std::memory_order_acq_rel);
            a.wait (old);
            woke.fetch_add (1, std::memory_order_acq_rel);
          }));
  wait_for_count (ready, waiter_count);
  sleep_ms (10);
  EXPECT_EQ (woke.load (), 0);
  a.store (std::weak_ptr<T> (sp2));
  a.notify_all ();
  join_all (waiters);
  EXPECT_EQ (woke.load (), waiter_count);
}
