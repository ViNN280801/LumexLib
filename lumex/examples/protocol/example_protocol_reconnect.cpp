// A client that outlives a connection, and a search for devices. Both use
// simulated lamps (example_protocol_lamp.hpp) behind TCP ports of the loopback
// interface, so the program runs anywhere without hardware.
//
// Part 1: the client is built over a stream_session (as_byte_stream () makes
// the session a byte stream that a framed adapter and a frame_link can use).
// The device drops the connection while a request is in flight (a handler of
// the row does it); the session opens a new one, the client identifies the
// device again before anything else, and on_reconnect says how that went. A
// request that may be sent twice (STATUS, idempotent) goes out again on the
// new connection and the device sees it twice; one that may not (START) ends
// with link_lost, never silently sent twice. The program checks each of these
// claims and exits with 1 when one fails.
//
// Part 2: async_discover tries candidates one by one (here five addresses of
// the loopback interface), asks each for its identity with the table's
// identification row and tells for each why it is, or is not, a device of the
// table: found, a device that answered but is no lamp of ours (wrong_device),
// one that never answered (no_response), a closed port (the system's own
// error).
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
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
namespace ss = lumex::applied::transport::session;

namespace
{
using ms = std::chrono::milliseconds;
using lumex::core::protocol::error::protocol_errc;

// The lines of the dialogue and of the session, with the other output.
std::mutex &
output_mutex ()
{
  static std::mutex made;
  return made;
}

void
say (std::string const &line)
{
  std::lock_guard<std::mutex> const lock (output_mutex ());
  std::cout << line << '\n';
}

void
print_record (tr::log::log_record_t const &record)
{
  say (std::string ("    [log ") + record.component + "] " + record.message);
}

// Counts events and lets a thread wait for the n-th.
class events_t
{
public:
  events_t () : count_ (0) {}

  void
  note ()
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    ++count_;
    changed_.notify_all ();
  }

  bool
  wait_for (int wanted, ms limit)
  {
    std::unique_lock<std::mutex> lock (mutex_);
    return changed_.wait_for (lock, limit,
                              [this, wanted] { return count_ >= wanted; });
  }

private:
  std::mutex mutex_;
  std::condition_variable changed_;
  int count_;
};

using status_value = pr::reply_of<lamp::lamp_text::STATUS_t>;
using start_value = pr::reply_of<lamp::lamp_text::START_t>;

