// The synchronous forms of the generated client. A sync call blocks its
// thread, so the runner is on another thread and the deadlines are real and
// short. What the virtual clock can prove here: the row's timeout and retries
// run on the timer source (Client_SyncRowTimeoutRunsOnTheTimerSource moves
// them from the test while a call blocks). What it cannot: the deadline of a
// call is wall-clock time by design, so those tests bound real durations,
// generously in one direction only. The synthetic mixer table.

#include <atomic>
#include <chrono>
#include <cstdint>
#include <future>
#include <memory>
#include <system_error>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/tests/applied/transport/protocol/client/LumexClientTestSupport.hpp"

namespace
{
using namespace lumex_client_test;
using bench_t = thread_bench_t<mixer>;
using clock_type = std::chrono::steady_clock;
} // namespace

TEST (LumexProtocolClientSync, Client_SyncCallReturnsReply)
{
  bench_t bench;
  bench.device->set_reply<mixer::RATE_t> (std::uint8_t (12));
  pr::call_result_t<mixer::RATE_t> const reply
      = bench.client->RATE (std::uint8_t (1));
  ASSERT_TRUE (reply.has_value ()) << reply.error ().message ();
  EXPECT_EQ (*reply, 12);
  // A row of two fields, a row of none.
  bench.device->set_reply<mixer::PAIR_t> (std::uint16_t (700));
  EXPECT_EQ (*bench.client->PAIR (std::uint8_t (1), std::uint16_t (2)), 700);
  bench.device->set_reply<mixer::PING_t> (std::uint8_t (1));
  EXPECT_EQ (*bench.client->PING (), 1);
}

TEST (LumexProtocolClientSync, Client_SyncArgumentNotFittingSendsNothing)
{
  bench_t bench;
  pr::call_result_t<mixer::RATE_t> const reply = bench.client->RATE (300);
  ASSERT_FALSE (reply.has_value ());
  EXPECT_EQ (reply.error (), protocol_errc::invalid_value);
  EXPECT_EQ (bench.client->statistics ().requests, 0u);
  EXPECT_EQ (bench.device->statistics ().requests, 0u);
}

TEST (LumexProtocolClientSync, Client_SyncDangerous)
{
  bench_t bench;
  bench.device->set_reply<mixer::ERASE_t> (std::uint8_t (8));
  EXPECT_EQ (*bench.client->ERASE (pr::confirm, std::uint8_t (1)), 8);
  EXPECT_EQ (*bench.client->ERASE (pr::confirm, std::uint8_t (1),
                                   tr::deadline_t (5000)),
             8);
}

TEST (LumexProtocolClientSync, Client_SyncRowTimeoutEndsTheCall)
{
  bench_t bench;
  bench.device->set_behavior (tr::testing::behavior::silent ());
  clock_type::time_point const started = clock_type::now ();
  pr::call_result_t<mixer::RATE_t> const reply
      = bench.client->RATE (std::uint8_t (1));
  ASSERT_FALSE (reply.has_value ());
  EXPECT_EQ (reply.error (), protocol_errc::timeout);
  EXPECT_GE (clock_type::now () - started, ms (190));
}

TEST (LumexProtocolClientSync, Client_SyncRowTimeoutRunsOnTheTimerSource)
{
  bench_t bench (true);
  bench.device->set_behavior (tr::testing::behavior::silent ());
  std::future<pr::call_result_t<mixer::RATE_t>> call
      = std::async (std::launch::async, [&bench] ()
                      { return bench.client->RATE (std::uint8_t (1)); });
  // No real time ends it, the clock of the test does.
  EXPECT_EQ (call.wait_for (ms (300)), std::future_status::timeout);
  // Moved on the runner thread: the clock may be moved from any thread, but a
  // wait it hands over from here takes the reference count of an Asio executor
  // across threads, and TSan does not see the fence of that count.
  while (call.wait_for (ms (5)) != std::future_status::ready)
    asio::post (bench.context, [&bench] () { bench.clock.advance (ms (50)); });
  pr::call_result_t<mixer::RATE_t> const reply = call.get ();
  ASSERT_FALSE (reply.has_value ());
  EXPECT_EQ (reply.error (), protocol_errc::timeout);
}

