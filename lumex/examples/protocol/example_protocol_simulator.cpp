// A device made from a table: testing::device_simulator on the mock transports
// and the virtual clock, with the generated client on the other end. Nothing
// here waits for real time and nothing needs hardware or a network: the
// program moves the clock itself, so a second of the device's life takes a
// moment. Shown here: the default replies (the sample of every row), a reply
// and a handler of your own, the state of the device and the refusals that
// follow from it, a stream that the device sends by itself, the behaviours of
// a bad device (silent, damaged, in pieces, ...) with the outcome class each
// should end in, the same table on a datagram transport, and the counters of
// the simulator. The device is the invented stage lamp of
// example_protocol_lamp.hpp.
#include <chrono>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "example_protocol_lamp.hpp"

#if defined(LUMEX_TRANSPORT_WITH_PROTOCOL)                                    \
    && defined(LUMEX_TRANSPORT_WITH_TESTING)
namespace pr = lumex::applied::transport::protocol;
namespace tr = lumex::applied::transport;
namespace tt = lumex::applied::transport::testing;

namespace
{
namespace field = lumex::core::protocol::field;
using ms = std::chrono::milliseconds;
using lumex::core::protocol::error::protocol_errc;

// A device of layer P and a client of it, on the mocks of the transport and a
// clock that only the program moves. A stream carries the text layer, a
// datagram transport (report 1) the binary one.
template <typename P> class bench_t
{
public:
  explicit bench_t (bool datagram)
  {
    tt::mock_options_t mock_options;
    mock_options.timers = clock.source ();
    using codec_type = typename lamp::wire_t<P>::codec_type;
    if (datagram)
      {
        tt::mock_datagram mock
            = tt::make_mock_datagram (context.get_executor (), mock_options);
        mock.transport.async_open ([] (std::error_code) {});
        context.poll ();
        context.restart ();
        device.reset (new tt::device_simulator<P> (
            mock.control, lamp::wire_t<P>::make_codec (), 1, clock));
        keep.reset (new tt::mock_datagram_control (mock.control));
        client.reset (new pr::client<P> (
            pr::frame_link (tr::framed::framed_datagram<codec_type> (
                                std::move (mock.transport),
                                lamp::wire_t<P>::make_codec ()),
                            1),
            options ()));
      }
    else
      {
        tt::mock_stream mock
            = tt::make_mock_stream (context.get_executor (), mock_options);
        mock.stream.async_open ([] (std::error_code) {});
        context.poll ();
        context.restart ();
        device.reset (new tt::device_simulator<P> (
            mock.control, lamp::wire_t<P>::make_codec (), clock));
        client.reset (new pr::client<P> (
            pr::frame_link (tr::framed::framed_stream<codec_type> (
                std::move (mock.stream), lamp::wire_t<P>::make_codec ())),
            options ()));
      }
    settle ();
  }

  ~bench_t ()
  {
    client.reset ();
    settle ();
    device.reset ();
    settle ();
  }

  bench_t (bench_t const &) = delete;
  bench_t &operator= (bench_t const &) = delete;

  pr::client_options_t
  options ()
  {
    pr::client_options_t made = pr::client_options_for<P> ();
    // The dialogue waits and retries on the virtual clock too.
    made.dialogue.timers = clock.source ();
    return made;
  }

  // Runs what is ready, without moving the clock.
  void
  settle ()
  {
    tt::run_until (context, clock, [] () { return false; }, ms (0));
  }

  // Moves the clock by `by`, running what falls due on the way.
  void
  advance (ms by)
  {
    tt::run_until (context, clock, [] () { return false; }, by);
  }

  // Runs and moves the clock until `done`, at most `budget` of virtual time.
  bool
  run_until (std::function<bool ()> const &done, ms budget = ms (60000))
  {
    return tt::run_until (context, clock, done, budget);
  }

  std::int64_t
  virtual_ms () const
  {
    return std::chrono::duration_cast<ms> (clock.now ()
                                           - tr::timing::time_point_t ())
        .count ();
  }

  asio::io_context context;
  tt::virtual_clock clock;
  std::unique_ptr<tt::mock_datagram_control> keep;
  std::unique_ptr<tt::device_simulator<P>> device;
  std::unique_ptr<pr::client<P>> client;
};

// What one call ended with.
template <typename Value> struct outcome_t
{
  outcome_t () : done (false), value () {}

  bool done;
  std::error_code error;
  Value value;
};

template <typename Value> struct record_t
{
  std::shared_ptr<outcome_t<Value>> out;

  void
  operator() (std::error_code error, Value value) const
  {
    out->done = true;
    out->error = error;
    out->value = value;
  }
};

template <typename Value>
record_t<Value>
into (std::shared_ptr<outcome_t<Value>> const &out)
{
  record_t<Value> made = { out };
  return made;
}

// The warnings of the transport go to the same stream as the rest, in order
// (the default sink writes them to the error stream). They name the frames in
// the words of the table, which is how a bad device is told from a bad cable.
void
print_record (tr::log::log_record_t const &record)
{
  std::cout << "    [log] " << record.message << '\n';
}

// STATUS, asked of the bench; the clock moves until the call ends.
template <typename P>
std::shared_ptr<outcome_t<pr::reply_of<typename P::STATUS_t>>>
ask_status (bench_t<P> &bench)
{
  using outcome_type = outcome_t<pr::reply_of<typename P::STATUS_t>>;
  std::shared_ptr<outcome_type> const out = std::make_shared<outcome_type> ();
  bench.client->STATUS (into<pr::reply_of<typename P::STATUS_t>> (out));
  bench.run_until ([out] () { return out->done; });
  return out;
}

// The class a call ended in, as the matrix of the library names it.
tt::outcome_class
observed (std::error_code const &error, pr::dialogue_statistics_t const &stats)
{
  using lumex::core::protocol::error::protocol_errc;
  if (!error)
    {
      if (stats.retries != 0)
        return tt::outcome_class::success_after_retries;
      if (stats.late_replies + stats.unknown_frames + stats.unexpected_replies
          != 0)
        return tt::outcome_class::dropped_counted;
      return tt::outcome_class::success;
    }
  if (error == protocol_errc::retries_exhausted)
    return tt::outcome_class::retries_exhausted;
  if (error == protocol_errc::timeout)
    return tt::outcome_class::timeout;
  if (error == protocol_errc::link_lost)
    return tt::outcome_class::link_lost;
  return tt::outcome_class::protocol_error;
}

bool
defaults_and_handlers ()
{
  std::cout
      << "--- 1. Every row has a reply: the sample value of its fields\n";
  bench_t<lamp::lamp_text> bench (false);
  std::shared_ptr<outcome_t<pr::reply_of<lamp::lamp_text::STATUS_t>>> const
      sample
      = ask_status (bench);
  std::cout << "  STATUS () -> "
            << lamp::show_status<lamp::lamp_text> (sample->value)
            << "  (nobody wrote a reply)\n";

  std::cout << "--- 2. A reply of your own, and a handler for a row\n";
  pr::reply_of<lamp::lamp_text::STATUS_t> lit;
  field::get<0> (lit) = lamp::run_state::on;
  field::get<1> (lit).raw = 625;
  field::get<2> (lit) = 1500;
  bench.device->set_reply<lamp::lamp_text::STATUS_t> (lit);
  std::cout << "  set_reply<STATUS> -> "
            << lamp::show_status<lamp::lamp_text> (ask_status (bench)->value)
            << '\n';
  // A handler sees the request, decoded, and returns the reply. This one
  // keeps the slot it was asked to erase and says the lamp is off again.
  int erased_slot = -1;
  pr::reply_of<lamp::lamp_text::STATUS_t> off = lit;
  field::get<0> (off) = lamp::run_state::off;
  bench.device->on<lamp::lamp_text::ERASE_LOG_t> (
      [&erased_slot, off] (tt::device_simulator<lamp::lamp_text>::request_of<
                           lamp::lamp_text::ERASE_LOG_t> const &request)
        {
          erased_slot = field::get<0> (request);
          return off;
        });
  using erase_value = pr::reply_of<lamp::lamp_text::ERASE_LOG_t>;
  std::shared_ptr<outcome_t<erase_value>> const erased
      = std::make_shared<outcome_t<erase_value>> ();
  bench.client->ERASE_LOG (pr::confirm, 7, into<erase_value> (erased));
  bench.run_until ([erased] () { return erased->done; });
  std::cout << "  ERASE_LOG (confirm, 7): the handler saw slot " << erased_slot
            << " and answered '" << to_string (field::get<0> (erased->value))
            << "'\n";
  return !sample->error && !erased->error && erased_slot == 7;
}

bool
state_and_refusals ()
{
  std::cout << "--- 3. The state of the device and what it refuses\n";
  bench_t<lamp::lamp_text> bench (false);
  using start_value = pr::reply_of<lamp::lamp_text::START_t>;
  std::shared_ptr<outcome_t<start_value>> const first
      = std::make_shared<outcome_t<start_value>> ();
  bench.client->START (1, into<start_value> (first));
  bench.run_until ([first] () { return first->done; });
  std::cout << "  START (1) in state off -> "
            << (first->error ? first->error.message () : "accepted") << '\n';
  bench.device->set_state (lamp::state_code (lamp::state::on));
  std::shared_ptr<outcome_t<start_value>> const second
      = std::make_shared<outcome_t<start_value>> ();
  bench.client->START (2, into<start_value> (second));
  bench.run_until ([second] () { return second->done; });
  std::cout << "  START (2) in state on -> "
            << (second->error ? second->error.message () : "accepted")
            << "  (accepted_in (off): the device answered with a NAK)\n";
  // Once the client has seen the state, it does not even send a row the
  // device would refuse (state_from<STATUS, 0> of the table).
  std::shared_ptr<outcome_t<pr::reply_of<lamp::lamp_text::STATUS_t>>> const
      seen
      = ask_status (bench);
  std::shared_ptr<outcome_t<start_value>> const own
      = std::make_shared<outcome_t<start_value>> ();
  bench.client->START (4, into<start_value> (own));
  bench.run_until ([own] () { return own->done; });
  std::cout << "  START (4) after STATUS showed 'on' -> "
            << (own->error ? own->error.message () : "accepted")
            << "  (refused by the client, nothing was sent)\n";
  // A row can also be refused by order, whatever the state.
  bench.device->set_state (lamp::state_code (lamp::state::off));
  ask_status (bench);
  bench.device->refuse<lamp::lamp_text::START_t> ();
  std::shared_ptr<outcome_t<start_value>> const third
      = std::make_shared<outcome_t<start_value>> ();
  bench.client->START (3, into<start_value> (third));
  bench.run_until ([third] () { return third->done; });
  std::cout << "  START (3) after refuse<START> () -> "
            << (third->error ? third->error.message () : "accepted") << '\n';
  tt::simulator_statistics_t const stats = bench.device->statistics ();
  std::cout << "  simulator: " << stats.requests << " requests, "
            << stats.refused << " refused\n";
  return !first->error && second->error == protocol_errc::nak
         && own->error == protocol_errc::not_accepted_in_state && !seen->error
         && third->error == protocol_errc::nak && stats.refused == 2;
}

bool
a_stream_of_the_device ()
{
  std::cout << "--- 4. A stream the device sends by itself, in virtual time\n";
  bench_t<lamp::lamp_text> bench (false);
  int reports = 0;
  bench.client->on<lamp::lamp_text::LEVEL_t> (
      [&reports] (pr::reply_of<lamp::lamp_text::LEVEL_t> const &level)
        {
          ++reports;
          if (reports == 1)
            std::cout << "  first report: brightness "
                      << field::get<0> (level).value () << " %\n";
        });
  bench.device->start_stream<lamp::lamp_text::LEVEL_t> ();
  std::int64_t const from = bench.virtual_ms ();
  bench.advance (ms (1000));
  bench.device->stop_stream<lamp::lamp_text::LEVEL_t> ();
  std::cout << "  " << reports << " reports in " << bench.virtual_ms () - from
            << " ms of virtual time (the row says stream (100))\n";
  return reports == 10;
}

bool
behaviours ()
{
  std::cout << "--- 5. A bad device: a behaviour shapes everything it sends\n"
               "  STATUS () has retries (2) and is idempotent; the outcome "
               "class is the one the library's matrix expects\n";
  tt::row_shape_t shape;
  shape.retries = 2;
  shape.idempotent = true;
  tt::behavior_kind const kinds[]
      = { tt::behavior_kind::perfect,      tt::behavior_kind::silent,
          tt::behavior_kind::intermittent, tt::behavior_kind::corrupted_crc,
          tt::behavior_kind::fragmented,   tt::behavior_kind::slow_trickle,
          tt::behavior_kind::zero_read,    tt::behavior_kind::repeating };
  bool all = true;
  for (std::size_t i = 0; i < sizeof kinds / sizeof kinds[0]; ++i)
    {
      bench_t<lamp::lamp_text> bench (false);
      bench.device->set_behavior (tt::behavior::of (kinds[i]));
      std::int64_t const from = bench.virtual_ms ();
      std::shared_ptr<outcome_t<pr::reply_of<lamp::lamp_text::STATUS_t>>> const
          out
          = ask_status (bench);
      std::int64_t const took = bench.virtual_ms () - from;
      // A little more time, for the late and the repeated frames to arrive.
      bench.advance (ms (5000));
      pr::dialogue_statistics_t const stats = bench.client->statistics ();
      tt::outcome_class const got = observed (out->error, stats);
      tt::outcome_class const wanted = tt::expected_outcome (kinds[i], shape);
      std::cout << "  " << tt::behavior_kind_name (kinds[i]) << ": "
                << tt::outcome_class_name (got) << " (expected "
                << tt::outcome_class_name (wanted) << "), requests "
                << stats.requests << ", timeouts " << stats.timeouts
                << ", late " << stats.late_replies << ", the call took "
                << took << " ms of virtual time\n";
      all = all && got == wanted;
    }
  // Behaviours compose: noise first, then a silent device.
  tt::behavior const mixed
      = tt::behavior::silent ().then (tt::behavior::garbage (7, 16));
  std::cout << "  silent ().then (garbage (7, 16)): " << mixed.stage_count ()
            << " stages, first " << tt::behavior_kind_name (mixed.kind ())
            << '\n';
  return all;
}

bool
a_datagram_transport ()
{
  std::cout << "--- 6. The binary layer on a datagram transport (report 1)\n";
  bool all = true;
  {
    // A datagram that is lost: the same retry as on a stream.
    bench_t<lamp::lamp_binary> bench (true);
    bench.device->set_behavior (tt::behavior::intermittent (1, 1000, 1));
    std::shared_ptr<outcome_t<pr::reply_of<lamp::lamp_binary::STATUS_t>>> const
        out
        = ask_status (bench);
    std::cout << "  intermittent: STATUS () -> "
              << (out->error
                      ? out->error.message ()
                      : lamp::show_status<lamp::lamp_binary> (out->value))
              << ", requests " << bench.client->statistics ().requests << '\n';
    all = all && !out->error;
  }
  {
    // A reply in pieces: on a datagram transport each piece is a datagram of
    // its own, and the framing takes a datagram as a whole. The stream twin
    // of this behaviour is read to the end, this one is a damaged frame.
    bench_t<lamp::lamp_binary> bench (true);
    bench.device->set_behavior (tt::behavior::fragmented (3, 4));
    std::shared_ptr<outcome_t<pr::reply_of<lamp::lamp_binary::STATUS_t>>> const
        out
        = ask_status (bench);
    std::cout << "  fragmented: STATUS () -> "
              << (out->error ? out->error.message () : "a reply") << "\n";
    all = all && out->error == protocol_errc::retries_exhausted;
  }
  return all;
}

bool
the_counters ()
{
  std::cout << "--- 7. What the simulator counts\n";
  bench_t<lamp::lamp_text> bench (false);
  ask_status (bench);
  ask_status (bench);
  bench.client->START (
      1, [] (std::error_code, pr::reply_of<lamp::lamp_text::START_t>) {});
  bench.settle ();
  bench.advance (ms (10));
  tt::simulator_statistics_t const stats = bench.device->statistics ();
  std::cout << "  requests " << stats.requests << ", replies " << stats.replies
            << ", refused " << stats.refused << ", unknown "
            << stats.unknown_requests << ", framing errors "
            << stats.framing_errors << "; STATUS asked "
            << stats.requests_of ("STATUS") << " time(s)\n";
  return stats.requests == 3 && stats.requests_of ("STATUS") == 2;
}
} // namespace

int
main ()
{
  std::cout << "=== lumex::transport: a device simulator on virtual time "
               "===\n\n";
  tr::log::set_log_sink (&print_record);
  tr::log::set_log_level (tr::log::log_level::warning);
  bool ok = true;
  struct step_t
  {
    char const *name;
    bool (*run) ();
  };
  step_t const steps[] = { { "defaults and handlers", &defaults_and_handlers },
                           { "state and refusals", &state_and_refusals },
                           { "a stream", &a_stream_of_the_device },
                           { "behaviours", &behaviours },
                           { "a datagram transport", &a_datagram_transport },
                           { "the counters", &the_counters } };
  for (std::size_t i = 0; i < sizeof steps / sizeof steps[0]; ++i)
    if (!steps[i].run ())
      {
        std::cout << "  ^ differs from what the section expects: "
                  << steps[i].name << '\n';
        ok = false;
      }
  std::cout << (ok ? "\nall as expected\n" : "\nSOMETHING DIFFERED\n");
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
