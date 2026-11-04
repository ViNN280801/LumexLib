// The lazy form of the behaviour applier: the reply is made only for a
// request the device processes (a simulator runs the handler of a row and
// changes its state inside the maker). The trial run on a copy of the state
// must leave the counters, the generators and the outages exactly where the
// plain plan leaves them.

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/applied/transport/LumexTransport"

namespace
{
namespace tr = lumex::applied::transport;
using bytes_t = std::vector<std::uint8_t>;
using ms = std::chrono::milliseconds;
using tr::testing::behavior;
using tr::testing::behavior_applier;
using tr::testing::reply_plan_t;

bytes_t
reply_bytes ()
{
  bytes_t out;
  for (std::uint8_t value = 0x20; value < 0x28; ++value)
    out.push_back (value);
  return out;
}

bytes_t
request_bytes ()
{
  bytes_t out;
  out.push_back (0xA1);
  out.push_back (0xA2);
  return out;
}

// The bytes of every bytes step of a plan, in order.
bytes_t
bytes_of (reply_plan_t const &plan)
{
  bytes_t out;
  for (std::size_t i = 0; i < plan.steps.size (); ++i)
    if (plan.steps[i].kind == tr::testing::plan_step_kind::bytes)
      out.insert (out.end (), plan.steps[i].bytes.begin (),
                  plan.steps[i].bytes.end ());
  return out;
}

struct counting_maker_t
{
  int *calls;

  bytes_t
  operator() () const
  {
    ++*calls;
    return reply_bytes ();
  }
};
} // namespace

TEST (LumexBehaviorsLazyReply, LazyPlan_MakesTheReplyOfAProcessedRequestOnce)
{
  behavior_applier applier (behavior::perfect ());
  int calls = 0;
  counting_maker_t maker = { &calls };
  reply_plan_t const plan
      = applier.plan (request_bytes (), true, maker, false);
  EXPECT_TRUE (plan.request_processed);
  EXPECT_EQ (calls, 1);
  EXPECT_EQ (bytes_of (plan), reply_bytes ());
}

TEST (LumexBehaviorsLazyReply, LazyPlan_SilentDeviceNeverCallsTheMaker)
{
  behavior_applier applier (behavior::silent ());
  int calls = 0;
  counting_maker_t maker = { &calls };
  reply_plan_t const plan
      = applier.plan (request_bytes (), true, maker, false);
  EXPECT_FALSE (plan.request_processed);
  EXPECT_EQ (calls, 0);
  EXPECT_TRUE (bytes_of (plan).empty ());
}

TEST (LumexBehaviorsLazyReply, LazyPlan_EchoingDeviceNeverCallsTheMaker)
{
  behavior_applier applier (behavior::echo ());
  int calls = 0;
  counting_maker_t maker = { &calls };
  reply_plan_t const plan
      = applier.plan (request_bytes (), true, maker, false);
  EXPECT_FALSE (plan.request_processed);
  EXPECT_EQ (calls, 0);
  EXPECT_EQ (bytes_of (plan), request_bytes ());
}

TEST (LumexBehaviorsLazyReply,
      LazyPlan_FrozenDeviceCallsTheMakerOnlyBeforeTheFreeze)
{
  behavior_applier applier (behavior::frozen (2));
  int calls = 0;
  counting_maker_t maker = { &calls };
  EXPECT_TRUE (
      applier.plan (request_bytes (), true, maker, false).request_processed);
  EXPECT_TRUE (
      applier.plan (request_bytes (), true, maker, false).request_processed);
  EXPECT_FALSE (
      applier.plan (request_bytes (), true, maker, false).request_processed);
  EXPECT_FALSE (
      applier.plan (request_bytes (), true, maker, false).request_processed);
  EXPECT_EQ (calls, 2);
}

TEST (LumexBehaviorsLazyReply,
      LazyPlan_UnpluggedDeviceProcessesAgainWhenTheTimeCame)
{
  tr::testing::virtual_clock clock;
  behavior_applier applier (behavior::cable_unplugged (ms (300)),
                            clock.source ());
  int calls = 0;
  counting_maker_t maker = { &calls };
  EXPECT_FALSE (
      applier.plan (request_bytes (), true, maker, false).request_processed);
  clock.advance (ms (299));
  EXPECT_FALSE (
      applier.plan (request_bytes (), true, maker, false).request_processed);
  clock.advance (ms (1));
  EXPECT_TRUE (
      applier.plan (request_bytes (), true, maker, false).request_processed);
  EXPECT_EQ (calls, 1);
}

