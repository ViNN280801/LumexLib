// Implementation selection of the atomic smart pointers: the documented rule
// behind LUMEX_ATOMIC_SMART_PTR_USES_STD and LUMEX_ATOMIC_WAIT_USES_STD, the
// inline namespace of each combination, constant initialization (LWG 3661),
// and, where the standard library has std::atomic<std::shared_ptr<T>>, a
// differential run of the same scenarios on both types.

#include <atomic>
#include <cstddef>
#include <iostream>
#include <memory>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/atomic/LumexAtomic"
#include "lumex/core/atomic/sync/LumexBitLock.hpp"
#include "lumex/tests/core/atomic/LumexAtomicTestSupport.hpp"

using namespace lumex_atomic_test;

namespace
{
struct Pair
{
  int a;
  int b;
};

int g_value = 3;

#if defined(__cpp_constinit)
// LWG 3661: constant initialization from nullptr and by default.
constinit atomic_shared_ptr<int> g_constinit_shared (nullptr);
constinit atomic_shared_ptr<int> g_constinit_default;
constinit atomic_weak_ptr<int> g_constinit_weak;
#endif
} // namespace

TEST (LumexAtomicSmartPtrConfigTest,
      GivenTheBuild_WhenSelectingTheImplementation_ThenTheDocumentedRuleHolds)
{
#if defined(LUMEX_FORCE_LOCK_BASED_ATOMIC_SHARED_PTR)
  EXPECT_EQ (LUMEX_ATOMIC_SMART_PTR_USES_STD, 0);
#elif LUMEX_HAS_STD_ATOMIC_SHARED_PTR
  EXPECT_EQ (LUMEX_ATOMIC_SMART_PTR_USES_STD, 1);
#else
  EXPECT_EQ (LUMEX_ATOMIC_SMART_PTR_USES_STD, 0);
#endif

#if defined(LUMEX_FORCE_ATOMIC_WAIT_TABLE)
  EXPECT_EQ (LUMEX_ATOMIC_WAIT_USES_STD, 0);
#elif LUMEX_HAS_STD_ATOMIC_WAIT
  EXPECT_EQ (LUMEX_ATOMIC_WAIT_USES_STD, 1);
#else
  EXPECT_EQ (LUMEX_ATOMIC_WAIT_USES_STD, 0);
#endif

#if __cplusplus < 202002L
  EXPECT_EQ (LUMEX_ATOMIC_SMART_PTR_USES_STD, 0)
      << "before C++20 only the lock-based form exists";
  EXPECT_EQ (LUMEX_ATOMIC_WAIT_USES_STD, 0)
      << "before C++20 only the table exists";
#endif
}

TEST (
    LumexAtomicSmartPtrConfigTest,
    GivenTheSelection_WhenNamingTheTypes_ThenTheMatchingAbiNamespaceHoldsThem)
{
  namespace smart_ptr = lumex::core::atomic::smart_ptr;
#if LUMEX_ATOMIC_SMART_PTR_USES_STD && LUMEX_ATOMIC_WAIT_USES_STD
  static_assert (
      std::is_same<
          atomic_shared_ptr<int>,
          smart_ptr::std_backed_std_wait::atomic_shared_ptr<int>>::value,
      "std_backed_std_wait");
  static_assert (
      std::is_same<
          atomic_weak_ptr<int>,
          smart_ptr::std_backed_std_wait::atomic_weak_ptr<int>>::value,
      "std_backed_std_wait");
  char const *const selected = "std_backed_std_wait";
#elif LUMEX_ATOMIC_SMART_PTR_USES_STD
  static_assert (
      std::is_same<
          atomic_shared_ptr<int>,
          smart_ptr::std_backed_table_wait::atomic_shared_ptr<int>>::value,
      "std_backed_table_wait");
  static_assert (
      std::is_same<
          atomic_weak_ptr<int>,
          smart_ptr::std_backed_table_wait::atomic_weak_ptr<int>>::value,
      "std_backed_table_wait");
  char const *const selected = "std_backed_table_wait";
#elif LUMEX_ATOMIC_WAIT_USES_STD
  static_assert (
      std::is_same<
          atomic_shared_ptr<int>,
          smart_ptr::lock_based_std_wait::atomic_shared_ptr<int>>::value,
      "lock_based_std_wait");
  static_assert (
      std::is_same<
          atomic_weak_ptr<int>,
          smart_ptr::lock_based_std_wait::atomic_weak_ptr<int>>::value,
      "lock_based_std_wait");
  char const *const selected = "lock_based_std_wait";
#else
  static_assert (
      std::is_same<
          atomic_shared_ptr<int>,
          smart_ptr::lock_based_table_wait::atomic_shared_ptr<int>>::value,
      "lock_based_table_wait");
  static_assert (
      std::is_same<
          atomic_weak_ptr<int>,
          smart_ptr::lock_based_table_wait::atomic_weak_ptr<int>>::value,
      "lock_based_table_wait");
  char const *const selected = "lock_based_table_wait";
#endif
  std::cout << "[ INFO     ] __cplusplus=" << __cplusplus
            << " atomic smart pointers: " << selected << '\n';
  SUCCEED ();
}

