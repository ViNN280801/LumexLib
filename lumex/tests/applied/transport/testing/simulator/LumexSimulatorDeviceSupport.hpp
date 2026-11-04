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
  ok (bytes_t const &frame, field::decode_context_t const *,
      typename layout::request_of<Row>::type const &)
  {
    return frame.empty ();
  }
};

// A field or a layout of the core.
template <typename P, typename Row> struct reply_check_t<P, Row, void, 1>
{
  // The default reply is the sample of the reply, patched as the row says:
  // the registry entries the request named, the state of the device for the
  // row of `state_from`. (The patch is the simulator's own code, which the
  // tests of the lab table pin; what this proves for a device table is that
  // the reply of every row is the sample and not an empty or another value.)
  static bool
  ok (bytes_t const &frame, field::decode_context_t const *context,
      typename layout::request_of<Row>::type const &request)
  {
    namespace sd = lumex::applied::transport::testing::simulator_detail;
    using request_type = typename layout::request_of<Row>::type;
    typename layout::reply_value_of<Row>::type expected
        = field::sample_value<typename Row::reply_type> ();
    sd::request_call_t call;
    sd::registry_store_t store;
    call.store = &store;
    call.context = context;
    sd::request_registry_t<Row, request_type>::prepare (call, request);
    sd::reply_patch_t<Row>::apply (expected, call);
    sd::state_patch_t<P, Row>::apply (expected, 0);
    bytes_t wanted;
    return layout::encode_reply_message<P, Row> (wanted, expected, context)
               .has_value ()
           && wanted == frame;
  }
};

// A reply kind of the dialogue.
template <typename P, typename Row> struct reply_check_t<P, Row, void, 2>
{
  static bool
  ok (bytes_t const &frame, field::decode_context_t const *context,
      typename layout::request_of<Row>::type const &)
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
  ok (bytes_t const &frame, field::decode_context_t const *context,
      typename layout::request_of<Row>::type const &)
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
  else if (!reply_check_t<P, Row>::ok (out->frame, &context, request))
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
  // A row accepted only in states that do not include the initial one (0) is
  // refused by the device: the request goes unanswered. The tables have such
  // rows only without a reply (a command), so the exchange still ends; the
  // count says how many, so that a refusal for any other reason shows.
  std::size_t expected_refused = 0;
  index::protocol_index_t const &where = bench_t<P>::index_of ();
  for (std::size_t i = 0; i < where.rows ().size (); ++i)
    {
      dsl::states_t const states
          = where.rows ().data ()[i].entry->options.accepted_states;
      bool initial = states.size () == 0;
      for (std::size_t n = 0; n < states.size (); ++n)
        initial = initial || states.data ()[n] == 0;
      if (!initial)
        ++expected_refused;
    }
  EXPECT_EQ (counted.refused, expected_refused) << P::protocol_name ();
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
