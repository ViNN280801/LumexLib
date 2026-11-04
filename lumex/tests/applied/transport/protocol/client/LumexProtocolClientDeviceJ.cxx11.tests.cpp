// One client template over the two wire forms of device J (the de-identified
// table of lumex/tests/core/protocol/devices): the text layer on a stream, the
// binary layer on a datagram transport, the same calls and the same values.
// Every row of both layers goes through a method of the client.

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/utility/sequence/LumexIndexSequence.hpp"
#include "lumex/tests/applied/transport/protocol/client/LumexClientTestSupport.hpp"
#include "lumex/tests/applied/transport/testing/simulator/LumexSimulatorDeviceSupport.hpp"
#include "lumex/tests/core/protocol/devices/device_j/LumexProtocolDeviceJTable.hpp"

namespace
{
using namespace lumex_client_test;
namespace dev = lumex_tests::tables_j;
namespace dsl = lumex::core::protocol::dsl;
namespace sequence = lumex::core::utility::sequence;

// A device of layer P and a client of P over a link the bench makes.
template <typename P> struct wire_t
{
  // A stream.
  wire_t ()
  {
    tr::testing::mock_options_t mock_options;
    mock_options.timers = clock.source ();
    tr::testing::mock_stream mock = tr::testing::make_mock_stream (
        context.get_executor (), mock_options);
    keep_stream.reset (new tr::testing::mock_stream_control (mock.control));
    mock.stream.async_open ([] (std::error_code) {});
    context.poll ();
    context.restart ();
    device.reset (new tr::testing::device_simulator<P> (
        mock.control, fr::slip::slip_codec (), clock));
    client.reset (new pr::client<P> (
        pr::frame_link (tr::framed::framed_stream<fr::slip::slip_codec> (
            std::move (mock.stream), fr::slip::slip_codec ())),
        options ()));
    settle ();
  }

  // A datagram transport, report 1.
  explicit wire_t (std::uint8_t report_id)
  {
    tr::testing::mock_options_t mock_options;
    mock_options.timers = clock.source ();
    tr::testing::mock_datagram mock = tr::testing::make_mock_datagram (
        context.get_executor (), mock_options);
    keep_datagram.reset (
        new tr::testing::mock_datagram_control (mock.control));
    mock.transport.async_open ([] (std::error_code) {});
    context.poll ();
    context.restart ();
    device.reset (new tr::testing::device_simulator<P> (
        mock.control, fr::slip::slip_codec (), report_id, clock));
    client.reset (new pr::client<P> (
        pr::frame_link (
            tr::framed::framed_datagram<fr::slip::slip_codec> (
                std::move (mock.transport), fr::slip::slip_codec ()),
            report_id),
        options ()));
    settle ();
  }

  ~wire_t ()
  {
    client.reset ();
    settle ();
    device.reset ();
    settle ();
  }

  pr::client_options_t
  options ()
  {
    pr::client_options_t made = pr::client_options_for<P> ();
    made.dialogue.timers = clock.source ();
    return made;
  }

  void
  settle ()
  {
    tr::testing::run_until (context, clock, [] () { return false; }, ms (0));
  }

  bool
  run_until (std::function<bool ()> const &done)
  {
    return tr::testing::run_until (context, clock, done, ms (60000));
  }

  asio::io_context context;
  tr::testing::virtual_clock clock;
  std::unique_ptr<tr::testing::mock_stream_control> keep_stream;
  std::unique_ptr<tr::testing::mock_datagram_control> keep_datagram;
  std::unique_ptr<tr::testing::device_simulator<P>> device;
  std::unique_ptr<pr::client<P>> client;
};

// Calls the method of a row with the sample of its request.
template <typename P, typename Row, bool Dangerous, typename Indices>
struct sample_call_t;

template <typename P, typename Row, std::size_t... I>
struct sample_call_t<P, Row, false, sequence::index_sequence<I...>>
{
  template <typename Record, typename Handler>
  static void
  run (pr::client<P> &client, Record const &request, Handler handler)
  {
    client.template lumex_invoke<Row> (field::get<I> (request)..., handler);
  }
};

template <typename P, typename Row, std::size_t... I>
struct sample_call_t<P, Row, true, sequence::index_sequence<I...>>
{
  template <typename Record, typename Handler>
  static void
  run (pr::client<P> &client, Record const &request, Handler handler)
  {
    client.template lumex_invoke<Row> (pr::confirm, field::get<I> (request)...,
                                       handler);
  }
};

template <typename P, typename Row>
void
call_row (wire_t<P> &wire, std::vector<std::string> &failed)
{
  using record = typename layout::request_of<Row>::type;
  record const request = lumex_simulator_test::readable_sample_request<Row> ();
  auto const out = std::make_shared<outcome_of<Row>> ();
  sample_call_t<P, Row, dialogue::detail::row_dangerous_t<Row>::value,
                sequence::make_index_sequence<record::size::value>>::
      run (*wire.client, request, into<Row> (out));
  if (!wire.run_until ([&] () { return out->calls != 0; }))
    failed.push_back (std::string (Row::identifier ()) + ": no end");
  else if (out->error)
    failed.push_back (std::string (Row::identifier ()) + ": "
                      + out->error.message ());
}

template <typename P, typename List> struct each_row_t;

template <typename P, typename... Rows>
struct each_row_t<P, dsl::type_list<Rows...>>
{
  static void
  run (wire_t<P> &wire, std::vector<std::string> &failed)
  {
    int const done[] = { 0, (call_row<P, Rows> (wire, failed), 0)... };
    (void)done;
  }
};

template <typename P>
void
check_every_row (wire_t<P> &wire)
{
  std::vector<std::string> failed;
  each_row_t<P, typename dsl::commands_of<P>::type>::run (wire, failed);
  for (std::size_t i = 0; i < failed.size (); ++i)
    ADD_FAILURE () << P::protocol_name () << ": " << failed[i];
}
} // namespace

