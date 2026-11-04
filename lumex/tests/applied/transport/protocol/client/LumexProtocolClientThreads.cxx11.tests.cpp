// The generated client between threads: destroyed while another thread calls
// it through a layer, the handlers on several threads, identify and calls at
// once on a runner of several threads. For TSan; repeated in the gate. The
// synthetic mixer table.

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