bool
reconnecting_client ()
{
  say ("--- 1. A client over a session that reconnects");
  loopback::device_t<lamp::lamp_text> device (
      lamp::wire_t<lamp::lamp_text>::make_codec ());
  device.simulator ().set_reply<lamp::lamp_text::WHO_t> (
      lamp::who_reply<lamp::lamp_text> (3, 12, 4711));

  // What the callbacks write and the main thread reads. They are declared
  // before the session and the client, so they outlive every callback.
  events_t reconnected;
  events_t ended;
  std::error_code start_error;
  std::error_code status_error;
  std::string status_text;
  std::error_code reconnect_errors[2];
  std::string reconnect_models[2];
  int reconnect_count = 0;
  // The device drops the connection the first time it is asked for each of
  // these rows, and counts how often it was asked.
  std::atomic<int> starts_seen (0);
  std::atomic<int> statuses_seen (0);
  device.simulator ().on<lamp::lamp_text::START_t> (
      [&] (tr::testing::device_simulator<lamp::lamp_text>::request_of<
           lamp::lamp_text::START_t> const &)
        {
          if (starts_seen++ == 0)
            device.drop_connection ();
          return start_value ();
        });
  device.simulator ().on<lamp::lamp_text::STATUS_t> (
      [&] (lumex::core::protocol::field::record<> const &)
        {
          if (statuses_seen++ == 0)
            device.drop_connection ();
          return status_value ();
        });

  tr::stream::io_runner runner (1);
  ss::session_options_t session_options;
  session_options.name = "lamp";
  session_options.policy.initial_delay = ms (50);
  session_options.policy.jitter = 0.0;
  ss::net_selector_t where;
  where.host = "127.0.0.1";
  where.port = device.port ();
  // The session is declared before the client, so it outlives it.
  ss::stream_session session (runner.get_executor (),
                              ss::make_tcp_factory (where), session_options);
  if (!session.open (tr::deadline_t (3000)).has_value ())
    {
      say ("  the session did not open");
      return false;
    }

  pr::client_options_t options = pr::client_options_for<lamp::lamp_text> ();
  options.dialogue.name = "lamp";
  // How long a call that survived the loss of the link waits for the device.
  options.dialogue.reconnect_wait_ms = 5000;
  pr::client<lamp::lamp_text> client (
      pr::frame_link (tr::framed::framed_stream<lamp::text_codec_t> (
          session.as_byte_stream (),
          lamp::wire_t<lamp::lamp_text>::make_codec ())),
      options);

  client.on_reconnect (
      [&] (std::error_code const &error, pr::identity_t const &who)
        {
          say (std::string ("  on_reconnect: ")
               + (error ? error.message () : "the device is checked") + ", "
               + who.model + " serial " + who.serial_number);
          if (reconnect_count < 2)
            {
              reconnect_errors[reconnect_count] = error;
              reconnect_models[reconnect_count] = who.model;
            }
          ++reconnect_count;
          reconnected.note ();
        });

  pr::identity_result_t const first = client.identify ();
  if (!first.has_value ())
    {
      say ("  identify failed: " + first.error ().message ());
      client.close ();
      return false;
    }
  say ("  identified: " + first.value ().model + ", firmware "
       + std::to_string (first.value ().firmware));

  // START may not be sent twice: the device drops the connection while it
  // waits for the answer.
  client.START (1,
                [&] (std::error_code error, start_value)
                  {
                    start_error = error;
                    ended.note ();
                  });
  bool const start_ended
      = ended.wait_for (1, ms (5000)) && reconnected.wait_for (1, ms (5000));
  say (std::string ("  START (1), in flight when the device reset the "
                    "connection: ")
       + (start_ended ? start_error.message () : "did not end"));

  // STATUS may be sent twice: the same, and it goes out again.
  client.STATUS (
      [&] (std::error_code error, status_value value)
        {
          status_error = error;
          if (!error)
            status_text = lamp::show_status<lamp::lamp_text> (value);
          ended.note ();
        });
  bool const status_ended
      = ended.wait_for (2, ms (5000)) && reconnected.wait_for (2, ms (5000));
  say (std::string ("  STATUS (), in flight when it reset again, sent "
                    "again: ")
       + (status_ended ? (status_error ? status_error.message () : status_text)
                       : "did not end"));
  pr::dialogue_statistics_t const stats = client.statistics ();
  tr::testing::simulator_statistics_t const seen
      = device.simulator ().statistics ();
  say ("  client counters: reconnects " + std::to_string (stats.reconnects)
       + ", requests " + std::to_string (stats.requests)
       + "; the device was asked START " + std::to_string (starts_seen.load ())
       + " time(s), STATUS " + std::to_string (statuses_seen.load ())
       + " time(s), WHO " + std::to_string (seen.requests_of ("WHO"))
       + " time(s)");
  client.close ();
  session.close ();
  // Three WHO: the first identify () and one check per new connection.
  return start_ended && status_ended && start_error == protocol_errc::link_lost
         && !status_error && !status_text.empty () && starts_seen.load () == 1
         && statuses_seen.load () == 2 && stats.reconnects == 2
         && reconnect_count == 2 && !reconnect_errors[0]
         && !reconnect_errors[1] && reconnect_models[0] == "StageLamp-3"
         && reconnect_models[1] == "StageLamp-3"
         && seen.requests_of ("WHO") == 3;
}

