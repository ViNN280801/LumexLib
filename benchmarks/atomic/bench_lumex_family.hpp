// bench_lumex_family.hpp
// What the series over LumexLib's own pointer family (the split-count engine
// and the lock-based engine over lumex::core::smart_ptr::shared_ptr and
// weak_ptr) need besides the common harness: how to make a shared pointer
// of the family, and the reserve of the split-count engine as a policy.
#ifndef LUMEX_BENCHMARKS_ATOMIC_BENCH_LUMEX_FAMILY_HPP
#define LUMEX_BENCHMARKS_ATOMIC_BENCH_LUMEX_FAMILY_HPP

#include <cstdint>

#include "lumex/core/atomic/LumexAtomic"
#include "lumex/core/smart_ptr/LumexSmartPtr"

#include "bench_atomic_smart_ptr.hpp"

namespace lumex_atomic_bench
{
namespace detail
{
/** One allocation, as std::make_shared gives the std series. */
template <> struct pointer_maker_t<lumex::core::smart_ptr::shared_ptr<int>>
{
  static lumex::core::smart_ptr::shared_ptr<int>
  make (int value)
  {
    return lumex::core::smart_ptr::make_shared<int> (value);
  }
};
} // namespace detail
} // namespace lumex_atomic_bench

#if LUMEX_ATOMIC_SMART_PTR_HAS_SPLIT_COUNT
namespace lumex_atomic_bench
{
/**
 * The split-count policy of production with another reserve. Everything else
 * (the hooks, the guess, the tick limit, the back-off) is the production
 * policy's, inherited, so the two series differ only in the units a writer
 * deposits beyond the ticks it saw.
 */
template <unsigned Reserve>
struct reserve_policy_t
    : lumex::core::atomic::smart_ptr::Detail::split_native_policy_t
{
  static std::uint32_t
  reserve () LUMEX_NOEXCEPT
  {
    return Reserve;
  }
};

/** The split-count engine of the shared pointer with a given reserve. */
template <unsigned Reserve> struct split_count_with_reserve_t
{
  typedef lumex::core::atomic::smart_ptr::Detail::basic_atomic_shared_ptr<
      int,
      lumex::core::atomic::smart_ptr::Detail::split_count_cell<
          lumex::core::smart_ptr::shared_ptr<int>, reserve_policy_t<Reserve>>>
      type;
};
} // namespace lumex_atomic_bench
#endif

#endif // LUMEX_BENCHMARKS_ATOMIC_BENCH_LUMEX_FAMILY_HPP
