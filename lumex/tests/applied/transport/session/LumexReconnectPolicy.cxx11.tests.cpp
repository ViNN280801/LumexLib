// reconnect_delay, is_link_lost and to_string of the sessions' types.

#include <chrono>
#include <cstdint>
#include <limits>
#include <system_error>

#include <gtest/gtest.h>

#include "lumex/applied/transport/error/LumexTransportErrc.hpp"
#include "lumex/applied/transport/session/LumexSessionTypes.hpp"

namespace
{
namespace tr = lumex::applied::transport;
namespace ss = tr::session;
using tr::error::transport_errc;
typedef std::chrono::milliseconds ms;

ss::reconnect_policy_t
policy (long initial, long max_delay, double multiplier, double jitter)
{
  ss::reconnect_policy_t result;
  result.initial_delay = ms (initial);
  result.max_delay = ms (max_delay);
  result.multiplier = multiplier;
  result.jitter = jitter;
  return result;
}
} // namespace

TEST (
    LumexReconnectPolicyTest,
    GivenNoJitter_WhenAttemptsFail_ThenTheDelayGrowsByTheMultiplierUpToTheMaximum)
{
  ss::reconnect_policy_t const p = policy (100, 1000, 2.0, 0.0);
  EXPECT_EQ (ss::reconnect_delay (p, 1, 0.5), ms (100));
  EXPECT_EQ (ss::reconnect_delay (p, 2, 0.5), ms (200));
  EXPECT_EQ (ss::reconnect_delay (p, 3, 0.5), ms (400));
  EXPECT_EQ (ss::reconnect_delay (p, 4, 0.5), ms (800));
  EXPECT_EQ (ss::reconnect_delay (p, 5, 0.5), ms (1000));
  EXPECT_EQ (ss::reconnect_delay (p, 100, 0.5), ms (1000));
  EXPECT_EQ (
      ss::reconnect_delay (p, std::numeric_limits<std::uint32_t>::max (), 0.5),
      ms (1000));
}

TEST (LumexReconnectPolicyTest,
      GivenJitter_WhenTheRandomNumberMoves_ThenTheDelayMovesWithinTheShare)
{
  ss::reconnect_policy_t const p = policy (100, 1000, 2.0, 0.5);
  EXPECT_EQ (ss::reconnect_delay (p, 1, 0.0), ms (50));
  EXPECT_EQ (ss::reconnect_delay (p, 1, 0.25), ms (75));
  EXPECT_EQ (ss::reconnect_delay (p, 1, 0.5), ms (100));
  EXPECT_EQ (ss::reconnect_delay (p, 1, 0.75), ms (125));
  EXPECT_EQ (ss::reconnect_delay (p, 1, 1.0), ms (150));
}

TEST (
    LumexReconnectPolicyTest,
    GivenJitterAtTheMaximum_WhenTheFactorIsAboveOne_ThenTheDelayStaysAtTheMaximum)
{
  ss::reconnect_policy_t const p = policy (1000, 1000, 2.0, 0.5);
  EXPECT_EQ (ss::reconnect_delay (p, 3, 1.0), ms (1000));
  EXPECT_EQ (ss::reconnect_delay (p, 3, 0.0), ms (500));
}

TEST (LumexReconnectPolicyTest,
      GivenOddInputs_WhenTheDelayIsComputed_ThenItIsClampedNotBroken)
{
  // A multiplier below 1 counts as 1; NaN as well.
  EXPECT_EQ (ss::reconnect_delay (policy (100, 1000, 0.5, 0.0), 5, 0.5),
             ms (100));
  EXPECT_EQ (
      ss::reconnect_delay (
          policy (100, 1000, std::numeric_limits<double>::quiet_NaN (), 0.0),
          5, 0.5),
      ms (100));
  // No initial delay: no delay, whatever the multiplier.
  EXPECT_EQ (ss::reconnect_delay (policy (0, 1000, 2.0, 0.5), 2000, 1.0),
             ms (0));
  // A jitter above 1 counts as 1, below 0 as 0; the random number is
  // clamped to [0, 1].
  EXPECT_EQ (ss::reconnect_delay (policy (100, 1000, 2.0, 7.0), 1, 1.0),
             ms (200));
  EXPECT_EQ (ss::reconnect_delay (policy (100, 1000, 2.0, -1.0), 1, 0.0),
             ms (100));
  EXPECT_EQ (ss::reconnect_delay (policy (100, 1000, 2.0, 0.5), 1, -3.0),
             ms (50));
  EXPECT_EQ (ss::reconnect_delay (policy (100, 1000, 2.0, 0.5), 1, 9.0),
             ms (150));
  // The maximum below the initial delay wins.
  EXPECT_EQ (ss::reconnect_delay (policy (500, 200, 2.0, 0.0), 1, 0.5),
             ms (200));
}

TEST (
    LumexReconnectPolicyTest,
    GivenTheTransportErrors_WhenAskedWhetherTheLinkIsLost_ThenOnlyTheErrorsOfOneOperationKeepIt)
{
  EXPECT_FALSE (ss::is_link_lost (std::error_code ()));
  EXPECT_FALSE (
      ss::is_link_lost (make_error_code (transport_errc::cancelled)));
  EXPECT_FALSE (ss::is_link_lost (make_error_code (transport_errc::timeout)));
  EXPECT_FALSE (
      ss::is_link_lost (make_error_code (transport_errc::would_deadlock)));
  EXPECT_FALSE (
      ss::is_link_lost (make_error_code (transport_errc::report_too_large)));
  EXPECT_TRUE (
      ss::is_link_lost (make_error_code (transport_errc::disconnected)));
  EXPECT_TRUE (ss::is_link_lost (make_error_code (transport_errc::not_open)));
  EXPECT_TRUE (ss::is_link_lost (make_error_code (transport_errc::closed)));
  EXPECT_TRUE (
      ss::is_link_lost (std::make_error_code (std::errc::no_such_device)));
}

TEST (LumexReconnectPolicyTest,
      GivenEachState_WhenNamed_ThenTheNameIsTheStateInLowerCase)
{
  EXPECT_STREQ (ss::to_string (ss::channel_state::closed), "closed");
  EXPECT_STREQ (ss::to_string (ss::channel_state::connecting), "connecting");
  EXPECT_STREQ (ss::to_string (ss::channel_state::open), "open");
  EXPECT_STREQ (ss::to_string (ss::channel_state::reconnecting),
                "reconnecting");
  EXPECT_STREQ (ss::to_string (ss::channel_state::faulted), "faulted");
  EXPECT_STREQ (ss::to_string (static_cast<ss::channel_state> (42)),
                "unknown");
}
