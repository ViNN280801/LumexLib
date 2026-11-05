// async_discover<P, Wire>: every reason of a candidate, mixed candidates on
// mocks and a loopback TCP peer, the out-of-band sequence before the
// identification, the bound of the probes, every token style, exactly one
// completion and never inline, the cancellation, the lifetime and many
// discoveries on one runner.

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/tests/applied/transport/protocol/client/LumexClientReconnectSupport.hpp"

namespace
{
using namespace lumex_client_test;
using results_t = std::vector<pr::discovery_result_t>;
using pr::discovery_errc;
using tr::error::transport_errc;

/// The wire of the tests: SLIP on a stream, SLIP on report 1.
struct wire_t
{
  static pr::frame_link
  link (pr::discovery_candidate_t const &, tr::stream::byte_stream opened)
  {
    return pr::frame_link (tr::framed::framed_stream<fr::slip::slip_codec> (
        std::move (opened), fr::slip::slip_codec ()));
  }

  static pr::frame_link
  link (pr::discovery_candidate_t const &,
        tr::datagram::message_transport opened)
  {
    return pr::frame_link (tr::framed::framed_datagram<fr::slip::slip_codec> (
                               std::move (opened), fr::slip::slip_codec ()),
                           1);
  }
};

/// With an out-of-band sequence.
struct switching_wire_t : wire_t
{
  static std::vector<std::uint8_t>
  out_of_band (pr::discovery_candidate_t const &)
  {
    return { 0xAA, 0x55 };
  }
};

/// No link for messages.
struct stream_wire_t
{
  static pr::frame_link
  link (pr::discovery_candidate_t const &, tr::stream::byte_stream opened)
  {
    return wire_t::link (pr::discovery_candidate_t (), std::move (opened));
  }
};

/// A device on a mock, and the candidate that reaches it.
struct place_t
{
  tr::testing::mock_stream mock;
  std::unique_ptr<tr::testing::device_simulator<gauge_a>> device;

  place_t (asio::io_context &context, tr::testing::virtual_clock &clock)
      : mock (tr::testing::make_mock_stream (
            context.get_executor (),
            [&clock] ()
              {
                tr::testing::mock_options_t options;
                options.timers = clock.source ();
                return options;
              }()))
  {
    device.reset (new tr::testing::device_simulator<gauge_a> (
        mock.control, fr::slip::slip_codec (), clock));
    device->set_reply<gauge_a::WHO_t> (gauge_who ("G100", 3, 1));
  }

  pr::discovery_candidate_t
  candidate ()
  {
    pr::discovery_candidate_t made;
    made.kind = pr::discovery_candidate_t::candidate_kind::stream;
    std::shared_ptr<tr::stream::stream_impl> const impl = mock.stream.impl ();
    made.stream_factory = [impl] (tr::executor_type const &)
      { return tr::stream::byte_stream (impl); };
    return made;
  }
};

struct bench_t
{
  bench_t () { options.timers = clock.source (); }

  ~bench_t ()
  {
    places.clear ();
    settle ();
  }

  place_t &
  add ()
  {
    places.emplace_back (new place_t (context, clock));
    return *places.back ();
  }

  void
  settle ()
  {
    tr::testing::run_until (context, clock, [] () { return false; }, ms (0));
  }

  bool
  run_until (std::function<bool ()> const &done, ms budget = ms (60000))
  {
    return tr::testing::run_until (context, clock, done, budget);
  }

  /// Runs a discovery to its end; one completion, never inside the call.
  template <typename Wire = wire_t>
  results_t
  discover (std::vector<pr::discovery_candidate_t> const &candidates)
  {
    int calls = 0;
    results_t out;
    std::error_code error;
    pr::async_discover<gauge, Wire> (context.get_executor (), candidates,
                                     options,
                                     [&] (std::error_code ended, results_t got)
                                       {
                                         ++calls;
                                         error = ended;
                                         out = std::move (got);
                                       });
    EXPECT_EQ (calls, 0);
    EXPECT_TRUE (run_until ([&] () { return calls != 0; }));
    settle ();
    clock.advance (ms (5000));
    settle ();
    EXPECT_EQ (calls, 1);
    EXPECT_FALSE (error) << error.message ();
    return out;
  }