// How a candidate speaks: the text form over a stream. The wire of a candidate
// of another kind would be another function of the same name.
struct lamp_wire_t
{
  static pr::frame_link
  link (pr::discovery_candidate_t const &, tr::stream::byte_stream opened)
  {
    return pr::frame_link (tr::framed::framed_stream<lamp::text_codec_t> (
        std::move (opened), lamp::wire_t<lamp::lamp_text>::make_codec ()));
  }
};

std::string
why (pr::discovery_result_t const &result)
{
  if (result.found)
    return "found: " + result.identity.model + ", firmware "
           + std::to_string (result.identity.firmware) + ", serial "
           + result.identity.serial_number + ", layer "
           + result.identity.layer;
  return std::string ("not a device of the table: ")
         + result.reason.category ().name () + ", " + result.reason.message ();
}

bool
searching ()
{
  say ("--- 2. async_discover over five addresses of the loopback interface");
  using device_type = loopback::device_t<lamp::lamp_text>;
  // Two lamps, a device that answers but is no lamp of ours (it was not told
  // who it is: model 0), a device that never answers, and a closed port.
  std::vector<std::unique_ptr<device_type>> devices;
  for (int i = 0; i < 4; ++i)
    devices.push_back (std::unique_ptr<device_type> (
        new device_type (lamp::wire_t<lamp::lamp_text>::make_codec ())));
  devices[0]->simulator ().set_reply<lamp::lamp_text::WHO_t> (
      lamp::who_reply<lamp::lamp_text> (3, 12, 4711));
  devices[1]->simulator ().set_reply<lamp::lamp_text::WHO_t> (
      lamp::who_reply<lamp::lamp_text> (5, 20, 815));
  devices[3]->simulator ().set_behavior (tr::testing::behavior::silent ());
  std::uint16_t closed_port = 0;
  {
    asio::io_context context;
    asio::ip::tcp::acceptor acceptor (
        context,
        asio::ip::tcp::endpoint (asio::ip::address_v4::loopback (), 0));
    closed_port = acceptor.local_endpoint ().port ();
  }

  std::vector<pr::discovery_candidate_t> candidates (5);
  for (std::size_t i = 0; i < candidates.size (); ++i)
    {
      candidates[i].kind = pr::discovery_candidate_t::candidate_kind::network;
      candidates[i].network.host = "127.0.0.1";
      candidates[i].network.port
          = i < devices.size () ? devices[i]->port () : closed_port;
    }

  tr::stream::io_runner runner (1);
  pr::discovery_options_t discovery;
  discovery.per_candidate = ms (600);
  discovery.parallel = 5;
  std::future<std::vector<pr::discovery_result_t>> done
      = pr::async_discover<lamp::lamp_text, lamp_wire_t> (
          runner.get_executor (), candidates, discovery, asio::use_future);
  std::vector<pr::discovery_result_t> const results = done.get ();

  static char const *const names[]
      = { "a lamp", "another lamp", "a device with no model",
          "a device that never answers", "a closed port" };
  int found = 0;
  for (std::size_t i = 0; i < results.size (); ++i)
    {
      say ("  candidate " + std::to_string (i) + " (" + names[i]
           + "): " + why (results[i]));
      if (results[i].found)
        ++found;
    }
  // The answers come in the order of the candidates.
  return results.size () == 5 && found == 2 && results[0].found
         && results[1].found
         && results[2].reason == pr::discovery_errc::wrong_device
         && results[3].reason == pr::discovery_errc::no_response
         && results[4].reason;
}
} // namespace

int
main ()
{
  say ("=== lumex::transport: reconnecting and discovering ===\n");
  // The warnings of the session and of the dialogue, in the words of the
  // table, go with the rest of the output.
  tr::log::set_log_sink (&print_record);
  tr::log::set_log_level (tr::log::log_level::warning);
  std::vector<std::string> allow;
  allow.push_back ("transport.dialogue");
  allow.push_back ("transport.session");
  tr::log::set_component_filter (allow, std::vector<std::string> ());

  bool ok = reconnecting_client ();
  say ("");
  ok = searching () && ok;
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
