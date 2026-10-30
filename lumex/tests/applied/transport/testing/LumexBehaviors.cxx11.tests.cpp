// The device behaviours: one test per behaviour at the byte level (what a
// mock stream or a mock datagram transport delivers for a known reply, and
// when, on the virtual clock), the composition order, determinism by seed,
// the expected outcome classes and the use from several threads.

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/applied/transport/LumexTransport"
#include "lumex/applied/transport/testing/LumexBehaviors.hpp"

namespace
{
namespace tr = lumex::applied::transport;
using ms = std::chrono::milliseconds;
using bytes_t = std::vector<std::uint8_t>;
using tr::datagram::message_t;
using tr::error::transport_errc;
using tr::testing::behavior;
using tr::testing::behavior_applier;
using tr::testing::behavior_kind;
using tr::testing::malicious_mode;
using tr::testing::mock_options_t;
using tr::testing::outcome_class;
using tr::testing::plan_step_kind;
using tr::testing::reply_plan_t;
using tr::testing::reply_shape;
using tr::testing::row_shape_t;
using tr::testing::virtual_clock;

using stream_mock_t = decltype (tr::testing::make_mock_stream (
    std::declval<tr::executor_type const &> (),
    std::declval<mock_options_t const &> ()));
using datagram_mock_t = decltype (tr::testing::make_mock_datagram (
    std::declval<tr::executor_type const &> (),
    std::declval<mock_options_t const &> ()));

// A reply of twelve distinct bytes and a request of three.
bytes_t
reply_bytes ()
{
  bytes_t bytes;
  for (std::uint8_t value = 0x10; value < 0x1C; ++value)
    bytes.push_back (value);
  return bytes;
}

bytes_t
request_bytes ()
{
  bytes_t bytes;
  bytes.push_back (0xA1);
  bytes.push_back (0xA2);
  bytes.push_back (0xA3);
  return bytes;
}

std::error_code
code (transport_errc value)
{
  return make_error_code (value);
}

void
drain (asio::io_context &context)
{
  for (;;)
    {
      if (context.stopped ())
        context.restart ();
      if (context.poll () == 0)
        return;
    }
}

bytes_t
concat (std::vector<bytes_t> const &parts)
{
  bytes_t all;
  for (std::size_t index = 0; index < parts.size (); ++index)
    all.insert (all.end (), parts[index].begin (), parts[index].end ());
  return all;
}

// One completed read or receive: when, how much, how it ended.
struct arrival_t
{
  ms at;
  std::size_t count = 0;
  std::error_code error;
  std::uint8_t report_id = 0;
};

struct read_log_t
{
  std::vector<arrival_t> arrivals;
  bytes_t bytes;
  std::size_t completed = 0;
  bool stopped = false;

  std::size_t
  data_arrivals () const
  {
    std::size_t total = 0;
    for (std::size_t index = 0; index < arrivals.size (); ++index)
      if (arrivals[index].count > 0)
        ++total;
    return total;
  }

  std::size_t
  empty_arrivals () const
  {
    std::size_t total = 0;
    for (std::size_t index = 0; index < arrivals.size (); ++index)
      if (arrivals[index].count == 0 && !arrivals[index].error)
        ++total;
    return total;
  }
};

ms
since (virtual_clock const &clock, tr::timing::time_point_t start)
{
  return std::chrono::duration_cast<ms> (clock.now () - start);
}

// Reads until the stream reports an error (or `limit` reads), logging each.
void
keep_reading (tr::stream::byte_stream &stream, virtual_clock const &clock,
              tr::timing::time_point_t start, std::shared_ptr<read_log_t> log)
{
  std::shared_ptr<bytes_t> const buffer = std::make_shared<bytes_t> (64);
  stream.async_read_some (
      tr::mutable_bytes_t (*buffer),
      [&stream, &clock, start, log, buffer] (std::error_code const &error,
                                             std::size_t count)
        {
          arrival_t arrival;
          arrival.at = since (clock, start);
          arrival.count = count;
          arrival.error = error;
          log->arrivals.push_back (arrival);
          log->bytes.insert (log->bytes.end (), buffer->begin (),
                             buffer->begin ()
                                 + static_cast<std::ptrdiff_t> (count));
          ++log->completed;
          if (!error && log->completed < 2000)
            keep_reading (stream, clock, start, log);
          else
            log->stopped = true;
        });
}

void
keep_receiving (tr::datagram::message_transport &transport,
                virtual_clock const &clock, tr::timing::time_point_t start,
                std::shared_ptr<read_log_t> log)
{
  transport.async_receive (
      [&transport, &clock, start, log] (std::error_code const &error,
                                        message_t message)
        {
          arrival_t arrival;
          arrival.at = since (clock, start);
          arrival.count = message.payload.size ();
          arrival.error = error;
          arrival.report_id = message.report_id;
          log->arrivals.push_back (arrival);
          log->bytes.insert (log->bytes.end (), message.payload.begin (),
                             message.payload.end ());
          ++log->completed;
          if (!error && log->completed < 2000)
            keep_receiving (transport, clock, start, log);
          else
            log->stopped = true;
        });
}

class LumexBehaviorsStreamTest : public ::testing::Test
{
protected:
  asio::io_context context_;
  virtual_clock clock_;
  tr::timing::time_point_t start_ = clock_.now ();
  stream_mock_t mock_
      = tr::testing::make_mock_stream (context_.get_executor (), scripted ());
  std::shared_ptr<read_log_t> log_ = std::make_shared<read_log_t> ();
  std::shared_ptr<behavior_applier> applier_;

  mock_options_t
  scripted () const
  {
    mock_options_t options;
    options.name = "behaviors";
    options.timers = clock_.source ();
    return options;
  }

  void
  TearDown () override
  {
    // The observer owns a copy of the control: a cycle until it is dropped.
    mock_.control.on_write (tr::testing::write_observer_t ());
  }

  std::error_code
  open_now ()
  {
    std::shared_ptr<std::error_code> const result
        = std::make_shared<std::error_code> ();
    std::shared_ptr<bool> const done = std::make_shared<bool> (false);
    mock_.stream.async_open (
        [result, done] (std::error_code const &error)
          {
            *result = error;
            *done = true;
          });
    drain (context_);
    EXPECT_TRUE (*done);
    return *result;
  }

  // Opens, starts the read loop, and answers every write with `reply`
  // through `shaped`.
  void
  start (behavior const &shaped, bytes_t const &reply = reply_bytes ())
  {
    ASSERT_FALSE (open_now ());
    applier_ = std::make_shared<behavior_applier> (shaped, clock_.source ());
    tr::testing::mock_stream_control const control = mock_.control;
    std::shared_ptr<behavior_applier> const applier = applier_;
    mock_.control.on_write ([control, applier, reply] (bytes_t const &chunk)
                              { applier->respond (control, chunk, reply); });
    keep_reading (mock_.stream, clock_, start_, log_);
    drain (context_);
  }

  void
  send (bytes_t const &request = request_bytes ())
  {
    std::shared_ptr<bytes_t> const data = std::make_shared<bytes_t> (request);
    mock_.stream.async_write_some (
        tr::const_bytes_t (*data),
        [data] (std::error_code const &, std::size_t) {});
    drain (context_);
  }

  // Lets the virtual clock run `budget` past now, serving every wait.
  void
  run (ms budget)
  {
    tr::testing::run_until (context_, clock_, [] { return false; }, budget);
  }

  void
  reopen ()
  {
    mock_.stream.close ();
    drain (context_);
    log_ = std::make_shared<read_log_t> ();
    ASSERT_FALSE (open_now ());
    keep_reading (mock_.stream, clock_, start_, log_);
    drain (context_);
  }
};

class LumexBehaviorsDatagramTest : public ::testing::Test
{
protected:
  asio::io_context context_;
  virtual_clock clock_;
  tr::timing::time_point_t start_ = clock_.now ();
  datagram_mock_t mock_ = tr::testing::make_mock_datagram (
      context_.get_executor (), scripted ());
  std::shared_ptr<read_log_t> log_ = std::make_shared<read_log_t> ();
  std::shared_ptr<behavior_applier> applier_;

  mock_options_t
  scripted () const
  {
    mock_options_t options;
    options.name = "behaviors";
    options.timers = clock_.source ();
    return options;
  }

  void
  TearDown () override
  {
    mock_.control.on_send (tr::testing::send_observer_t ());
  }

  std::error_code
  open_now ()
  {
    std::shared_ptr<std::error_code> const result
        = std::make_shared<std::error_code> ();
    mock_.transport.async_open ([result] (std::error_code const &error)
                                  { *result = error; });
    drain (context_);
    return *result;
  }

