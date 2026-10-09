// bench_lumex_lock_free.cpp
// LumexLib's lock-free atomic_shared_ptr_lock_free at C++20 with the two
// policies for the destruction of a replaced value: immediate (the writer
// scans the hazard slots for the replaced box and destroys it in the call)
// and deferred (the box is retired and a pass of the hazard domain destroys
// it later). The comparison of the two decides whether the common name may
// pick the lock-free engine.
#include "lumex/core/atomic/LumexAtomic"

#include "bench_atomic_smart_ptr.hpp"

#if LUMEX_ATOMIC_BENCH_STANDARD < 202002L
#error "this unit must be built at C++20"
#endif


lumex_atomic_bench::implementation_t
lumex_atomic_bench::lumex_lock_free_implementation ()
{
#if LUMEX_ATOMIC_SMART_PTR_HAS_LOCK_FREE
  return detail::make_implementation<
      lumex::core::atomic::smart_ptr::atomic_shared_ptr_lock_free<
          int, lumex::core::atomic::smart_ptr::reclaim::immediate>> (
      "lumex_lock_free", "LumexLib lock-free, C++20",
      "lock_free_" LUMEX_ATOMIC_BENCH_STRINGIFY (
          LUMEX_ATOMIC_SMART_PTR_ABI_NAMESPACE),
      LUMEX_ATOMIC_BENCH_STANDARD);
#else
  return unavailable_implementation ();
#endif
}

lumex_atomic_bench::implementation_t
lumex_atomic_bench::lumex_lock_free_deferred_implementation ()
{
#if LUMEX_ATOMIC_SMART_PTR_HAS_LOCK_FREE
  return detail::make_implementation<
      lumex::core::atomic::smart_ptr::atomic_shared_ptr_lock_free<
          int, lumex::core::atomic::smart_ptr::reclaim::deferred>> (
      "lumex_lock_free_deferred", "LumexLib lock-free, C++20, deferred",
      "lock_free_deferred_" LUMEX_ATOMIC_BENCH_STRINGIFY (
          LUMEX_ATOMIC_SMART_PTR_ABI_NAMESPACE),
      LUMEX_ATOMIC_BENCH_STANDARD);
#else
  return unavailable_implementation ();
#endif
}
