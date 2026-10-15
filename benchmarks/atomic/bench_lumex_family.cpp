// bench_lumex_family.cpp
// The split-count engine (reserve 8, the production value, and reserve 4)
// and the lock-based engine over LumexLib's own pointer family at C++20,
// where they sleep in std::atomic::wait, with their weak twins. The engines
// are named, so they are measured whatever the family alias pair is. The
// reserve 4 series instantiates the engine's cell with a policy that
// differs from the production one in the reserve alone (no engine header is
// changed). The checks below fail the build at the wrong standard.
#include "lumex/core/atomic/LumexAtomic"

#include "bench_atomic_smart_ptr.hpp"
#include "bench_lumex_family.hpp"

#if LUMEX_ATOMIC_BENCH_STANDARD < 202002L
#error "this unit must be built at C++20"
#endif
#if !LUMEX_ATOMIC_WAIT_USES_STD
#error "this unit must sleep in std::atomic::wait"
#endif

#define LUMEX_ATOMIC_BENCH_FAMILY_PATH(engine)                                \
  engine                                                                      \
      "_" LUMEX_ATOMIC_BENCH_STRINGIFY (LUMEX_ATOMIC_SMART_PTR_ABI_NAMESPACE)

lumex_atomic_bench::implementation_t
lumex_atomic_bench::lumex_split_count_implementation ()
{
#if LUMEX_ATOMIC_SMART_PTR_HAS_SPLIT_COUNT
  // The series is the production engine; stop if its reserve is not 8.
  if (lumex::core::atomic::smart_ptr::Detail::split_native_policy_t::reserve ()
      != 8u)
    return unavailable_implementation ();
  return detail::make_implementation<
      lumex::core::atomic::smart_ptr::atomic_shared_ptr_lock_free_split_count<
          int>> ("lumex_split_count",
                 "LumexLib split-count r8 (own ptr), C++20",
                 LUMEX_ATOMIC_BENCH_FAMILY_PATH ("split_count"),
                 LUMEX_ATOMIC_BENCH_STANDARD);
#else
  return unavailable_implementation ();
#endif
}

lumex_atomic_bench::implementation_t
lumex_atomic_bench::lumex_split_count_r4_implementation ()
{
#if LUMEX_ATOMIC_SMART_PTR_HAS_SPLIT_COUNT
  if (reserve_policy_t<4u>::reserve () != 4u)
    return unavailable_implementation ();
  return detail::make_implementation<split_count_with_reserve_t<4u>::type> (
      "lumex_split_count_r4", "LumexLib split-count r4 (own ptr), C++20",
      LUMEX_ATOMIC_BENCH_FAMILY_PATH ("split_count_r4"),
      LUMEX_ATOMIC_BENCH_STANDARD);
#else
  return unavailable_implementation ();
#endif
}

lumex_atomic_bench::implementation_t
lumex_atomic_bench::lumex_split_count_nocasres_implementation ()
{
#if LUMEX_ATOMIC_SMART_PTR_HAS_SPLIT_COUNT
  if (no_cas_reserve_policy_t::cas_reserve () != 0u
      || no_cas_reserve_policy_t::reserve () != 8u)
    return unavailable_implementation ();
  return detail::make_implementation<split_count_without_cas_reserve_t::type> (
      "lumex_split_count_nocasres",
      "LumexLib split-count r8, no cas reserve (own ptr), C++20",
      LUMEX_ATOMIC_BENCH_FAMILY_PATH ("split_count_nocasres"),
      LUMEX_ATOMIC_BENCH_STANDARD);
#else
  return unavailable_implementation ();
#endif
}

lumex_atomic_bench::implementation_t
lumex_atomic_bench::lumex_lock_based_lumex_implementation ()
{
#if LUMEX_ATOMIC_SMART_PTR_HAS_LUMEX_FAMILY
  return detail::make_implementation<
      lumex::core::atomic::smart_ptr::atomic_shared_ptr_lock_based_lumex<
          int>> ("lumex_lock_based_lumex",
                 "LumexLib lock-based (own ptr), C++20",
                 LUMEX_ATOMIC_BENCH_FAMILY_PATH ("lock_based_lumex"),
                 LUMEX_ATOMIC_BENCH_STANDARD);
#else
  return unavailable_implementation ();
#endif
}

lumex_atomic_bench::implementation_t
lumex_atomic_bench::lumex_split_count_weak_implementation ()
{
#if LUMEX_ATOMIC_SMART_PTR_HAS_SPLIT_COUNT
  return detail::make_weak_implementation<
      lumex::core::atomic::smart_ptr::atomic_weak_ptr_lock_free_split_count<
          int>> ("lumex_split_count_weak",
                 "LumexLib split-count weak (own ptr), C++20",
                 LUMEX_ATOMIC_BENCH_FAMILY_PATH ("split_count"),
                 LUMEX_ATOMIC_BENCH_STANDARD);
#else
  return unavailable_implementation ();
#endif
}

lumex_atomic_bench::implementation_t
lumex_atomic_bench::lumex_lock_based_lumex_weak_implementation ()
{
#if LUMEX_ATOMIC_SMART_PTR_HAS_LUMEX_FAMILY
  return detail::make_weak_implementation<
      lumex::core::atomic::smart_ptr::atomic_weak_ptr_lock_based_lumex<int>> (
      "lumex_lock_based_lumex_weak",
      "LumexLib lock-based weak (own ptr), C++20",
      LUMEX_ATOMIC_BENCH_FAMILY_PATH ("lock_based_lumex"),
      LUMEX_ATOMIC_BENCH_STANDARD);
#else
  return unavailable_implementation ();
#endif
}