  void
  start (behavior const &shaped, std::uint8_t report_id,
         bytes_t const &reply = reply_bytes ())
  {
    ASSERT_FALSE (open_now ());
    applier_ = std::make_shared<behavior_applier> (shaped, clock_.source ());
    tr::testing::mock_datagram_control const control = mock_.control;
    std::shared_ptr<behavior_applier> const applier = applier_;
    mock_.control.on_send (
        [control, applier, report_id, reply] (message_t const &message)
          { applier->respond (control, report_id, message.payload, reply); });
    keep_receiving (mock_.transport, clock_, start_, log_);
    drain (context_);
  }

  void
  send (bytes_t const &request = request_bytes ())
  {
    message_t message;
    message.payload = request;
    mock_.transport.async_send (message, [] (std::error_code const &) {});
    drain (context_);
  }

  void
  run (ms budget)
  {
    tr::testing::run_until (context_, clock_, [] { return false; }, budget);
  }
};

// The plan of one request/reply pair by a fresh applier of `shaped`.
reply_plan_t
plan_of (behavior const &shaped, bytes_t const &request = request_bytes (),
         bytes_t const &reply = reply_bytes ())
{
  virtual_clock clock;
  behavior_applier applier (shaped, clock.source ());
  return applier.plan (request, reply);
}

std::vector<std::size_t>
sizes_of (reply_plan_t const &plan)
{
  std::vector<std::size_t> sizes;
  for (std::size_t index = 0; index < plan.steps.size (); ++index)
    if (plan.steps[index].kind == plan_step_kind::bytes)
      sizes.push_back (plan.steps[index].bytes.size ());
  return sizes;
}

bytes_t
bytes_of_plan (reply_plan_t const &plan)
{
  bytes_t all;
  for (std::size_t index = 0; index < plan.steps.size (); ++index)
    if (plan.steps[index].kind == plan_step_kind::bytes)
      all.insert (all.end (), plan.steps[index].bytes.begin (),
                  plan.steps[index].bytes.end ());
  return all;
}

std::size_t
differences (bytes_t const &left, bytes_t const &right)
{
  std::size_t count = 0;
  for (std::size_t index = 0; index < left.size (); ++index)
    if (left[index] != right[index])
      ++count;
  return count;
}
} // namespace

// --- the value

TEST (LumexBehaviors, GivenTheKinds_WhenListed_ThenThereAreEighteenWithNames)
{
  std::vector<behavior_kind> const kinds = tr::testing::all_behavior_kinds ();
  ASSERT_EQ (kinds.size (), 18u);
  std::set<std::string> names;
  for (std::size_t index = 0; index < kinds.size (); ++index)
    names.insert (tr::testing::behavior_kind_name (kinds[index]));
  EXPECT_EQ (names.size (), 18u);
  char const *const expected[] = { "perfect",
                                   "echo",
                                   "silent",
                                   "frozen",
                                   "partial_then_frozen",
                                   "slow_trickle",
                                   "fragmented",
                                   "flood",
                                   "repeating",
                                   "garbage",
                                   "corrupted_crc",
                                   "oversized",
                                   "zero_read",
                                   "intermittent",
                                   "disconnecting",
                                   "works_then_disconnects",
                                   "cable_unplugged",
                                   "malicious" };
  for (std::size_t index = 0; index < 18; ++index)
    EXPECT_EQ (std::string (tr::testing::behavior_kind_name (kinds[index])),
               expected[index]);
}

TEST (LumexBehaviors, GivenTheOutcomeClasses_WhenNamed_ThenEachHasItsName)
{
  EXPECT_EQ (
      std::string (tr::testing::outcome_class_name (outcome_class::success)),
      "success");
  EXPECT_EQ (std::string (tr::testing::outcome_class_name (
                 outcome_class::success_after_retries)),
             "success_after_retries");
  EXPECT_EQ (
      std::string (tr::testing::outcome_class_name (outcome_class::timeout)),
      "timeout");
  EXPECT_EQ (std::string (tr::testing::outcome_class_name (
                 outcome_class::retries_exhausted)),
             "retries_exhausted");
  EXPECT_EQ (
      std::string (tr::testing::outcome_class_name (outcome_class::link_lost)),
      "link_lost");
  EXPECT_EQ (std::string (tr::testing::outcome_class_name (
                 outcome_class::protocol_error)),
             "protocol_error");
  EXPECT_EQ (std::string (tr::testing::outcome_class_name (
                 outcome_class::dropped_counted)),
             "dropped_counted");
}

TEST (LumexBehaviors, GivenEveryFactory_WhenCalled_ThenItReportsItsKind)
{
  struct row_t
  {
    behavior value;
    behavior_kind kind;
  };
  row_t const rows[] = {
    { behavior::perfect (), behavior_kind::perfect },
    { behavior::echo (), behavior_kind::echo },
    { behavior::silent (), behavior_kind::silent },
    { behavior::frozen (1), behavior_kind::frozen },
    { behavior::partial_then_frozen (1, 2),
      behavior_kind::partial_then_frozen },
    { behavior::slow_trickle (ms (1)), behavior_kind::slow_trickle },
    { behavior::fragmented (1), behavior_kind::fragmented },
    { behavior::flood (1), behavior_kind::flood },
    { behavior::repeating (1, ms (1)), behavior_kind::repeating },
    { behavior::garbage (1, 1), behavior_kind::garbage },
    { behavior::corrupted_crc (1), behavior_kind::corrupted_crc },
    { behavior::oversized (1), behavior_kind::oversized },
    { behavior::zero_read (), behavior_kind::zero_read },
    { behavior::intermittent (1, 1, 1), behavior_kind::intermittent },
    { behavior::disconnecting (1), behavior_kind::disconnecting },
    { behavior::works_then_disconnects (1),
      behavior_kind::works_then_disconnects },
    { behavior::cable_unplugged (ms (1)), behavior_kind::cable_unplugged },
    { behavior::malicious (1), behavior_kind::malicious },
  };
  for (std::size_t index = 0; index < 18; ++index)
    {
      EXPECT_EQ (rows[index].value.kind (), rows[index].kind) << index;
      EXPECT_EQ (rows[index].value.stage_count (), 1u) << index;
      EXPECT_EQ (rows[index].value.stage_kind (0), rows[index].kind) << index;
    }
}

TEST (LumexBehaviors,
      GivenTheDefaultOfEveryKind_WhenMade_ThenItHasThatKindAndANonEmptyName)
{
  std::vector<behavior_kind> const kinds = tr::testing::all_behavior_kinds ();
  for (std::size_t index = 0; index < kinds.size (); ++index)
    {
      behavior const made = behavior::of (kinds[index]);
      EXPECT_EQ (made.kind (), kinds[index]) << index;
      EXPECT_EQ (made.stage_count (), 1u) << index;
    }
}

TEST (LumexBehaviors,
      GivenTheDefaultOversized_WhenPlanned_ThenItExceedsSixtyFourKibibytes)
{
  reply_plan_t const plan = plan_of (behavior::of (behavior_kind::oversized));
  EXPECT_GT (bytes_of_plan (plan).size (), 65535u);
}

TEST (LumexBehaviors, GivenADefaultBehavior_WhenAsked_ThenItIsPerfect)
{
  behavior const made;
  EXPECT_EQ (made.kind (), behavior_kind::perfect);
  EXPECT_EQ (made.stage_count (), 1u);
  reply_plan_t const plan = plan_of (made);
  EXPECT_EQ (bytes_of_plan (plan), reply_bytes ());
}

TEST (LumexBehaviors, GivenThen_WhenComposed_ThenTheStagesKeepTheirOrder)
{
  behavior const first = behavior::garbage (1, 2);
  behavior const second = behavior::fragmented (2);
  behavior const third = behavior::silent ();
  behavior const composed = first.then (second).then (third);
  ASSERT_EQ (composed.stage_count (), 3u);
  EXPECT_EQ (composed.stage_kind (0), behavior_kind::garbage);
  EXPECT_EQ (composed.stage_kind (1), behavior_kind::fragmented);
  EXPECT_EQ (composed.stage_kind (2), behavior_kind::silent);
  EXPECT_EQ (composed.kind (), behavior_kind::garbage);
  // The operands are unchanged.
  EXPECT_EQ (first.stage_count (), 1u);
  EXPECT_EQ (second.stage_count (), 1u);
  // The grouping does not change the order.
  behavior const grouped = first.then (second.then (third));
  ASSERT_EQ (grouped.stage_count (), 3u);
  EXPECT_EQ (grouped.stage_kind (0), behavior_kind::garbage);
  EXPECT_EQ (grouped.stage_kind (1), behavior_kind::fragmented);
  EXPECT_EQ (grouped.stage_kind (2), behavior_kind::silent);
}