TEST (LumexBehaviorsLazyReply, LazyPlan_MovesTheStateAsThePlainPlanDoes)
{
  // The same seeded behaviour, once lazily and once plainly, gives the same
  // plans request after request (the trial run changed nothing).
  behavior const shaped = behavior::intermittent (11, 500, 0, ms (20))
                              .then (behavior::fragmented (3, 3, ms (2)))
                              .then (behavior::frozen (6));
  behavior_applier lazy (shaped);
  behavior_applier plain (shaped);
  int calls = 0;
  counting_maker_t maker = { &calls };
  for (int i = 0; i < 12; ++i)
    {
      reply_plan_t const left
          = lazy.plan (request_bytes (), true, maker, false);
      reply_plan_t const right
          = plain.plan (request_bytes (), reply_bytes (), false);
      EXPECT_EQ (left.request_processed, right.request_processed) << i;
      EXPECT_EQ (left.steps.size (), right.steps.size ()) << i;
      EXPECT_EQ (bytes_of (left), bytes_of (right)) << i;
    }
}

TEST (LumexBehaviorsLazyReply, LazyPlan_ARowWithoutAReplyMakesNothing)
{
  behavior_applier applier (behavior::perfect ());
  int calls = 0;
  reply_plan_t const plan = applier.plan (
      request_bytes (), false,
      [&calls] () -> bytes_t
        {
          ++calls;
          return bytes_t ();
        },
      false);
  EXPECT_TRUE (plan.request_processed);
  EXPECT_TRUE (plan.steps.empty ());
}

TEST (LumexBehaviorsLazyReply, LazyPlan_MakerThatThrowsLeavesTheApplierAsItWas)
{
  behavior_applier applier (behavior::frozen (1));
  bool thrown = false;
  try
    {
      applier.plan (
          request_bytes (), true,
          [] () -> bytes_t { throw std::runtime_error ("maker"); }, false);
    }
  catch (std::runtime_error const &)
    {
      thrown = true;
    }
  EXPECT_TRUE (thrown);
  // The reply that threw did not count: the one reply of the freeze is
  // still to come.
  int calls = 0;
  counting_maker_t maker = { &calls };
  EXPECT_TRUE (
      applier.plan (request_bytes (), true, maker, false).request_processed);
  EXPECT_FALSE (
      applier.plan (request_bytes (), true, maker, false).request_processed);
  EXPECT_EQ (calls, 1);
}

TEST (LumexBehaviorsLazyReply,
      LazyRespond_FeedsAMockStreamAndReportsProcessing)
{
  asio::io_context context;
  tr::testing::virtual_clock clock;
  tr::testing::mock_options_t options;
  options.timers = clock.source ();
  tr::testing::mock_stream mock
      = tr::testing::make_mock_stream (context.get_executor (), options);
  behavior_applier applier (behavior::frozen (1), clock.source ());
  int calls = 0;
  counting_maker_t maker = { &calls };
  EXPECT_TRUE (applier.respond (mock.control, request_bytes (), true, maker));
  EXPECT_FALSE (applier.respond (mock.control, request_bytes (), true, maker));
  EXPECT_EQ (calls, 1);
  mock.stream.async_open ([] (std::error_code) {});
  context.poll ();
}

TEST (LumexBehaviorsLazyReply, LazyRespond_DeliversADatagramWithItsReportId)
{
  asio::io_context context;
  tr::testing::virtual_clock clock;
  tr::testing::mock_options_t options;
  options.timers = clock.source ();
  tr::testing::mock_datagram mock
      = tr::testing::make_mock_datagram (context.get_executor (), options);
  mock.transport.async_open ([] (std::error_code) {});
  context.poll ();
  context.restart ();
  behavior_applier applier (behavior::perfect (), clock.source ());
  int calls = 0;
  counting_maker_t maker = { &calls };
  EXPECT_TRUE (
      applier.respond (mock.control, 9, request_bytes (), true, maker));
  EXPECT_EQ (calls, 1);
  bool received = false;
  mock.transport.async_receive (
      [&received] (std::error_code error, tr::datagram::message_t message)
        {
          EXPECT_FALSE (error);
          EXPECT_EQ (message.report_id, 9);
          EXPECT_EQ (message.payload, reply_bytes ());
          received = true;
        });
  context.poll ();
  EXPECT_TRUE (received);
}

TEST (LumexBehaviorsLazyReply,
      LazyPlan_ARequestNotProcessedCountsLikeThePlainPlan)
{
  // During the window of the unplugged cable the request is not processed;
  // the stage after it still counts it as a reply, as the plain plan does
  // (the stand-in reply stands for the reply that was never made).
  tr::testing::virtual_clock clock;
  behavior const shaped
      = behavior::cable_unplugged (ms (100)).then (behavior::frozen (1));
  behavior_applier lazy (shaped, clock.source ());
  behavior_applier plain (shaped, clock.source ());
  int calls = 0;
  counting_maker_t maker = { &calls };
  EXPECT_FALSE (
      lazy.plan (request_bytes (), true, maker, false).request_processed);
  EXPECT_FALSE (
      plain.plan (request_bytes (), reply_bytes (), false).request_processed);
  clock.advance (ms (150));
  bool const left
      = lazy.plan (request_bytes (), true, maker, false).request_processed;
  bool const right
      = plain.plan (request_bytes (), reply_bytes (), false).request_processed;
  EXPECT_EQ (left, right);
  EXPECT_FALSE (right);
  EXPECT_EQ (calls, 0);
}