TEST (LumexProtocolClientDeviceJ, Client_EveryRowOfTheTextLayerOverAStream)
{
  wire_t<dev::j_text> wire;
  check_every_row (wire);
}

TEST (LumexProtocolClientDeviceJ, Client_EveryRowOfTheBinaryLayerOverADatagram)
{
  wire_t<dev::j_hid> wire (std::uint8_t (1));
  check_every_row (wire);
}

namespace
{
// The same calls through one template: what the application writes once.
template <typename P>
std::vector<std::string>
scenario (wire_t<P> &wire)
{
  std::vector<std::string> values;
  typename pr::reply_of<typename P::stop_35_t> status
      = field::sample_value<typename P::stop_35_t::reply_type> ();
  wire.device->template set_reply<typename P::report_ac_t> (status);
  wire.device->template set_reply<typename P::stop_35_t> (status);
  wire.device->template set_reply<typename P::start_64_t> (status);
  auto const report = std::make_shared<outcome_of<typename P::report_ac_t>> ();
  wire.client->report_ac (into<typename P::report_ac_t> (report));
  auto const start = std::make_shared<outcome_of<typename P::start_64_t>> ();
  wire.client->start_64 (std::uint8_t (3),
                         into<typename P::start_64_t> (start));
  auto const stop = std::make_shared<outcome_of<typename P::stop_35_t>> ();
  wire.client->stop_35 (into<typename P::stop_35_t> (stop));
  EXPECT_TRUE (wire.run_until ([&] () { return stop->calls != 0; }));
  EXPECT_FALSE (report->error) << report->error.message ();
  EXPECT_FALSE (start->error) << start->error.message ();
  EXPECT_FALSE (stop->error) << stop->error.message ();
  values.push_back (report->value == status ? "same" : "other");
  values.push_back (start->value == status ? "same" : "other");
  values.push_back (stop->value == status ? "same" : "other");
  return values;
}
} // namespace

TEST (LumexProtocolClientDeviceJ, Client_SameTableTwoWireForms)
{
  wire_t<dev::j_text> text;
  wire_t<dev::j_hid> binary (std::uint8_t (1));
  std::vector<std::string> const over_text = scenario (text);
  std::vector<std::string> const over_binary = scenario (binary);
  EXPECT_EQ (over_text, over_binary);
  ASSERT_EQ (over_text.size (), 3u);
  EXPECT_EQ (over_text[0], "same");
}
