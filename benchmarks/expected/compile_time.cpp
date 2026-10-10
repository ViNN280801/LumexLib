// compile_time.cpp
// One translation unit for the compile-time table: it includes the header of
// the result type under test (BENCH_EXPECTED_IMPL, see
// bench_expected_impl.hpp; 0 includes only the standard headers the others
// need, for the baseline) and uses the common part of the API: construction,
// copy, move, assignment, value (), error (), has_value (), swap and, where
// the type has them, value_or and the monadic operations, for expected<int,
// int>, expected<std::string, std::string> and expected<void, int>.
// run_benchmark.py --compile-time measures the wall time of
// `-fsyntax-only -O0` and of `-c -O2` of this file.
#include <string>
#include <utility>
#include <vector>

#if BENCH_EXPECTED_IMPL != 0
#include "bench_expected_impl.hpp"
#endif

#if BENCH_EXPECTED_IMPL != 0

namespace
{
using int_result = bench::expected<int, unsigned>;
struct text_error
{
  std::string text;
  text_error (std::string value) : text (std::move (value)) {}
};

using text_result = bench::expected<std::string, text_error>;
using void_result = bench::expected<void, int>;

int
use_int (int n)
{
  int_result a = bench::ok<int_result> (n);
  int_result b = bench::err<int_result> (static_cast<unsigned> (n + 1));
  int_result c (a);
  int_result d (std::move (b));
  c = d;
  d = std::move (a);
  swap (c, d);
  int total = c.has_value () ? c.value () : static_cast<int> (c.error ());
  total += d.has_value () ? bench::deref (d) : static_cast<int> (d.error ());
#if BENCH_EXPECTED_HAS_VALUE_OR
  total += c.value_or (3);
#endif
#if BENCH_EXPECTED_HAS_MONADIC
  total += c.transform ([] (int v) { return v + 1; })
               .and_then ([] (int v) { return bench::ok<int_result> (v * 2); })
               .or_else (
                   [] (unsigned e)
                     { return bench::ok<int_result> (static_cast<int> (e)); })
               .value ();
#endif
  return total;
}

std::size_t
use_text (std::string const &text)
{
  text_result a = bench::ok<text_result> (text);
  text_result b = bench::err<text_result> (text + "!");
  text_result c (a);
  c = b;
  std::size_t total
      = c.has_value () ? c.value ().size () : c.error ().text.size ();
  total += a.has_value () ? bench::deref (a).size () : 0;
#if BENCH_EXPECTED_HAS_MONADIC
  total += a.transform ([] (std::string const &s) { return s.size (); })
               .value_or (0);
#endif
  return total;
}

int
use_void (int n)
{
  void_result a
      = n > 0 ? bench::ok_void<void_result> () : bench::err<void_result> (n);
  void_result b (a);
  return b.has_value () ? 0 : b.error ();
}
} // namespace

int
compile_time_entry (int n, std::string const &text)
{
  return use_int (n) + static_cast<int> (use_text (text)) + use_void (n);
}

#else

int
compile_time_entry (int n, std::string const &text)
{
  return n + static_cast<int> (text.size ());
}

#endif
