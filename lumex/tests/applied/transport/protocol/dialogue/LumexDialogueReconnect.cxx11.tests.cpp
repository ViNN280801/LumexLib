// The dialogue engine over a link that connects again, without the client: the
// steps of a reconnect, what passes before the check, the end of a check that
// is called twice or throws, and a link of one's own.

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/tests/applied/transport/protocol/dialogue/LumexDialogueTestSupport.hpp"

namespace
{
using namespace lumex_dialogue_test;
namespace ss = tr::session;

struct reconnect_bench_t
{
  explicit reconnect_bench_t (pr::reconnect_steps_t steps
                              = pr::reconnect_steps_t ())
  {
    tr::testing::mock_options_t mock_options;
    mock_options.timers = clock.source ();
    mock.reset (new tr::testing::mock_stream (tr::testing::make_mock_stream (
        context.get_executor (), mock_options)));
    std::shared_ptr<tr::stream::stream_impl> const impl = mock->stream.impl ();
    ss::session_options_t options;
    options.policy.initial_delay = ms (100);
    options.policy.jitter = 0;
    options.timers = clock.source ();
    session.reset (new ss::stream_session (
        context.get_executor (), [impl] (tr::executor_type const &)
          { return tr::stream::byte_stream (impl); }, options));
    session->start ();
    settle ();
    pr::dialogue_options_t dialogue_options;
    dialogue_options.timers = clock.source ();
    dialogue_options.reconnect = std::move (steps);
    tr::stream::byte_stream view = session->as_byte_stream ();
    view_impl = view.impl ();
    pr::frame_link link (tr::framed::framed_stream<fr::slip::slip_codec> (
        std::move (view), fr::slip::slip_codec ()));
    EXPECT_TRUE (link.reconnects ());
    engine.reset (new pr::dialogue_engine (std::move (link), pump_index (),
                                           dialogue_options));
    settle ();
    advance (ms (200));
    engine->on_reconnect (
        [this] (std::error_code const &error)
          {
            ++reports;
            report = error;
          });
  }

