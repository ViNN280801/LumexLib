// bench_lumex_default.cpp
// LumexLib's common name atomic_shared_ptr at C++20 with nothing forced: the
// lock-free engine where the build has it, the lock-based one otherwise,
// never the wrapper of the standard library's type. The check below fails the
// build if the common name is not what the configuration macros say.
#include "lumex/core/atomic/LumexAtomic"

#include "bench_atomic_smart_ptr.hpp"

#if defined(LUMEX_ATOMIC_SMART_PTR_FORCE_LOCK_BASED)                         \
    || defined(LUMEX_ATOMIC_SMART_PTR_DISABLE_LOCK_FREE)                      \
    || defined(LUMEX_ATOMIC_WAIT_FORCE_TABLE)
#error "this unit measures the default selection"
#endif
#if LUMEX_ATOMIC_BENCH_STANDARD < 202002L
#error "this unit must be built at C++20"
#endif

#if LUMEX_ATOMIC_SMART_PTR_COMMON_IS_LOCK_FREE
static_assert (
    std::is_same<lumex::core::atomic::smart_ptr::atomic_shared_ptr<int>,
                 lumex::core::atomic::smart_ptr::atomic_shared_ptr_lock_free<
                     int>>::value,
    "the common name must resolve to the lock-free engine");
#define LUMEX_ATOMIC_BENCH_DEFAULT_LABEL "LumexLib common name, C++20 (lock-free)"

#else
static_assert (
    std::is_same<lumex::core::atomic::smart_ptr::atomic_shared_ptr<int>,
                 lumex::core::atomic::smart_ptr::atomic_shared_ptr_lock_based<
                     int>>::value,
    "the common name must resolve to the lock-based engine");
#define LUMEX_ATOMIC_BENCH_DEFAULT_LABEL "LumexLib common name, C++20 (lock-based)"
#endif

lumex_atomic_bench::implementation_t
lumex_atomic_bench::lumex_default_implementation ()
{
  return detail::make_implementation<
      lumex::core::atomic::smart_ptr::atomic_shared_ptr<int>> (
      "lumex_default", LUMEX_ATOMIC_BENCH_DEFAULT_LABEL,
      "common_" LUMEX_ATOMIC_BENCH_STRINGIFY (
          LUMEX_ATOMIC_SMART_PTR_ABI_NAMESPACE),
      LUMEX_ATOMIC_BENCH_STANDARD);
}