TEST (LumexBehaviors, GivenThen_WhenOnePerfectIsComposed_ThenItAddsNoStage)
{
  behavior const composed = behavior::perfect ().then (behavior::silent ());
  ASSERT_EQ (composed.stage_count (), 1u);
  EXPECT_EQ (composed.kind (), behavior_kind::silent);
  behavior const other = behavior::silent ().then (behavior::perfect ());
  ASSERT_EQ (other.stage_count (), 1u);
  EXPECT_EQ (other.kind (), behavior_kind::silent);
}

TEST (LumexBehaviors,
      GivenDatagrams_WhenAskedPerKind_ThenOnlyZeroReadDoesNotApply)
{
  std::vector<behavior_kind> const kinds = tr::testing::all_behavior_kinds ();
  for (std::size_t index = 0; index < kinds.size (); ++index)
    EXPECT_EQ (behavior::of (kinds[index]).applies_to_datagrams (),
               kinds[index] != behavior_kind::zero_read)
        << tr::testing::behavior_kind_name (kinds[index]);
  EXPECT_FALSE (behavior::garbage (1, 1)
                    .then (behavior::zero_read ())
                    .applies_to_datagrams ());
  EXPECT_TRUE (behavior::garbage (1, 1)
                   .then (behavior::fragmented (1))
                   .applies_to_datagrams ());
}

TEST_F (LumexBehaviorsStreamTest,
        GivenEchoThenSilent_WhenComposed_ThenNothingIsDelivered)
{
  ASSERT_NO_FATAL_FAILURE (
      start (behavior::echo ().then (behavior::silent ())));
  send ();
  run (ms (100));
  EXPECT_TRUE (log_->bytes.empty ());
}

TEST_F (LumexBehaviorsStreamTest,
        GivenSilentThenEcho_WhenComposed_ThenTheRequestIsEchoed)
{
  ASSERT_NO_FATAL_FAILURE (
      start (behavior::silent ().then (behavior::echo ())));
  send ();
  run (ms (100));
  EXPECT_EQ (log_->bytes, request_bytes ());
}

// --- determinism

TEST (LumexBehaviors,
      GivenAFixedSeed_WhenGarbageIsPlanned_ThenTheBytesAreThePinnedOnes)
{
  // splitmix64 from the seed, each byte the top byte of the next output.
  reply_plan_t const plan = plan_of (behavior::garbage (1, 8));
  ASSERT_EQ (plan.steps.size (), 2u);
  std::uint8_t const expected[] = { 145, 190, 248, 113, 113, 195, 224, 133 };
  EXPECT_EQ (plan.steps[0].bytes, bytes_t (expected, expected + 8));
  EXPECT_EQ (plan.steps[1].bytes, reply_bytes ());
}

TEST (LumexBehaviors,
      GivenAFixedSeed_WhenFragmentedIsPlanned_ThenTheSizesArePinned)
{
  std::size_t const expected_one[] = { 3, 2, 1, 3, 1, 2 };
  EXPECT_EQ (sizes_of (plan_of (behavior::fragmented (1))),
             std::vector<std::size_t> (expected_one, expected_one + 6));
  std::size_t const expected_two[] = { 2, 3, 1, 1, 2, 1, 2 };
  EXPECT_EQ (sizes_of (plan_of (behavior::fragmented (2))),
             std::vector<std::size_t> (expected_two, expected_two + 7));
}

TEST (LumexBehaviors,
      GivenTheSameSeed_WhenTwoAppliersPlanTheSameRequests_ThenTheBytesAreEqual)
{
  std::vector<behavior> shaped;
  shaped.push_back (behavior::fragmented (7));
  shaped.push_back (behavior::garbage (7, 16));
  shaped.push_back (behavior::corrupted_crc (7, 500));
  shaped.push_back (behavior::intermittent (7, 500, 0));
  shaped.push_back (behavior::malicious (7));
  for (std::size_t index = 0; index < shaped.size (); ++index)
    {
      virtual_clock clock_a;
      virtual_clock clock_b;
      behavior_applier a (shaped[index], clock_a.source ());
      behavior_applier b (shaped[index], clock_b.source ());
      for (int round = 0; round < 40; ++round)
        {
          reply_plan_t const left = a.plan (request_bytes (), reply_bytes ());
          reply_plan_t const right = b.plan (request_bytes (), reply_bytes ());
          ASSERT_EQ (left.steps.size (), right.steps.size ()) << index;
          for (std::size_t step = 0; step < left.steps.size (); ++step)
            {
              EXPECT_EQ (left.steps[step].bytes, right.steps[step].bytes)
                  << index;
              EXPECT_EQ (left.steps[step].at, right.steps[step].at) << index;
            }
        }
    }
}

TEST (LumexBehaviors,
      GivenDifferentSeeds_WhenPlanned_ThenTheBytesDifferAndTheStateAdvances)
{
  EXPECT_NE (sizes_of (plan_of (behavior::fragmented (3))),
             sizes_of (plan_of (behavior::fragmented (4))));
  EXPECT_NE (plan_of (behavior::garbage (3, 16)).steps[0].bytes,
             plan_of (behavior::garbage (4, 16)).steps[0].bytes);
  virtual_clock clock;
  behavior_applier applier (behavior::garbage (3, 16), clock.source ());
  reply_plan_t const first = applier.plan (request_bytes (), reply_bytes ());
  reply_plan_t const second = applier.plan (request_bytes (), reply_bytes ());
  EXPECT_NE (first.steps[0].bytes, second.steps[0].bytes);
  applier.reset ();
  reply_plan_t const again = applier.plan (request_bytes (), reply_bytes ());
  EXPECT_EQ (first.steps[0].bytes, again.steps[0].bytes);
}

// --- perfect, echo, silent

TEST_F (LumexBehaviorsStreamTest, Perfect_DeliversTheReplyWholeAtOnce)
{
  ASSERT_NO_FATAL_FAILURE (start (behavior::perfect ()));
  send ();
  run (ms (10));
  EXPECT_EQ (log_->bytes, reply_bytes ());
  ASSERT_EQ (log_->arrivals.size (), 1u);
  EXPECT_EQ (log_->arrivals[0].at, ms (0));
  EXPECT_TRUE (
      applier_->plan (request_bytes (), reply_bytes ()).request_processed);
}

TEST_F (LumexBehaviorsStreamTest, Perfect_NeedsNoMoveOfTheClock)
{
  ASSERT_NO_FATAL_FAILURE (start (behavior::perfect ()));
  send ();
  EXPECT_EQ (log_->bytes, reply_bytes ());
}

TEST_F (LumexBehaviorsStreamTest, Echo_ReturnsTheRequestInPlaceOfTheReply)
{
  ASSERT_NO_FATAL_FAILURE (start (behavior::echo ()));
  send ();
  run (ms (10));
  EXPECT_EQ (log_->bytes, request_bytes ());
  EXPECT_FALSE (plan_of (behavior::echo ()).request_processed);
}

TEST_F (LumexBehaviorsStreamTest, Echo_WaitsItsDelayOnTheVirtualClock)
{
  ASSERT_NO_FATAL_FAILURE (start (behavior::echo (ms (25))));
  send ();
  run (ms (24));
  EXPECT_TRUE (log_->bytes.empty ());
  run (ms (10));
  EXPECT_EQ (log_->bytes, request_bytes ());
  ASSERT_EQ (log_->arrivals.size (), 1u);
  EXPECT_EQ (log_->arrivals[0].at, ms (25));
}

TEST_F (LumexBehaviorsStreamTest, Echo_OfAnEmptyRequestSaysNothing)
{
  reply_plan_t const plan
      = plan_of (behavior::echo (), bytes_t (), reply_bytes ());
  EXPECT_TRUE (plan.steps.empty ());
}

TEST_F (LumexBehaviorsStreamTest, Silent_NeverAnswersButTheRequestsAreWritten)
{
  ASSERT_NO_FATAL_FAILURE (start (behavior::silent ()));
  send ();
  send ();
  run (ms (5000));
  EXPECT_TRUE (log_->bytes.empty ());
  EXPECT_EQ (mock_.control.written ().size (), 6u);
  EXPECT_FALSE (plan_of (behavior::silent ()).request_processed);
}

// --- frozen, partial_then_frozen

TEST_F (LumexBehaviorsStreamTest, Frozen_AnswersTheFirstRepliesThenNever)
{
  ASSERT_NO_FATAL_FAILURE (start (behavior::frozen (2)));
  send ();
  run (ms (10));
  EXPECT_EQ (log_->bytes, reply_bytes ());
  send ();
  run (ms (10));
  EXPECT_EQ (log_->bytes, concat ({ reply_bytes (), reply_bytes () }));
  send ();
  send ();
  run (ms (5000));
  EXPECT_EQ (log_->bytes.size (), 24u);
}

