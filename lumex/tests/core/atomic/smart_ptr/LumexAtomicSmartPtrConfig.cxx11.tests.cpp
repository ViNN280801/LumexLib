// Engine selection of the atomic smart pointers: the documented rule behind
// LUMEX_ATOMIC_SMART_PTR_HAS_LOCK_FREE, _HAS_STD_BACKED, _COMMON_IS_LOCK_FREE
// and LUMEX_ATOMIC_WAIT_USES_STD, the engine behind the common alias
// templates, the inline namespace of each way of sleeping and constant
// initialization (LWG 3661). The differential run against
// std::atomic<std::shared_ptr<T>> is in
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
      GivenTheBuild_WhenSelectingTheEngines_ThenTheDocumentedRuleHolds)
{
  // The wrapper of the standard library's type exists when the library has
  // the type, and no common name ever resolves to it.
  EXPECT_EQ (LUMEX_ATOMIC_SMART_PTR_HAS_STD_BACKED,
             LUMEX_HAS_STD_ATOMIC_SHARED_PTR ? 1 : 0);
  // The lock-free engine exists with the hazard pointer library, lock-free
  // pointer atomics and without the test switch.
#if defined(LUMEX_ATOMIC_SMART_PTR_DISABLE_LOCK_FREE)
  EXPECT_EQ (LUMEX_ATOMIC_SMART_PTR_HAS_LOCK_FREE, 0);
#elif defined(LUMEX_ATOMIC_HAS_HAZARD_POINTER)
  EXPECT_EQ (LUMEX_ATOMIC_SMART_PTR_HAS_LOCK_FREE,
             ATOMIC_POINTER_LOCK_FREE == 2 ? 1 : 0);
#else
  EXPECT_EQ (LUMEX_ATOMIC_SMART_PTR_HAS_LOCK_FREE, 0);
#endif
  // The common name is the lock-free engine where it exists, else the
  // lock-based one; the switch moves it to the lock-based one.
#if defined(LUMEX_ATOMIC_SMART_PTR_FORCE_LOCK_BASED)
  EXPECT_EQ (LUMEX_ATOMIC_SMART_PTR_COMMON_IS_LOCK_FREE, 0);
#else
  EXPECT_EQ (LUMEX_ATOMIC_SMART_PTR_COMMON_IS_LOCK_FREE,
             LUMEX_ATOMIC_SMART_PTR_HAS_LOCK_FREE);
#endif

#if defined(LUMEX_ATOMIC_WAIT_FORCE_TABLE)
  EXPECT_EQ (LUMEX_ATOMIC_WAIT_USES_STD, 0);
#elif LUMEX_HAS_STD_ATOMIC_WAIT
  EXPECT_EQ (LUMEX_ATOMIC_WAIT_USES_STD, 1);
#else
  EXPECT_EQ (LUMEX_ATOMIC_WAIT_USES_STD, 0);
#endif

#if __cplusplus < 202002L
  EXPECT_EQ (LUMEX_ATOMIC_SMART_PTR_HAS_STD_BACKED, 0)
      << "before C++20 only the lock-based and lock-free forms exist";
  EXPECT_EQ (LUMEX_ATOMIC_WAIT_USES_STD, 0)
      << "before C++20 only the table exists";
#endif
}

