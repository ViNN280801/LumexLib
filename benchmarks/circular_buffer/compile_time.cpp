// compile_time.cpp
// The translation unit run_benchmark.py compiles with -fsyntax-only to time
// what it costs to include and use a bounded ring. BENCH_RING_IMPL is 1 (this
// library), 2 (boost::circular_buffer), 3 (std::deque as a bounded queue), 4
// (std::vector ring, wrap by a compare), 5 (the same with `%`), or 0 for the
// same unit with a stub instead of a ring, which is the cost of the headers
// every variant shares. BENCH_COMPILE_MODE is 0 to include the header and
// declare one object only, 1 to use the members the benchmark scenarios use
// (construction, size, capacity, push_back, pop_front, front, back,
// operator[], iteration, clear, copy and move, containers of rings).
#include <cstddef>
#include <deque>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#ifndef BENCH_COMPILE_MODE
#define BENCH_COMPILE_MODE 1
#endif

#if BENCH_RING_IMPL == 0
namespace bench
{
template <typename T> struct ring
{
  T *items;
  std::size_t capacity;
};
} // namespace bench
#define BENCH_COMPILE_TIME_BASELINE 1
#else
#include "benchmarks/circular_buffer/bench_circular_buffer_impl.hpp"
#define BENCH_COMPILE_TIME_BASELINE 0
#endif

int
main ()
{
  std::size_t result = 0;
#if BENCH_COMPILE_TIME_BASELINE
  bench::ring<int> a = { nullptr, 4 };
  bench::ring<std::string> b = { nullptr, 4 };
  std::vector<bench::ring<int>> v (4, a);
  result += a.capacity + b.capacity + v.size ();
#elif BENCH_COMPILE_MODE == 0
  bench::ring<int> a (4);
  result += a.capacity ();
#else
  bench::ring<int> a (4);
  bench::ring<std::string> s (4);
  for (int i = 0; i < 6; ++i)
    {
      a.push_back (i);
      s.push_back (std::string ("abc"));
    }
  s.push_back (std::string ("def"));
  result += a.size () + a.capacity () + static_cast<std::size_t> (a.front ())
            + static_cast<std::size_t> (a.back ())
            + static_cast<std::size_t> (a[1]);
  result += s.front ().size () + s.back ().size () + s[2].size ();
  for (bench::ring<int>::iterator it = a.begin (); it != a.end (); ++it)
    result += static_cast<std::size_t> (*it);
  for (bench::ring<std::string>::const_iterator it
       = static_cast<bench::ring<std::string> const &> (s).begin ();
       it != static_cast<bench::ring<std::string> const &> (s).end (); ++it)
    result += (*it).size ();
  a.pop_front ();
  bench::ring<int> copy (a);
  bench::ring<int> moved (std::move (copy));
  bench::ring<std::string> copy_s (s);
  bench::ring<std::string> moved_s (std::move (copy_s));
  result += moved.size () + moved_s.size ();
  a.clear ();
  s.clear ();
  std::vector<bench::ring<int>> v (4, moved);
  v.push_back (a);
  std::vector<bench::ring<std::string>> w;
  w.push_back (moved_s);
  w.push_back (bench::ring<std::string> (2));
  result += v.size () + w.size () + a.size ();
#endif
  return result == 0 ? 1 : 0;
}