TEST (LumexBehaviors, Frozen_StopsProcessingRequestsAfterTheFreeze)
{
  virtual_clock clock;
  behavior_applier applier (behavior::frozen (1), clock.source ());
  EXPECT_TRUE (
      applier.plan (request_bytes (), reply_bytes ()).request_processed);
  EXPECT_FALSE (
      applier.plan (request_bytes (), reply_bytes ()).request_processed);
  EXPECT_FALSE (applier.plan (request_bytes (), bytes_t ()).request_processed);
}

TEST (LumexBehaviors, Frozen_CountsOnlyRepliesNotRequestsWithoutOne)
{
  virtual_clock clock;
  behavior_applier applier (behavior::frozen (1), clock.source ());
  EXPECT_TRUE (applier.plan (request_bytes (), bytes_t ()).request_processed);
  EXPECT_TRUE (applier.plan (request_bytes (), bytes_t ()).request_processed);
  EXPECT_FALSE (
      applier.plan (request_bytes (), reply_bytes ()).steps.empty ());
  EXPECT_TRUE (applier.plan (request_bytes (), reply_bytes ()).steps.empty ());
}

TEST (LumexBehaviors, Frozen_OfZeroNeverAnswers)
{
  EXPECT_TRUE (plan_of (behavior::frozen (0)).steps.empty ());
}

TEST_F (LumexBehaviorsStreamTest,
        PartialThenFrozen_DeliversAPrefixOfTheReplyThenNothingMore)
{
  ASSERT_NO_FATAL_FAILURE (start (behavior::partial_then_frozen (1, 5)));
  send ();
  run (ms (10));
  EXPECT_EQ (log_->bytes, reply_bytes ());
  send ();
  run (ms (10));
  bytes_t expected = reply_bytes ();
  bytes_t const reply = reply_bytes ();
  expected.insert (expected.end (), reply.begin (), reply.begin () + 5);
  EXPECT_EQ (log_->bytes, expected);
  send ();
  run (ms (5000));
  EXPECT_EQ (log_->bytes, expected);
}

TEST (LumexBehaviors, PartialThenFrozen_NeverDeliversTheWholeFrame)
{
  EXPECT_EQ (
      bytes_of_plan (plan_of (behavior::partial_then_frozen (0, 100))).size (),
      11u);
  bytes_t one;
  one.push_back (0x42);
  EXPECT_TRUE (
      plan_of (behavior::partial_then_frozen (0, 100), request_bytes (), one)
          .steps.empty ());
  EXPECT_TRUE (
      bytes_of_plan (plan_of (behavior::partial_then_frozen (0, 0))).empty ());
}

TEST (LumexBehaviors, PartialThenFrozen_ProcessesTheCutRequestButNotTheNext)
{
  virtual_clock clock;
  behavior_applier applier (behavior::partial_then_frozen (0, 4),
                            clock.source ());
  EXPECT_TRUE (
      applier.plan (request_bytes (), reply_bytes ()).request_processed);
  EXPECT_FALSE (
      applier.plan (request_bytes (), reply_bytes ()).request_processed);
}

// --- slow_trickle, fragmented, zero_read

TEST_F (LumexBehaviorsStreamTest, SlowTrickle_DeliversOneByteEveryPeriod)
{
  ASSERT_NO_FATAL_FAILURE (start (behavior::slow_trickle (ms (10))));
  send ();
  run (ms (500));
  EXPECT_EQ (log_->bytes, reply_bytes ());
  ASSERT_EQ (log_->arrivals.size (), 12u);
  for (std::size_t index = 0; index < 12; ++index)
    {
      EXPECT_EQ (log_->arrivals[index].count, 1u) << index;
      EXPECT_EQ (log_->arrivals[index].at,
                 ms (10 * static_cast<long long> (index + 1)))
          << index;
    }
}

TEST_F (LumexBehaviorsStreamTest, Fragmented_SplitsAtSeededPositions)
{
  ASSERT_NO_FATAL_FAILURE (start (behavior::fragmented (1, 3, ms (5))));
  send ();
  run (ms (500));
  EXPECT_EQ (log_->bytes, reply_bytes ());
  std::size_t const expected[] = { 3, 2, 1, 3, 1, 2 };
  ASSERT_EQ (log_->arrivals.size (), 6u);
  for (std::size_t index = 0; index < 6; ++index)
    {
      EXPECT_EQ (log_->arrivals[index].count, expected[index]) << index;
      EXPECT_EQ (log_->arrivals[index].at,
                 ms (5 * static_cast<long long> (index)))
          << index;
    }
}

TEST (LumexBehaviors,
      Fragmented_NeverExceedsTheLargestFragmentAndKeepsEveryByte)
{
  for (std::uint32_t seed = 0; seed < 20; ++seed)
    {
      reply_plan_t const plan
          = plan_of (behavior::fragmented (seed, 4, ms (1)));
      EXPECT_EQ (bytes_of_plan (plan), reply_bytes ()) << seed;
      std::vector<std::size_t> const sizes = sizes_of (plan);
      for (std::size_t index = 0; index < sizes.size (); ++index)
        {
          EXPECT_GE (sizes[index], 1u);
          EXPECT_LE (sizes[index], 4u);
        }
    }
  // A largest fragment of zero is read as one.
  EXPECT_EQ (sizes_of (plan_of (behavior::fragmented (1, 0, ms (1)))).size (),
             12u);
}

TEST_F (LumexBehaviorsStreamTest, ZeroRead_EmptyReadsBetweenChunks)
{
  ASSERT_NO_FATAL_FAILURE (start (
      behavior::fragmented (1, 3, ms (5)).then (behavior::zero_read (2))));
  send ();
  run (ms (500));
  EXPECT_EQ (log_->bytes, reply_bytes ());
  // Two empty reads before each of the six chunks.
  EXPECT_EQ (log_->empty_arrivals (), 12u);
  EXPECT_EQ (log_->data_arrivals (), 6u);
  ASSERT_GE (log_->arrivals.size (), 3u);
  EXPECT_EQ (log_->arrivals[0].count, 0u);
  EXPECT_EQ (log_->arrivals[1].count, 0u);
  EXPECT_EQ (log_->arrivals[2].count, 3u);
}

TEST_F (LumexBehaviorsStreamTest, ZeroRead_AloneEmptiesTheReadsBeforeTheReply)
{
  ASSERT_NO_FATAL_FAILURE (start (behavior::zero_read (3)));
  send ();
  run (ms (10));
  EXPECT_EQ (log_->bytes, reply_bytes ());
  EXPECT_EQ (log_->empty_arrivals (), 3u);
  EXPECT_EQ (log_->data_arrivals (), 1u);
}

TEST (LumexBehaviors, ZeroRead_AddsNothingWithoutAReply)
{
  EXPECT_TRUE (plan_of (behavior::zero_read (), request_bytes (), bytes_t ())
                   .steps.empty ());
}

// --- flood, repeating, garbage, malicious

TEST_F (LumexBehaviorsStreamTest, Flood_BurstsACountingPatternBeforeTheReply)
{
  ASSERT_NO_FATAL_FAILURE (start (behavior::flood (300)));
  send ();
  run (ms (10));
  ASSERT_EQ (log_->bytes.size (), 312u);
  for (std::size_t index = 0; index < 300; ++index)
    ASSERT_EQ (log_->bytes[index], static_cast<std::uint8_t> (index & 0xFF))
        << index;
  EXPECT_EQ (bytes_t (log_->bytes.begin () + 300, log_->bytes.end ()),
             reply_bytes ());
  for (std::size_t index = 0; index < log_->arrivals.size (); ++index)
    EXPECT_EQ (log_->arrivals[index].at, ms (0));
}

TEST (LumexBehaviors, Flood_StillFloodsWhenThereIsNoReply)
{
  reply_plan_t const plan
      = plan_of (behavior::flood (10), request_bytes (), bytes_t ());
  ASSERT_EQ (plan.steps.size (), 1u);
  EXPECT_EQ (plan.steps[0].bytes.size (), 10u);
}

TEST_F (LumexBehaviorsStreamTest, Repeating_SendsTheReplyAgainAtTheGap)
{
  ASSERT_NO_FATAL_FAILURE (start (behavior::repeating (3, ms (10))));
  send ();
  run (ms (500));
  EXPECT_EQ (log_->bytes, concat ({ reply_bytes (), reply_bytes (),
                                    reply_bytes (), reply_bytes () }));
  ASSERT_EQ (log_->arrivals.size (), 4u);
  for (std::size_t index = 0; index < 4; ++index)
    EXPECT_EQ (log_->arrivals[index].at,
               ms (10 * static_cast<long long> (index)))
        << index;
}