TEST (LumexAtomicSmartPtrConfigTest,
      GivenTheSelection_WhenNamingTheTypes_ThenTheCommonNamesAreTheEngines)
{
  namespace smart_ptr = lumex::core::atomic::smart_ptr;
#if LUMEX_ATOMIC_SMART_PTR_COMMON_IS_LOCK_FREE
  static_assert (
      std::is_same<smart_ptr::atomic_shared_ptr<int>,
                   smart_ptr::atomic_shared_ptr_lock_free<int>>::value,
      "the common name is the lock-free engine");
  static_assert (
      std::is_same<smart_ptr::atomic_weak_ptr<int>,
                   smart_ptr::atomic_weak_ptr_lock_free<int>>::value,
      "the common name is the lock-free engine");
  char const *const selected = "lock_free";
#else
  static_assert (
      std::is_same<smart_ptr::atomic_shared_ptr<int>,
                   smart_ptr::atomic_shared_ptr_lock_based<int>>::value,
      "the common name is the lock-based engine");
  static_assert (
      std::is_same<smart_ptr::atomic_weak_ptr<int>,
                   smart_ptr::atomic_weak_ptr_lock_based<int>>::value,
      "the common name is the lock-based engine");
  char const *const selected = "lock_based";
#endif
  // The engine is in the class name; the inline namespace holds the way of
  // sleeping only.
#if LUMEX_ATOMIC_WAIT_USES_STD
  static_assert (
      std::is_same<smart_ptr::atomic_shared_ptr<int>,
                   smart_ptr::std_wait::atomic_shared_ptr<int>>::value,
      "std_wait");
  static_assert (
      std::is_same<smart_ptr::atomic_weak_ptr<int>,
                   smart_ptr::std_wait::atomic_weak_ptr<int>>::value,
      "std_wait");
  char const *const sleeping = "std_wait";
#else
  static_assert (
      std::is_same<smart_ptr::atomic_shared_ptr<int>,
                   smart_ptr::table_wait::atomic_shared_ptr<int>>::value,
      "table_wait");
  static_assert (
      std::is_same<smart_ptr::atomic_weak_ptr<int>,
                   smart_ptr::table_wait::atomic_weak_ptr<int>>::value,
      "table_wait");
  char const *const sleeping = "table_wait";
#endif
  // The three explicit names are distinct types (the lock-free one and the
  // wrapper only where they exist).
  static_assert (
      !std::is_same<smart_ptr::atomic_shared_ptr_lock_based<int>,
                    smart_ptr::atomic_weak_ptr_lock_based<int>>::value,
      "distinct templates");
#if LUMEX_ATOMIC_SMART_PTR_HAS_STD_BACKED
  static_assert (
      !std::is_same<smart_ptr::atomic_shared_ptr_lock_based<int>,
                    smart_ptr::atomic_shared_ptr_std_backed<int>>::value,
      "distinct engines");
  static_assert (
      !std::is_same<smart_ptr::atomic_shared_ptr<int>,
                    smart_ptr::atomic_shared_ptr_std_backed<int>>::value,
      "no common name resolves to the wrapper");
  static_assert (
      !std::is_same<smart_ptr::atomic_weak_ptr<int>,
                    smart_ptr::atomic_weak_ptr_std_backed<int>>::value,
      "no common name resolves to the wrapper");
#endif
  std::cout << "[ INFO     ] __cplusplus=" << __cplusplus
            << " atomic smart pointers: " << selected << ", " << sleeping
            << '\n';
  SUCCEED ();
}

TEST (LumexAtomicSmartPtrConfigTest,
      GivenEachEngine_WhenNamingValueType_ThenItIsThePointerOfTheEngine)
{
  namespace smart_ptr = lumex::core::atomic::smart_ptr;
  static_assert (
      std::is_same<smart_ptr::atomic_shared_ptr_lock_based<int>::value_type,
                   std::shared_ptr<int>>::value,
      "lock-based shared engine");
  static_assert (
      std::is_same<smart_ptr::atomic_weak_ptr_lock_based<int>::value_type,
                   std::weak_ptr<int>>::value,
      "lock-based weak engine");
#if LUMEX_ATOMIC_SMART_PTR_HAS_LOCK_FREE
  static_assert (
      std::is_same<smart_ptr::atomic_shared_ptr_lock_free<int>::value_type,
                   std::shared_ptr<int>>::value,
      "lock-free shared engine");
  static_assert (
      std::is_same<smart_ptr::atomic_weak_ptr_lock_free<int>::value_type,
                   std::weak_ptr<int>>::value,
      "lock-free weak engine");
#endif
#if LUMEX_ATOMIC_SMART_PTR_HAS_STD_BACKED
  static_assert (
      std::is_same<smart_ptr::atomic_shared_ptr_std_backed<int>::value_type,
                   std::shared_ptr<int>>::value,
      "standard-backed shared engine");
  static_assert (
      std::is_same<smart_ptr::atomic_weak_ptr_std_backed<int>::value_type,
                   std::weak_ptr<int>>::value,
      "standard-backed weak engine");
#endif
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
