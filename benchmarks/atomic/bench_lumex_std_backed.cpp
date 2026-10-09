// bench_lumex_std_backed.cpp
// LumexLib's atomic_shared_ptr_std_backed at C++20: the wrapper of the
// standard library's std::atomic<std::shared_ptr<T>> with the library's own
// conforming wait, where the library has the type (libstdc++ 12 and later,
// the MSVC STL). It was the C++20 default before the lock-free engine and is
// an explicit name now.
#include "lumex/core/atomic/LumexAtomic"

#include "bench_atomic_smart_ptr.hpp"

#if LUMEX_ATOMIC_BENCH_STANDARD < 202002L
#error "this unit must be built at C++20"
#endif

lumex_atomic_bench::implementation_t
lumex_atomic_bench::lumex_std_backed_implementation ()
{
#if LUMEX_ATOMIC_SMART_PTR_HAS_STD_BACKED
  return detail::make_implementation<
      lumex::core::atomic::smart_ptr::atomic_shared_ptr_std_backed<int>> (
      "lumex_std_backed", "LumexLib std-backed, C++20 (wraps std)",
      "std_backed_" LUMEX_ATOMIC_BENCH_STRINGIFY (
          LUMEX_ATOMIC_SMART_PTR_ABI_NAMESPACE),
      LUMEX_ATOMIC_BENCH_STANDARD);
#else
  return unavailable_implementation ();
#endif
}