TEST (LumexBehaviors, Repeating_CopiesEveryStepOfAFragmentedReplyTogether)
{
  reply_plan_t const plan
      = plan_of (behavior::fragmented (1, 3, ms (1))
                     .then (behavior::repeating (1, ms (100))));
  EXPECT_EQ (bytes_of_plan (plan).size (), 24u);
  EXPECT_EQ (plan.steps.size (), 12u);
  EXPECT_EQ (plan.steps[6].at, ms (100));
}

TEST_F (LumexBehaviorsStreamTest, Garbage_PutsNoiseBeforeTheReplyEveryTime)
{
  ASSERT_NO_FATAL_FAILURE (start (behavior::garbage (1, 8)));
  send ();
  send ();
  run (ms (10));
  ASSERT_EQ (log_->bytes.size (), 40u);
  std::uint8_t const noise[] = { 145, 190, 248, 113, 113, 195, 224, 133 };
  EXPECT_EQ (bytes_t (log_->bytes.begin (), log_->bytes.begin () + 8),
             bytes_t (noise, noise + 8));
  EXPECT_EQ (bytes_t (log_->bytes.begin () + 8, log_->bytes.begin () + 20),
             reply_bytes ());
  EXPECT_NE (bytes_t (log_->bytes.begin () + 20, log_->bytes.begin () + 28),
             bytes_t (noise, noise + 8));
}

TEST (LumexBehaviors, Garbage_OfZeroBytesAddsNothing)
{
  EXPECT_EQ (plan_of (behavior::garbage (1, 0)).steps.size (), 1u);
}

TEST (LumexBehaviors, Malicious_EveryModeMakesTheRequestedNumberOfPatternBytes)
{
  struct mode_t
  {
    malicious_mode mode;
    std::uint8_t first;
    std::uint8_t second;
  };
  mode_t const modes[] = {
    { malicious_mode::end_flood, 0xC0, 0xC0 },
    { malicious_mode::nul_flood, 0x00, 0x00 },
    { malicious_mode::ff_flood, 0xFF, 0xFF },
    { malicious_mode::control_chars, 0x00, 0x01 },
    { malicious_mode::alternating_extremes, 0x00, 0xFF },
    { malicious_mode::bad_escape, 0xC0, 0xDB },
    { malicious_mode::long_unterminated, 0xAB, 0xAB },
    { malicious_mode::max_length_prefix, 0xFF, 0xFF },
    { malicious_mode::dense_escapes, 0xDB, 0xDC },
  };
  for (std::size_t index = 0; index < 9; ++index)
    {
      reply_plan_t const plan
          = plan_of (behavior::malicious (1, modes[index].mode, 37));
      ASSERT_EQ (plan.steps.size (), 2u) << index;
      bytes_t const &attack = plan.steps[0].bytes;
      EXPECT_EQ (attack.size (), 37u) << index;
      EXPECT_EQ (attack[0], modes[index].first) << index;
      EXPECT_EQ (attack[1], modes[index].second) << index;
      EXPECT_EQ (plan.steps[1].bytes, reply_bytes ()) << index;
    }
}

TEST (LumexBehaviors, Malicious_PatternsRepeatTheirUnitAndDifferFromEachOther)
{
  bytes_t const control
      = plan_of (behavior::malicious (1, malicious_mode::control_chars, 70))
            .steps[0]
            .bytes;
  EXPECT_EQ (control[31], 0x1F);
  EXPECT_EQ (control[32], 0x00);
  bytes_t const dense
      = plan_of (behavior::malicious (1, malicious_mode::dense_escapes, 5))
            .steps[0]
            .bytes;
  std::uint8_t const expected_dense[] = { 0xDB, 0xDC, 0xDB, 0xDC, 0xDB };
  EXPECT_EQ (dense, bytes_t (expected_dense, expected_dense + 5));
  bytes_t const length
      = plan_of (behavior::malicious (1, malicious_mode::max_length_prefix, 4))
            .steps[0]
            .bytes;
  EXPECT_NE (length, bytes_t (4, 0xFF));
}

TEST (LumexBehaviors, Malicious_MixedPicksModesBySeedAndCoversSeveral)
{
  virtual_clock clock;
  behavior_applier applier (behavior::malicious (5), clock.source ());
  std::set<std::uint8_t> firsts;
  std::vector<bytes_t> seen;
  for (int round = 0; round < 60; ++round)
    {
      reply_plan_t const plan
          = applier.plan (request_bytes (), reply_bytes ());
      ASSERT_EQ (plan.steps[0].bytes.size (), 64u);
      firsts.insert (plan.steps[0].bytes[0]);
      seen.push_back (plan.steps[0].bytes);
    }
  EXPECT_GE (firsts.size (), 4u);
  behavior_applier again (behavior::malicious (5), clock.source ());
  for (int round = 0; round < 60; ++round)
    EXPECT_EQ (again.plan (request_bytes (), reply_bytes ()).steps[0].bytes,
               seen[static_cast<std::size_t> (round)]);
}

// --- corrupted_crc, oversized

TEST (LumexBehaviors, CorruptedCrc_FlipsInsideTheFrame)
{
  // Seed 1: the decision draw 465 (below 1000), then position 10, mask 1.
  reply_plan_t const plan = plan_of (behavior::corrupted_crc (1));
  bytes_t const flipped = bytes_of_plan (plan);
  bytes_t const original = reply_bytes ();
  ASSERT_EQ (flipped.size (), original.size ());
  EXPECT_EQ (differences (flipped, original), 1u);
  EXPECT_EQ (flipped[10], static_cast<std::uint8_t> (original[10] ^ 0x01));
  EXPECT_EQ (flipped[0], original[0]);
  EXPECT_EQ (flipped[11], original[11]);
}

TEST (LumexBehaviors, CorruptedCrc_AlwaysChangesExactlyOneInnerByte)
{
  for (std::uint32_t seed = 0; seed < 200; ++seed)
    {
      bytes_t const flipped
          = bytes_of_plan (plan_of (behavior::corrupted_crc (seed)));
      bytes_t const original = reply_bytes ();
      ASSERT_EQ (flipped.size (), original.size ()) << seed;
      EXPECT_EQ (differences (flipped, original), 1u) << seed;
      EXPECT_EQ (flipped[0], original[0]) << seed;
      EXPECT_EQ (flipped[11], original[11]) << seed;
    }
}

TEST (LumexBehaviors, CorruptedCrc_ShortFramesStillChangeOneByte)
{
  for (std::size_t size = 1; size <= 3; ++size)
    {
      bytes_t const reply (size, 0x55);
      bytes_t const flipped = bytes_of_plan (
          plan_of (behavior::corrupted_crc (3), request_bytes (), reply));
      ASSERT_EQ (flipped.size (), size);
      EXPECT_EQ (differences (flipped, reply), 1u) << size;
    }
}

TEST (LumexBehaviors, CorruptedCrc_RateZeroNeverAndTheLimitStops)
{
  virtual_clock clock;
  behavior_applier never (behavior::corrupted_crc (1, 0), clock.source ());
  for (int round = 0; round < 50; ++round)
    EXPECT_EQ (bytes_of_plan (never.plan (request_bytes (), reply_bytes ())),
               reply_bytes ());
  behavior_applier once (behavior::corrupted_crc (1, 1000, 2),
                         clock.source ());
  EXPECT_NE (bytes_of_plan (once.plan (request_bytes (), reply_bytes ())),
             reply_bytes ());
  EXPECT_NE (bytes_of_plan (once.plan (request_bytes (), reply_bytes ())),
             reply_bytes ());
  for (int round = 0; round < 10; ++round)
    EXPECT_EQ (bytes_of_plan (once.plan (request_bytes (), reply_bytes ())),
               reply_bytes ());
}

TEST (LumexBehaviors, CorruptedCrc_RateIsAFractionOfTheFramesBySeed)
{
  virtual_clock clock;
  behavior_applier half (behavior::corrupted_crc (9, 500), clock.source ());
  int corrupted = 0;
  for (int round = 0; round < 400; ++round)
    if (bytes_of_plan (half.plan (request_bytes (), reply_bytes ()))
        != reply_bytes ())
      ++corrupted;
  EXPECT_GT (corrupted, 140);
  EXPECT_LT (corrupted, 260);
}

TEST_F (LumexBehaviorsStreamTest, Oversized_ReplacesTheReplyWithOneBigBlock)
{
  ASSERT_NO_FATAL_FAILURE (start (behavior::oversized (3000, 0x5A)));
  send ();
  run (ms (10));
  EXPECT_EQ (log_->bytes, bytes_t (3000, 0x5A));
}

TEST (LumexBehaviors, Oversized_NeedsAReplyToReplace)
{
  EXPECT_TRUE (
      plan_of (behavior::oversized (100), request_bytes (), bytes_t ())
          .steps.empty ());
  EXPECT_EQ (bytes_of_plan (plan_of (behavior::oversized (100))),
             bytes_t (100, 0xAB));
}

// --- intermittent

