// bench_lumex_cxx11.cpp
// LumexLib's atomic_shared_ptr as a C++11 program gets it: the lock-based
// engine and, where the build has it, the lock-free engine, both sleeping on
// the striped wait table. This unit is built at C++11 (MSVC: C++14, its
// lowest mode) and linked into the C++20 benchmark; the check below fails the
// build if it ever measured anything else.
#include "lumex/core/atomic/LumexAtomic"

#include "bench_atomic_smart_ptr.hpp"

#if LUMEX_ATOMIC_WAIT_USES_STD
#error "the C++11 unit must sleep on the table"
#endif


lumex_atomic_bench::implementation_t
lumex_atomic_bench::lumex_lock_based_cxx11_implementation ()
{
  return detail::make_implementation<
      lumex::core::atomic::smart_ptr::atomic_shared_ptr_lock_based<int> > (
      "lumex_lock_based_cxx11", "LumexLib lock-based, C++11",
      "lock_based_" LUMEX_ATOMIC_BENCH_STRINGIFY (
          LUMEX_ATOMIC_SMART_PTR_ABI_NAMESPACE),
      LUMEX_ATOMIC_BENCH_STANDARD);
}

lumex_atomic_bench::implementation_t
lumex_atomic_bench::lumex_lock_free_cxx11_implementation ()
{
#if LUMEX_ATOMIC_SMART_PTR_HAS_LOCK_FREE
  return detail::make_implementation<
      lumex::core::atomic::smart_ptr::atomic_shared_ptr_lock_free<int> > (
      "lumex_lock_free_cxx11", "LumexLib lock-free, C++11",
      "lock_free_" LUMEX_ATOMIC_BENCH_STRINGIFY (
          LUMEX_ATOMIC_SMART_PTR_ABI_NAMESPACE),
      LUMEX_ATOMIC_BENCH_STANDARD);
#else
  return unavailable_implementation ();
#endif
}
