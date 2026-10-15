// bench_lumex_family_cxx11.cpp
// The split-count engine and the lock-based engine over LumexLib's own
// pointer family, as a C++11 program gets them (both sleep on the striped
// wait table). This unit is built at C++11 (MSVC: C++14) and linked into the
// C++20 benchmark; the check below fails the build if it ever measured
// anything else. The split-count series is the production engine,
// atomic_shared_ptr_lock_free_split_count<int>, with its reserve of 8.
#include "lumex/core/atomic/LumexAtomic"

#include "bench_atomic_smart_ptr.hpp"
#include "bench_lumex_family.hpp"

#if LUMEX_ATOMIC_WAIT_USES_STD
#error "the C++11 unit must sleep on the table"
#endif

lumex_atomic_bench::implementation_t
lumex_atomic_bench::lumex_split_count_cxx11_implementation ()
{
#if LUMEX_ATOMIC_SMART_PTR_HAS_SPLIT_COUNT
  if (lumex::core::atomic::smart_ptr::Detail::split_native_policy_t::reserve ()
      != 8u)
    return unavailable_implementation ();
  return detail::make_implementation<
      lumex::core::atomic::smart_ptr::atomic_shared_ptr_lock_free_split_count<
          int>> ("lumex_split_count_cxx11",
                 "LumexLib split-count (own ptr), C++11",
                 "split_count_" LUMEX_ATOMIC_BENCH_STRINGIFY (
                     LUMEX_ATOMIC_SMART_PTR_ABI_NAMESPACE),
                 LUMEX_ATOMIC_BENCH_STANDARD);
#else
  return unavailable_implementation ();
#endif
}

lumex_atomic_bench::implementation_t
lumex_atomic_bench::lumex_lock_based_lumex_cxx11_implementation ()
{
#if LUMEX_ATOMIC_SMART_PTR_HAS_LUMEX_FAMILY
  return detail::make_implementation<
      lumex::core::atomic::smart_ptr::atomic_shared_ptr_lock_based_lumex<
          int>> ("lumex_lock_based_lumex_cxx11",
                 "LumexLib lock-based (own ptr), C++11",
                 "lock_based_lumex_" LUMEX_ATOMIC_BENCH_STRINGIFY (
                     LUMEX_ATOMIC_SMART_PTR_ABI_NAMESPACE),
                 LUMEX_ATOMIC_BENCH_STANDARD);
#else
  return unavailable_implementation ();
#endif
}
