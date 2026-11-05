// The generated client over a session, the part of how a connection is given
// up: the watchdog of a link that worked is a new cycle of the session, one of
// a link that received nothing is a failed attempt, a failed check is a failed
// attempt whatever the link carried (over a framed stream on a stream session
// and a framed datagram on a message session). On a virtual clock; the device
// table is invented.

#include <cstdint>
#include <memory>
#include <system_error>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/tests/applied/transport/protocol/client/LumexClientReconnectBench.hpp"

namespace
{
using namespace lumex_client_test;
using watched = rt::watched_t;

using watched_bench_t = stream_bench_t<watched>;

/// A device of the watched table that checks well; the bench is identified.
struct watched_device_t
{
  explicit watched_device_t (std::uint32_t max_attempts)
      : bench (pr::client_options_for<watched> (), true, max_attempts)
  {
    bench.device->set_reply<watched::WHO_t> (
        gauge_who<pr::reply_of<watched::WHO_t>> ("W1", 1, 0));
    EXPECT_FALSE (bench.identify ()->error);
    bench.client->on_reconnect (
        [this] (std::error_code const &error, pr::identity_t const &)
          {
            if (!error)
              ++checked;
          });
  }

  std::uint64_t
  expiries () const
  {
    return bench.client->statistics ().watchdog_expiries;
  }

  std::size_t
  opens () const
  {
    return bench.mock->control.open_count ();
  }

