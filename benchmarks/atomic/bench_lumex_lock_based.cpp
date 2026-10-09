// bench_lumex_lock_based.cpp
// LumexLib's lock-based atomic_shared_ptr at C++20, where it sleeps in
// std::atomic::wait. CMake builds this unit with
// LUMEX_ATOMIC_SMART_PTR_FORCE_LOCK_BASED, so the lock-based implementation
// is measured even where the standard library has its own
// std::atomic<std::shared_ptr<T>>; the checks below fail the build if the
// switch did not take effect.
#include "lumex/core/atomic/LumexAtomic"

#include "bench_atomic_smart_ptr.hpp"

#if !defined(LUMEX_ATOMIC_SMART_PTR_FORCE_LOCK_BASED)                        \
    || LUMEX_ATOMIC_SMART_PTR_COMMON_IS_LOCK_FREE
#error "this unit must build the forced lock-based implementation"
#endif
#if LUMEX_ATOMIC_BENCH_STANDARD < 202002L
#error "this unit must be built at C++20"
#endif

lumex_atomic_bench::implementation_t
lumex_atomic_bench::lumex_lock_based_implementation ()
{
  return detail::make_implementation<
      lumex::core::atomic::smart_ptr::atomic_shared_ptr_lock_based<int>> (
      "lumex_lock_based", "LumexLib lock-based, C++20",
      LUMEX_ATOMIC_BENCH_STRINGIFY (LUMEX_ATOMIC_SMART_PTR_ABI_NAMESPACE),
      LUMEX_ATOMIC_BENCH_STANDARD);
}
