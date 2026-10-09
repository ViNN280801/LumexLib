// bench_boost.cpp
// boost::atomic_shared_ptr as a competitor, when CMake found Boost
// (LUMEX_ATOMIC_BENCH_HAVE_BOOST). In the Boost versions that have it
// (1.53 and later, header-only) it is a spinlock around a boost::shared_ptr,
// so it is a lock-based series, not a lock-free one. It is another smart
// pointer family: a small adapter gives it the interface the harness uses.
#include "bench_atomic_smart_ptr.hpp"

#if defined(LUMEX_ATOMIC_BENCH_HAVE_BOOST)
#include <boost/make_shared.hpp>
#include <boost/smart_ptr/atomic_shared_ptr.hpp>
#include <boost/version.hpp>

namespace
{
class boost_atomic_t
{
public:
  typedef boost::shared_ptr<int> value_type;

  boost_atomic_t () {}
  explicit boost_atomic_t (value_type desired) : atom_ (desired) {}

  bool
  is_lock_free () const
  {
    return atom_.is_lock_free ();
  }

  value_type
  load (std::memory_order = std::memory_order_seq_cst) const
  {
    return atom_.load ();
  }

  void
  store (value_type desired)
  {
    atom_.store (desired);
  }

  value_type
  exchange (value_type desired)
  {
    return atom_.exchange (desired);
  }

  bool
  compare_exchange_strong (value_type &expected, value_type desired)
  {
    return atom_.compare_exchange_strong (expected, desired);
  }

private:
  boost::atomic_shared_ptr<int> atom_;
};
} // namespace

namespace lumex_atomic_bench
{
namespace detail
{
template <> struct atomic_traits_t<boost_atomic_t>
{
  typedef boost_atomic_t::value_type value_type;

  static value_type
  make (int value)
  {
    return boost::make_shared<int> (value);
  }

  static void
  settle ()
  {
  }
};
} // namespace detail
} // namespace lumex_atomic_bench
#endif

lumex_atomic_bench::implementation_t
lumex_atomic_bench::boost_implementation ()
{
#if defined(LUMEX_ATOMIC_BENCH_HAVE_BOOST)
  return detail::make_implementation<boost_atomic_t> (
      "boost", "boost::atomic_shared_ptr (spinlock), Boost " BOOST_LIB_VERSION,
      "boost " BOOST_LIB_VERSION, LUMEX_ATOMIC_BENCH_STANDARD);
#else
  return unavailable_implementation ();
#endif
}
