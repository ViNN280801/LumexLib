// The values a layer gives the dialogue (how many exchanges are in flight, the
// default timeout, the default retries, the late reply window) at a change of
// layer: a value the application set stays, one it did not set follows the
// layer, there and back. Each value is measured from outside: the requests
// that go out at once, the time and the attempts of an exchange nobody
// answers, the wait of the request after a late one.

#include <cstddef>
#include <cstdint>
#include <memory>
#include <system_error>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/tests/applied/transport/protocol/client/LumexClientTestSupport.hpp"

namespace
{
using namespace lumex_client_test;

using plain = tables::plain_t;
using quick = tables::quick_t;
using slow = tables::slow_t;
using windowed = tables::windowed_t;
using patient = tables::patient_t;

/// An engine without a client and without the features of a table: its values
/// are those of its options alone.
class bare_bench_t
{
public:
  explicit bare_bench_t (pr::dialogue_options_t options)
  {
    tr::testing::mock_options_t mock_options;
    mock_options.timers = clock.source ();
    tr::testing::mock_stream mock = tr::testing::make_mock_stream (
        context.get_executor (), mock_options);
    mock.stream.async_open ([] (std::error_code) {});
    context.poll ();
    context.restart ();
    control.reset (new tr::testing::mock_stream_control (mock.control));
    options.timers = clock.source ();
    options.features = nullptr;
    engine_.reset (new pr::dialogue_engine (
        pr::frame_link (tr::framed::framed_stream<fr::slip::slip_codec> (
            std::move (mock.stream), fr::slip::slip_codec ())),
        *index::protocol_index<plain> (), options));
    settle ();
  }

  ~bare_bench_t ()
  {
    engine_.reset ();
    settle ();
  }

  bare_bench_t (bare_bench_t const &) = delete;
  bare_bench_t &operator= (bare_bench_t const &) = delete;

