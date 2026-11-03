// The check of the device tables for the device simulator tests: every
// request row of a layer is sent, as the sample request of the row, to a
// simulator of that layer through the dialogue engine; the exchange must end
// without an error and the reply must decode the way the row says (a field
// or a layout of the core, the message of the row `reply_row` names, a reply
// kind of the dialogue, nothing for a row without a reply). The tables are
// the de-identified ones of lumex/tests/core/protocol/devices, referred to
// as devices A to M; nothing here names a real device.
#ifndef LUMEX_TESTS_APPLIED_TRANSPORT_TESTING_SIMULATOR_DEVICE_SUPPORT_HPP
#define LUMEX_TESTS_APPLIED_TRANSPORT_TESTING_SIMULATOR_DEVICE_SUPPORT_HPP

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/tests/applied/transport/testing/simulator/LumexSimulatorTestSupport.hpp"

namespace lumex_simulator_test
{
namespace dsl = lumex::core::protocol::dsl;
namespace field = lumex::core::protocol::field;

using prepare_t = std::function<void (index::session_context_t &)>;

// How the reply of a row is read.
template <typename P, typename Row,
          typename ReplyRow = typename dialogue::detail::reply_row_of_t<
              typename Row::options_type>::type,
          int Form = dialogue::detail::reply_form_t<Row>::value>
struct reply_check_t;

// No reply: the exchange ended with the write.
template <typename P, typename Row> struct reply_check_t<P, Row, void, 0>
{
  static bool
  ok (bytes_t const &frame, field::decode_context_t const *)
  {
    return frame.empty ();
  }
};

// A field or a layout of the core.
template <typename P, typename Row> struct reply_check_t<P, Row, void, 1>
{
  static bool
  ok (bytes_t const &frame, field::decode_context_t const *context)
  {
    return layout::decode_reply_message<P, Row> (view_of (frame), context)
        .has_value ();
  }
};

// A reply kind of the dialogue.
template <typename P, typename Row> struct reply_check_t<P, Row, void, 2>
{
  static bool
  ok (bytes_t const &frame, field::decode_context_t const *context)
  {
    return dialogue::decode_reply_kind<P, Row> (view_of (frame), context)
        .has_value ();
  }
};

// The message of another row.
template <typename P, typename Row, typename ReplyRow>
struct reply_check_t<P, Row, ReplyRow, 0>
{
  static bool
  ok (bytes_t const &frame, field::decode_context_t const *context)
  {
    return layout::decode_reply_message<P, ReplyRow> (view_of (frame), context)
        .has_value ();
  }
};

template <typename P, typename Row>
void
check_row (bench_t<P> &bench, std::vector<std::string> &failed)
{
  using request_type = typename layout::request_of<Row>::type;
  field::decode_context_t const context
      = bench.device->session ().decode_context ();
  request_type const request = readable_sample_request<Row> ();
  outcome_ptr const out = bench.start_exchange (
      bench.template make_exchange_of<Row> (request, &context));
  bool const ended = bench.run_until ([&] () { return out->calls != 0; });
  std::string const name = Row::identifier ();
  if (!ended)
    failed.push_back (name + ": no end");
  else if (out->error)
    failed.push_back (name + ": " + out->error.message ());
  else if (!reply_check_t<P, Row>::ok (out->frame, &context))
    failed.push_back (name + ": the reply does not decode");
}

template <typename P, typename List> struct each_row_t;

template <typename P, typename... Rows>
struct each_row_t<P, dsl::type_list<Rows...>>
{
  static void
  run (bench_t<P> &bench, std::vector<std::string> &failed)
  {
    int const done[] = { 0, (check_row<P, Rows> (bench, failed), 0)... };
    (void)done;
  }

  static std::size_t
  count () LUMEX_NOEXCEPT
  {
    return sizeof...(Rows);
  }
};

template <typename P>
void
check_rows (bench_t<P> &bench, prepare_t const &prepare)
{
  if (prepare)
    prepare (bench.device->session ());
  std::vector<std::string> failed;
  using rows = each_row_t<P, typename dsl::commands_of<P>::type>;
  rows::run (bench, failed);
  for (std::size_t i = 0; i < failed.size (); ++i)
    ADD_FAILURE () << P::protocol_name () << ": " << failed[i];
  tr::testing::simulator_statistics_t const counted
      = bench.device->statistics ();
  EXPECT_EQ (counted.requests, rows::count ()) << P::protocol_name ();
  EXPECT_EQ (counted.malformed_requests, 0u) << P::protocol_name ();
  EXPECT_EQ (counted.unknown_requests, 0u) << P::protocol_name ();
  EXPECT_EQ (counted.encode_errors, 0u) << P::protocol_name ();
}

/// Every row of layer `P` over a mock stream.
template <typename P>
void
check_layer (prepare_t const &prepare = prepare_t ())
{
  bench_t<P> bench;
  check_rows<P> (bench, prepare);
}

/// Every row of layer `P` over a mock datagram transport (report ID 1).
template <typename P>
void
check_datagram_layer (prepare_t const &prepare = prepare_t ())
{
  bench_t<P> bench (std::uint8_t (1));
  check_rows<P> (bench, prepare);
}
} // namespace lumex_simulator_test

#endif // !LUMEX_TESTS_APPLIED_TRANSPORT_TESTING_SIMULATOR_DEVICE_SUPPORT_HPP