TEST (LumexProtocolClientSync, Client_SyncWithDeadline)
{
  bench_t bench;
  bench.device->set_behavior (tr::testing::behavior::silent ());
  clock_type::time_point const started = clock_type::now ();
  // The row has 900 ms in three attempts, the deadline ends it first.
  pr::call_result_t<mixer::PING_t> const reply
      = bench.client->PING (tr::deadline_t (50));
  clock_type::duration const took = clock_type::now () - started;
  ASSERT_FALSE (reply.has_value ());
  EXPECT_EQ (reply.error (), protocol_errc::timeout);
  EXPECT_GE (took, ms (49));
  EXPECT_LT (took, ms (880));
  // The exchange was cancelled: the next call does not wait behind it.
  bench.device->set_behavior (tr::testing::behavior::perfect ());
  bench.device->set_reply<mixer::RATE_t> (std::uint8_t (2));
  EXPECT_EQ (*bench.client->RATE (std::uint8_t (1), tr::deadline_t (5000)), 2);
  EXPECT_EQ (bench.client->statistics ().retries, 0u);
}

TEST (LumexProtocolClientSync, Client_SyncDeadlineOfAnyDuration)
{
  bench_t bench;
  bench.device->set_reply<mixer::RATE_t> (std::uint8_t (2));
  EXPECT_EQ (*bench.client->RATE (std::uint8_t (1), std::chrono::seconds (5)),
             2);
}

TEST (LumexProtocolClientSync, Client_SyncDeadlineOfZeroEndsAtOnce)
{
  bench_t bench;
  bench.device->set_behavior (tr::testing::behavior::silent ());
  pr::call_result_t<mixer::RATE_t> const reply
      = bench.client->RATE (std::uint8_t (1), tr::deadline_t (0));
  ASSERT_FALSE (reply.has_value ());
  EXPECT_EQ (reply.error (), protocol_errc::timeout);
}

TEST (LumexProtocolClientSync,
      Client_SyncDeadlineEndsAtItsTimeNotAtTheNextPoll)
{
  // The wait looks at a stopped context every 50 ms; a deadline inside that
  // step ends the wait when it falls due. The time is real, so the best of a
  // few calls is taken: only a wait that never ends before 50 ms fails.
  bench_t bench;
  bench.device->set_behavior (tr::testing::behavior::silent ());
  clock_type::duration best = std::chrono::seconds (1);
  for (int i = 0; i < 5; ++i)
    {
      clock_type::time_point const started = clock_type::now ();
      pr::call_result_t<mixer::RATE_t> const reply
          = bench.client->RATE (std::uint8_t (1), tr::deadline_t (3));
      clock_type::duration const took = clock_type::now () - started;
      ASSERT_FALSE (reply.has_value ());
      EXPECT_EQ (reply.error (), protocol_errc::timeout);
      if (took < best)
        best = took;
    }
  EXPECT_LT (best, ms (40));
}

TEST (LumexProtocolClientSync, Client_SyncDeadlineOfTheLargestDuration)
{
  // The deadline is compared with the time spent in its own unit: converted
  // to the unit of the clock it overflows, and the call ended at once.
  bench_t bench;
  bench.device->set_reply<mixer::RATE_t> (std::uint8_t (2));
  pr::call_result_t<mixer::RATE_t> const reply
      = bench.client->RATE (std::uint8_t (1), (tr::deadline_t::max)());
  ASSERT_TRUE (reply.has_value ()) << reply.error ().message ();
  EXPECT_EQ (*reply, 2);
  // Past the 292 years a nanosecond count holds.
  pr::call_result_t<mixer::RATE_t> const long_one = bench.client->RATE (
      std::uint8_t (1), tr::deadline_t (1000LL * 3600 * 24 * 365 * 300));
  ASSERT_TRUE (long_one.has_value ()) << long_one.error ().message ();
  EXPECT_EQ (*long_one, 2);
}

TEST (LumexProtocolClientSync,
      Client_SyncDeadlineOfTheLargestDurationOnASilentDevice)
{
  // No deadline of that size ends the call: the timeout of the row does.
  bench_t bench;
  bench.device->set_behavior (tr::testing::behavior::silent ());
  clock_type::time_point const started = clock_type::now ();
  pr::call_result_t<mixer::RATE_t> const reply
      = bench.client->RATE (std::uint8_t (1), (tr::deadline_t::max)());
  ASSERT_FALSE (reply.has_value ());
  EXPECT_EQ (reply.error (), protocol_errc::timeout);
  EXPECT_GE (clock_type::now () - started, ms (190));
}

