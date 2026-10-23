// compile_time.cpp
// The translation unit compile_time.py compiles with -fsyntax-only (or /Zs)
// to time what a protocol table costs. BENCH_PROTOCOL_TABLE is 0 (the module
// headers only), 1 (the 40-field layout of the tests), 2 (the layout and the
// 100-row protocol of the tests), 3 (as 2, plus the run-time entries of all
// 100 rows and the client methods of three of them), 4 (a layout of 40
// fields with every field kind, bench_protocol_fields.hpp) or 5 (as 4, plus
// its encoder and decoder in the binary, hexadecimal and tokenized layouts).
#include <cstddef>

// Tables 4 and 5 are measured against the headers alone (table 0).
#if BENCH_PROTOCOL_TABLE >= 4
#if BENCH_PROTOCOL_TABLE == 4
#define BENCH_FIELDS_TABLE 4
#else
#define BENCH_FIELDS_TABLE 5
#endif
#undef BENCH_PROTOCOL_TABLE
#define BENCH_PROTOCOL_TABLE 0
#else
#define BENCH_FIELDS_TABLE 0
#endif

#include "benchmarks/protocol/bench_protocol_table.hpp"

#if BENCH_FIELDS_TABLE >= 4
#include "benchmarks/protocol/bench_protocol_fields.hpp"
#endif

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

#if BENCH_FIELDS_TABLE >= 5
namespace bench_fields
{
namespace layout = lumex::core::protocol::layout;
namespace field = lumex::core::protocol::field;

typedef field::record_of<sink_t::fields_type>::type sink;

template <typename Layout>
std::size_t
round_trip (sink const &value, field::decode_context_t const &context)
{
  layout::bytes_t bytes;
  if (!layout::encode<Layout, sink_t> (bytes, value, &context))
    return 0;
  return layout::decode<Layout, sink_t> (
             layout::bytes_view_t (bytes.data (), bytes.size ()), &context)
                 .has_value ()
             ? bytes.size ()
             : 0;
}
} // namespace bench_fields
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
#if BENCH_FIELDS_TABLE >= 4
  result += bench_fields::sink_t::fields_type::count::value;
#endif
#if BENCH_FIELDS_TABLE >= 5
  bench_fields::sink const value = bench_fields::sink ();
  lumex::core::protocol::field::decode_context_t const context
      = { nullptr, nullptr };
  result += bench_fields::round_trip<
      lumex::core::protocol::layout::binary_layout<>> (value, context);
  result += bench_fields::round_trip<
      lumex::core::protocol::layout::fixed_width_text_layout<16>> (value,
                                                                   context);
  result += bench_fields::round_trip<
      lumex::core::protocol::layout::tokenized_text_layout<','>> (value,
                                                                  context);
#endif
  return static_cast<int> (result % 2);
}
