// The generated client between threads: destroyed while another thread calls
// it through a layer, the handlers on several threads, identify and calls at
// once on a runner of several threads, set_session against identify and
// against another set_session. For TSan; repeated in the gate. The synthetic
// mixer table.

#include <atomic>
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

/// The session the dialogue decodes with, read on its strand, after what was
/// posted to the strand before the call has run.
template <typename Bench>
index::session_context_t
session_of_the_engine (Bench &bench)
{
  std::promise<index::session_context_t> got;
  asio::post (bench.client->get_strand (), [&bench, &got] ()
                { got.set_value (bench.client->engine ().session ()); });
  return got.get_future ().get ();
}

/// Whether the two sessions say the same of everything the tests set.
bool
same_session (index::session_context_t const &left,
              index::session_context_t const &right)
{
  std::int64_t a = -1;
  std::int64_t b = -2;
  bool const has_a = left.find ("k", a);
  bool const has_b = right.find ("k", b);
  return left.model () == right.model ()
         && left.firmware () == right.firmware ()
         && left.layer () == right.layer () && has_a == has_b
         && (!has_a || a == b);
}
} // namespace

TEST (LumexProtocolClientThreads, Client_IdentifyAndCallsAtOnceOnTwoRunners)
{
  thread_bench_t<mixer_v1, mixer> bench;
  bench.device->set_reply<mixer_v1::INFO_t> (mixer_info ("M100", 3, 1, 1));
  bench.device->set_reply<mixer_v1::RATE_t> (std::uint8_t (6));
  std::thread second ([&bench] () { bench.context.run (); });
  std::atomic<int> left (60);
  std::atomic<int> wrong (0);
  std::promise<void> all;
  std::atomic<bool> told (false);
  auto const finished = [&] ()
    {
      if (left.fetch_sub (1) == 1 && !told.exchange (true))
        all.set_value ();
    };
  std::thread caller (
      [&] ()
        {
          for (int i = 0; i < 30; ++i)
            bench.client->RATE (std::uint8_t (1),
                                [&] (std::error_code error, std::uint8_t value)
                                  {
                                    if (error || value != 6)
                                      ++wrong;
                                    finished ();
                                  });
        });
  std::thread identifier (
      [&] ()
        {
          for (int i = 0; i < 30; ++i)
            bench.client->async_identify (
                [&] (std::error_code error, pr::identity_t identity)
                  {
                    if (error || identity.layer != "mixer_v1_t")
                      ++wrong;
                    finished ();
                  });
        });
  std::thread reader (
      [&] ()
        {
          for (int i = 0; i < 200; ++i)
            (void)bench.client->session ();
        });
  caller.join ();
  identifier.join ();
  reader.join ();
  all.get_future ().wait ();
  EXPECT_EQ (wrong.load (), 0);
  bench.stop ();
  second.join ();
}

TEST (LumexProtocolClientThreads, Client_ACallRacesTheDestruction)
{
  for (int round = 0; round < 20; ++round)
    {
      thread_bench_t<mixer_v1, mixer> bench;
      pr::client_layer<mixer, mixer> layer = bench.client->as<mixer> ();
      std::atomic<int> calls (0);
      std::atomic<bool> go (false);
      std::thread caller (
          [&] ()
            {
              while (!go.load ())
                std::this_thread::yield ();
              for (int i = 0; i < 20; ++i)
                layer.RATE (std::uint8_t (1),
                            [&calls] (std::error_code, std::uint8_t)
                              { ++calls; });
            });
      go.store (true);
      bench.client.reset ();
      caller.join ();
      for (int i = 0; i < 5000 && calls.load () != 20; ++i)
        std::this_thread::sleep_for (ms (1));
      EXPECT_EQ (calls.load (), 20);
    }
}