TEST (LumexProtocolClientSync, Client_SyncIdentifyWithTheLargestDeadline)
{
  thread_bench_t<mixer_v2, mixer> bench;
  bench.device->set_reply<mixer_v2::INFO_t> (mixer_info ("M200", 15, 2));
  pr::identity_result_t const identity
      = bench.client->identify ((tr::deadline_t::max)());
  ASSERT_TRUE (identity.has_value ()) << identity.error ().message ();
  EXPECT_EQ (identity->layer, "mixer_v2_t");
}

TEST (LumexProtocolClientSync,
      Client_SyncIdentifyWithTheLargestDeadlineOnASilentDevice)
{
  thread_bench_t<mixer_v2, mixer> bench;
  bench.device->set_behavior (tr::testing::behavior::silent ());
  clock_type::time_point const started = clock_type::now ();
  pr::identity_result_t const identity
      = bench.client->identify ((tr::deadline_t::max)());
  ASSERT_FALSE (identity.has_value ());
  EXPECT_EQ (identity.error (), protocol_errc::timeout);
  EXPECT_GE (clock_type::now () - started, ms (190));
}

TEST (LumexProtocolClientSync, Client_WouldDeadlockOnRunnerThread)
{
  bench_t bench;
  bench.device->set_reply<mixer::RATE_t> (std::uint8_t (1));
  pr::call_result_t<mixer::RATE_t> on_context = bench.client->RATE (0);
  bench.on_runner ([&] ()
                     { on_context = bench.client->RATE (std::uint8_t (1)); });
  ASSERT_FALSE (on_context.has_value ());
  EXPECT_EQ (on_context.error (), transport_errc::would_deadlock);
  EXPECT_EQ (bench.device->statistics ().requests, 1u);
}

TEST (LumexProtocolClientSync, Client_WouldDeadlockOnTheStrandAndWithADeadline)
{
  bench_t bench;
  std::promise<pr::call_result_t<mixer::RATE_t>> got;
  asio::post (bench.client->get_strand (),
              [&] ()
                {
                  got.set_value (bench.client->RATE (std::uint8_t (1),
                                                     tr::deadline_t (60000)));
                });
  pr::call_result_t<mixer::RATE_t> const reply = got.get_future ().get ();
  ASSERT_FALSE (reply.has_value ());
  EXPECT_EQ (reply.error (), transport_errc::would_deadlock);
  EXPECT_EQ (bench.device->statistics ().requests, 0u);
}

TEST (LumexProtocolClientSync, Client_WouldDeadlockInsideAHandlerAndOfIdentify)
{
  thread_bench_t<mixer_v1, mixer> bench;
  std::promise<pr::identity_result_t> got;
  bench.client->RATE (std::uint8_t (1), [&] (std::error_code, std::uint8_t)
                        { got.set_value (bench.client->identify ()); });
  pr::identity_result_t const identity = got.get_future ().get ();
  ASSERT_FALSE (identity.has_value ());
  EXPECT_EQ (identity.error (), transport_errc::would_deadlock);
}

TEST (LumexProtocolClientSync, Client_ASyncCallFromAnotherContextsThreadWorks)
{
  bench_t bench;
  bench.device->set_reply<mixer::RATE_t> (std::uint8_t (4));
  asio::io_context other;
  pr::call_result_t<mixer::RATE_t> reply = bench.client->RATE (0);
  asio::post (other,
              [&] () { reply = bench.client->RATE (std::uint8_t (1)); });
  other.run ();
  ASSERT_TRUE (reply.has_value ()) << reply.error ().message ();
  EXPECT_EQ (*reply, 4);
}

