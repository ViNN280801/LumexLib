// compile_time.cpp
// The translation unit run_benchmark.py compiles with -fsyntax-only to time
// what it costs to include and use a span: BENCH_SPAN_IMPL is 1 (this
// library), 2 (std::span), 3 (boost::span), or 0 for the same unit without a
// span, which is the cost of the headers every variant shares.
#include <array>
#include <cstddef>
#include <vector>

#if BENCH_SPAN_IMPL == 0
namespace bench
{
template <typename T> struct span
{
  T *first;
  std::size_t count;
};
} // namespace bench
#define BENCH_COMPILE_TIME_BASELINE 1
#else
#include "benchmarks/span/bench_span_impl.hpp"
#define BENCH_COMPILE_TIME_BASELINE 0
#endif

namespace
{
int
sum (bench::span<int const> view)
{
  int total = 0;
#if !BENCH_COMPILE_TIME_BASELINE
  for (int const element : view)
    total += element;
#else
  total = static_cast<int> (view.count);
#endif
  return total;
}
} // namespace

int
main ()
{
  int values[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
  std::array<int, 8> array_values = { { 1, 2, 3, 4, 5, 6, 7, 8 } };
  std::vector<int> vector_values (values, values + 8);
  int result = 0;
#if BENCH_COMPILE_TIME_BASELINE
  result += sum (bench::span<int const>{ values, 8 });
#else
  bench::span<int> dynamic (values);
  bench::span<int, 8> fixed (values);
  result += sum (dynamic) + sum (fixed) + sum (array_values)
            + sum (vector_values);
  result += sum (dynamic.first (4)) + sum (dynamic.last (4))
            + sum (dynamic.subspan (2, 4));
  result
      += static_cast<int> (fixed.first<4> ().size () + fixed.last<4> ().size ()
                           + fixed.subspan<2, 4> ().size ());
  bench::span<int, 8> converted (dynamic);
  result += static_cast<int> (converted.size_bytes () + dynamic.front ()
                              + dynamic.back ());
#if BENCH_SPAN_HAS_AS_BYTES
  result += static_cast<int> (bench::as_bytes (dynamic).size ());
#endif
#endif
  return result == 0 ? 1 : 0;
}