TEST (LumexBehaviors, Intermittent_LosesTheFirstReplyOnlyWithOneOutage)
{
  virtual_clock clock;
  behavior_applier applier (behavior::intermittent (1, 1000, 1),
                            clock.source ());
  reply_plan_t const first = applier.plan (request_bytes (), reply_bytes ());
  EXPECT_TRUE (first.steps.empty ());
  EXPECT_TRUE (first.request_processed);
  for (int round = 0; round < 10; ++round)
    EXPECT_EQ (bytes_of_plan (applier.plan (request_bytes (), reply_bytes ())),
               reply_bytes ());
}

TEST (LumexBehaviors, Intermittent_AnOutageSilencesTheRepliesInsideItsWindow)
{
  virtual_clock clock;
  behavior_applier applier (behavior::intermittent (1, 1000, 1, ms (100)),
                            clock.source ());
  EXPECT_TRUE (applier.plan (request_bytes (), reply_bytes ()).steps.empty ());
  clock.advance (ms (50));
  EXPECT_TRUE (applier.plan (request_bytes (), reply_bytes ()).steps.empty ());
  clock.advance (ms (49));
  EXPECT_TRUE (applier.plan (request_bytes (), reply_bytes ()).steps.empty ());
  clock.advance (ms (1));
  EXPECT_FALSE (
      applier.plan (request_bytes (), reply_bytes ()).steps.empty ());
}

TEST (LumexBehaviors, Intermittent_ProbabilityAndLimitFollowTheSeed)
{
  virtual_clock clock;
  behavior_applier limited (behavior::intermittent (4, 500, 5),
                            clock.source ());
  int lost = 0;
  for (int round = 0; round < 400; ++round)
    if (limited.plan (request_bytes (), reply_bytes ()).steps.empty ())
      ++lost;
  EXPECT_EQ (lost, 5);
  behavior_applier unlimited (behavior::intermittent (4, 500, 0),
                              clock.source ());
  lost = 0;
  for (int round = 0; round < 400; ++round)
    if (unlimited.plan (request_bytes (), reply_bytes ()).steps.empty ())
      ++lost;
  EXPECT_GT (lost, 140);
  EXPECT_LT (lost, 260);
  behavior_applier none (behavior::intermittent (4, 0, 0), clock.source ());
  for (int round = 0; round < 50; ++round)
    EXPECT_FALSE (none.plan (request_bytes (), reply_bytes ()).steps.empty ());
}

TEST_F (LumexBehaviorsStreamTest, Intermittent_TheStreamStaysUpBetweenLosses)
{
  ASSERT_NO_FATAL_FAILURE (start (behavior::intermittent (1, 1000, 1)));
  send ();
  run (ms (10));
  EXPECT_TRUE (log_->bytes.empty ());
  send ();
  run (ms (10));
  EXPECT_EQ (log_->bytes, reply_bytes ());
  EXPECT_FALSE (log_->stopped);
}

// --- disconnecting, works_then_disconnects, cable_unplugged

TEST_F (LumexBehaviorsStreamTest,
        Disconnecting_CutsTheReplyAfterTheBytesAndHangsUp)
{
  ASSERT_NO_FATAL_FAILURE (start (behavior::disconnecting (5)));
  send ();
  run (ms (10));
  ASSERT_TRUE (log_->stopped);
  bytes_t const reply = reply_bytes ();
  EXPECT_EQ (log_->bytes, bytes_t (reply.begin (), reply.begin () + 5));
  ASSERT_FALSE (log_->arrivals.empty ());
  EXPECT_EQ (log_->arrivals.back ().error,
             code (transport_errc::disconnected));
}

TEST_F (LumexBehaviorsStreamTest, Disconnecting_HangsUpWithoutTheClockMoving)
{
  ASSERT_NO_FATAL_FAILURE (start (behavior::disconnecting (0)));
  send ();
  EXPECT_TRUE (log_->stopped);
  EXPECT_TRUE (log_->bytes.empty ());
}

TEST_F (LumexBehaviorsStreamTest,
        Disconnecting_CountsBytesAcrossRepliesAndFiresOnce)
{
  ASSERT_NO_FATAL_FAILURE (start (behavior::disconnecting (15)));
  send ();
  run (ms (10));
  EXPECT_FALSE (log_->stopped);
  EXPECT_EQ (log_->bytes, reply_bytes ());
  send ();
  run (ms (10));
  ASSERT_TRUE (log_->stopped);
  EXPECT_EQ (log_->bytes.size (), 15u);
  // The device is plugged back: it answers whole from then on.
  reopen ();
  send ();
  run (ms (10));
  EXPECT_EQ (log_->bytes, reply_bytes ());
  EXPECT_FALSE (log_->stopped);
}

TEST (LumexBehaviors, Disconnecting_OfZeroHangsUpInsteadOfTheFirstReply)
{
  reply_plan_t const plan = plan_of (behavior::disconnecting (0));
  ASSERT_EQ (plan.steps.size (), 1u);
  EXPECT_EQ (plan.steps[0].kind, plan_step_kind::disconnect);
}

TEST (LumexBehaviors, Disconnecting_ExactlyTheReplyLengthHangsUpAfterItWhole)
{
  reply_plan_t const plan = plan_of (behavior::disconnecting (12));
  ASSERT_EQ (plan.steps.size (), 2u);
  EXPECT_EQ (plan.steps[0].bytes, reply_bytes ());
  EXPECT_EQ (plan.steps[1].kind, plan_step_kind::disconnect);
}

TEST (LumexBehaviors, Disconnecting_HangsUpAfterTheLastKeptTrickledByte)
{
  reply_plan_t const plan = plan_of (
      behavior::slow_trickle (ms (10)).then (behavior::disconnecting (3)));
  ASSERT_EQ (plan.steps.size (), 4u);
  EXPECT_EQ (plan.steps[2].at, ms (30));
  EXPECT_EQ (plan.steps[3].kind, plan_step_kind::disconnect);
  EXPECT_EQ (plan.steps[3].at, ms (30));
}

TEST_F (LumexBehaviorsStreamTest, WorksThenDisconnects_AnswersWholeThenHangsUp)
{
  ASSERT_NO_FATAL_FAILURE (start (behavior::works_then_disconnects (2)));
  send ();
  run (ms (10));
  EXPECT_FALSE (log_->stopped);
  send ();
  run (ms (10));
  ASSERT_TRUE (log_->stopped);
  EXPECT_EQ (log_->bytes, concat ({ reply_bytes (), reply_bytes () }));
  EXPECT_EQ (log_->arrivals.back ().error,
             code (transport_errc::disconnected));
  // Plugged back, it works again and does not hang up a second time.
  reopen ();
  send ();
  send ();
  send ();
  run (ms (10));
  EXPECT_EQ (log_->bytes.size (), 36u);
  EXPECT_FALSE (log_->stopped);
}

TEST (LumexBehaviors, WorksThenDisconnects_OfZeroHangsUpWithoutAnyReply)
{
  reply_plan_t const plan = plan_of (behavior::works_then_disconnects (0));
  ASSERT_EQ (plan.steps.size (), 1u);
  EXPECT_EQ (plan.steps[0].kind, plan_step_kind::disconnect);
}

TEST (LumexBehaviors, WorksThenDisconnects_IgnoresRequestsWithoutAReply)
{
  virtual_clock clock;
  behavior_applier applier (behavior::works_then_disconnects (1),
                            clock.source ());
  for (int round = 0; round < 5; ++round)
    EXPECT_TRUE (applier.plan (request_bytes (), bytes_t ()).steps.empty ());
  EXPECT_EQ (applier.plan (request_bytes (), reply_bytes ()).steps.size (),
             2u);
}

TEST_F (LumexBehaviorsStreamTest, CableUnplugged_OpenFailsUntilReplug)
{
  ASSERT_FALSE (open_now ());
  applier_ = std::make_shared<behavior_applier> (
      behavior::cable_unplugged (ms (500)), clock_.source ());
  keep_reading (mock_.stream, clock_, start_, log_);
  drain (context_);
  applier_->attach (mock_.control);
  drain (context_);
  ASSERT_TRUE (log_->stopped);
  EXPECT_EQ (log_->arrivals.back ().error,
             code (transport_errc::disconnected));
  mock_.stream.close ();
  drain (context_);
  EXPECT_EQ (open_now (), code (transport_errc::disconnected));
  run (ms (499));
  EXPECT_EQ (open_now (), code (transport_errc::disconnected));
  run (ms (1));
  EXPECT_FALSE (open_now ());
  EXPECT_EQ (mock_.control.open_count (), 2u);
}