  watched_bench_t bench;
  int checked = 0;
};

/// The virtual times at which the session opened the mock again, while the
/// bench runs `span` in steps of 5 ms.
std::vector<long long>
open_times (watched_device_t &device, ms span)
{
  std::vector<long long> times;
  std::size_t seen = device.opens ();
  for (long long at = 0; at < span.count (); at += 5)
    {
      device.bench.advance (ms (5));
      while (seen < device.opens ())
        {
          times.push_back (at);
          ++seen;
        }
    }
  return times;
}

TEST (LumexProtocolClientReconnectDrop,
      Watchdog_AfterGoodTrafficIsNeverAFailedAttempt)
{
  // Falsified by a watchdog that counts as a failed attempt (the session
  // would end for good at the fourth expiry, and its wait would grow), and by
  // one whose check of the new link starts late.
  watched_device_t device (3);
  for (int turn = 0; turn < 8; ++turn)
    {
      // Good traffic: the keepalive is answered.
      device.bench.advance (ms (500));
      std::uint64_t const before = device.expiries ();
      std::size_t const opens = device.opens ();
      device.bench.device->refuse<watched::BEAT_t> (true);
      ASSERT_TRUE (device.bench.run_until (
          [&] () { return device.expiries () > before; }))
          << turn;
      device.bench.device->refuse<watched::BEAT_t> (false);
      device.bench.advance (ms (30));
      EXPECT_EQ (device.opens (), opens + 1) << turn;
      EXPECT_EQ (device.bench.session->state (), ss::channel_state::open)
          << turn;
      ASSERT_TRUE (device.bench.run_until (
          [&] () { return device.checked == turn + 1; }))
          << turn;
    }
  EXPECT_EQ (device.bench.session->session_statistics ().failed_attempts, 0u);
  EXPECT_EQ (device.bench.session->session_statistics ().reconnects, 8u);
}

TEST (LumexProtocolClientReconnectDrop,
      Watchdog_ALinkThatPassesTheCheckAndFallsSilentIsANewCycleEachTime)
{
  // A device that answers the check and nothing else: the session starts a
  // new cycle at the rate of the watchdog, and the wait does not grow. A
  // known behaviour: the writes of the keepalive to a silent device are no
  // sign of life, the answer to the check is.
  watched_device_t device (3);
  device.bench.device->refuse<watched::BEAT_t> (true);
  std::size_t const opens = device.opens ();
  std::vector<long long> const times = open_times (device, ms (6000));
  EXPECT_GE (device.expiries (), 10u);
  EXPECT_EQ (device.bench.session->state (), ss::channel_state::open);
  EXPECT_EQ (device.bench.session->session_statistics ().failed_attempts, 0u);
  ASSERT_GE (times.size (), 10u);
  EXPECT_EQ (device.opens (), opens + times.size ());
  // Each link lives the time of the watchdog and the next one opens at once:
  // the gaps do not grow.
  for (std::size_t i = 2; i < times.size (); ++i)
    EXPECT_LE (times[i] - times[i - 1], times[1] - times[0] + 60) << i;
}

TEST (LumexProtocolClientReconnectDrop,
      Watchdog_ALinkThatReceivedNothingIsAFailedAttempt)
{
  // Falsified by a watchdog that is always a new cycle (a device that opens
  // and says nothing would be asked every watchdog for ever), and by one
  // whose wait does not grow.
  watched_device_t device (4);
  device.bench.device->set_behavior (tr::testing::behavior::silent ());
  std::size_t const opens = device.opens ();
  std::vector<long long> const times = open_times (device, ms (8000));
  EXPECT_EQ (device.bench.session->state (), ss::channel_state::faulted);
  EXPECT_EQ (device.bench.session->session_statistics ().failed_attempts, 4u);
  ASSERT_EQ (times.size (), 4u);
  EXPECT_EQ (device.opens (), opens + 4);
  // The first expiry ended a link that worked; each next one a silent link
  // that opened after the wait of its attempt: 100, 200, 400 ms.
  EXPECT_GE (times[2] - times[1], times[1] - times[0] + 80);
  EXPECT_GE (times[3] - times[2], times[2] - times[1] + 160);
}

/// A device that sends noise and then nothing: the check times out on a link
/// that carried data.
template <typename Bench>
void
check_fails_on_a_link_that_carried_data (Bench &bench, std::size_t &opens_seen)
{
  bench.device->template set_reply<gauge_a::WHO_t> (gauge_who ("G100", 3, 1));
  auto const out = std::make_shared<outcome_t<pr::identity_t>> ();
  record_t<pr::identity_t> done = { out };
  bench.client->async_identify (done);
  ASSERT_TRUE (bench.run_until ([&] () { return out->calls != 0; }));
  ASSERT_FALSE (out->error) << out->error.message ();
  bench.settle ();
  opens_seen = static_cast<std::size_t> (
      bench.session->session_statistics ().attempts);
  bench.device->set_behavior (tr::testing::behavior::silent ().then (
      tr::testing::behavior::garbage (7, 3)));
  bench.lose ();
  ASSERT_TRUE (bench.run_until (
      [&] () { return bench.session->state () == ss::channel_state::faulted; },
      ms (120000)));
}

TEST (LumexProtocolClientReconnectDrop,
      Check_FailingOnALinkThatCarriedDataIsAFailedAttempt_FramedStream)
{
  // Falsified by a check that gives the connection up as a silent link (it
  // received noise, so the session would start a new cycle each time and
  // never end it), and by a forwarder of the framed stream that drops the
  // call.
  stream_bench_t<gauge_a, gauge> bench (pr::client_options_for<gauge> (), true,
                                        3);
  std::size_t opens = 0;
  check_fails_on_a_link_that_carried_data (bench, opens);
  EXPECT_EQ (bench.session->session_statistics ().failed_attempts, 3u);
  EXPECT_EQ (bench.session->session_statistics ().attempts, opens + 3);
}

TEST (LumexProtocolClientReconnectDrop,
      Check_FailingOnALinkThatCarriedDataIsAFailedAttempt_FramedDatagram)
{
  // The same over a message session.
  datagram_bench_t<gauge_a, gauge> bench (3);
  std::size_t opens = 0;
  check_fails_on_a_link_that_carried_data (bench, opens);
  EXPECT_EQ (bench.session->session_statistics ().failed_attempts, 3u);
  EXPECT_EQ (bench.session->session_statistics ().attempts, opens + 3);
}
} // namespace
