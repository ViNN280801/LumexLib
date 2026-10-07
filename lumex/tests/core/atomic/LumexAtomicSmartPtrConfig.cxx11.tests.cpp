// Implementation selection of the atomic smart pointers: the documented rule
// behind LUMEX_ATOMIC_SMART_PTR_USES_STD and LUMEX_ATOMIC_WAIT_USES_STD, the
// inline namespace of each combination and constant initialization (LWG
// 3661). The differential run against std::atomic<std::shared_ptr<T>> is in
// LumexAtomicSmartPtrConfig.cxx20.tests.cpp.

#include <atomic>
#include <iostream>
#include <memory>
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/core/atomic/LumexAtomic"
#include "lumex/core/atomic/sync/LumexBitLock.hpp"
#include "lumex/tests/core/atomic/LumexAtomicTestSupport.hpp"

using namespace lumex_atomic_test;

namespace
{
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
#if defined(LUMEX_ATOMIC_SMART_PTR_FORCE_LOCK_BASED)
  EXPECT_EQ (LUMEX_ATOMIC_SMART_PTR_USES_STD, 0);
#elif LUMEX_HAS_STD_ATOMIC_SHARED_PTR
  EXPECT_EQ (LUMEX_ATOMIC_SMART_PTR_USES_STD, 1);
#else
  EXPECT_EQ (LUMEX_ATOMIC_SMART_PTR_USES_STD, 0);
#endif

#if defined(LUMEX_ATOMIC_WAIT_FORCE_TABLE)
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
  static_assert (std::is_same<sync::Detail::bit_lock,
                              sync::std_wait::Detail::bit_lock>::value,
                 "std::atomic::wait");
#else
  static_assert (std::is_same<sync::Detail::bit_lock,
                              sync::table_wait::Detail::bit_lock>::value,
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
