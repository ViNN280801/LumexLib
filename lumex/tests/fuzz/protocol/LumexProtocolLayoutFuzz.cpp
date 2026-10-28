// libFuzzer target of the protocol layout decoders (lumex/core/protocol/
// layout): the first byte of an input chooses a layout and a row of the
// synthetic gauge table
// (lumex/tests/core/protocol/LumexProtocolFieldTables.hpp: the 40-field sink
// layout with every field kind, a request with scaled and signed fields, a
// text-key request), the rest is the message. A decoder must never read past
// the input (AddressSanitizer), and a message that decodes must encode to
// bytes that decode and encode to the same bytes again; anything else stops
// the run. Built with LUMEX_BUILD_FUZZERS (Clang only); CTest runs it for a
// short bounded time under the label fuzz.

#include <cstddef>
#include <cstdint>
#include <cstdlib>

#include "lumex/core/protocol/LumexProtocol"
#include "lumex/tests/core/protocol/LumexProtocolFieldTables.hpp"

namespace
{
namespace field = lumex::core::protocol::field;
namespace layout = lumex::core::protocol::layout;
namespace tables = lumex_tests::field_tables;

void
require (bool condition)
{
  if (!condition)
    std::abort ();
}

template <typename P, typename Row>
void
check_reply (layout::bytes_view_t message,
             field::decode_context_t const &context)
{
  using value_type = typename layout::reply_value_of<Row>::type;
  layout::decode_result_t<value_type> const first
      = layout::decode_reply<P, Row> (message, &context);
  if (!first.has_value ())
    return;
  layout::bytes_t once;
  require (layout::encode_reply<P, Row> (once, *first, &context).has_value ());
  layout::decode_result_t<value_type> const second
      = layout::decode_reply<P, Row> (
          layout::bytes_view_t (once.data (), once.size ()), &context);
  require (second.has_value ());
  layout::bytes_t twice;
  require (
      layout::encode_reply<P, Row> (twice, *second, &context).has_value ());
  require (once == twice);
}

template <typename P, typename Row>
void
check_request (layout::bytes_view_t message)
{
  using value_type = typename layout::request_of<Row>::type;
  layout::decode_result_t<value_type> const first
      = layout::decode_request<P, Row> (message);
  if (!first.has_value ())
    return;
  layout::bytes_t once;
  require (layout::encode_request_record<P, Row> (once, *first).has_value ());
  layout::decode_result_t<value_type> const second
      = layout::decode_request<P, Row> (
          layout::bytes_view_t (once.data (), once.size ()));
  require (second.has_value ());
  layout::bytes_t twice;
  require (
      layout::encode_request_record<P, Row> (twice, *second).has_value ());
  require (once == twice);
}
} // namespace

extern "C" int
LLVMFuzzerTestOneInput (std::uint8_t const *data, std::size_t size)
{
  if (size == 0)
    return 0;
  // An exact copy: a read past the message is a read past this buffer.
  layout::bytes_t const message (data + 1, data + size);
  layout::bytes_view_t const view (message.data (), message.size ());
  tables::test_session const session
      = { static_cast<std::int64_t> (1 + (data[0] >> 7)), true };
  field::decode_context_t const context = session.context ();
  switch (data[0] % 8u)
    {
    case 0:
      check_reply<tables::gauge, tables::gauge::READ_SINK_t> (view, context);
      break;
    case 1:
      check_reply<tables::gauge_hex, tables::gauge::READ_SINK_t> (view,
                                                                  context);
      break;
    case 2:
      check_reply<tables::gauge_text, tables::gauge::READ_SINK_t> (view,
                                                                   context);
      break;
    case 3:
      check_request<tables::gauge, tables::gauge::SET_LEVEL_t> (view);
      break;
    case 4:
      check_request<tables::gauge_decimal, tables::gauge::SET_LEVEL_t> (view);
      break;
    case 5:
      check_request<tables::gauge_text, tables::gauge::SET_LEVEL_t> (view);
      break;
    case 6:
      check_request<tables::gauge_text, tables::gauge::SET_NAME_t> (view);
      break;
    default:
      check_request<tables::gauge_hex, tables::gauge::SET_NAME_t> (view);
      break;
    }
  return 0;
}