TEST (LumexAtomicSmartPtrConfigTest,
      GivenTheWaitSelection_WhenNamingTheLock_ThenTheMatchingNamespaceHoldsIt)
{
  namespace sync = lumex::core::atomic::sync;
#if LUMEX_ATOMIC_WAIT_USES_STD
  static_assert (std::is_same<sync::Detail::BitLock,
                              sync::std_wait::Detail::BitLock>::value,
                 "std::atomic::wait");
#else
  static_assert (std::is_same<sync::Detail::BitLock,
                              sync::table_wait::Detail::BitLock>::value,
                 "striped table");
#endif
  SUCCEED ();
}

TEST (LumexAtomicSmartPtrConfigTest,
      GivenConstantInitialization_WhenTheObjectsAreUsed_ThenTheyWork)
{
#if defined(__cpp_constinit)
  EXPECT_FALSE (g_constinit_shared.load ());
  EXPECT_FALSE (g_constinit_default.load ());
  EXPECT_TRUE (g_constinit_weak.load ().expired ());
  g_constinit_shared = std::make_shared<int> (3);
  EXPECT_EQ (*g_constinit_shared.load (), 3);
  g_constinit_shared = nullptr;
  EXPECT_FALSE (g_constinit_shared.load ());
#else
  GTEST_SKIP () << "constinit needs C++20";
#endif
}

#if LUMEX_HAS_STD_ATOMIC_SHARED_PTR
namespace
{
/// One scripted run on any atomic shared pointer type; returns the trace.
template <typename Atomic>
std::vector<long>
run_script (std::shared_ptr<Pair> const &owner)
{
  std::vector<long> trace;
  std::shared_ptr<int> const view_a (owner, &owner->a);
  std::shared_ptr<int> const view_b (owner, &owner->b);
  std::shared_ptr<int> const other_owner_same_pointer (
      std::make_shared<int> (0), &g_value);
  std::shared_ptr<int> const empty_storing (std::shared_ptr<int> (), &g_value);
  std::shared_ptr<int> const null_owner (static_cast<int *> (nullptr));

  Atomic a (view_a);
  std::shared_ptr<int> e = view_b;
  trace.push_back (a.compare_exchange_strong (e, view_b) ? 1 : 0);
  trace.push_back (e.get () == view_a.get () ? 1 : 0);
  e = view_a;
  trace.push_back (a.compare_exchange_strong (e, empty_storing) ? 1 : 0);
  e = std::shared_ptr<int> ();
  trace.push_back (a.compare_exchange_strong (e, null_owner) ? 1 : 0);
  trace.push_back (e.get () == &g_value ? 1 : 0);
  trace.push_back (e.use_count ());
  trace.push_back (a.compare_exchange_strong (e, null_owner) ? 1 : 0);
  e = std::shared_ptr<int> ();
  trace.push_back (a.compare_exchange_strong (e, view_a) ? 1 : 0);
  trace.push_back (e.use_count ());
  e = null_owner;
  trace.push_back (
      a.compare_exchange_strong (e, other_owner_same_pointer) ? 1 : 0);
  e = std::shared_ptr<int> (std::make_shared<int> (0), &g_value);
  trace.push_back (a.compare_exchange_strong (e, view_a) ? 1 : 0);
  std::shared_ptr<int> const previous = a.exchange (view_b);
  trace.push_back (previous.get () == &g_value ? 1 : 0);
  trace.push_back (a.load ().get () == &owner->b ? 1 : 0);
  a.store (nullptr);
  trace.push_back (a.load () ? 1 : 0);
  trace.push_back (owner.use_count ());
  return trace;
}
} // namespace

TEST (LumexAtomicSmartPtrConfigTest,
      GivenTheStandardType_WhenRunningTheSameScenario_ThenOutcomesMatch)
{
  std::shared_ptr<Pair> const owner = std::make_shared<Pair> (Pair{ 10, 20 });
  std::vector<long> const ours = run_script<atomic_shared_ptr<int>> (owner);
  std::vector<long> const standard
      = run_script<std::atomic<std::shared_ptr<int>>> (owner);
  ASSERT_EQ (ours.size (), standard.size ());
  for (std::size_t i = 0; i < ours.size (); ++i)
    EXPECT_EQ (ours[i], standard[i]) << "step " << i;
}
#endif