  asio::io_context context;
  tr::testing::virtual_clock clock;
  pr::discovery_options_t options;
  std::vector<std::unique_ptr<place_t>> places;
};

pr::discovery_candidate_t
failing_candidate (std::error_code const &error)
{
  pr::discovery_candidate_t made;
  made.kind = pr::discovery_candidate_t::candidate_kind::stream;
  made.stream_factory = [error] (tr::executor_type const &executor)
    {
      tr::testing::mock_stream mock = tr::testing::make_mock_stream (executor);
      mock.control.fail_open (error);
      return std::move (mock.stream);
    };
  return made;
}

TEST (LumexDiscover, Discover_FoundWrongDeviceNoResponseBusyOpenTimeout)
{
  // Falsified by every reason mapped to one, and by results out of order.
  bench_t bench;
  place_t &found = bench.add ();
  place_t &other = bench.add ();
  other.device->set_reply<gauge_a::WHO_t> (gauge_who ("G900", 9, 9));
  place_t &silent = bench.add ();
  silent.device->set_behavior (tr::testing::behavior::silent ());
  // A device that holds the open.
  place_t &held = bench.add ();
  held.mock.control.hold_next_open ();
  std::vector<pr::discovery_candidate_t> candidates;
  candidates.push_back (found.candidate ());
  candidates.push_back (other.candidate ());
  candidates.push_back (silent.candidate ());
  candidates.push_back (failing_candidate (
      std::make_error_code (std::errc::device_or_resource_busy)));
  candidates.push_back (held.candidate ());
  candidates.push_back (
      failing_candidate (std::make_error_code (std::errc::no_such_device)));
  candidates.push_back (
      failing_candidate (make_error_code (transport_errc::timeout)));
  pr::discovery_candidate_t thrower;
  thrower.kind = pr::discovery_candidate_t::candidate_kind::stream;
  thrower.stream_factory
      = [] (tr::executor_type const &) -> tr::stream::byte_stream
    {
      throw std::system_error (
          std::make_error_code (std::errc::no_such_file_or_directory));
    };
  candidates.push_back (thrower);
  pr::discovery_candidate_t runtime_thrower;
  runtime_thrower.kind = pr::discovery_candidate_t::candidate_kind::stream;
  runtime_thrower.stream_factory
      = [] (tr::executor_type const &) -> tr::stream::byte_stream
    { throw std::runtime_error ("no"); };
  candidates.push_back (runtime_thrower);
  results_t const results = bench.discover (candidates);
  ASSERT_EQ (results.size (), candidates.size ());
  EXPECT_TRUE (results[0].found);
  EXPECT_FALSE (results[0].reason);
  EXPECT_EQ (results[0].identity.model, "G100");
  EXPECT_EQ (results[0].identity.firmware, 3u);
  EXPECT_EQ (results[0].identity.layer, "gauge_a_t");
  EXPECT_EQ (results[0].identity.serial_number, "1");
  EXPECT_FALSE (results[1].found);
  EXPECT_EQ (results[1].reason, discovery_errc::wrong_device);
  EXPECT_EQ (results[1].identity.model, "G900");
  EXPECT_FALSE (results[2].found);
  EXPECT_EQ (results[2].reason, discovery_errc::no_response);
  EXPECT_EQ (results[3].reason, discovery_errc::busy);
  EXPECT_EQ (results[4].reason, discovery_errc::open_timeout);
  EXPECT_EQ (results[5].reason, std::errc::no_such_device);
  EXPECT_EQ (results[6].reason, discovery_errc::open_timeout);
  EXPECT_EQ (results[7].reason, std::errc::no_such_file_or_directory);
  EXPECT_EQ (results[8].reason, transport_errc::invalid_endpoint);
  // The category says who it is.
  EXPECT_STREQ (pr::discovery_category ().name (), "lumex.discovery");
  EXPECT_FALSE (results[1].reason.message ().empty ());
  EXPECT_FALSE (make_error_code (discovery_errc::busy).message ().empty ());
  EXPECT_FALSE (
      make_error_code (discovery_errc::open_timeout).message ().empty ());
  EXPECT_FALSE (
      make_error_code (discovery_errc::no_response).message ().empty ());
  EXPECT_FALSE (
      std::error_code (99, pr::discovery_category ()).message ().empty ());
}

TEST (LumexDiscover, Discover_OverARealTcpPeerAndARefusingOne)
{
  // A loopback listener that answers the identification, a port nobody listens
  // on, and a mock in one list.
  asio::io_context context;
  asio::ip::tcp::acceptor acceptor (
      context, asio::ip::tcp::endpoint (asio::ip::address_v4::loopback (), 0));
  std::uint16_t const port = acceptor.local_endpoint ().port ();
  asio::ip::tcp::acceptor closed_probe (
      context, asio::ip::tcp::endpoint (asio::ip::address_v4::loopback (), 0));
  std::uint16_t const free_port = closed_probe.local_endpoint ().port ();
  closed_probe.close ();
  auto const peer = std::make_shared<asio::ip::tcp::socket> (context);
  auto const buffer = std::make_shared<std::array<std::uint8_t, 64>> ();
  std::vector<std::uint8_t> const reply = wire_of (
      [] ()
        {
          std::vector<std::uint8_t> message (1, 0x01);
          pr::reply_of<gauge::WHO_t> const who = gauge_who ("NET1", 2, 2, 7);
          std::string const model = "NET1";
          message.insert (message.end (), model.begin (), model.end ());
          message.insert (message.end (), 8 - model.size (), 0);
          message.push_back (2);
          message.push_back (0);
          message.push_back (2);
          message.push_back (7);
          (void)who;
          return message;
        }());
  acceptor.async_accept (
      *peer,
      [peer, buffer, reply] (std::error_code error)
        {
          if (error)
            return;
          peer->async_read_some (
              asio::buffer (*buffer),
              [peer, buffer, reply] (std::error_code, std::size_t)
                {
                  asio::async_write (*peer, asio::buffer (reply),
                                     [peer] (std::error_code, std::size_t) {});
                });
        });
  std::vector<pr::discovery_candidate_t> candidates (2);
  candidates[0].kind = pr::discovery_candidate_t::candidate_kind::network;
  candidates[0].network.host = "127.0.0.1";
  candidates[0].network.port = port;
  candidates[1].kind = pr::discovery_candidate_t::candidate_kind::network;
  candidates[1].network.host = "127.0.0.1";
  candidates[1].network.port = free_port;
  pr::discovery_options_t options;
  options.per_candidate = ms (2000);
  int calls = 0;
  results_t results;
  pr::async_discover<gauge, wire_t> (context.get_executor (), candidates,
                                     options,
                                     [&] (std::error_code, results_t got)
                                       {
                                         ++calls;
                                         results = std::move (got);
                                         acceptor.close ();
                                         peer->close ();
                                       });
  std::thread runner ([&] () { context.run_for (std::chrono::seconds (20)); });
  runner.join ();
  ASSERT_EQ (calls, 1);
  ASSERT_EQ (results.size (), 2u);
  EXPECT_TRUE (results[0].found) << results[0].reason.message ();
  EXPECT_EQ (results[0].identity.model, "NET1");
  EXPECT_EQ (results[0].identity.layer, "gauge_b_t");
  EXPECT_EQ (results[0].identity.serial_number, "7");
  EXPECT_FALSE (results[1].found);
  EXPECT_TRUE (results[1].reason);
}

TEST (LumexDiscover, Discover_DatagramCandidate)
{
  bench_t bench;
  tr::testing::mock_options_t mock_options;
  mock_options.timers = bench.clock.source ();
  tr::testing::mock_datagram mock = tr::testing::make_mock_datagram (
      bench.context.get_executor (), mock_options);
  tr::testing::device_simulator<gauge_a> device (
      mock.control, fr::slip::slip_codec (), 1, bench.clock);
  device.set_reply<gauge_a::WHO_t> (gauge_who ("D1", 1, 1));
  std::shared_ptr<tr::datagram::message_impl> const impl
      = mock.transport.impl ();
  std::vector<pr::discovery_candidate_t> candidates (1);
  candidates[0].kind = pr::discovery_candidate_t::candidate_kind::message;
  candidates[0].message_factory = [impl] (tr::executor_type const &)
    { return tr::datagram::message_transport (impl); };
  results_t const results = bench.discover (candidates);
  ASSERT_EQ (results.size (), 1u);
  EXPECT_TRUE (results[0].found) << results[0].reason.message ();
  EXPECT_EQ (results[0].identity.model, "D1");
}

TEST (LumexDiscover, Discover_ACandidateOfAKindTheWireHasNoLinkFor)
{
  bench_t bench;
  tr::testing::mock_datagram mock
      = tr::testing::make_mock_datagram (bench.context.get_executor ());
  std::shared_ptr<tr::datagram::message_impl> const impl
      = mock.transport.impl ();
  std::vector<pr::discovery_candidate_t> candidates (2);
  candidates[0].kind = pr::discovery_candidate_t::candidate_kind::message;
  candidates[0].message_factory = [impl] (tr::executor_type const &)
    { return tr::datagram::message_transport (impl); };
  // No factory.
  candidates[1].kind = pr::discovery_candidate_t::candidate_kind::stream;
  results_t const results = bench.discover<stream_wire_t> (candidates);
  ASSERT_EQ (results.size (), 2u);
  EXPECT_EQ (results[0].reason, protocol_errc::not_supported);
  EXPECT_EQ (results[1].reason, protocol_errc::not_supported);
}

TEST (LumexDiscover, Discover_OutOfBandSwitchBeforeIdentify)
{
  // Falsified by a sequence after the identification, or through the codec.
  bench_t bench;
  place_t &place = bench.add ();
  std::vector<std::uint8_t> seen_first;
  place.mock.control.on_write (
      [&seen_first] (std::vector<std::uint8_t> const &chunk)
        {
          if (seen_first.empty ())
            seen_first = chunk;
        });
  // The mock keeps one observer, so the device answers by hand.
  std::vector<pr::discovery_candidate_t> candidates;
  candidates.push_back (place.candidate ());
  bench.options.per_candidate = ms (200);
  results_t const results = bench.discover<switching_wire_t> (candidates);
  ASSERT_EQ (results.size (), 1u);
  EXPECT_EQ (seen_first, (std::vector<std::uint8_t>{ 0xAA, 0x55 }));
  std::vector<std::uint8_t> const written = place.mock.control.written ();
  ASSERT_GE (written.size (), 3u);
  EXPECT_EQ (written[0], 0xAA);
  EXPECT_EQ (written[1], 0x55);
  // Then the framed identification, whole.
  std::vector<std::uint8_t> const framed = wire_of ({ 0x01 });
  EXPECT_EQ (std::vector<std::uint8_t> (written.begin () + 2, written.end ()),
             framed);
}

TEST (LumexDiscover, Discover_OutOfBandPauseAndFailure)
{
  // The pause comes before the identification; a sequence that cannot be
  // written ends the probe with its error.
  bench_t bench;
  place_t &place = bench.add ();
  bench.options.out_of_band_pause = ms (100);
  bench.options.per_candidate = ms (1000);
  std::vector<pr::discovery_candidate_t> candidates;
  candidates.push_back (place.candidate ());
  int calls = 0;
  results_t results;
  pr::async_discover<gauge, switching_wire_t> (
      bench.context.get_executor (), candidates, bench.options,
      [&] (std::error_code, results_t got)
        {
          ++calls;
          results = std::move (got);
        });
  bench.settle ();
  EXPECT_EQ (place.device->statistics ().requests, 0u);
  bench.clock.advance (ms (90));
  bench.settle ();
  EXPECT_EQ (place.device->statistics ().requests, 0u);
  ASSERT_TRUE (bench.run_until ([&] () { return calls != 0; }));
  EXPECT_TRUE (results[0].found) << results[0].reason.message ();

  place_t &other = bench.add ();
  other.mock.control.fail_next_write (
      make_error_code (transport_errc::disconnected));
  std::vector<pr::discovery_candidate_t> second;
  second.push_back (other.candidate ());
  results_t const failed = bench.discover<switching_wire_t> (second);
  EXPECT_EQ (failed[0].reason, transport_errc::disconnected);
}

/// Candidates whose open never ends: only the time of the probe ends them, so
/// the calls of the factory show how many probes run.
std::vector<pr::discovery_candidate_t>
held_candidates (std::size_t count,
                 std::shared_ptr<std::atomic<int>> const &made,
                 tr::testing::virtual_clock &clock)
{
  std::vector<pr::discovery_candidate_t> candidates (count);
  tr::timing::timer_source const timers = clock.source ();
  for (std::size_t i = 0; i < count; ++i)
    {
      candidates[i].kind = pr::discovery_candidate_t::candidate_kind::stream;
      candidates[i].stream_factory
          = [made, timers] (tr::executor_type const &executor)
        {
          ++*made;
          tr::testing::mock_options_t options;
          options.timers = timers;
          tr::testing::mock_stream mock
              = tr::testing::make_mock_stream (executor, options);
          mock.control.hold_next_open ();
          return std::move (mock.stream);
        };
    }
  return candidates;
}

TEST (LumexDiscover, Discover_ParallelBound)
{
  // Falsified by a bound that is not kept (more at once), and by one at a time
  // (fewer).
  bench_t bench;
  auto const made = std::make_shared<std::atomic<int>> (0);
  bench.options.parallel = 3;
  bench.options.per_candidate = ms (100);
  int calls = 0;
  results_t results;
  pr::async_discover<gauge, wire_t> (bench.context.get_executor (),
                                     held_candidates (10, made, bench.clock),
                                     bench.options,
                                     [&] (std::error_code, results_t got)
                                       {
                                         ++calls;
                                         results = std::move (got);
                                       });
  EXPECT_EQ (made->load (), 0);
  bench.settle ();
  EXPECT_EQ (made->load (), 3);
  bench.clock.advance (ms (99));
  bench.settle ();
  EXPECT_EQ (made->load (), 3);
  bench.clock.advance (ms (1));
  bench.settle ();
  EXPECT_EQ (made->load (), 6);
  bench.clock.advance (ms (100));
  bench.settle ();
  EXPECT_EQ (made->load (), 9);
  EXPECT_EQ (calls, 0);
  bench.clock.advance (ms (100));
  bench.settle ();
  EXPECT_EQ (made->load (), 10);
  EXPECT_EQ (calls, 0);
  bench.clock.advance (ms (100));
  bench.settle ();
  EXPECT_EQ (calls, 1);
  ASSERT_EQ (results.size (), 10u);
  for (std::size_t i = 0; i < results.size (); ++i)
    EXPECT_EQ (results[i].reason, discovery_errc::open_timeout);
}

TEST (LumexDiscover, Discover_TheTimeOfTheCandidateEndsAnIdentification)
{
  // Falsified by the time read as one of the open: it opened and is silent.
  bench_t bench;
  place_t &silent = bench.add ();
  silent.device->set_behavior (tr::testing::behavior::silent ());
  bench.options.per_candidate = ms (50);
  std::vector<pr::discovery_candidate_t> candidates;
  candidates.push_back (silent.candidate ());
  results_t const results = bench.discover (candidates);
  EXPECT_EQ (results[0].reason, discovery_errc::no_response);
  // Within the time of the candidate, before the row had ended it.
  EXPECT_EQ (silent.device->statistics ().requests, 1u);
}

TEST (LumexDiscover, Discover_AProbeClosesWhenItEndsNotAtTheEnd)
{
  // Falsified by a transport that stays open until the discovery ends: the
  // next candidate could be the same port.
  bench_t bench;
  place_t &first = bench.add ();
  place_t &second = bench.add ();
  bench.options.parallel = 1;
  bench.options.per_candidate = ms (100);
  bench.options.out_of_band_pause = ms (500);
  std::vector<pr::discovery_candidate_t> candidates;
  candidates.push_back (first.candidate ());
  candidates.push_back (second.candidate ());
  int calls = 0;
  results_t results;
  pr::async_discover<gauge, switching_wire_t> (
      bench.context.get_executor (), candidates, bench.options,
      [&] (std::error_code, results_t got)
        {
          ++calls;
          results = std::move (got);
        });
  bench.settle ();
  EXPECT_TRUE (first.mock.stream.is_open ());
  EXPECT_FALSE (second.mock.stream.is_open ());
  // The first is over: its time, while the out-of-band sequence settles.
  bench.clock.advance (ms (100));
  bench.settle ();
  EXPECT_FALSE (first.mock.stream.is_open ());
  EXPECT_TRUE (second.mock.stream.is_open ());
  EXPECT_EQ (calls, 0);
  ASSERT_TRUE (bench.run_until ([&] () { return calls != 0; }));
  EXPECT_EQ (results[0].reason, discovery_errc::no_response);
  EXPECT_EQ (results[1].reason, discovery_errc::no_response);
}

TEST (LumexDiscover, Discover_ParallelOfZeroIsOne)
{
  bench_t bench;
  auto const made = std::make_shared<std::atomic<int>> (0);
  bench.options.parallel = 0;
  bench.options.per_candidate = ms (100);
  int calls = 0;
  pr::async_discover<gauge, wire_t> (
      bench.context.get_executor (), held_candidates (2, made, bench.clock),
      bench.options, [&] (std::error_code, results_t) { ++calls; });
  bench.settle ();
  EXPECT_EQ (made->load (), 1);
  ASSERT_TRUE (bench.run_until ([&] () { return calls != 0; }));
  EXPECT_EQ (made->load (), 2);
}

TEST (LumexDiscover, Discover_EachProbeClosesWhatItOpened)
{
  // Falsified by a probe that leaves its transport open.
  bench_t bench;
  place_t &found = bench.add ();
  place_t &silent = bench.add ();
  silent.device->set_behavior (tr::testing::behavior::silent ());
  std::vector<pr::discovery_candidate_t> candidates;
  candidates.push_back (found.candidate ());
  candidates.push_back (silent.candidate ());
  bench.discover (candidates);
  EXPECT_FALSE (found.mock.stream.is_open ());
  EXPECT_FALSE (silent.mock.stream.is_open ());
}

TEST (LumexDiscover, Discover_AProbeSendsOnlyTheIdentification)
{
  // Falsified by a probe with the features of the table: a keepalive or a
  // stream row would go to what may be another device.
  bench_t bench;
  place_t &place = bench.add ();
  std::vector<pr::discovery_candidate_t> candidates;
  candidates.push_back (place.candidate ());
  bench.discover (candidates);
  EXPECT_EQ (place.device->statistics ().requests, 1u);
}

TEST (LumexDiscover, Discover_AProbeSendsNoKeepalive)
{
  // Falsified by a probe with the features of the table: the keepalive of a
  // row without fields would go to what may be another device.
  bench_t bench;
  tr::testing::mock_options_t mock_options;
  mock_options.timers = bench.clock.source ();
  tr::testing::mock_stream mock = tr::testing::make_mock_stream (
      bench.context.get_executor (), mock_options);
  tr::testing::device_simulator<rt::watched_t> device (
      mock.control, fr::slip::slip_codec (), bench.clock);
  device.set_behavior (tr::testing::behavior::silent ());
  std::shared_ptr<tr::stream::stream_impl> const impl = mock.stream.impl ();
  std::vector<pr::discovery_candidate_t> candidates (1);
  candidates[0].kind = pr::discovery_candidate_t::candidate_kind::stream;
  candidates[0].stream_factory = [impl] (tr::executor_type const &)
    { return tr::stream::byte_stream (impl); };
  // More than the period of the keepalive.
  bench.options.per_candidate = ms (450);
  int calls = 0;
  results_t results;
  pr::async_discover<rt::watched_t, wire_t> (
      bench.context.get_executor (), candidates, bench.options,
      [&] (std::error_code, results_t got)
        {
          ++calls;
          results = std::move (got);
        });
  ASSERT_TRUE (bench.run_until ([&] () { return calls != 0; }));
  EXPECT_EQ (results[0].reason, discovery_errc::no_response);
  // The identification alone.
  EXPECT_EQ (device.statistics ().requests, 1u);
}

TEST (LumexDiscover, Discover_NoLimitOfTheCandidateLeavesTheRowTimeout)
{
  // per_candidate 0: the timeout of the row ends it.
  bench_t bench;
  place_t &silent = bench.add ();
  silent.device->set_behavior (tr::testing::behavior::silent ());
  bench.options.per_candidate = ms (0);
  std::vector<pr::discovery_candidate_t> candidates;
  candidates.push_back (silent.candidate ());
  results_t const results = bench.discover (candidates);
  EXPECT_EQ (results[0].reason, discovery_errc::no_response);
}

TEST (LumexDiscover, Discover_AnEmptyListEndsNeverInline)
{
  bench_t bench;
  int calls = 0;
  std::error_code error = make_error_code (transport_errc::timeout);
  std::size_t count = 99;
  pr::async_discover<gauge, wire_t> (bench.context.get_executor (),
                                     std::vector<pr::discovery_candidate_t> (),
                                     bench.options,
                                     [&] (std::error_code ended, results_t got)
                                       {
                                         ++calls;
                                         error = ended;
                                         count = got.size ();
                                       });
  EXPECT_EQ (calls, 0);
  bench.settle ();
  EXPECT_EQ (calls, 1);
  EXPECT_FALSE (error);
  EXPECT_EQ (count, 0u);
}

struct threaded_t
{
  threaded_t ()
      : guard (asio::make_work_guard (context)),
        runner ([this] () { context.run (); })
  {
  }