  pr::dialogue_engine &
  engine ()
  {
    return *engine_;
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

  asio::io_context context;
  tr::testing::virtual_clock clock;
  std::unique_ptr<tr::testing::mock_stream_control> control;

private:
  std::unique_ptr<pr::dialogue_engine> engine_;
};

/// A bench of a client; what the measures need of any bench. `Device` is the
/// layer the simulated device speaks, `Client` the one the client is typed by.
template <typename Device, typename Client = Device> class layer_bench_t
{
public:
  explicit layer_bench_t (pr::client_options_t options
                          = pr::client_options_for<Client> ())
      : bench_ (std::move (options))
  {
    bench_.device->set_behavior (tr::testing::behavior::silent ());
  }

  /// identify () against a device that answers `reply`; silent again after.
  template <typename Row>
  void
  identify_as (pr::reply_of<Row> const &reply)
  {
    bench_.device->template set_reply<Row> (reply);
    bench_.device->set_behavior (tr::testing::behavior::perfect ());
    bool done = false;
    std::error_code ended;
    bench_.client->async_identify (
        [&done, &ended] (std::error_code error, pr::identity_t)
          {
            ended = error;
            done = true;
          });
    EXPECT_TRUE (bench_.run_until ([&done] () { return done; }));
    EXPECT_FALSE (ended) << ended.message ();
    bench_.device->set_behavior (tr::testing::behavior::silent ());
  }

  pr::dialogue_engine &
  engine ()
  {
    return bench_.client->engine ();
  }

  void
  settle ()
  {
    bench_.settle ();
  }

  void
  advance (ms by)
  {
    bench_.advance (by);
  }

private:
  client_bench_t<Device, Client> bench_;
};

/// The request of `row` (no request fields) for the layer in use.
pr::exchange_t
exchange_now (pr::dialogue_engine &engine, char const *row_name, bool by_order)
{
  pr::exchange_t out;
  EXPECT_TRUE ((
      layout::encode_message<plain, plain::ASK_t> (out.message).has_value ()));
  out.row = engine.index ().find_name (row_name);
  EXPECT_NE (out.row, nullptr);
  out.row_policy = true;
  if (by_order)
    out.how = dialogue::correlation::by_order;
  return out;
}

/// How many requests go out at once when many wait.
template <typename Bench>
std::size_t
measure_in_flight (Bench &bench)
{
  pr::dialogue_engine &engine = bench.engine ();
  std::uint64_t const before = engine.statistics ().requests;
  auto const out = std::make_shared<outcome_t<pr::reply_frame_t>> ();
  for (int i = 0; i < 8; ++i)
    {
      record_t<pr::reply_frame_t> done = { out };
      engine.async_exchange (exchange_now (engine, "ASK", false), done);
    }
  bench.settle ();
  std::size_t const sent
      = static_cast<std::size_t> (engine.statistics ().requests - before);
  engine.cancel ();
  bench.settle ();
  return sent;
}

struct attempts_t
{
  unsigned timeout_ms;
  unsigned attempts;
};

/// The timeout of one attempt and the attempts of an exchange nobody answers.
template <typename Bench>
attempts_t
measure_attempts (Bench &bench)
{
  pr::dialogue_engine &engine = bench.engine ();
  std::uint64_t const before = engine.statistics ().requests;
  auto const out = std::make_shared<outcome_t<pr::reply_frame_t>> ();
  record_t<pr::reply_frame_t> done = { out };
  engine.async_exchange (exchange_now (engine, "ASK", false), done);
  bench.settle ();
  unsigned elapsed = 0;
  while (out->calls == 0 && elapsed < 5000)
    {
      bench.advance (ms (10));
      elapsed += 10;
    }
  EXPECT_EQ (out->calls, 1);
  attempts_t made;
  made.attempts
      = static_cast<unsigned> (engine.statistics ().requests - before);
  made.timeout_ms = made.attempts != 0 ? elapsed / made.attempts : 0;
  return made;
}

/// The wait of a request that follows a late reply of a by_order row.
template <typename Bench>
unsigned
measure_window (Bench &bench)
{
  pr::dialogue_engine &engine = bench.engine ();
  auto const first = std::make_shared<outcome_t<pr::reply_frame_t>> ();
  record_t<pr::reply_frame_t> first_done = { first };
  engine.async_exchange (exchange_now (engine, "ORDERED", true), first_done);
  bench.settle ();
  unsigned elapsed = 0;
  while (first->calls == 0 && elapsed < 5000)
    {
      bench.advance (ms (10));
      elapsed += 10;
    }
  EXPECT_EQ (first->calls, 1);
  std::uint64_t const sent = engine.statistics ().requests;
  auto const second = std::make_shared<outcome_t<pr::reply_frame_t>> ();
  record_t<pr::reply_frame_t> second_done = { second };
  engine.async_exchange (exchange_now (engine, "ORDERED", true), second_done);
  bench.settle ();
  unsigned waited = 0;
  while (engine.statistics ().requests == sent && waited < 2000)
    {
      bench.advance (ms (10));
      waited += 10;
    }
  engine.cancel ();
  bench.settle ();
  // The cancellation opens a window too: let it pass.
  bench.advance (ms (1000));
  return waited;
}

template <typename To, typename Bench>
void
change_to (Bench &bench)
{
  bench.engine ().select_layer (*index::protocol_index<To> (),
                                dialogue::features_of<To> ());
  bench.settle ();
}

/// The three values of quick and slow layers at the moment.
template <typename Bench>
void
expect_values (Bench &bench, std::size_t in_flight, unsigned timeout_ms,
               unsigned retries)
{
  EXPECT_EQ (measure_in_flight (bench), in_flight);
  attempts_t const made = measure_attempts (bench);
  EXPECT_EQ (made.timeout_ms, timeout_ms);
  EXPECT_EQ (made.attempts, retries + 1);
}
} // namespace

TEST (LumexDialogueLayerValues,
      GivenNothingSet_ThenTheValuesFollowThereAndBack)
{
  layer_bench_t<quick> bench;
  {
    SCOPED_TRACE ("quick");
    expect_values (bench, 2, 100, 1);
  }
  change_to<slow> (bench);
  {
    SCOPED_TRACE ("slow");
    expect_values (bench, 4, 300, 2);
  }
  change_to<quick> (bench);
  {
    SCOPED_TRACE ("quick again");
    expect_values (bench, 2, 100, 1);
  }
}

TEST (LumexDialogueLayerValues,
      GivenMaxInFlightSet_ThenItStaysAndTheRestFollow)
{
  pr::client_options_t options = pr::client_options_for<quick> ();
  options.dialogue.max_in_flight = 3;
  layer_bench_t<quick> bench (options);
  {
    SCOPED_TRACE ("quick");
    expect_values (bench, 3, 100, 1);
  }
  change_to<slow> (bench);
  {
    SCOPED_TRACE ("slow");
    expect_values (bench, 3, 300, 2);
  }
  change_to<quick> (bench);
  {
    SCOPED_TRACE ("quick again");
    expect_values (bench, 3, 100, 1);
  }
}

TEST (LumexDialogueLayerValues, GivenTheTimeoutSet_ThenItStaysAndTheRestFollow)
{
  pr::client_options_t options = pr::client_options_for<quick> ();
  options.dialogue.defaults.timeout_ms = 250;
  layer_bench_t<quick> bench (options);
  {
    SCOPED_TRACE ("quick");
    expect_values (bench, 2, 250, 1);
  }
  change_to<slow> (bench);
  {
    SCOPED_TRACE ("slow");
    expect_values (bench, 4, 250, 2);
  }
  change_to<quick> (bench);
  {
    SCOPED_TRACE ("quick again");
    expect_values (bench, 2, 250, 1);
  }
}

TEST (LumexDialogueLayerValues,
      GivenTheRetriesSet_ThenTheyStayAndTheRestFollow)
{
  pr::client_options_t options = pr::client_options_for<quick> ();
  options.dialogue.defaults.retries = 4;
  layer_bench_t<quick> bench (options);
  {
    SCOPED_TRACE ("quick");
    expect_values (bench, 2, 100, 4);
  }
  change_to<slow> (bench);
  {
    SCOPED_TRACE ("slow");
    expect_values (bench, 4, 300, 4);
  }
  change_to<quick> (bench);
  {
    SCOPED_TRACE ("quick again");
    expect_values (bench, 2, 100, 4);
  }
}

TEST (LumexDialogueLayerValues, GivenNoWindowSet_ThenTheWindowFollows)
{
  layer_bench_t<windowed> bench;
  EXPECT_EQ (measure_window (bench), 100u);
  change_to<patient> (bench);
  EXPECT_EQ (measure_window (bench), 300u);
  change_to<windowed> (bench);
  EXPECT_EQ (measure_window (bench), 100u);
}

TEST (LumexDialogueLayerValues, GivenTheWindowSet_ThenItStays)
{
  pr::client_options_t options = pr::client_options_for<windowed> ();
  options.dialogue.late_reply_window_ms = 70;
  layer_bench_t<windowed> bench (options);
  EXPECT_EQ (measure_window (bench), 70u);
  change_to<patient> (bench);
  EXPECT_EQ (measure_window (bench), 70u);
  change_to<windowed> (bench);
  EXPECT_EQ (measure_window (bench), 70u);
}

TEST (LumexDialogueLayerValues,
      GivenEverythingSetBesideTheWindow_ThenTheWindowFollows)
{
  pr::client_options_t options = pr::client_options_for<windowed> ();
  options.dialogue.max_in_flight = 1;
  options.dialogue.defaults.timeout_ms = 400;
  options.dialogue.defaults.retries = 3;
  layer_bench_t<windowed> bench (options);
  change_to<patient> (bench);
  EXPECT_EQ (measure_window (bench), 300u);
}

TEST (LumexDialogueLayerValues,
      GivenPlainClientOptions_ThenTheyStayOverTheLayers)
{
  // Nothing of the table is in them: one in flight, 1000 ms, no retry. Each
  // differs from what the layer in use gives, so each counts as set.
  layer_bench_t<quick> bench ((pr::client_options_t ()));
  {
    SCOPED_TRACE ("quick");
    expect_values (bench, 1, 1000, 0);
  }
  change_to<slow> (bench);
  {
    SCOPED_TRACE ("slow");
    expect_values (bench, 1, 1000, 0);
  }
  change_to<quick> (bench);
  {
    SCOPED_TRACE ("quick again");
    expect_values (bench, 1, 1000, 0);
  }
}

TEST (LumexDialogueLayerValues, GivenPlainClientOptions_ThenTheWindowStays)
{
  layer_bench_t<windowed> bench ((pr::client_options_t ()));
  EXPECT_EQ (measure_window (bench), 0u);
  change_to<patient> (bench);
  EXPECT_EQ (measure_window (bench), 0u);
  change_to<windowed> (bench);
  EXPECT_EQ (measure_window (bench), 0u);
}

TEST (LumexDialogueLayerValues,
      GivenAValueEqualToTheTable_ThenItCountsAsNotSet)
{
  // 4 in flight is what slow gives: it is no setting of the application once
  // slow is the layer in use, so quick takes its own 2 back. On quick, 4
  // differs from the table, so slow finds it set and keeps it.
  pr::client_options_t options = pr::client_options_for<quick> ();
  options.dialogue.max_in_flight = 4;
  layer_bench_t<quick> bench (options);
  EXPECT_EQ (measure_in_flight (bench), 4u);
  change_to<slow> (bench);
  EXPECT_EQ (measure_in_flight (bench), 4u);
  change_to<quick> (bench);
  EXPECT_EQ (measure_in_flight (bench), 2u);
}

TEST (LumexDialogueLayerValues,
      GivenALayerThatGivesNoTimeout_ThenTheLibrarysValueIsBack)
{
  // plain says nothing of the timeout: its value is the library's (1000 ms),
  // as that of a new client over plain, and not the 300 ms the dialogue was
  // made with on slow.
  layer_bench_t<slow> bench;
  {
    SCOPED_TRACE ("slow");
    expect_values (bench, 4, 300, 2);
  }
  change_to<plain> (bench);
  {
    SCOPED_TRACE ("plain");
    expect_values (bench, 1, 1000, 0);
  }
  change_to<slow> (bench);
  {
    SCOPED_TRACE ("slow again");
    expect_values (bench, 4, 300, 2);
  }
}

TEST (LumexDialogueLayerValues,
      GivenAnEngineWithoutFeatures_ThenTheLayerGivesTheValues)
{
  // No table was in use: the options are what the dialogue has, nothing the
  // application set against a table.
  bare_bench_t bench ((pr::dialogue_options_t ()));
  EXPECT_EQ (measure_in_flight (bench), 1u);
  change_to<slow> (bench);
  {
    SCOPED_TRACE ("slow");
    expect_values (bench, 4, 300, 2);
  }
  change_to<quick> (bench);
  {
    SCOPED_TRACE ("quick");
    expect_values (bench, 2, 100, 1);
  }
}

TEST (LumexDialogueLayerValues,
      GivenATimeoutSetOnALayerWithNone_ThenItStaysOverTheLayers)
{
  // plain gives no timeout, so the library's 1000 ms is what it gives and
  // 250 ms differs from it: set by the application, it stays, also on plain
  // again.
  pr::client_options_t options = pr::client_options_for<plain> ();
  options.dialogue.defaults.timeout_ms = 250;
  layer_bench_t<plain> bench (options);
  {
    SCOPED_TRACE ("plain");
    expect_values (bench, 1, 250, 0);
  }
  change_to<quick> (bench);
  {
    SCOPED_TRACE ("quick");
    expect_values (bench, 2, 250, 1);
  }
  change_to<plain> (bench);
  {
    SCOPED_TRACE ("plain again");
    expect_values (bench, 1, 250, 0);
  }
}

TEST (LumexDialogueLayerValues,
      GivenATimeoutSetOnALayerWithNone_ThenItStaysOverIdentify)
{
  using base = tables::timed_base_t;
  using fast = tables::timed_fast_t;
  pr::reply_of<fast::WHO_t> fast_one;
  field::get<0> (fast_one) = 1;
  // Found like a device: the base gives no timeout, so 1000 ms is what it
  // gives and the 2000 ms set differs from it; the layer found gives 100 ms.
  {
    pr::client_options_t options = pr::client_options_for<base> ();
    options.dialogue.defaults.timeout_ms = 2000;
    layer_bench_t<fast, base> bench (options);
    bench.identify_as<fast::WHO_t> (fast_one);
    attempts_t const made = measure_attempts (bench);
    EXPECT_EQ (made.timeout_ms, 2000u);
    EXPECT_EQ (made.attempts, 2u);
  }
  // Nothing set: the timeout follows to the layer found.
  {
    layer_bench_t<fast, base> bench;
    bench.identify_as<fast::WHO_t> (fast_one);
    attempts_t const made = measure_attempts (bench);
    EXPECT_EQ (made.timeout_ms, 100u);
    EXPECT_EQ (made.attempts, 2u);
  }
}

TEST (LumexDialogueLayerValues,
      GivenAnEngineWithoutFeaturesAndAWindow_ThenTheLayerGivesIt)
{
  pr::dialogue_options_t options;
  options.late_reply_window_ms = 70;
  bare_bench_t bench (options);
  change_to<patient> (bench);
  EXPECT_EQ (measure_window (bench), 300u);
  change_to<windowed> (bench);
  EXPECT_EQ (measure_window (bench), 100u);
}

TEST (LumexDialogueLayerValues, GivenFeaturesOfNoneInFlight_ThenOneIsInFlight)
{
  // A table made by hand may say 0: it is taken as 1.
  static pr::protocol_features_t const none = [] ()
    {
      pr::protocol_features_t made;
      made.max_in_flight = 0;
      return made;
    }();
  bare_bench_t bench ((pr::dialogue_options_t ()));
  bench.engine ().select_layer (*index::protocol_index<quick> (), &none);
  bench.settle ();
  EXPECT_EQ (measure_in_flight (bench), 1u);
  change_to<slow> (bench);
  EXPECT_EQ (measure_in_flight (bench), 4u);
}

TEST (LumexDialogueLayerValues,
      GivenAQueuedExchange_ThenItTakesThePolicyOfTheLayerItStartsIn)
{
  // quick lets two go out (100 ms, one retry); the third waits and goes out
  // on slow (300 ms, two retries) once slow is the layer: three attempts of
  // 300 ms. The two that started keep what they have: two of 100 ms.
  layer_bench_t<quick> bench;
  pr::dialogue_engine &engine = bench.engine ();
  std::vector<std::shared_ptr<outcome_t<pr::reply_frame_t>>> outs;
  for (int i = 0; i < 3; ++i)
    {
      outs.push_back (std::make_shared<outcome_t<pr::reply_frame_t>> ());
      record_t<pr::reply_frame_t> done = { outs.back () };
      engine.async_exchange (exchange_now (engine, "ASK", false), done);
    }
  bench.settle ();
  EXPECT_EQ (engine.statistics ().requests, 2u);
  change_to<slow> (bench);
  EXPECT_EQ (engine.statistics ().requests, 3u);
  unsigned ended[3] = { 0, 0, 0 };
  unsigned elapsed = 0;
  while ((outs[0]->calls == 0 || outs[1]->calls == 0 || outs[2]->calls == 0)
         && elapsed < 5000)
    {
      bench.advance (ms (10));
      elapsed += 10;
      for (std::size_t i = 0; i < 3; ++i)
        if (outs[i]->calls != 0 && ended[i] == 0)
          ended[i] = elapsed;
    }
  EXPECT_EQ (ended[0], 200u);
  EXPECT_EQ (ended[1], 200u);
  EXPECT_EQ (ended[2], 900u);
  EXPECT_EQ (engine.statistics ().requests, 7u);
}