TEST (LumexBehaviors, CableUnplugged_DropsRepliesWhileUnpluggedAndAnswersAfter)
{
  virtual_clock clock;
  behavior_applier applier (behavior::cable_unplugged (ms (200)),
                            clock.source ());
  reply_plan_t const during = applier.plan (request_bytes (), reply_bytes ());
  EXPECT_TRUE (during.steps.empty ());
  EXPECT_FALSE (during.request_processed);
  clock.advance (ms (199));
  EXPECT_TRUE (applier.plan (request_bytes (), reply_bytes ()).steps.empty ());
  clock.advance (ms (1));
  reply_plan_t const after = applier.plan (request_bytes (), reply_bytes ());
  EXPECT_EQ (bytes_of_plan (after), reply_bytes ());
  EXPECT_TRUE (after.request_processed);
}

// --- the order of the steps

TEST (LumexBehaviors,
      GivenStepsOutOfTimeOrder_WhenPlanned_ThenTheyAreSortedStably)
{
  // Repeating puts its copies after the original at a later time; a
  // zero-gap copy keeps the schedule order of the equal times.
  reply_plan_t const plan = plan_of (behavior::repeating (2, ms (0)),
                                     request_bytes (), reply_bytes ());
  ASSERT_EQ (plan.steps.size (), 3u);
  for (std::size_t index = 0; index < 3; ++index)
    EXPECT_EQ (plan.steps[index].at, ms (0));
}

TEST (
    LumexBehaviors,
    GivenLaterCopiesOfFragments_WhenPlanned_ThenTheStepsInterleaveByTimeStably)
{
  bytes_t reply;
  for (std::size_t index = 0; index < 64; ++index)
    reply.push_back (static_cast<std::uint8_t> (index));
  // Piece n at n ms, its copy at n + 1 ms: the copy of piece n and piece n + 1
  // share a time, and the piece (made first) comes first.
  reply_plan_t const plan
      = plan_of (behavior::fragmented (1, 1, ms (1))
                     .then (behavior::repeating (1, ms (1))),
                 request_bytes (), reply);
  ASSERT_EQ (plan.steps.size (), 128u);
  bytes_t expected;
  for (std::size_t time = 0; time <= 64; ++time)
    {
      if (time < 64)
        expected.push_back (reply[time]);
      if (time >= 1)
        expected.push_back (reply[time - 1]);
    }
  EXPECT_EQ (bytes_of_plan (plan), expected);
  for (std::size_t index = 1; index < plan.steps.size (); ++index)
    EXPECT_LE (plan.steps[index - 1].at, plan.steps[index].at);
}

TEST (LumexBehaviors,
      GivenZeroReadOnADatagram_WhenPlanned_ThenThereIsNoEmptyReadStep)
{
  virtual_clock clock;
  behavior_applier applier (behavior::zero_read (3), clock.source ());
  reply_plan_t const datagram
      = applier.plan (request_bytes (), reply_bytes (), true);
  ASSERT_EQ (datagram.steps.size (), 1u);
  EXPECT_EQ (datagram.steps[0].kind, plan_step_kind::bytes);
  reply_plan_t const stream
      = applier.plan (request_bytes (), reply_bytes (), false);
  EXPECT_EQ (stream.steps.size (), 4u);
}

// --- the datagram twin

TEST_F (LumexBehaviorsDatagramTest, Perfect_DeliversOneMessageWithTheReportId)
{
  ASSERT_NO_FATAL_FAILURE (start (behavior::perfect (), 5));
  send ();
  run (ms (10));
  ASSERT_EQ (log_->arrivals.size (), 1u);
  EXPECT_EQ (log_->arrivals[0].report_id, 5);
  EXPECT_EQ (log_->bytes, reply_bytes ());
}

TEST_F (LumexBehaviorsDatagramTest, Fragmented_SplitsTheReplyAcrossDatagrams)
{
  ASSERT_NO_FATAL_FAILURE (start (behavior::fragmented (1, 3, ms (5)), 7));
  send ();
  run (ms (500));
  EXPECT_EQ (log_->bytes, reply_bytes ());
  std::size_t const expected[] = { 3, 2, 1, 3, 1, 2 };
  ASSERT_EQ (log_->arrivals.size (), 6u);
  for (std::size_t index = 0; index < 6; ++index)
    {
      EXPECT_EQ (log_->arrivals[index].count, expected[index]) << index;
      EXPECT_EQ (log_->arrivals[index].report_id, 7) << index;
    }
}

TEST_F (LumexBehaviorsDatagramTest,
        PartialThenFrozen_DeliversATruncatedDatagram)
{
  ASSERT_NO_FATAL_FAILURE (start (behavior::partial_then_frozen (0, 4), 0));
  send ();
  run (ms (10));
  ASSERT_EQ (log_->arrivals.size (), 1u);
  EXPECT_EQ (log_->arrivals[0].count, 4u);
  send ();
  run (ms (100));
  EXPECT_EQ (log_->arrivals.size (), 1u);
}

TEST_F (LumexBehaviorsDatagramTest,
        SlowTrickle_DelaysTheWholeDatagramBySizeTimesPeriod)
{
  ASSERT_NO_FATAL_FAILURE (start (behavior::slow_trickle (ms (10)), 0));
  send ();
  run (ms (119));
  EXPECT_TRUE (log_->arrivals.empty ());
  run (ms (1));
  ASSERT_EQ (log_->arrivals.size (), 1u);
  EXPECT_EQ (log_->arrivals[0].at, ms (120));
  EXPECT_EQ (log_->bytes, reply_bytes ());
}

TEST_F (LumexBehaviorsDatagramTest, ZeroRead_ChangesNothingOnDatagrams)
{
  ASSERT_NO_FATAL_FAILURE (start (behavior::zero_read (3), 0));
  send ();
  run (ms (10));
  ASSERT_EQ (log_->arrivals.size (), 1u);
  EXPECT_EQ (log_->bytes, reply_bytes ());
}

TEST_F (LumexBehaviorsDatagramTest, Echo_ReturnsTheRequestPayload)
{
  ASSERT_NO_FATAL_FAILURE (start (behavior::echo (), 2));
  send ();
  run (ms (10));
  EXPECT_EQ (log_->bytes, request_bytes ());
  ASSERT_EQ (log_->arrivals.size (), 1u);
  EXPECT_EQ (log_->arrivals[0].report_id, 2);
}

TEST_F (LumexBehaviorsDatagramTest,
        WorksThenDisconnects_EndsTheReceivesAfterTheReplies)
{
  ASSERT_NO_FATAL_FAILURE (start (behavior::works_then_disconnects (1), 0));
  send ();
  run (ms (10));
  ASSERT_TRUE (log_->stopped);
  EXPECT_EQ (log_->bytes, reply_bytes ());
  EXPECT_EQ (log_->arrivals.back ().error,
             code (transport_errc::disconnected));
}

TEST_F (LumexBehaviorsDatagramTest, CableUnplugged_OpenFailsUntilReplug)
{
  ASSERT_FALSE (open_now ());
  applier_ = std::make_shared<behavior_applier> (
      behavior::cable_unplugged (ms (300)), clock_.source ());
  keep_receiving (mock_.transport, clock_, start_, log_);
  drain (context_);
  applier_->attach (mock_.control);
  drain (context_);
  ASSERT_TRUE (log_->stopped);
  mock_.transport.close ();
  drain (context_);
  EXPECT_EQ (open_now (), code (transport_errc::disconnected));
  run (ms (300));
  EXPECT_FALSE (open_now ());
}

// --- the expected outcome

namespace
{
row_shape_t
shape_of (reply_shape reply, std::size_t retries = 0, bool idempotent = false,
          bool reconnects = false, bool watchdog = false)
{
  row_shape_t shape;
  shape.reply = reply;
  shape.retries = retries;
  shape.idempotent = idempotent;
  shape.reconnects = reconnects;
  shape.watchdog = watchdog;
  return shape;
}

outcome_class
outcome (behavior_kind kind, row_shape_t const &shape)
{
  return tr::testing::expected_outcome (kind, shape);
}
} // namespace

