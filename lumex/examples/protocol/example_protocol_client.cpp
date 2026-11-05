// The client that the table of a protocol generates, over a real TCP
// connection on the loopback interface. The device is an invented stage lamp
// (example_protocol_lamp.hpp) that a testing::device_simulator makes from the
// same table; the program never sees the simulator on the wire, only bytes.
// The table has two layers, one wire form with hexadecimal digits in a line
// and one with bytes behind a length byte, and one function template runs the
// same calls on both clients. Shown here: identify (), a method per row in the
// synchronous form, with a callback and as a std::future, a reply the device
// refuses, a lost reply that the client asks for again (retries counted), a
// dangerous row that needs confirm, and the counters of both ends.
#include <atomic>
#include <chrono>
#include <cstdint>
#include <future>
#include <iostream>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "example_protocol_lamp.hpp"
#include "example_protocol_loopback.hpp"

#if defined(LUMEX_TRANSPORT_WITH_PROTOCOL)                                    \
    && defined(LUMEX_TRANSPORT_WITH_TESTING)
namespace pr = lumex::applied::transport::protocol;
namespace tr = lumex::applied::transport;

namespace
{
namespace field = lumex::core::protocol::field;

// The transport log of this binary, restricted to the dialogue: the lines of
// the engine are the frames in the words of the table.
void
print_record (tr::log::log_record_t const &record)
{
  std::cout << "    [log] " << record.message << '\n';
}

// What the simulated lamp remembers between two requests: the brightness it
// was given and whether a channel runs. The simulator calls these handlers on
// the thread of the device; the main thread only reads the replies.
template <typename P> class lamp_model_t
{
public:
  explicit lamp_model_t (loopback::device_t<P> &device)
      : device_ (device), level_raw_ (0)
  {
    // Who the device is: StageLamp-3, firmware 12, serial 4711.
    device_.simulator ().template set_reply<typename P::WHO_t> (
        lamp::who_reply<P> (3, 12, 4711));
    device_.simulator ().template on<typename P::STATUS_t> (
        [this] (field::record<> const &) { return status (); });
    device_.simulator ().template on<typename P::SET_LEVEL_t> (
        [this] (typename tr::testing::device_simulator<P>::template request_of<
                typename P::SET_LEVEL_t> const &request)
          {
            level_raw_ = field::get<0> (request).raw;
            return status ();
          });
    // A started channel runs: the state decides what the device accepts.
    device_.simulator ().template on<typename P::START_t> (
        [this] (typename tr::testing::device_simulator<P>::template request_of<
                typename P::START_t> const &request)
          {
            device_.simulator ().set_state (1);
            pr::reply_of<typename P::START_t> started;
            field::get<0> (started) = field::get<0> (request);
            return started;
          });
  }

private:
  pr::reply_of<typename P::STATUS_t>
  status () const
  {
    pr::reply_of<typename P::STATUS_t> out;
    field::get<0> (out) = device_.simulator ().state () == 1
                              ? lamp::run_state::on
                              : lamp::run_state::off;
    field::get<1> (out).raw = level_raw_;
    field::get<2> (out) = 1500;
    return out;
  }

  loopback::device_t<P> &device_;
  std::atomic<std::uint16_t> level_raw_;
};

// The same calls for every layer of the table: what an application writes
// once.
template <typename P>
bool
talk (char const *title, loopback::device_t<P> &device,
      tr::stream::io_runner &runner)
{
  std::cout << "=== " << title << " ===\n";
  lamp_model_t<P> model (device);

  // The stream and the framing are the only places that know the wire.
  tr::net::tcp_options_t tcp_options;
  tcp_options.host = "127.0.0.1";
  tcp_options.port = device.port ();
  tr::stream::byte_stream stream
      = tr::net::make_tcp_stream (runner.get_executor (), tcp_options);
  if (!stream.open (tr::deadline_t (3000)).has_value ())
    {
      std::cout << "cannot connect\n";
      return false;
    }
  using codec_type = typename lamp::wire_t<P>::codec_type;
  pr::client_options_t options = pr::client_options_for<P> ();
  options.dialogue.name = title;
  pr::client<P> client (
      pr::frame_link (tr::framed::framed_stream<codec_type> (
          std::move (stream), lamp::wire_t<P>::make_codec ())),
      options);

  std::cout << "--- identify (): WHO, read as an identity\n";
  pr::identity_result_t const who = client.identify ();
  if (!who.has_value ())
    {
      std::cout << "identify failed: " << who.error ().message () << '\n';
      return false;
    }
  std::cout << "  model " << who.value ().model << ", firmware "
            << who.value ().firmware << ", serial "
            << who.value ().serial_number << ", layer " << who.value ().layer
            << '\n';

  std::cout << "--- synchronous calls: one method per row\n";
  pr::call_result_t<typename P::STATUS_t> const status = client.STATUS ();
  if (!status.has_value ())
    {
      std::cout << "STATUS failed: " << status.error ().message () << '\n';
      return false;
    }
  std::cout << "  STATUS () -> " << lamp::show_status<P> (status.value ())
            << '\n';
  // An argument is the engineering value of its field (tenths on the wire).
  pr::call_result_t<typename P::SET_LEVEL_t> const level
      = client.SET_LEVEL (62.5, tr::deadline_t (500));
  std::cout << "  SET_LEVEL (62.5, deadline 500 ms) -> "
            << (level.has_value () ? "accepted" : level.error ().message ())
            << '\n';
  pr::call_result_t<typename P::START_t> const started = client.START (1);
  std::cout << "  START (1) -> "
            << (started.has_value ()
                    ? "channel "
                          + std::to_string (
                              int (field::get<0> (started.value ())))
                    : started.error ().message ())
            << '\n';

  std::cout << "--- the channel runs now, so START again is refused (NAK)\n";
  pr::call_result_t<typename P::START_t> const again = client.START (2);
  std::cout << "  START (2) -> "
            << (again.has_value () ? "accepted" : again.error ().message ())
            << '\n';

  std::cout << "--- asynchronous calls: a callback and a future\n";
  std::promise<std::string> reported;
  client.STATUS (
      [&reported] (std::error_code error,
                   pr::reply_of<typename P::STATUS_t> value)
        {
          reported.set_value (error ? error.message ()
                                    : lamp::show_status<P> (value));
        });
  std::cout << "  callback: " << reported.get_future ().get () << '\n';
  try
    {
      std::future<pr::reply_of<typename P::STATUS_t>> later
          = client.STATUS (asio::use_future);
      std::cout << "  future:   " << lamp::show_status<P> (later.get ())
                << '\n';
    }
  catch (std::system_error const &error)
    {
      std::cout << "  future failed: " << error.code ().message () << '\n';
    }

  std::cout << "--- a lost reply: the client asks again (retries (2) of "
               "STATUS)\n";
  pr::dialogue_statistics_t const before = client.statistics ();
  // The device loses the first reply it would send and is perfect after it.
  device.simulator ().set_behavior (
      tr::testing::behavior::intermittent (1, 1000, 1));
  pr::call_result_t<typename P::STATUS_t> const retried = client.STATUS ();
  device.simulator ().set_behavior (tr::testing::behavior::perfect ());
  pr::dialogue_statistics_t const after = client.statistics ();
  std::cout << "  STATUS () -> "
            << (retried.has_value () ? lamp::show_status<P> (retried.value ())
                                     : retried.error ().message ())
            << "\n  requests written " << after.requests - before.requests
            << ", timeouts " << after.timeouts - before.timeouts
            << ", retries " << after.retries - before.retries << '\n';

  std::cout << "--- a dangerous row: the first argument is confirm\n";
  // client.ERASE_LOG (3) does not compile: a row marked dangerous asks for
  // the confirmation in the call itself.
  pr::call_result_t<typename P::ERASE_LOG_t> const erased
      = client.ERASE_LOG (pr::confirm, 3);
  std::cout << "  ERASE_LOG (confirm, 3) -> "
            << (erased.has_value () ? "done" : erased.error ().message ())
            << '\n';

  std::cout << "--- the counters\n";
  pr::dialogue_statistics_t const total = client.statistics ();
  std::cout << "  client: requests " << total.requests << ", replies "
            << total.replies << ", retries " << total.retries << ", timeouts "
            << total.timeouts << ", naks " << total.naks << ", late replies "
            << total.late_replies << '\n';
  tr::testing::simulator_statistics_t const seen
      = device.simulator ().statistics ();
  std::cout << "  device: requests " << seen.requests << ", replies "
            << seen.replies << ", refused " << seen.refused
            << ", START requested " << seen.requests_of ("START") << " time(s)"
            << '\n';
  client.close ();
  return retried.has_value () && total.retries == before.retries + 1
         && again.has_value () == false;
}
} // namespace