TEST (LumexProtocolClientThreads, Client_SubscribingRacesTheSelectionOfALayer)
{
  for (int round = 0; round < 10; ++round)
    {
      thread_bench_t<mixer_v2, mixer> bench;
      bench.device->set_reply<mixer_v2::INFO_t> (mixer_info ("M200", 15, 2));
      std::vector<pr::subscription_t> made;
      std::thread subscriber (
          [&] ()
            {
              for (int i = 0; i < 50; ++i)
                made.push_back (bench.client->on<mixer::LEVEL_t> (
                    [] (pr::reply_of<mixer::LEVEL_t> const &) {}));
            });
      EXPECT_TRUE (
          bench.client->identify (tr::deadline_t (20000)).has_value ());
      subscriber.join ();
      for (std::size_t i = 0; i < made.size (); ++i)
        EXPECT_NE (made[i].id, 0u);
      // Whenever it subscribed, the frame comes to it: the row of the layer.
      std::atomic<int> heard (0);
      bench.client->on<mixer::LEVEL_t> (
          [&heard] (pr::reply_of<mixer::LEVEL_t> const &) { ++heard; });
      EXPECT_TRUE (bench.device->start_stream<mixer_v2::LEVEL_t> (
          pr::reply_of<mixer::LEVEL_t> ()));
      bench.on_runner ([&bench] () { bench.clock.advance (ms (150)); });
      for (int i = 0; i < 5000 && heard.load () == 0; ++i)
        std::this_thread::sleep_for (ms (1));
      EXPECT_GE (heard.load (), 1);
      bench.device.reset ();
      bench.client.reset ();
    }
}

TEST (LumexProtocolClientThreads, Client_SetSessionRacesIdentify)
{
  for (int round = 0; round < 40; ++round)
    {
      thread_bench_t<mixer_v2, mixer> bench;
      bench.device->set_reply<mixer_v2::INFO_t> (mixer_info ("M200", 15, 2));
      bench.device->set_reply<mixer_v2::ONLY_V2_t> (std::uint16_t (500));
      std::atomic<bool> stop (false);
      std::thread setter (
          [&bench, &stop] ()
            {
              std::int64_t turn = 0;
              while (!stop.load ())
                {
                  index::session_context_t made;
                  made.set ("k", ++turn);
                  bench.client->set_session (made);
                }
            });
      std::this_thread::sleep_for (ms (1));
      bool const identified
          = bench.client->identify (tr::deadline_t (20000)).has_value ();
      EXPECT_TRUE (identified);
      std::this_thread::sleep_for (ms (2));
      stop.store (true);
      setter.join ();
      // However the calls interleaved, the layer identify found is the layer
      // of the session, the rows of it answer, and the dialogue decodes with
      // the session the client shows.
      index::session_context_t const shown = bench.client->session ();
      EXPECT_EQ (shown.layer (), "mixer_v2_t") << "round " << round;
      pr::call_result_t<mixer_v2::ONLY_V2_t> const reply
          = bench.client->as<mixer_v2> ().ONLY_V2 (std::uint8_t (1),
                                                   tr::deadline_t (20000));
      ASSERT_TRUE (reply.has_value ()) << "round " << round;
      EXPECT_EQ (*reply, 500);
      EXPECT_TRUE (same_session (shown, session_of_the_engine (bench)))
          << "round " << round;
    }
}

TEST (LumexProtocolClientThreads, Client_TwoSetSessionsLeaveTheEngineTheLast)
{
  // Two calls released together, once per turn: whichever published last, the
  // dialogue ends with that one, so it is the last of a turn that is read.
  thread_bench_t<mixer> bench;
  int const turns = 2000;
  std::atomic<int> released (0);
  std::atomic<int> finished (0);
  std::vector<std::thread> setters;
  for (std::int64_t who = 1; who <= 2; ++who)
    setters.push_back (std::thread (
        [&bench, &released, &finished, who, turns] ()
          {
            for (int turn = 1; turn <= turns; ++turn)
              {
                while (released.load () < turn)
                  std::this_thread::yield ();
                index::session_context_t made;
                made.set ("k", who * 100000 + turn);
                bench.client->set_session (made);
                finished.fetch_add (1);
              }
          }));
  int differing = 0;
  for (int turn = 1; turn <= turns; ++turn)
    {
      released.store (turn);
      while (finished.load () < 2 * turn)
        std::this_thread::yield ();
      if (!same_session (bench.client->session (),
                         session_of_the_engine (bench)))
        ++differing;
    }
  for (std::size_t i = 0; i < setters.size (); ++i)
    setters[i].join ();
  EXPECT_EQ (differing, 0);
}
