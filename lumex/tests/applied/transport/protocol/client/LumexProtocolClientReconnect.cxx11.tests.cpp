// The generated client over a session, the part of the identification: what a
// new connection repeats (the identification, the layer, the streams), the
// epoch that ends a partial frame, a device that comes back as another or as
// none, a loss during the check or before the streams are on, a table without
// the means to check. On a virtual clock; the device table is invented.

#include <cstddef>
#include <cstdint>
#include <memory>
#include <system_error>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/tests/applied/transport/protocol/client/LumexClientReconnectBench.hpp"

namespace
{
using namespace lumex_client_test;

TEST (LumexProtocolClientReconnect, Reconnect_ReidentifiesAndReselectsLayer)
{
  // Falsified by a client that keeps what it knew: no WHO on the new link.
  identified_bench_t bench;
  auto const before = bench.requests ();
  bench.reconnect ();
  EXPECT_GT (bench.requests (), before);
  EXPECT_FALSE (bench.seen->error) << bench.seen->error.message ();
  EXPECT_EQ (bench.seen->identity.model, "G100");
  EXPECT_EQ (bench.seen->identity.layer, "gauge_a_t");
  EXPECT_EQ (bench.client->session ().model (), "G100");
  EXPECT_EQ (bench.client->session ().layer (), "gauge_a_t");
  EXPECT_EQ (bench.client->session ().firmware (), 3u);
  ASSERT_TRUE (bench.client->identity ().has_value ());
  EXPECT_EQ (bench.client->identity ()->model, "G100");
  // The layer works again.
  bench.device->set_reply<gauge_a::ONLY_A_t> (std::uint8_t (9));
  auto const only = new_outcome<gauge_a::ONLY_A_t> ();
  bench.client->as<gauge_a> ().ONLY_A (into<gauge_a::ONLY_A_t> (only));
  ASSERT_TRUE (bench.run_until ([&] () { return only->calls != 0; }));
  EXPECT_FALSE (only->error) << only->error.message ();
  EXPECT_EQ (only->value, 9);
}

TEST (LumexProtocolClientReconnect,
      Reconnect_BetweenLossAndCheckNothingIsAccepted)
{
  // Falsified by a client whose layer stays through the loss: as<L> goes out.
  identified_bench_t bench;
  bench.device->set_behavior (tr::testing::behavior::silent ());
  bench.lose ();
  // The link is open again and the identification under way.
  EXPECT_EQ (bench.session->state (), ss::channel_state::open);
  EXPECT_EQ (bench.client->identity ().error (), protocol_errc::link_lost);
  EXPECT_TRUE (bench.client->session ().model ().empty ());
  EXPECT_TRUE (bench.client->session ().layer ().empty ());
  EXPECT_EQ (bench.client->session ().firmware (), 0u);
  std::uint64_t const requests = bench.device->statistics ().requests;
  auto const as = new_outcome<gauge_a::ONLY_A_t> ();
  bench.client->as<gauge_a> ().ONLY_A (into<gauge_a::ONLY_A_t> (as));
  auto const own = new_outcome<gauge::SET_t> ();
  bench.client->SET (std::uint8_t (1), into<gauge::SET_t> (own));
  bench.settle ();
  EXPECT_EQ (as->calls, 1);
  EXPECT_EQ (as->error, protocol_errc::link_lost);
  EXPECT_EQ (own->calls, 1);
  EXPECT_EQ (own->error, protocol_errc::link_lost);
  EXPECT_EQ (bench.device->statistics ().requests, requests);
}

TEST (LumexProtocolClientReconnect, Reconnect_NoGapsOfAStreamDuringTheOutage)
{
  // Falsified by a watch that goes on: a lost link would count as silence.
  identified_bench_t bench;
  auto const go = new_outcome<gauge::GO_t> ();
  bench.client->GO (into<gauge::GO_t> (go));
  ASSERT_TRUE (bench.run_until ([&] () { return go->calls != 0; }));
  bench.mock->control.fail_opens_for (
      ms (2000), make_error_code (tr::error::transport_errc::disconnected));
  bench.lose ();
  std::uint64_t const gaps = bench.client->statistics ().stream_gaps;
  bench.advance (ms (1500));
  EXPECT_EQ (bench.client->statistics ().stream_gaps, gaps);
}

TEST (LumexProtocolClientReconnect, Reconnect_ReenablesStreams)
{
  // Falsified by a restoration that forgets the row, and by one that sends
  // what the application never turned on.
  identified_bench_t bench;
  bench.reconnect ();
  EXPECT_EQ (requests_of (bench.device->statistics (), "GO"), 0u);
  auto const go = new_outcome<gauge::GO_t> ();
  bench.client->GO (into<gauge::GO_t> (go));
  ASSERT_TRUE (bench.run_until ([&] () { return go->calls != 0; }));
  EXPECT_FALSE (go->error) << go->error.message ();
  EXPECT_EQ (requests_of (bench.device->statistics (), "GO"), 1u);
  bench.reconnect ();
  EXPECT_FALSE (bench.seen->error);
  EXPECT_EQ (requests_of (bench.device->statistics (), "GO"), 2u);
  // A stream that does not come back is a gap again: it is watched.
  std::uint64_t const gaps = bench.client->statistics ().stream_gaps;
  bench.advance (ms (500));
  EXPECT_GT (bench.client->statistics ().stream_gaps, gaps);
}

TEST (LumexProtocolClientReconnect, Reconnect_ALostStreamRowIsReported)
{
  // Falsified by a restoration that stops on it: the dialogue stays closed.
  identified_bench_t bench;
  auto const go = new_outcome<gauge::GO_t> ();
  bench.client->GO (into<gauge::GO_t> (go));
  ASSERT_TRUE (bench.run_until ([&] () { return go->calls != 0; }));
  auto const go_too = new_outcome<gauge::GO_TOO_t> ();
  bench.client->GO_TOO (into<gauge::GO_TOO_t> (go_too));
  ASSERT_TRUE (bench.run_until ([&] () { return go_too->calls != 0; }));
  bench.device->refuse<gauge_a::GO_t> ();
  bench.reconnect ();
  EXPECT_EQ (bench.seen->error, protocol_errc::timeout);
  // The row after it still went.
  EXPECT_EQ (requests_of (bench.device->statistics (), "GO_TOO"), 2u);
  bench.device->set_reply<gauge_a::READ_t> (std::uint16_t (5));
  auto const read = new_outcome<gauge::READ_t> ();
  bench.client->READ (into<gauge::READ_t> (read));
  ASSERT_TRUE (bench.run_until ([&] () { return read->calls != 0; }));
  EXPECT_FALSE (read->error) << read->error.message ();
}

TEST (LumexProtocolClientReconnect, Reconnect_PartialFrameNotCarriedOver)
{
  // Falsified by a view without the epoch: half a reply glues to the next.
  identified_bench_t bench;
  bench.device->set_reply<gauge_a::READ_t> (std::uint16_t (0x1234));
  std::vector<std::uint8_t> half = wire_of ({ 0x10, 0x34 });
  half.pop_back ();
  bench.mock->control.feed (half);
  bench.settle ();
  bench.reconnect ();
  EXPECT_FALSE (bench.seen->error) << bench.seen->error.message ();
  auto const read = new_outcome<gauge::READ_t> ();
  bench.client->READ (into<gauge::READ_t> (read));
  ASSERT_TRUE (bench.run_until ([&] () { return read->calls != 0; }));
  EXPECT_FALSE (read->error) << read->error.message ();
  EXPECT_EQ (read->value, 0x1234);
  EXPECT_EQ (bench.client->statistics ().retries, 0u);
  // The half of the old connection was never a frame.
  EXPECT_EQ (bench.client->statistics ().unexpected_replies, 0u);
}

TEST (LumexProtocolClientReconnect,
      Reconnect_PartialRequestAtTheDeviceDoesNotGlue)
{
  // Falsified by a simulator that keeps a partial frame across an open.
  identified_bench_t bench;
  bench.device->set_reply<gauge_a::READ_t> (std::uint16_t (6));
  // Half a request reaches the device, written by hand on the mock.
  std::vector<std::uint8_t> half = wire_of ({ 0x10 });
  half.pop_back ();
  bool written = false;
  bench.mock->stream.async_write_all (
      tr::const_bytes_t (half.data (), half.size ()),
      [&written] (std::error_code, std::size_t) { written = true; });
  EXPECT_TRUE (bench.run_until ([&] () { return written; }));
  bench.reconnect ();
  std::uint64_t const errors = bench.device->statistics ().framing_errors;
  EXPECT_FALSE (bench.seen->error) << bench.seen->error.message ();
  auto const read = new_outcome<gauge::READ_t> ();
  bench.client->READ (into<gauge::READ_t> (read));
  ASSERT_TRUE (bench.run_until ([&] () { return read->calls != 0; }));
  EXPECT_FALSE (read->error) << read->error.message ();
  EXPECT_EQ (bench.client->statistics ().retries, 0u);
  EXPECT_EQ (bench.device->statistics ().framing_errors, errors);
  // The half of the old connection was never a request.
  EXPECT_EQ (requests_of (bench.device->statistics (), "READ"), 1u);
}

TEST (LumexProtocolClientReconnect, Reconnect_DatagramPartialAndStaleMessages)
{
  // Falsified by a view that hands on a message of the old connection.
  datagram_bench_t<gauge_a, gauge> bench;
  bench.device->set_reply<gauge_a::WHO_t> (gauge_who ("G100", 3, 1));
  auto const out = std::make_shared<outcome_t<pr::identity_t>> ();
  record_t<pr::identity_t> done = { out };
  bench.client->async_identify (done);
  ASSERT_TRUE (bench.run_until ([&] () { return out->calls != 0; }));
  ASSERT_FALSE (out->error) << out->error.message ();
  auto const seen = watch_reconnects (*bench.client);
  bench.lose ();
  ASSERT_TRUE (bench.run_until ([&] () { return seen->calls != 0; }));
  EXPECT_FALSE (seen->error) << seen->error.message ();
  EXPECT_EQ (seen->identity.layer, "gauge_a_t");
  EXPECT_EQ (bench.client->statistics ().reconnects, 1u);
  bench.device->set_reply<gauge_a::READ_t> (std::uint16_t (11));
  auto const read = new_outcome<gauge::READ_t> ();
  bench.client->READ (into<gauge::READ_t> (read));
  ASSERT_TRUE (bench.run_until ([&] () { return read->calls != 0; }));
  EXPECT_FALSE (read->error) << read->error.message ();
  EXPECT_EQ (read->value, 11);
}

TEST (LumexProtocolClientReconnect, Reconnect_ReenumerationUnderVirtualClock)
{
  // The node goes away and comes back 14 s later: attempts at the times of the
  // policy, a call that survives, and a client that comes back.
  identified_bench_t bench;
  bench.device->set_behavior (tr::testing::behavior::silent ());
  auto const read = new_outcome<gauge::SLOW_t> ();
  bench.client->SLOW (into<gauge::SLOW_t> (read));
  bench.settle ();
  bench.device->set_behavior (tr::testing::behavior::perfect ());
  bench.mock->control.fail_opens_for (
      ms (14000), make_error_code (tr::error::transport_errc::disconnected));
  std::size_t const opens = bench.mock->control.open_count ();
  bench.mock->control.disconnect ();
  bench.settle ();
  bench.advance (ms (13900));
  EXPECT_EQ (bench.mock->control.open_count (), opens);
  EXPECT_EQ (read->calls, 0);
  ASSERT_GE (bench.session->session_statistics ().failed_attempts, 3u);
  ASSERT_TRUE (bench.run_until ([&] () { return bench.seen->calls != 0; }));
  EXPECT_FALSE (bench.seen->error) << bench.seen->error.message ();
  EXPECT_EQ (bench.mock->control.open_count (), opens + 1);
  ASSERT_TRUE (bench.run_until ([&] () { return read->calls != 0; }));
  EXPECT_FALSE (read->error) << read->error.message ();
}

TEST (LumexProtocolClientReconnect, Reconnect_ToAnotherLayer)
{
  // Falsified by a client that keeps its old layer, and by calls that follow
  // to the new device.
  identified_bench_t bench;
  bench.device->set_behavior (tr::testing::behavior::silent ());
  auto const read = new_outcome<gauge::SLOW_t> ();
  bench.client->SLOW (into<gauge::SLOW_t> (read));
  auto const only = new_outcome<gauge_a::ONLY_A_t> ();
  bench.client->as<gauge_a> ().ONLY_A (into<gauge_a::ONLY_A_t> (only));
  bench.settle ();
  bench.device->set_behavior (tr::testing::behavior::perfect ());
  bench.device->set_reply<gauge_a::WHO_t> (gauge_who ("G200", 4, 2));
  int const before = bench.seen->calls;
  bench.mock->control.disconnect ();
  ASSERT_TRUE (
      bench.run_until ([&] () { return bench.seen->calls != before; }));
  bench.settle ();
  // It works, and says who it is.
  EXPECT_FALSE (bench.seen->error) << bench.seen->error.message ();
  EXPECT_EQ (bench.seen->identity.layer, "gauge_b_t");
  EXPECT_EQ (bench.seen->identity.model, "G200");
  EXPECT_STREQ (bench.client->engine ().index ().protocol_name (),
                "gauge_b_t");
  EXPECT_EQ (bench.client->session ().layer (), "gauge_b_t");
  EXPECT_EQ (read->calls, 1);
  EXPECT_EQ (read->error, protocol_errc::link_lost);
  // A row the new layer has not ends the same way.
  EXPECT_EQ (only->calls, 1);
  EXPECT_EQ (only->error, protocol_errc::link_lost);
  // The old layer is closed, the new one open.
  auto const old = new_outcome<gauge_a::ONLY_A_t> ();
  bench.client->as<gauge_a> ().ONLY_A (into<gauge_a::ONLY_A_t> (old));
  bench.settle ();
  EXPECT_EQ (old->error, protocol_errc::not_supported);
}

TEST (LumexProtocolClientReconnect, Reconnect_ToAnotherDeviceOfTheSameLayer)
{
  // Falsified by a comparison of the layer alone: the waiting call goes to a
  // device it was not made for.
  identified_bench_t bench;
  auto const go = new_outcome<gauge::GO_t> ();
  bench.client->GO (into<gauge::GO_t> (go));
  ASSERT_TRUE (bench.run_until ([&] () { return go->calls != 0; }));
  bench.device->set_behavior (tr::testing::behavior::silent ());
  auto const read = new_outcome<gauge::SLOW_t> ();
  bench.client->SLOW (into<gauge::SLOW_t> (read));
  bench.settle ();
  bench.device->set_behavior (tr::testing::behavior::perfect ());
  bench.device->set_reply<gauge_a::WHO_t> (gauge_who ("G100", 3, 1, 2));
  int const before = bench.seen->calls;
  bench.mock->control.disconnect ();
  ASSERT_TRUE (
      bench.run_until ([&] () { return bench.seen->calls != before; }));
  bench.settle ();
  EXPECT_EQ (bench.seen->identity.serial_number, "2");
  EXPECT_EQ (read->error, protocol_errc::link_lost);
  EXPECT_EQ (requests_of (bench.device->statistics (), "SLOW"), 1u);
  // No stream row goes to it.
  EXPECT_EQ (requests_of (bench.device->statistics (), "GO"), 1u);
}

TEST (LumexProtocolClientReconnect,
      Reconnect_ADeviceThatNamesNoLayerClosesTheClient)
{
  // Falsified by a gate that opens on any identification.
  identified_bench_t bench;
  bench.device->set_reply<gauge_a::WHO_t> (gauge_who ("G900", 9, 9));
  bench.reconnect ();
  EXPECT_EQ (bench.seen->error, protocol_errc::not_supported);
  EXPECT_EQ (bench.seen->identity.model, "G900");
  EXPECT_EQ (bench.client->identity ().error (), protocol_errc::not_supported);
  EXPECT_TRUE (bench.client->session ().layer ().empty ());
  auto const as = new_outcome<gauge_a::ONLY_A_t> ();
  bench.client->as<gauge_a> ().ONLY_A (into<gauge_a::ONLY_A_t> (as));
  auto const own = new_outcome<gauge::READ_t> ();
  bench.client->READ (into<gauge::READ_t> (own));
  bench.settle ();
  EXPECT_EQ (as->error, protocol_errc::not_supported);
  EXPECT_EQ (own->error, protocol_errc::not_supported);
  EXPECT_EQ (requests_of (bench.device->statistics (), "READ"), 0u);
  // An identify of the application that finds the device opens it.
  bench.device->set_reply<gauge_a::WHO_t> (gauge_who ("G100", 3, 1));
  ASSERT_FALSE (bench.identify ()->error);
  bench.device->set_reply<gauge_a::READ_t> (std::uint16_t (8));
  auto const read = new_outcome<gauge::READ_t> ();
  bench.client->READ (into<gauge::READ_t> (read));
  ASSERT_TRUE (bench.run_until ([&] () { return read->calls != 0; }));
  EXPECT_FALSE (read->error) << read->error.message ();
  EXPECT_EQ (read->value, 8);
}

TEST (LumexProtocolClientReconnect,
      Reconnect_ACheckThatFailsEndsWhatWaitsAtOnce)
{
  // Falsified by what waited staying for the time of the outage.
  identified_bench_t bench;
  bench.device->set_behavior (tr::testing::behavior::silent ());
  auto const read = new_outcome<gauge::SLOW_t> ();
  bench.client->SLOW (into<gauge::SLOW_t> (read));
  bench.settle ();
  bench.device->set_behavior (tr::testing::behavior::perfect ());
  bench.device->set_reply<gauge_a::WHO_t> (gauge_who ("G900", 9, 9));
  bench.reconnect ();
  EXPECT_EQ (bench.seen->error, protocol_errc::not_supported);
  EXPECT_EQ (read->calls, 1);
  EXPECT_EQ (read->error, protocol_errc::link_lost);
}

TEST (LumexProtocolClientReconnect,
      Reconnect_ANewConnectionChecksAfterAFailedCheck)
{
  identified_bench_t bench;
  bench.device->set_reply<gauge_a::WHO_t> (gauge_who ("G900", 9, 9));
  bench.reconnect ();
  EXPECT_EQ (bench.seen->error, protocol_errc::not_supported);
  bench.device->set_reply<gauge_a::WHO_t> (gauge_who ("G100", 3, 1));
  bench.reconnect ();
  EXPECT_FALSE (bench.seen->error) << bench.seen->error.message ();
  ASSERT_TRUE (bench.client->identity ().has_value ());
}

TEST (LumexProtocolClientReconnect,
      Reconnect_AnIdentifyThatTimesOutConnectsAgain)
{
  // Falsified by a dialogue that stays with the silent device, and by one that
  // opens on the timeout.
  identified_bench_t bench;
  bench.device->set_behavior (tr::testing::behavior::silent ());
  std::size_t const opens = bench.mock->control.open_count ();
  bench.lose ();
  auto const read = new_outcome<gauge::READ_t> ();
  ASSERT_TRUE (bench.run_until ([&] () { return bench.seen->calls != 0; }));
  EXPECT_EQ (bench.seen->error, protocol_errc::timeout);
  bench.client->READ (into<gauge::READ_t> (read));
  bench.settle ();
  EXPECT_EQ (read->error, protocol_errc::link_lost);
  // Round again, and then the device answers.
  bench.device->set_behavior (tr::testing::behavior::perfect ());
  int const before = bench.seen->calls;
  ASSERT_TRUE (
      bench.run_until ([&] () { return bench.seen->calls != before; }));
  EXPECT_FALSE (bench.seen->error) << bench.seen->error.message ();
  EXPECT_GE (bench.mock->control.open_count (), opens + 2);
}

TEST (LumexProtocolClientReconnect, Reconnect_ALossDuringTheCheckStartsItAgain)
{
  // Falsified by a check of the past that ends the check of the new link.
  identified_bench_t bench;
  bench.device->set_behavior (tr::testing::behavior::silent ());
  bench.lose ();
  EXPECT_EQ (bench.session->state (), ss::channel_state::open);
  std::uint64_t const reconnects = bench.client->statistics ().reconnects;
  bench.device->set_behavior (tr::testing::behavior::perfect ());
  bench.mock->control.disconnect ();
  ASSERT_TRUE (bench.run_until ([&] () { return bench.seen->calls != 0; }));
  EXPECT_FALSE (bench.seen->error) << bench.seen->error.message ();
  EXPECT_EQ (bench.seen->calls, 1);
  EXPECT_EQ (bench.client->statistics ().reconnects, reconnects + 1);
  ASSERT_TRUE (bench.client->identity ().has_value ());
}

TEST (LumexProtocolClientReconnect, Reconnect_ALossBeforeTheStreamsAreOn)
{
  identified_bench_t bench;
  auto const go = new_outcome<gauge::GO_t> ();
  bench.client->GO (into<gauge::GO_t> (go));
  ASSERT_TRUE (bench.run_until ([&] () { return go->calls != 0; }));
  bool lost_once = false;
  bench.device->on<gauge_a::GO_t> (
      [&bench, &lost_once] (field::record<> const &)
        {
          if (lost_once)
            return;
          lost_once = true;
          bench.mock->control.disconnect ();
        });
  bench.lose ();
  ASSERT_TRUE (bench.run_until ([&] () { return bench.seen->calls != 0; }));
  EXPECT_TRUE (lost_once);
  EXPECT_FALSE (bench.seen->error) << bench.seen->error.message ();
  EXPECT_EQ (requests_of (bench.device->statistics (), "GO"), 3u);
}

TEST (LumexProtocolClientReconnect, Reconnect_ATableWithoutLayerSet)
{
  // The identity says the own layer; another model is another device.
  stream_bench_t<rt::single_t> bench;
  bench.device->set_reply<rt::single_t::WHO_t> (
      gauge_who<pr::reply_of<rt::single_t::WHO_t>> ("S1", 1, 0));
  ASSERT_FALSE (bench.identify ()->error);
  auto const seen = std::make_shared<reconnects_seen_t> ();
  bench.client->on_reconnect (
      [seen] (std::error_code const &error, pr::identity_t const &identity)
        {
          ++seen->calls;
          seen->error = error;
          seen->identity = identity;
        });
  bench.lose ();
  ASSERT_TRUE (bench.run_until ([&] () { return seen->calls != 0; }));
  EXPECT_FALSE (seen->error) << seen->error.message ();
  EXPECT_EQ (seen->identity.layer, "single_t");
  EXPECT_EQ (bench.client->identity ()->model, "S1");
}

TEST (LumexProtocolClientReconnect, Reconnect_ATableWithoutIdentifyWith)
{
  // Nothing to check: the same device, the calls go on, the state is
  // forgotten.
  stream_bench_t<rt::plain_gauge_t> bench;
  auto const seen = std::make_shared<reconnects_seen_t> ();
  bench.client->on_reconnect (
      [seen] (std::error_code const &error, pr::identity_t const &identity)
        {
          ++seen->calls;
          seen->error = error;
          seen->identity = identity;
        });
  bench.client->engine ().set_state (7);
  bench.settle ();
  EXPECT_TRUE (bench.client->engine ().state_known ());
  auto const refused = new_outcome<rt::plain_gauge_t::BUSY_t> ();
  bench.client->BUSY (into<rt::plain_gauge_t::BUSY_t> (refused));
  bench.settle ();
  EXPECT_EQ (refused->error, protocol_errc::not_accepted_in_state);
  bench.lose ();
  EXPECT_TRUE (bench.run_until ([&] () { return seen->calls != 0; }));
  EXPECT_FALSE (seen->error) << seen->error.message ();
  EXPECT_TRUE (seen->identity.model.empty ());
  EXPECT_EQ (bench.client->identity ().error (), protocol_errc::not_supported);
  EXPECT_FALSE (bench.client->engine ().state_known ());
  bench.device->set_reply<rt::plain_gauge_t::READ_t> (std::uint16_t (3));
  auto const read = new_outcome<rt::plain_gauge_t::READ_t> ();
  bench.client->READ (into<rt::plain_gauge_t::READ_t> (read));
  ASSERT_TRUE (bench.run_until ([&] () { return read->calls != 0; }));
  EXPECT_FALSE (read->error) << read->error.message ();
  // The state is known by no frame yet: accepted_in lets it go.
  bench.device->set_state (1);
  bench.device->set_reply<rt::plain_gauge_t::BUSY_t> (std::uint8_t (1));
  auto const busy = new_outcome<rt::plain_gauge_t::BUSY_t> ();
  bench.client->BUSY (into<rt::plain_gauge_t::BUSY_t> (busy));
  ASSERT_TRUE (bench.run_until ([&] () { return busy->calls != 0; }));
  EXPECT_FALSE (busy->error) << busy->error.message ();
}

TEST (LumexProtocolClientReconnect,
      Reconnect_ATableWithoutAReaderOfTheIdentity)
{
  // Compiles and goes on: it cannot identify, so it checks nothing.
  stream_bench_t<rt::unread_t> bench;
  auto const seen = std::make_shared<reconnects_seen_t> ();
  bench.client->on_reconnect (
      [seen] (std::error_code const &error, pr::identity_t const &)
        {
          ++seen->calls;
          seen->error = error;
        });
  bench.lose ();
  EXPECT_TRUE (bench.run_until ([&] () { return seen->calls != 0; }));
  EXPECT_FALSE (seen->error) << seen->error.message ();
}

TEST (LumexProtocolClientReconnect,
      Reconnect_AnApplicationIdentifyDuringTheCheckEndsOnce)
{
  // Falsified by a check that takes the identify of the application for its
  // own, and by one that leaves it without an end.
  stream_bench_t<gauge_a, gauge> bench;
  bench.device->set_reply<gauge_a::WHO_t> (gauge_who ("G100", 3, 1));
  ASSERT_FALSE (bench.identify ()->error);
  auto const seen = watch_reconnects (*bench.client);
  bench.device->set_behavior (tr::testing::behavior::silent ());
  bench.lose ();
  bench.advance (ms (110));
  auto const app = std::make_shared<outcome_t<pr::identity_t>> ();
  record_t<pr::identity_t> done = { app };
  bench.client->async_identify (done);
  bench.advance (ms (20));
  bench.device->set_behavior (tr::testing::behavior::perfect ());
  ASSERT_TRUE (bench.run_until ([&] () { return app->calls != 0; }));
  // A check that failed is reported too: the end that comes has no error.
  ASSERT_TRUE (
      bench.run_until ([&] () { return seen->calls != 0 && !seen->error; }));
  bench.settle ();
  EXPECT_EQ (app->calls, 1);
  bench.device->set_reply<gauge_a::READ_t> (std::uint16_t (5));
  auto const read = new_outcome<gauge::READ_t> ();
  bench.client->READ (into<gauge::READ_t> (read));
  ASSERT_TRUE (bench.run_until ([&] () { return read->calls != 0; }));
  EXPECT_FALSE (read->error) << read->error.message ();
  EXPECT_EQ (read->value, 5);
}

/// A dialogue_engine over a session, without a client: the exchanges are made
/// by hand, so a row can be sent with what the generated client never sends.
class engine_bench_t
{
public:
  struct factory_t
  {
    std::shared_ptr<tr::stream::stream_impl> impl;

