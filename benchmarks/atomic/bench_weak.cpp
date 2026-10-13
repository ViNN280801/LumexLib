// bench_weak.cpp
// The weak atomics over std::weak_ptr at C++20: LumexLib's lock-based and
// lock-free (box, immediate destruction) engines, the std-backed wrapper and
// the standard library's std::atomic<std::weak_ptr<int>>, the competitors of
// the weak engines over the library's own weak_ptr in bench_lumex_family.cpp.
#include "lumex/core/atomic/LumexAtomic"

#include "bench_atomic_smart_ptr.hpp"

#if LUMEX_ATOMIC_BENCH_STANDARD < 202002L
#error "this unit must be built at C++20"
#endif

lumex_atomic_bench::implementation_t
lumex_atomic_bench::lumex_lock_based_weak_implementation ()
{
  return detail::make_weak_implementation<
      lumex::core::atomic::smart_ptr::atomic_weak_ptr_lock_based<int>> (
      "lumex_lock_based_weak", "LumexLib lock-based weak (std ptr), C++20",
      "lock_based_" LUMEX_ATOMIC_BENCH_STRINGIFY (
          LUMEX_ATOMIC_SMART_PTR_ABI_NAMESPACE),
      LUMEX_ATOMIC_BENCH_STANDARD);
}

lumex_atomic_bench::implementation_t
lumex_atomic_bench::lumex_lock_free_weak_implementation ()
{
#if LUMEX_ATOMIC_SMART_PTR_HAS_LOCK_FREE
  return detail::make_weak_implementation<
      lumex::core::atomic::smart_ptr::atomic_weak_ptr_lock_free<int>> (
      "lumex_lock_free_weak", "LumexLib lock-free weak (box), C++20",
      "lock_free_" LUMEX_ATOMIC_BENCH_STRINGIFY (
          LUMEX_ATOMIC_SMART_PTR_ABI_NAMESPACE),
      LUMEX_ATOMIC_BENCH_STANDARD);
#else
  return unavailable_implementation ();
#endif
}

lumex_atomic_bench::implementation_t
lumex_atomic_bench::lumex_std_backed_weak_implementation ()
{
#if LUMEX_ATOMIC_SMART_PTR_HAS_STD_BACKED
  return detail::make_weak_implementation<
      lumex::core::atomic::smart_ptr::atomic_weak_ptr_std_backed<int>> (
      "lumex_std_backed_weak", "LumexLib std-backed weak, C++20 (wraps std)",
      "std_backed_" LUMEX_ATOMIC_BENCH_STRINGIFY (
          LUMEX_ATOMIC_SMART_PTR_ABI_NAMESPACE),
      LUMEX_ATOMIC_BENCH_STANDARD);
#else
  return unavailable_implementation ();
#endif
}

lumex_atomic_bench::implementation_t
lumex_atomic_bench::std_weak_implementation ()
{
#if defined(__cpp_lib_atomic_shared_ptr)
  return detail::make_weak_implementation<std::atomic<std::weak_ptr<int>>> (
      "std_weak", "std::atomic<std::weak_ptr>, the standard library", "std",
      LUMEX_ATOMIC_BENCH_STANDARD);
#else
  return unavailable_implementation ();
#endif
}
