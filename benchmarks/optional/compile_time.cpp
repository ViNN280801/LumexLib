// compile_time.cpp
// The translation unit run_benchmark.py compiles with -fsyntax-only to time
// what it costs to include and use an optional. BENCH_OPTIONAL_IMPL is 1 (this
// library), 2 (std::optional), 3 (boost::optional), or 0 for the same unit
// with a stub instead of an optional, which is the cost of the headers every
// variant shares. BENCH_COMPILE_MODE is 0 to include the header and declare
// one object only, 1 to use the whole API (construction, observers,
// modifiers, comparisons, make_optional-like factories, containers).
#include <string>
#include <unordered_set>
#include <vector>

#ifndef BENCH_COMPILE_MODE
#define BENCH_COMPILE_MODE 1
#endif

#if BENCH_OPTIONAL_IMPL == 0
namespace bench
{
template <typename T> struct optional
{
  bool engaged;
  T value;
};
} // namespace bench
#define BENCH_COMPILE_TIME_BASELINE 1
#else
#include "benchmarks/optional/bench_optional_impl.hpp"
#define BENCH_COMPILE_TIME_BASELINE 0
#endif

int
main ()
{
  int result = 0;
#if BENCH_COMPILE_TIME_BASELINE
  bench::optional<int> a = { true, 1 };
  bench::optional<std::string> b = { true, std::string ("x") };
  std::vector<bench::optional<int>> v (4, a);
  result += a.value + static_cast<int> (b.value.size () + v.size ());
#elif BENCH_COMPILE_MODE == 0
  bench::optional<int> a;
  result += a ? 1 : 0;
#else
  bench::optional<int> a;
  bench::optional<int> b (7);
  bench::optional<std::string> s (std::string ("abc"));
  bench::optional<std::string> t;
  t = s;
  t = std::move (s);
  t.emplace ("def");
  result += b.value () + *b + b.value_or (3) + (a ? 1 : 0);
  result
      += static_cast<int> (t->size ()) + static_cast<int> (t.value ().size ());
  a = b;
  a.reset ();
  a = BENCH_NULLOPT;
  result += (a == b) + (a != b) + (a < b) + (a >= b) + (a == BENCH_NULLOPT)
            + (b == 7) + (3 < b);
  swap (a, b);
  std::vector<bench::optional<int>> v (4, b);
  v.push_back (a);
  std::vector<bench::optional<std::string>> w;
  w.push_back (t);
  w.push_back (bench::optional<std::string> ());
  result += static_cast<int> (v.size () + w.size ());
#if BENCH_HAS_HASH
  std::unordered_set<bench::optional<int>> set;
  set.insert (b);
  result += static_cast<int> (set.size ());
#endif
#endif
  return result == 0 ? 1 : 0;
}