    tr::stream::byte_stream
    operator() (tr::executor_type const &) const
    {
      return tr::stream::byte_stream (impl);
    }
  };

  static tr::testing::mock_options_t
  timed (tr::testing::virtual_clock &clock)
  {
    tr::testing::mock_options_t options;
    options.timers = clock.source ();
    return options;
  }

  engine_bench_t (pr::dialogue_options_t options, bool with_device)
      : mock (tr::testing::make_mock_stream (context.get_executor (),
                                             timed (clock))),
        session (context.get_executor (), factory_t{ mock.stream.impl () },
                 bench_session_options (clock)),
        indexed (index::protocol_index<gauge> ())
  {
    if (with_device)
      device.reset (new tr::testing::device_simulator<gauge_a> (
          mock.control, fr::slip::slip_codec (), clock));
    session.start ();
    settle ();
    options.timers = clock.source ();
    engine.reset (new pr::dialogue_engine (
        pr::frame_link (tr::framed::framed_stream<fr::slip::slip_codec> (
            session.as_byte_stream (), fr::slip::slip_codec ())),
        *indexed, options));
    settle ();
    advance (ms (200));
  }

  ~engine_bench_t ()
  {
    engine.reset ();
    settle ();
  }

  void
  settle ()
  {
    tr::testing::run_until (context, clock, [] () { return false; }, ms (0));
  }