TEST (LumexBehaviorsOutcome,
      GivenARowWithOneReply_WhenEachBehaviorIsAsked_ThenItIsTheTableClass)
{
  row_shape_t const plain = shape_of (reply_shape::one);
  row_shape_t const retried = shape_of (reply_shape::one, 2, true);
  struct row_t
  {
    behavior_kind kind;
    outcome_class plain;
    outcome_class retried;
  };
  row_t const rows[] = {
    { behavior_kind::perfect, outcome_class::success, outcome_class::success },
    { behavior_kind::echo, outcome_class::protocol_error,
      outcome_class::protocol_error },
    { behavior_kind::silent, outcome_class::timeout,
      outcome_class::retries_exhausted },
    { behavior_kind::frozen, outcome_class::timeout,
      outcome_class::retries_exhausted },
    { behavior_kind::partial_then_frozen, outcome_class::timeout,
      outcome_class::retries_exhausted },
    { behavior_kind::slow_trickle, outcome_class::success,
      outcome_class::success },
    { behavior_kind::fragmented, outcome_class::success,
      outcome_class::success },
    { behavior_kind::flood, outcome_class::dropped_counted,
      outcome_class::dropped_counted },
    { behavior_kind::repeating, outcome_class::dropped_counted,
      outcome_class::dropped_counted },
    { behavior_kind::garbage, outcome_class::dropped_counted,
      outcome_class::dropped_counted },
    { behavior_kind::corrupted_crc, outcome_class::timeout,
      outcome_class::success_after_retries },
    { behavior_kind::oversized, outcome_class::timeout,
      outcome_class::retries_exhausted },
    { behavior_kind::zero_read, outcome_class::success,
      outcome_class::success },
    { behavior_kind::intermittent, outcome_class::timeout,
      outcome_class::success_after_retries },
    { behavior_kind::disconnecting, outcome_class::link_lost,
      outcome_class::link_lost },
    { behavior_kind::works_then_disconnects, outcome_class::link_lost,
      outcome_class::link_lost },
    { behavior_kind::cable_unplugged, outcome_class::link_lost,
      outcome_class::link_lost },
    { behavior_kind::malicious, outcome_class::dropped_counted,
      outcome_class::dropped_counted },
  };
  EXPECT_EQ (sizeof (rows) / sizeof (rows[0]), 18u);
  for (std::size_t index = 0; index < 18; ++index)
    {
      EXPECT_EQ (outcome (rows[index].kind, plain), rows[index].plain)
          << tr::testing::behavior_kind_name (rows[index].kind);
      EXPECT_EQ (outcome (rows[index].kind, retried), rows[index].retried)
          << tr::testing::behavior_kind_name (rows[index].kind);
    }
}

TEST (
    LumexBehaviorsOutcome,
    GivenARowWithAReply_WhenTheLinkIsCut_ThenAReconnectResendsOnlyIdempotentRows)
{
  behavior_kind const cuts[]
      = { behavior_kind::disconnecting, behavior_kind::works_then_disconnects,
          behavior_kind::cable_unplugged };
  for (std::size_t index = 0; index < 3; ++index)
    {
      EXPECT_EQ (
          outcome (cuts[index], shape_of (reply_shape::one, 0, true, true)),
          outcome_class::success_after_retries);
      EXPECT_EQ (
          outcome (cuts[index], shape_of (reply_shape::one, 0, false, true)),
          outcome_class::link_lost);
      EXPECT_EQ (
          outcome (cuts[index], shape_of (reply_shape::one, 0, true, false)),
          outcome_class::link_lost);
      EXPECT_EQ (outcome (cuts[index],
                          shape_of (reply_shape::dialogue_ack, 0, true, true)),
                 outcome_class::success_after_retries);
    }
}

TEST (LumexBehaviorsOutcome,
      GivenADialogueAck_WhenAsked_ThenItIsTheClassOfAOneReplyRow)
{
  std::vector<behavior_kind> const kinds = tr::testing::all_behavior_kinds ();
  for (std::size_t index = 0; index < kinds.size (); ++index)
    for (std::size_t retries = 0; retries < 3; ++retries)
      EXPECT_EQ (outcome (kinds[index], shape_of (reply_shape::dialogue_ack,
                                                  retries, true, true)),
                 outcome (kinds[index],
                          shape_of (reply_shape::one, retries, true, true)))
          << tr::testing::behavior_kind_name (kinds[index]);
}

TEST (
    LumexBehaviorsOutcome,
    GivenARowWithoutAReply_WhenAsked_ThenOnlyInjectedBytesAndAnUnpluggedCableShow)
{
  row_shape_t const none = shape_of (reply_shape::none);
  behavior_kind const dropped[]
      = { behavior_kind::echo, behavior_kind::flood, behavior_kind::garbage,
          behavior_kind::malicious };
  for (std::size_t index = 0; index < 4; ++index)
    EXPECT_EQ (outcome (dropped[index], none), outcome_class::dropped_counted)
        << tr::testing::behavior_kind_name (dropped[index]);
  behavior_kind const quiet[] = { behavior_kind::perfect,
                                  behavior_kind::silent,
                                  behavior_kind::frozen,
                                  behavior_kind::partial_then_frozen,
                                  behavior_kind::slow_trickle,
                                  behavior_kind::fragmented,
                                  behavior_kind::repeating,
                                  behavior_kind::corrupted_crc,
                                  behavior_kind::oversized,
                                  behavior_kind::zero_read,
                                  behavior_kind::intermittent,
                                  behavior_kind::disconnecting,
                                  behavior_kind::works_then_disconnects };
  for (std::size_t index = 0; index < 13; ++index)
    EXPECT_EQ (outcome (quiet[index], none), outcome_class::success)
        << tr::testing::behavior_kind_name (quiet[index]);
  EXPECT_EQ (outcome (behavior_kind::cable_unplugged, none),
             outcome_class::link_lost);
  EXPECT_EQ (outcome (behavior_kind::cable_unplugged,
                      shape_of (reply_shape::none, 0, true, true)),
             outcome_class::success_after_retries);
}

TEST (LumexBehaviorsOutcome,
      GivenAStream_WhenAsked_ThenTheWatchdogAndTheReconnectDecide)
{
  row_shape_t const plain = shape_of (reply_shape::stream);
  row_shape_t const watched
      = shape_of (reply_shape::stream, 0, false, false, true);
  row_shape_t const resumed = shape_of (reply_shape::stream, 0, false, true);
  behavior_kind const silent_kinds[]
      = { behavior_kind::silent, behavior_kind::frozen,
          behavior_kind::partial_then_frozen, behavior_kind::oversized };
  for (std::size_t index = 0; index < 4; ++index)
    {
      EXPECT_EQ (outcome (silent_kinds[index], plain), outcome_class::timeout);
      EXPECT_EQ (outcome (silent_kinds[index], watched),
                 outcome_class::link_lost);
    }
  behavior_kind const counted[]
      = { behavior_kind::intermittent, behavior_kind::corrupted_crc,
          behavior_kind::garbage,      behavior_kind::flood,
          behavior_kind::repeating,    behavior_kind::malicious };
  for (std::size_t index = 0; index < 6; ++index)
    EXPECT_EQ (outcome (counted[index], plain),
               outcome_class::dropped_counted);
  behavior_kind const clean[]
      = { behavior_kind::perfect, behavior_kind::slow_trickle,
          behavior_kind::fragmented, behavior_kind::zero_read };
  for (std::size_t index = 0; index < 4; ++index)
    EXPECT_EQ (outcome (clean[index], plain), outcome_class::success);
  EXPECT_EQ (outcome (behavior_kind::echo, plain),
             outcome_class::protocol_error);
  behavior_kind const cuts[]
      = { behavior_kind::disconnecting, behavior_kind::works_then_disconnects,
          behavior_kind::cable_unplugged };
  for (std::size_t index = 0; index < 3; ++index)
    {
      EXPECT_EQ (outcome (cuts[index], plain), outcome_class::link_lost);
      EXPECT_EQ (outcome (cuts[index], resumed),
                 outcome_class::success_after_retries);
    }
}

// --- threads

TEST (LumexBehaviors,
      GivenSeveralThreads_WhenOneApplierIsPlanned_ThenTheCountIsExact)
{
  virtual_clock clock;
  behavior_applier applier (behavior::frozen (100), clock.source ());
  std::mutex mutex;
  std::size_t answered = 0;
  std::vector<std::thread> threads;
  for (int index = 0; index < 4; ++index)
    threads.push_back (std::thread (
        [&applier, &mutex, &answered]
          {
            for (int round = 0; round < 100; ++round)
              {
                reply_plan_t const plan
                    = applier.plan (request_bytes (), reply_bytes ());
                if (!plan.steps.empty ())
                  {
                    std::lock_guard<std::mutex> const lock (mutex);
                    ++answered;
                  }
              }
          }));
  for (std::size_t index = 0; index < threads.size (); ++index)
    threads[index].join ();
  EXPECT_EQ (answered, 100u);
}

TEST (LumexBehaviors, GivenAReset_WhenPlanned_ThenTheStatesStartOver)
{
  virtual_clock clock;
  behavior_applier applier (behavior::frozen (1), clock.source ());
  EXPECT_FALSE (
      applier.plan (request_bytes (), reply_bytes ()).steps.empty ());
  EXPECT_TRUE (applier.plan (request_bytes (), reply_bytes ()).steps.empty ());
  applier.reset ();
  EXPECT_FALSE (
      applier.plan (request_bytes (), reply_bytes ()).steps.empty ());
  EXPECT_EQ (applier.get ().kind (), behavior_kind::frozen);
}