  ~reconnect_bench_t ()
  {
    engine.reset ();
    settle ();
    view_impl.reset ();
    session.reset ();
    settle ();
    mock.reset ();
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

  outcome_ptr
  start (pr::exchange_t request)
  {
    outcome_ptr const out = std::make_shared<outcome_t> ();
    record_t done = { out, &clock };
    engine->async_exchange (std::move (request), done);
    return out;
  }

  asio::io_context context;
  tr::testing::virtual_clock clock;
  std::unique_ptr<tr::testing::mock_stream> mock;
  std::unique_ptr<ss::stream_session> session;
  std::shared_ptr<tr::stream::stream_impl> view_impl;
  std::unique_ptr<pr::dialogue_engine> engine;
  int reports = 0;
  std::error_code report;
};

TEST (LumexDialogueReconnect, WithoutSteps_ALinkThatComesBackIsTheSameDevice)
{
  // Nothing to check, nothing to wait for.
  reconnect_bench_t bench;
  bench.mock->control.disconnect ();
  bench.settle ();
  EXPECT_EQ (bench.reports, 1);
  EXPECT_FALSE (bench.report);
  EXPECT_EQ (bench.engine->statistics ().reconnects, 1u);
  EXPECT_TRUE (bench.engine->link_is_up ());
}

TEST (LumexDialogueReconnect, LinkState_LostThenUpWithTheOpenAndTheForget)
{
  int forgets = 0;
  pr::reconnect_steps_t steps;
  steps.forget = [&forgets] () { ++forgets; };
  reconnect_bench_t bench (steps);
  std::vector<pr::link_state> states;
  bench.engine->on_link_state (
      [&] (pr::link_state state, std::error_code const &)
        { states.push_back (state); });
  bench.mock->control.fail_opens_for (
      ms (150), make_error_code (transport_errc::disconnected));
  bench.mock->control.disconnect ();
  bench.settle ();
  EXPECT_EQ (forgets, 1);
  ASSERT_EQ (states.size (), 1u);
  EXPECT_EQ (states[0], pr::link_state::lost);
  EXPECT_FALSE (bench.engine->link_is_up ());
  bench.advance (ms (500));
  ASSERT_EQ (states.size (), 2u);
  EXPECT_EQ (states[1], pr::link_state::up);
  EXPECT_TRUE (bench.engine->link_is_up ());
}

TEST (LumexDialogueReconnect, OnReconnect_NeverInsideTheStep)
{
  // Falsified by a report called where the restoration ends: it would come
  // before the step that opened the connection told the link state.
  reconnect_bench_t bench;
  std::vector<int> order;
  bench.engine->on_link_state (
      [&] (pr::link_state state, std::error_code const &)
        {
          if (state == pr::link_state::up)
            order.push_back (1);
        });
  bench.engine->on_reconnect ([&] (std::error_code const &)
                                { order.push_back (2); });
  bench.mock->control.disconnect ();
  bench.settle ();
  ASSERT_EQ (order.size (), 2u);
  EXPECT_EQ (order[0], 1);
  EXPECT_EQ (order[1], 2);
}

TEST (LumexDialogueReconnect, Check_BeforeItNothingButBeforeCheckGoes)
{
  // Falsified by a gate that takes every exchange, and by one that takes none.
  pr::reconnect_done_t held;
  pr::reconnect_steps_t steps;
  steps.check
      = [&held] (pr::reconnect_done_t done) { held = std::move (done); };
  reconnect_bench_t bench (steps);
  bench.mock->control.disconnect ();
  bench.settle ();
  ASSERT_TRUE (static_cast<bool> (held));
  std::size_t const written = bench.mock->control.written ().size ();
  outcome_ptr const refused = bench.start (make_exchange<pump::INFO_t> ());
  bench.settle ();
  EXPECT_EQ (refused->calls, 1);
  EXPECT_EQ (refused->error, protocol_errc::link_lost);
  EXPECT_EQ (bench.mock->control.written ().size (), written);
  pr::exchange_t allowed = make_exchange<pump::INFO_t> ();
  allowed.before_check = true;
  outcome_ptr const passed = bench.start (std::move (allowed));
  bench.settle ();
  EXPECT_GT (bench.mock->control.written ().size (), written);
  EXPECT_EQ (passed->calls, 0);
  EXPECT_EQ (bench.reports, 0);
  held (std::error_code (), true);
  bench.settle ();
  EXPECT_EQ (bench.reports, 1);
  EXPECT_FALSE (bench.report);
  // A second end changes nothing.
  held (make_error_code (std::errc::invalid_argument), false);
  bench.settle ();
  EXPECT_EQ (bench.reports, 1);
  outcome_ptr const open = bench.start (make_exchange<pump::INFO_t> ());
  bench.settle ();
  EXPECT_EQ (open->calls, 0);
  EXPECT_GT (bench.mock->control.written ().size (), written);
  // It holds the strand: it goes before the context.
  held = pr::reconnect_done_t ();
}

TEST (LumexDialogueReconnect, Check_OfAConnectionThatIsOverChangesNothing)
{
  // Falsified by an end without its number: the failure of the check of the
  // old connection would close the new one.
  std::vector<pr::reconnect_done_t> held;
  pr::reconnect_steps_t steps;
  steps.check = [&held] (pr::reconnect_done_t done)
    { held.push_back (std::move (done)); };
  reconnect_bench_t bench (steps);
  bench.mock->control.disconnect ();
  bench.settle ();
  ASSERT_EQ (held.size (), 1u);
  bench.advance (ms (200));
  bench.mock->control.disconnect ();
  bench.settle ();
  ASSERT_EQ (held.size (), 2u);
  held[0](make_error_code (std::errc::invalid_argument), true);
  bench.settle ();
  EXPECT_EQ (bench.reports, 0);
  held[1](std::error_code (), true);
  bench.settle ();
  EXPECT_EQ (bench.reports, 1);
  EXPECT_FALSE (bench.report);
  held.clear ();
}

TEST (LumexDialogueReconnect, Check_ThatThrowsIsAFailedCheck)
{
  // Falsified by an exception that leaves the dialogue waiting for ever.
  bool thrown = false;
  pr::reconnect_steps_t steps;
  steps.check = [&thrown] (pr::reconnect_done_t)
    {
      thrown = true;
      throw std::bad_alloc ();
    };
  reconnect_bench_t bench (steps);
  bench.mock->control.disconnect ();
  bench.settle ();
  EXPECT_TRUE (thrown);
  EXPECT_EQ (bench.reports, 1);
  EXPECT_EQ (bench.report, std::errc::not_enough_memory);
  outcome_ptr const refused = bench.start (make_exchange<pump::INFO_t> ());
  bench.settle ();
  EXPECT_EQ (refused->error, protocol_errc::not_supported);
}

TEST (LumexDialogueReconnect, DeviceChecked_OpensAfterAFailedCheckOnly)
{
  pr::reconnect_steps_t steps;
  steps.check = [] (pr::reconnect_done_t done)
    { done (make_error_code (std::errc::invalid_argument), true); };
  reconnect_bench_t bench (steps);
  // Nothing while all is well.
  bench.engine->device_checked ();
  bench.settle ();
  bench.mock->control.disconnect ();
  bench.settle ();
  EXPECT_EQ (bench.reports, 1);
  outcome_ptr const refused = bench.start (make_exchange<pump::INFO_t> ());
  bench.settle ();
  EXPECT_EQ (refused->error, protocol_errc::not_supported);
  bench.engine->device_checked ();
  bench.settle ();
  std::size_t const written = bench.mock->control.written ().size ();
  outcome_ptr const open = bench.start (make_exchange<pump::INFO_t> ());
  bench.settle ();
  EXPECT_GT (bench.mock->control.written ().size (), written);
  EXPECT_EQ (open->calls, 0);
}

TEST (LumexDialogueReconnect, Exchange_WithoutARowIsNeverSentAgain)
{
  // Falsified by a request whose idempotence nobody can know going out again.
  reconnect_bench_t bench;
  pr::exchange_t request = make_exchange<pump::INFO_t> ();
  request.row = nullptr;
  request.how = dialogue::correlation::by_order;
  outcome_ptr const out = bench.start (std::move (request));
  bench.settle ();
  std::size_t const written = bench.mock->control.written ().size ();
  bench.mock->control.disconnect ();
  bench.settle ();
  EXPECT_EQ (out->calls, 1);
  EXPECT_EQ (out->error, protocol_errc::link_lost);
  EXPECT_EQ (bench.mock->control.written ().size (), written);
}

TEST (LumexDialogueReconnect, Reader_NoTimerOfTheReadingWhileTheLinkIsDown)
{
  // Falsified by the restart of the reading of a link that is not there: it
  // would fail and count on and on.
  reconnect_bench_t bench;
  bench.mock->control.fail_opens_for (
      ms (100000), make_error_code (transport_errc::disconnected));
  bench.mock->control.disconnect ();
  bench.settle ();
  // A read of a link that is not there counts as an error of the stream.
  tr::statistics::statistics_t const before
      = bench.view_impl->counters ().snapshot ();
  bench.advance (ms (20000));
  tr::statistics::statistics_t const after
      = bench.view_impl->counters ().snapshot ();
  EXPECT_EQ (after.reads, before.reads);
  EXPECT_EQ (after.errors, before.errors);
}

TEST (LumexDialogueReconnect, Reader_StartsWithTheNewConnection)
{
  // Falsified by a reading that waits for the next request: what the device
  // says by itself would be lost.
  reconnect_bench_t bench;
  bench.mock->control.disconnect ();
  bench.settle ();
  bytes_t heard;
  bench.engine->on_unknown ([&] (pr::reply_frame_t const &frame)
                              { heard = frame; });
  bytes_t const frame = bytes ({ 0xEE });
  bytes_t wire;
  fr::slip::slip_codec ().encode (
      layout::bytes_view_t (frame.data (), frame.size ()), wire);
  bench.mock->control.feed (wire);
  bench.settle ();
  EXPECT_EQ (heard, frame);
}

// A link of one's own that says nothing of reconnecting: as before.
TEST (LumexDialogueReconnect, ALinkWithoutASession_DoesNotReconnect)
{
  asio::io_context context;
  tr::testing::virtual_clock clock;
  tr::testing::mock_options_t mock_options;
  mock_options.timers = clock.source ();
  tr::testing::mock_stream mock
      = tr::testing::make_mock_stream (context.get_executor (), mock_options);
  mock.stream.async_open ([] (std::error_code) {});
  context.poll ();
  context.restart ();
  pr::frame_link link (tr::framed::framed_stream<fr::slip::slip_codec> (
      std::move (mock.stream), fr::slip::slip_codec ()));
  EXPECT_FALSE (link.reconnects ());
  EXPECT_FALSE (pr::frame_link ().reconnects ());
}

// The defaults of frame_link_impl.
class own_link_t final : public pr::frame_link_impl
{
public:
  explicit own_link_t (asio::io_context &context)
      : strand_ (
            asio::make_strand (tr::executor_type (context.get_executor ()))),
        name_ ("own")
  {
  }

  tr::executor_type
  get_executor () const override
  {
    return strand_.get_inner_executor ();
  }

  strand_type
  get_strand () const override
  {
    return strand_;
  }

  std::string const &
  name () const override
  {
    return name_;
  }

  bool
  is_open () const override
  {
    return !closed_;
  }

  void
  start_read (pr::link_read_handler_t) override
  {
  }

  void
  start_write (std::shared_ptr<pr::link_frame_t const>,
               tr::status_handler_t) override
  {
  }

  void
  cancel () LUMEX_NOEXCEPT override
  {
  }

  void
  close () LUMEX_NOEXCEPT override
  {
    closed_ = true;
  }

private:
  strand_type strand_;
  std::string name_;
  bool closed_ = false;
};

TEST (LumexDialogueReconnect, FrameLinkImpl_DefaultsSayNo)
{
  asio::io_context context;
  own_link_t link (context);
  EXPECT_FALSE (link.reconnects ());
  link.watch_connection (tr::connection_handler_t ());
  EXPECT_TRUE (link.is_open ());
  link.drop_connection ();
  EXPECT_FALSE (link.is_open ());
}
} // namespace
