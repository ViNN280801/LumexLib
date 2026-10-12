// compile_time.cpp
// The translation unit run_benchmark.py compiles to time what it costs to
// include and use a string view. BENCH_STRING_VIEW_IMPL is 1 (this library),
// 2 (std::string_view), 3 (boost::string_view), or 0 for the same unit with a
// stub instead of a view, which is the cost of the headers every variant
// shares. BENCH_COMPILE_MODE is 0 to include the header and declare one
// object only, 1 to use the whole API (construction, observers, searches,
// comparisons, slicing, containers, hashing where the view has it).
#include <algorithm>
#include <map>
#include <string>
#include <vector>

#ifndef BENCH_COMPILE_MODE
#define BENCH_COMPILE_MODE 1
#endif

#if BENCH_STRING_VIEW_IMPL == 0
namespace bench
{
struct view
{
  char const *data;
  std::size_t size;
};
} // namespace bench
#define BENCH_COMPILE_TIME_BASELINE 1
#else
#include "benchmarks/string_view/bench_string_view_impl.hpp"
#define BENCH_COMPILE_TIME_BASELINE 0
#endif

#if BENCH_COMPILE_TIME_BASELINE

bench::view g_view = { "text", 4 };

#elif BENCH_COMPILE_MODE == 0

bench::view g_view ("text");

#else

namespace
{
std::size_t
use (bench::view const &v, std::string const &s)
{
  std::size_t total = 0;
  bench::view const a (s);
  bench::view const b ("literal");
  bench::view const c (s.data (), s.size ());
  total += v.size () + a.size () + b.size () + c.size ();
  total += v.empty () ? 1 : 0;
  total += static_cast<unsigned char> (v[0])
           + static_cast<unsigned char> (v.front ())
           + static_cast<unsigned char> (v.back ());
  for (char const ch : v)
    total += static_cast<unsigned char> (ch);
  total += v.find ('/') + v.find (b) + v.rfind ('.') + v.rfind (b);
  total += v.find_first_of (b) + v.find_last_of (b) + v.find_first_not_of (b)
           + v.find_last_not_of (b);
  total += v.substr (1, 2).size () + static_cast<std::size_t> (v.compare (b));
  total += (v == b) + (v != b) + (v < b) + (v > b) + (v <= b) + (v >= b);
  total += (v == "text") + (v == s);
  bench::view w = v;
  w.remove_prefix (1);
  w.remove_suffix (1);
  std::string const owned (w.data (), w.size ());
  total += owned.size ();
#if !(BENCH_STRING_VIEW_IMPL == BENCH_STRING_VIEW_STD && __cplusplus < 202002L)
  total += v.starts_with (b) + v.ends_with (b) + v.starts_with ('t')
           + v.ends_with ('t');
#endif
#if BENCH_HAS_HASH
  total += bench::hash_of (v);
#endif
  std::vector<bench::view> list (4, v);
  std::sort (list.begin (), list.end ());
  std::map<bench::view, int> map;
  map[v] = 1;
  total += map.size () + list.size ();
  return total;
}
} // namespace

std::size_t
use_it (bench::view const &v, std::string const &s)
{
  return use (v, s);
}

#endif