int
main ()
{
  std::cout << "=== lumex::transport: a generated client over loopback TCP "
               "===\n\n";
  // The lines of the dialogue, frames as the table describes them.
  tr::log::set_log_sink (&print_record);
  tr::log::set_log_level (tr::log::log_level::info);
  std::vector<std::string> allow;
  allow.push_back ("transport.dialogue");
  tr::log::set_component_filter (allow, std::vector<std::string> ());

  // One thread runs the streams; the synchronous calls wait on this one.
  tr::stream::io_runner runner (1);
  bool ok = true;
  {
    loopback::device_t<lamp::lamp_text> device (
        lamp::wire_t<lamp::lamp_text>::make_codec ());
    ok = talk<lamp::lamp_text> ("text form: hex digits in a line", device,
                                runner)
         && ok;
  }
  std::cout << '\n';
  {
    loopback::device_t<lamp::lamp_binary> device (
        lamp::wire_t<lamp::lamp_binary>::make_codec ());
    ok = talk<lamp::lamp_binary> ("binary form: bytes behind a length", device,
                                  runner)
         && ok;
  }
  return ok ? 0 : 1;
}
#else
int
main ()
{
  std::cout << "built without the dialogue layer or the testing support "
               "(LUMEX_BUILD_PROTOCOL, LUMEX_BUILD_FRAMING, "
               "LUMEX_TRANSPORT_WITH_TESTING): nothing to show\n";
  return 0;
}
#endif