  void
  advance (ms by)
  {
    tr::testing::run_until (context, clock, [] () { return false; }, by);
  }

  /// The exchange of the row GO (it enables a stream), its message followed
  /// by `extra`.
  pr::exchange_t
  go (std::vector<std::uint8_t> const &extra) const
  {
    using traits = dialogue::row_traits_t<gauge, gauge::GO_t>;
    pr::exchange_t request;
    layout::encode_message<gauge, gauge::GO_t> (request.message);
    request.message.insert (request.message.end (), extra.begin (),
                            extra.end ());
    request.row = (*indexed).find_name (gauge::GO_t::identifier ());
    request.how = traits::how ();
    request.reply_row = nullptr;
    request.matcher = traits::matcher ();
    request.reader = dialogue::reply_reader_of<gauge, gauge::GO_t> ();
    return request;
  }

  asio::io_context context;
  tr::testing::virtual_clock clock;
  tr::testing::mock_stream mock;
  ss::stream_session session;
  index::index_result_t const &indexed;
  std::unique_ptr<tr::testing::device_simulator<gauge_a>> device;
  std::unique_ptr<pr::dialogue_engine> engine;
};

TEST (LumexProtocolClientReconnect,
      Reconnect_ACheckThatEndsTwiceSendsTheStreamsOnce)
{
  // Falsified by a restoration that starts again at the second end of the
  // check: the stream row would go out twice on one connection.
  pr::dialogue_options_t options = dialogue::options_for<gauge> ();
  std::vector<pr::reconnect_done_t> held;
  options.reconnect.check = [&held] (pr::reconnect_done_t done)
    { held.push_back (std::move (done)); };
  engine_bench_t bench (options, true);
  // The handlers hold a strand of the context: they go before the bench.
  struct drop_held_t
  {
    std::vector<pr::reconnect_done_t> &held;
    ~drop_held_t () { held.clear (); }
  } const drop_held = { held };
  int calls = 0;
  std::error_code ended;
  bench.engine->async_exchange (bench.go ({}),
                                [&] (std::error_code error, pr::reply_frame_t)
                                  {
                                    ++calls;
                                    ended = error;
                                  });
  tr::testing::run_until (
      bench.context, bench.clock, [&] () { return calls != 0; }, ms (5000));
  ASSERT_EQ (calls, 1);
  ASSERT_FALSE (ended) << ended.message ();
  EXPECT_EQ (requests_of (bench.device->statistics (), "GO"), 1u);
  bench.mock.control.disconnect ();
  bench.advance (ms (400));
  ASSERT_EQ (held.size (), 1u);
  held[0](std::error_code (), true);
  held[0](std::error_code (), true);
  bench.settle ();
  bench.advance (ms (50));
  // The first send and the one of the restoration.
  EXPECT_EQ (requests_of (bench.device->statistics (), "GO"), 2u);
}

TEST (LumexProtocolClientReconnect,
      Reconnect_AStreamRowKeepsTheMessageOfItsLastCall)
{
  // Falsified by a restoration that sends the message of the first call of
  // the row (the table cannot make a row with fields enable a stream, so the
  // exchange is made by hand): the device would be set up with the old one.
  engine_bench_t bench (dialogue::options_for<gauge> (), false);
  for (std::uint8_t level = 1; level <= 2; ++level)
    {
      int calls = 0;
      std::error_code ended;
      bench.engine->async_exchange (
          bench.go ({ level }),
          [&] (std::error_code error, pr::reply_frame_t)
            {
              ++calls;
              ended = error;
            });
      bench.settle ();
      bench.mock.control.feed (wire_of ({ 0x14, 0 }));
      tr::testing::run_until (
          bench.context, bench.clock, [&] () { return calls != 0; },
          ms (5000));
      ASSERT_EQ (calls, 1);
      ASSERT_FALSE (ended) << ended.message ();
    }
  std::size_t const mark = bench.mock.control.written ().size ();
  bench.mock.control.disconnect ();
  bench.advance (ms (400));
  std::vector<std::vector<std::uint8_t>> const frames
      = frames_after (bench.mock.control.written (), mark);
  ASSERT_FALSE (frames.empty ());
  EXPECT_EQ (frames[0], (std::vector<std::uint8_t>{ 0x14, 2 }));
}
} // namespace
