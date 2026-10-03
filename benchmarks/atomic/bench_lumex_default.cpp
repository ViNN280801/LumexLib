// bench_lumex_default.cpp
// LumexLib's atomic_shared_ptr at C++20 with nothing forced: it wraps the
// standard library's std::atomic<std::shared_ptr<T>> where the library has
// one (libstdc++ 12+, the MSVC STL) and is lock-based otherwise (libc++).
// The selected implementation is recorded in the results.
#include "lumex/core/atomic/LumexAtomic"

#include "bench_atomic_smart_ptr.hpp"

#if defined(LUMEX_FORCE_LOCK_BASED_ATOMIC_SHARED_PTR)                         \
    || defined(LUMEX_FORCE_ATOMIC_WAIT_TABLE)
#error "this unit measures the default selection"
#endif
#if LUMEX_ATOMIC_BENCH_STANDARD < 202002L
#error "this unit must be built at C++20"
#endif

#if LUMEX_ATOMIC_SMART_PTR_USES_STD
#define LUMEX_ATOMIC_BENCH_DEFAULT_LABEL "LumexLib default, C++20 (wraps std)"
#else
#define LUMEX_ATOMIC_BENCH_DEFAULT_LABEL "LumexLib default, C++20 (lock-based)"
#endif

lumex_atomic_bench::implementation_t
lumex_atomic_bench::lumex_default_implementation ()
{
  return detail::make_implementation<
      lumex::core::atomic::smart_ptr::atomic_shared_ptr<int>> (
      "lumex_default", LUMEX_ATOMIC_BENCH_DEFAULT_LABEL,
      LUMEX_ATOMIC_BENCH_STRINGIFY (LUMEX_ATOMIC_SMART_PTR_ABI_NAMESPACE),
      LUMEX_ATOMIC_BENCH_STANDARD);
}
