// compile_time.cpp
// The translation unit compile_time.py compiles with -fsyntax-only (or /Zs)
// to time what a protocol table costs. BENCH_PROTOCOL_TABLE is 0 (the module
// headers only), 1 (the 40-field layout of the tests), 2 (the layout and the
// 100-row protocol of the tests) or 3 (as 2, plus the run-time entries of
// all 100 rows and the client methods of three of them).
#include <cstddef>

#include "benchmarks/protocol/bench_protocol_table.hpp"

#if BENCH_PROTOCOL_TABLE >= 3
namespace bench
{
struct client : large_protocol::lumex_methods<client>
{
  template <typename Row, typename... Args>
  int
  lumex_invoke (Args...)
  {
    return static_cast<int> (Row::key ());
  }
};
} // namespace bench
#endif

int
main ()
{
  std::size_t result = 0;
#if BENCH_PROTOCOL_TABLE >= 1
  result += bench::wide_t::fields_type::count::value;
#endif
#if BENCH_PROTOCOL_TABLE >= 2
  result += lumex::core::protocol::dsl::command_count<
      bench::large_protocol>::value;
#endif
#if BENCH_PROTOCOL_TABLE >= 3
  result
      += lumex::core::protocol::dsl::command_entries<bench::large_protocol> ()
             .size ();
  bench::client user;
  result += static_cast<std::size_t> (user.CMD_000 () + user.CMD_050 (1)
                                      + user.CMD_099 (2));
#endif
  return static_cast<int> (result % 2);
}
