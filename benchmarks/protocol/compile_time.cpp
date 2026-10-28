// compile_time.cpp
// The translation unit compile_time.py compiles with -fsyntax-only (or /Zs)
// to time what a protocol table costs. BENCH_PROTOCOL_TABLE is 0 (the module
// headers only), 1 (the 40-field layout of the tests), 2 (the layout and the
// 100-row protocol of the tests), 3 (as 2, plus the run-time entries of all
// 100 rows and the client methods of three of them), 4 (a layout of 40
// fields with every field kind, bench_protocol_fields.hpp) or 5 (as 4, plus
// its encoder and decoder in the binary, hexadecimal and tokenized layouts),
// 6 (the largest test table of a device, device B of
// lumex/tests/core/protocol/devices with the family table of device A it
// extends: ten protocol layers, five registries) or 7 (as 6, plus the
// run-time index of six of these layers with their registries: those of
// device B but the aligned variant of the family table, which is only
// checked for its alignment, and the family table).
#include <cstddef>

// Tables 6 and 7 are measured against the headers alone (table 0).
#if BENCH_PROTOCOL_TABLE >= 6
#if BENCH_PROTOCOL_TABLE == 6
#define BENCH_DEVICE_TABLE 6
#else
#define BENCH_DEVICE_TABLE 7
#endif
#undef BENCH_PROTOCOL_TABLE
#define BENCH_PROTOCOL_TABLE 0
#else
#define BENCH_DEVICE_TABLE 0
#endif

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

#if BENCH_DEVICE_TABLE >= 6
#include "lumex/tests/core/protocol/devices/device_b/LumexProtocolDeviceB.hpp"
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

using sink = field::record_of<sink_t::fields_type>::type;

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
#if BENCH_DEVICE_TABLE >= 6
  result += lumex::core::protocol::dsl::command_count<
      lumex_tests::device_b::device_b2>::value;
  result += lumex::core::protocol::dsl::command_count<
      lumex_tests::device_b::device_b1>::value;
#endif
#if BENCH_DEVICE_TABLE >= 7
  {
    namespace index = lumex::core::protocol::index;
    namespace b = lumex_tests::device_b;
    result += index::build_index<b::device_b2, b::b2_work, b::b2_service,
                                 b::b2_floating> ()
                  .has_value ();
    result += index::build_index<b::device_b2_m2> ().has_value ();
    result += index::build_index<b::b2_system> ().has_value ();
    result += index::build_index<b::device_b1, b::b1_params_unique> ()
                  .has_value ();
    result += index::build_index<b::device_b1_rx> ().has_value ();
    result
        += index::build_index<lumex_tests::device_a::family_a> ().has_value ();
  }
#endif
  return static_cast<int> (result % 2);
}