TEST (LumexProtocolClientSync, Client_SyncOnAStoppedContextEnds)
{
  bench_t bench;
  bench.device->set_behavior (tr::testing::behavior::silent ());
  bench.stop ();
  pr::call_result_t<mixer::RATE_t> const reply
      = bench.client->RATE (std::uint8_t (1));
  ASSERT_FALSE (reply.has_value ());
  EXPECT_EQ (reply.error (), transport_errc::cancelled);
  // The deadline form too.
  pr::call_result_t<mixer::RATE_t> const timed
      = bench.client->RATE (std::uint8_t (1), tr::deadline_t (60000));
  ASSERT_FALSE (timed.has_value ());
  EXPECT_EQ (timed.error (), transport_errc::cancelled);
}

TEST (LumexProtocolClientSync, Client_SyncEndsWhenAnotherThreadCloses)
{
  bench_t bench;
  bench.device->set_behavior (tr::testing::behavior::silent ());
  std::future<pr::call_result_t<mixer::RATE_t>> call = std::async (
      std::launch::async,
      [&bench] ()
        {
          return bench.client->RATE (std::uint8_t (1), tr::deadline_t (60000));
        });
  // Whether the close comes before or after the request, the call ends.
  std::this_thread::sleep_for (ms (10));
  bench.client->close ();
  pr::call_result_t<mixer::RATE_t> const reply = call.get ();
  ASSERT_FALSE (reply.has_value ());
  EXPECT_TRUE (reply.error () == transport_errc::cancelled
               || reply.error () == transport_errc::closed)
      << reply.error ().message ();
}

TEST (LumexProtocolClientSync, Client_SyncIdentify)
{
  thread_bench_t<mixer_v2, mixer> bench;
  bench.device->set_reply<mixer_v2::INFO_t> (mixer_info ("M200", 15, 2));
  pr::identity_result_t const identity = bench.client->identify ();
  ASSERT_TRUE (identity.has_value ()) << identity.error ().message ();
  EXPECT_EQ (identity->layer, "mixer_v2_t");
  EXPECT_EQ (bench.client->session ().layer (), "mixer_v2_t");
  // A call started after it sees the layer, with no wait for the strand.
  bench.device->set_reply<mixer_v2::ONLY_V2_t> (std::uint16_t (3));
  pr::call_result_t<mixer_v2::ONLY_V2_t> const reply
      = bench.client->as<mixer_v2> ().ONLY_V2 (std::uint8_t (1));
  ASSERT_TRUE (reply.has_value ()) << reply.error ().message ();
  EXPECT_EQ (*reply, 3);
  pr::identity_result_t const timed
      = bench.client->identify (tr::deadline_t (5000));
  EXPECT_TRUE (timed.has_value ());
}

TEST (LumexProtocolClientSync, Client_SyncIdentifyWithADeadlineOnASilentDevice)
{
  thread_bench_t<mixer_v2, mixer> bench;
  bench.device->set_behavior (tr::testing::behavior::silent ());
  pr::identity_result_t const identity
      = bench.client->identify (tr::deadline_t (20));
  ASSERT_FALSE (identity.has_value ());
  EXPECT_EQ (identity.error (), protocol_errc::timeout);
  EXPECT_TRUE (bench.client->session ().layer ().empty ());
}

TEST (LumexProtocolClientSync, Client_SyncFromManyThreads)
{
  thread_bench_t<mixer_v1, mixer> bench;
  bench.device->set_reply<mixer_v1::INFO_t> (mixer_info ("M100", 3, 1));
  bench.device->set_reply<mixer_v1::RATE_t> (std::uint8_t (6));
  bench.device->set_reply<mixer_v1::ONLY_V1_t> (std::uint8_t (7));
  std::atomic<int> wrong (0);
  std::vector<std::thread> threads;
  for (int t = 0; t < 4; ++t)
    threads.emplace_back (
        [&bench, &wrong, t] ()
          {
            for (int i = 0; i < 25; ++i)
              {
                if (t == 0 && i % 5 == 0)
                  {
                    if (!bench.client->identify (tr::deadline_t (20000)))
                      ++wrong;
                    continue;
                  }
                pr::call_result_t<mixer::RATE_t> const reply
                    = bench.client->RATE (std::uint8_t (1),
                                          tr::deadline_t (20000));
                if (!reply || *reply != 6)
                  ++wrong;
                (void)bench.client->session ();
              }
          });
  for (std::size_t i = 0; i < threads.size (); ++i)
    threads[i].join ();
  EXPECT_EQ (wrong.load (), 0);
}