  ~threaded_t ()
  {
    guard.reset ();
    context.stop ();
    runner.join ();
  }

  asio::io_context context;
  asio::executor_work_guard<asio::io_context::executor_type> guard;
  std::thread runner;
};

/// A device on a mock of the runner, on the steady clock.
struct real_place_t
{
  explicit real_place_t (asio::io_context &context)
      : mock (tr::testing::make_mock_stream (context.get_executor ()))
  {
    device.reset (new tr::testing::device_simulator<gauge_a> (
        mock.control, fr::slip::slip_codec (), clock));
    device->set_reply<gauge_a::WHO_t> (gauge_who ("G100", 3, 1));
    std::shared_ptr<tr::stream::stream_impl> const impl = mock.stream.impl ();
    candidates.resize (1);
    candidates[0].kind = pr::discovery_candidate_t::candidate_kind::stream;
    candidates[0].stream_factory = [impl] (tr::executor_type const &)
      { return tr::stream::byte_stream (impl); };
  }

  tr::testing::virtual_clock clock;
  tr::testing::mock_stream mock;
  std::unique_ptr<tr::testing::device_simulator<gauge_a>> device;
  std::vector<pr::discovery_candidate_t> candidates;
};

TEST (LumexDiscover, Token_UseFuture)
{
  threaded_t bench;
  real_place_t place (bench.context);
  std::future<results_t> future = pr::async_discover<gauge, wire_t> (
      bench.context.get_executor (), place.candidates,
      pr::discovery_options_t (), asio::use_future);
  ASSERT_EQ (future.wait_for (std::chrono::seconds (10)),
             std::future_status::ready);
  results_t const results = future.get ();
  ASSERT_EQ (results.size (), 1u);
  EXPECT_TRUE (results[0].found);
}

TEST (LumexDiscover, Token_Deferred)
{
  threaded_t bench;
  real_place_t place (bench.context);
  auto deferred = pr::async_discover<gauge, wire_t> (
      bench.context.get_executor (), place.candidates,
      pr::discovery_options_t (), asio::deferred);
  std::this_thread::sleep_for (ms (5));
  // Nothing started before it is given a token.
  EXPECT_EQ (place.device->statistics ().requests, 0u);
  std::promise<results_t> promise;
  std::move (deferred) ([&] (std::error_code, results_t got)
                          { promise.set_value (std::move (got)); });
  std::future<results_t> future = promise.get_future ();
  ASSERT_EQ (future.wait_for (std::chrono::seconds (10)),
             std::future_status::ready);
  EXPECT_TRUE (future.get ()[0].found);
}

#if defined(LUMEX_TRANSPORT_WITH_SPAWN)
TEST (LumexDiscover, Token_YieldContext)
{
  threaded_t bench;
  real_place_t place (bench.context);
  std::promise<results_t> promise;
  asio::spawn (
      bench.context,
      [&] (asio::yield_context yield)
        {
          promise.set_value (pr::async_discover<gauge, wire_t> (
              bench.context.get_executor (), place.candidates,
              pr::discovery_options_t (), yield));
        },
      asio::detached);
  std::future<results_t> future = promise.get_future ();
  ASSERT_EQ (future.wait_for (std::chrono::seconds (10)),
             std::future_status::ready);
  EXPECT_TRUE (future.get ()[0].found);
}
#endif

TEST (LumexDiscover, Token_RunsOnTheExecutorOfTheHandler)
{
  // A handler bound elsewhere runs there, and no work is kept on it.
  bench_t bench;
  asio::io_context other;
  bool ran = false;
  auto guard = asio::make_work_guard (other);
  pr::async_discover<gauge, wire_t> (
      bench.context.get_executor (), std::vector<pr::discovery_candidate_t> (),
      bench.options,
      asio::bind_executor (other,
                           [&] (std::error_code, results_t) { ran = true; }));
  bench.settle ();
  EXPECT_FALSE (ran);
  other.poll ();
  EXPECT_TRUE (ran);
}

TEST (LumexDiscover, Cancel_EndsEveryProbeWithOneCompletion)
{
  // Falsified by a signal that ends nothing, by a second completion when the
  // probes end, and by a probe started after it.
  bench_t bench;
  auto const made = std::make_shared<std::atomic<int>> (0);
  bench.options.parallel = 2;
  bench.options.per_candidate = ms (1000);
  asio::cancellation_signal signal;
  int calls = 0;
  std::error_code error;
  results_t results;
  pr::async_discover<gauge, wire_t> (
      bench.context.get_executor (), held_candidates (4, made, bench.clock),
      bench.options,
      asio::bind_cancellation_slot (signal.slot (),
                                    [&] (std::error_code ended, results_t got)
                                      {
                                        ++calls;
                                        error = ended;
                                        results = std::move (got);
                                      }));
  bench.settle ();
  EXPECT_EQ (made->load (), 2);
  signal.emit (asio::cancellation_type::terminal);
  bench.settle ();
  EXPECT_EQ (calls, 1);
  EXPECT_EQ (error, transport_errc::cancelled);
  ASSERT_EQ (results.size (), 4u);
  for (std::size_t i = 0; i < results.size (); ++i)
    EXPECT_EQ (results[i].reason, transport_errc::cancelled);
  EXPECT_EQ (made->load (), 2);
  bench.clock.advance (ms (2000));
  bench.settle ();
  EXPECT_EQ (calls, 1);
  EXPECT_EQ (made->load (), 2);
  // A signal after the result was handed on ends nothing.
  signal.emit (asio::cancellation_type::terminal);
  bench.settle ();
  EXPECT_EQ (calls, 1);
}

TEST (LumexDiscover, Cancel_APartialAndATotalCancelToo)
{
  for (asio::cancellation_type_t type :
       { asio::cancellation_type::partial, asio::cancellation_type::total })
    {
      bench_t bench;
      auto const made = std::make_shared<std::atomic<int>> (0);
      asio::cancellation_signal signal;
      int calls = 0;
      std::error_code error;
      pr::async_discover<gauge, wire_t> (
          bench.context.get_executor (),
          held_candidates (1, made, bench.clock), bench.options,
          asio::bind_cancellation_slot (signal.slot (),
                                        [&] (std::error_code ended, results_t)
                                          {
                                            ++calls;
                                            error = ended;
                                          }));
      bench.settle ();
      signal.emit (type);
      bench.settle ();
      EXPECT_EQ (calls, 1);
      EXPECT_EQ (error, transport_errc::cancelled);
    }
}

TEST (LumexDiscover, Cancel_DuringTheIdentification)
{
  bench_t bench;
  place_t &silent = bench.add ();
  silent.device->set_behavior (tr::testing::behavior::silent ());
  std::vector<pr::discovery_candidate_t> candidates;
  candidates.push_back (silent.candidate ());
  asio::cancellation_signal signal;
  int calls = 0;
  std::error_code error;
  pr::async_discover<gauge, wire_t> (
      bench.context.get_executor (), candidates, bench.options,
      asio::bind_cancellation_slot (signal.slot (),
                                    [&] (std::error_code ended, results_t)
                                      {
                                        ++calls;
                                        error = ended;
                                      }));
  bench.settle ();
  EXPECT_EQ (silent.device->statistics ().requests, 1u);
  signal.emit (asio::cancellation_type::terminal);
  bench.settle ();
  EXPECT_EQ (calls, 1);
  EXPECT_EQ (error, transport_errc::cancelled);
  EXPECT_FALSE (silent.mock.stream.is_open ());
}

TEST (LumexDiscover, Lifetime_TheContextGoesFirst)
{
  // The handler dies uncalled; a handler that holds what holds the discovery
  // is no leak (LeakSanitizer).
  int calls = 0;
  std::weak_ptr<int> watch;
  {
    auto const canary = std::make_shared<int> (1);
    watch = canary;
    asio::io_context context;
    tr::testing::virtual_clock clock;
    auto const made = std::make_shared<std::atomic<int>> (0);
    pr::discovery_options_t options;
    options.timers = clock.source ();
    pr::async_discover<gauge, wire_t> (
        context.get_executor (), held_candidates (3, made, clock), options,
        [canary, &calls] (std::error_code, results_t) { ++calls; });
    context.poll ();
    EXPECT_EQ (made->load (), 3);
  }
  EXPECT_EQ (calls, 0);
  EXPECT_TRUE (watch.expired ());
}

TEST (LumexDiscover, Lifetime_AHandlerMayStartAnother)
{
  bench_t bench;
  place_t &place = bench.add ();
  std::vector<pr::discovery_candidate_t> candidates;
  candidates.push_back (place.candidate ());
  int calls = 0;
  pr::async_discover<gauge, wire_t> (
      bench.context.get_executor (), candidates, bench.options,
      [&] (std::error_code, results_t)
        {
          pr::async_discover<gauge, wire_t> (
              bench.context.get_executor (), candidates, bench.options,
              [&] (std::error_code, results_t got)
                { calls += got[0].found ? 1 : 100; });
        });
  ASSERT_TRUE (bench.run_until ([&] () { return calls != 0; }));
  EXPECT_EQ (calls, 1);
}

TEST (LumexDiscover, Threads_ManyDiscoveriesOnOneRunner)
{
  // Under TSan: a few at once on two threads of a pool, each with exactly one
  // completion.
  asio::io_context context;
  auto guard = asio::make_work_guard (context);
  std::vector<std::unique_ptr<real_place_t>> places;
  for (int i = 0; i < 4; ++i)
    places.emplace_back (new real_place_t (context));
  std::thread first ([&] () { context.run (); });
  std::thread second ([&] () { context.run (); });
  std::atomic<int> calls (0);
  std::atomic<int> found (0);
  for (int round = 0; round < 3; ++round)
    {
      int const target = (round + 1) * 4;
      for (std::size_t i = 0; i < places.size (); ++i)
        pr::async_discover<gauge, wire_t> (context.get_executor (),
                                           places[i]->candidates,
                                           pr::discovery_options_t (),
                                           [&] (std::error_code, results_t got)
                                             {
                                               if (got[0].found)
                                                 ++found;
                                               ++calls;
                                             });
      auto const end
          = std::chrono::steady_clock::now () + std::chrono::seconds (20);
      while (calls.load () < target && std::chrono::steady_clock::now () < end)
        std::this_thread::sleep_for (ms (1));
      EXPECT_EQ (calls.load (), target);
      std::this_thread::sleep_for (ms (5));
    }
  EXPECT_EQ (found.load (), 12);
  places.clear ();
  guard.reset ();
  context.stop ();
  first.join ();
  second.join ();
}
} // namespace
