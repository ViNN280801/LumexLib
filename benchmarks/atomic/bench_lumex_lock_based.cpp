// bench_lumex_lock_based.cpp
// LumexLib's lock-based atomic_shared_ptr_lock_based at C++20, where it
// sleeps in std::atomic::wait. The engine is named, so it is measured even
// where the common name is the lock-free one; the checks below fail the
// build at the wrong standard.
#include "lumex/core/atomic/LumexAtomic"

#include "bench_atomic_smart_ptr.hpp"

#if LUMEX_ATOMIC_BENCH_STANDARD < 202002L
#error "this unit must be built at C++20"
#endif
#if !LUMEX_ATOMIC_WAIT_USES_STD
#error "this unit must sleep in std::atomic::wait"
#endif

lumex_atomic_bench::implementation_t
lumex_atomic_bench::lumex_lock_based_implementation ()
{
  return detail::make_implementation<
      lumex::core::atomic::smart_ptr::atomic_shared_ptr_lock_based<int>> (
      "lumex_lock_based", "LumexLib lock-based, C++20",
      "lock_based_" LUMEX_ATOMIC_BENCH_STRINGIFY (
          LUMEX_ATOMIC_SMART_PTR_ABI_NAMESPACE),
      LUMEX_ATOMIC_BENCH_STANDARD);
}
