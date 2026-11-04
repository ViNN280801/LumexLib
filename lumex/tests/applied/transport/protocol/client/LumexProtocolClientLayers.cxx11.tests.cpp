// identify () and the layer selection of the generated client: the identity,
// the session, the layer the device names, as<L> (), what follows the layer in
// the dialogue. The synthetic mixer table.

#include <cstdint>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/tests/applied/transport/protocol/client/LumexClientTestSupport.hpp"

namespace
{
using namespace lumex_client_test;

/// Runs identify () on `bench` to its end.
template <typename Bench>
std::shared_ptr<outcome_t<pr::identity_t>>
identify (Bench &bench)
{
  std::shared_ptr<outcome_t<pr::identity_t>> const out
      = std::make_shared<outcome_t<pr::identity_t>> ();
  record_t<pr::identity_t> done = { out };
  bench.client->async_identify (done);
  EXPECT_TRUE (bench.run_until ([&] () { return out->calls != 0; }));
  EXPECT_EQ (out->calls, 1);
  return out;
}
} // namespace

TEST (LumexProtocolClientLayers, Client_IdentifySelectsLayer)
{
  log_capture_t const log;
  client_bench_t<mixer_v2, mixer> bench;
  bench.device->set_reply<mixer_v2::INFO_t> (mixer_info ("M200", 15, 2));
  EXPECT_STREQ (bench.client->engine ().index ().protocol_name (), "mixer_t");
  // The handler of identify already sees the layer in the dialogue too.
  std::string layer_in_handler;
  auto const out = std::make_shared<outcome_t<pr::identity_t>> ();
  bench.client->async_identify (
      [&] (std::error_code error, pr::identity_t identity)
        {
          layer_in_handler = bench.client->engine ().index ().protocol_name ();
          record_t<pr::identity_t> done = { out };
          done (error, std::move (identity));
        });
  ASSERT_TRUE (bench.run_until ([&] () { return out->calls != 0; }));
  EXPECT_EQ (layer_in_handler, "mixer_v2_t");
  ASSERT_FALSE (out->error) << out->error.message ();
  EXPECT_EQ (out->value.model, "M200");
  EXPECT_EQ (out->value.firmware, 15u);
  EXPECT_EQ (out->value.layer, "mixer_v2_t");
  index::session_context_t const session = bench.client->session ();
  EXPECT_EQ (session.model (), "M200");
  EXPECT_EQ (session.firmware (), 15u);
  EXPECT_EQ (session.layer (), "mixer_v2_t");
  EXPECT_STREQ (bench.client->engine ().index ().protocol_name (),
                "mixer_v2_t");
  bench.device->set_reply<mixer_v2::ONLY_V2_t> (std::uint16_t (500));
  auto const only = new_outcome<mixer_v2::ONLY_V2_t> ();
  bench.client->as<mixer_v2> ().ONLY_V2 (std::uint8_t (1),
                                         into<mixer_v2::ONLY_V2_t> (only));
  ASSERT_TRUE (bench.run_until ([&] () { return only->calls != 0; }));
  ASSERT_FALSE (only->error) << only->error.message ();
  EXPECT_EQ (only->value, 500);
  // Logs and describe use the layer found.
  EXPECT_TRUE (log_capture_t::any_holds ("mixer_v2_t.ONLY_V2"));
}

TEST (LumexProtocolClientLayers, Client_IdentifySelectsTheFirstLayerToo)
{
  // Falsified by a choice by order: the other layer of the set.
  client_bench_t<mixer_v1, mixer> bench;
  bench.device->set_reply<mixer_v1::INFO_t> (mixer_info ("M100", 3, 1));
  auto const out = identify (bench);
  ASSERT_FALSE (out->error) << out->error.message ();
  EXPECT_EQ (out->value.layer, "mixer_v1_t");
  bench.settle ();
  EXPECT_STREQ (bench.client->engine ().index ().protocol_name (),
                "mixer_v1_t");
  auto const other = new_outcome<mixer_v2::ONLY_V2_t> ();
  bench.client->as<mixer_v2> ().ONLY_V2 (std::uint8_t (1),
                                         into<mixer_v2::ONLY_V2_t> (other));
  bench.settle ();
  EXPECT_EQ (other->error, protocol_errc::not_supported);
  // A row of both layers: only the identified layer opens as<>.
  auto const shared = new_outcome<mixer_v2::RATE_t> ();
  bench.client->as<mixer_v2> ().RATE (std::uint8_t (1),
                                      into<mixer_v2::RATE_t> (shared));
  bench.settle ();
  EXPECT_EQ (shared->error, protocol_errc::not_supported);
  bench.device->set_reply<mixer_v1::ONLY_V1_t> (std::uint8_t (4));
  auto const mine = new_outcome<mixer_v1::ONLY_V1_t> ();
  bench.client->as<mixer_v1> ().ONLY_V1 (into<mixer_v1::ONLY_V1_t> (mine));
  ASSERT_TRUE (bench.run_until ([&] () { return mine->calls != 0; }));
  EXPECT_FALSE (mine->error) << mine->error.message ();
  EXPECT_EQ (mine->value, 4);
}

TEST (LumexProtocolClientLayers, Client_ADeviceThatNamesNoLayerChangesNothing)
{
  client_bench_t<mixer, mixer> bench;
  bench.device->set_reply<mixer::INFO_t> (mixer_info ("M900", 7, 9));
  auto const out = identify (bench);
  EXPECT_EQ (out->error, protocol_errc::not_supported);
  // What it said reaches the caller; the session and the layer stay.
  EXPECT_EQ (out->value.model, "M900");
  bench.settle ();
  index::session_context_t const session = bench.client->session ();
  EXPECT_TRUE (session.model ().empty ());
  EXPECT_EQ (session.firmware (), 0u);
  EXPECT_TRUE (session.layer ().empty ());
  EXPECT_STREQ (bench.client->engine ().index ().protocol_name (), "mixer_t");
  auto const as = new_outcome<mixer_v1::RATE_t> ();
  bench.client->as<mixer_v1> ().RATE (std::uint8_t (1),
                                      into<mixer_v1::RATE_t> (as));
  bench.settle ();
  EXPECT_EQ (as->error, protocol_errc::not_supported);
}

TEST (LumexProtocolClientLayers, Client_IdentifyOverALostLink)
{
  client_bench_t<mixer, mixer> bench;
  bench.device->set_reply<mixer::INFO_t> (mixer_info ("M100", 3, 1));
  bench.device->set_behavior (tr::testing::behavior::silent ());
  bench.control->fail_next_read (
      make_error_code (transport_errc::disconnected));
  auto const out = identify (bench);
  EXPECT_TRUE (out->error);
  EXPECT_TRUE (bench.client->session ().layer ().empty ());
  EXPECT_STREQ (bench.client->engine ().index ().protocol_name (), "mixer_t");
  // The link came back: identify works.
  bench.device->set_behavior (tr::testing::behavior::perfect ());
  auto const again = identify (bench);
  EXPECT_FALSE (again->error) << again->error.message ();
  EXPECT_EQ (again->value.layer, "mixer_v1_t");
}

TEST (LumexProtocolClientLayers, Client_ACancellationSlotEndsIdentify)
{
  client_bench_t<mixer, mixer> bench;
  bench.device->set_behavior (tr::testing::behavior::silent ());
  asio::cancellation_signal signal;
  auto const out = std::make_shared<outcome_t<pr::identity_t>> ();
  record_t<pr::identity_t> done = { out };
  bench.client->async_identify (
      asio::bind_cancellation_slot (signal.slot (), done));
  bench.settle ();
  EXPECT_EQ (out->calls, 0);
  signal.emit (asio::cancellation_type::terminal);
  bench.settle ();
  ASSERT_EQ (out->calls, 1);
  EXPECT_EQ (out->error, transport_errc::cancelled);
  EXPECT_TRUE (bench.client->session ().layer ().empty ());
}

TEST (LumexProtocolClientLayers, Client_AsBeforeIdentifyIsNotSupported)
{
  client_bench_t<mixer_v1, mixer> bench;
  auto const out = new_outcome<mixer_v1::ONLY_V1_t> ();
  bench.client->as<mixer_v1> ().ONLY_V1 (into<mixer_v1::ONLY_V1_t> (out));
  EXPECT_EQ (out->calls, 0);
  bench.settle ();
  ASSERT_EQ (out->calls, 1);
  EXPECT_EQ (out->error, protocol_errc::not_supported);
  EXPECT_EQ (bench.requests (), 0u);
  // Its own layer through as<> too: nothing was identified yet.
  auto const own = new_outcome<mixer::RATE_t> ();
  bench.client->as<mixer> ().RATE (std::uint8_t (1),
                                   into<mixer::RATE_t> (own));
  bench.settle ();
  EXPECT_EQ (own->error, protocol_errc::not_supported);
  EXPECT_EQ (bench.requests (), 0u);
}

TEST (LumexProtocolClientLayers, Client_AsABaseOfTheIdentifiedLayerWorks)
{
  client_bench_t<mixer_v2, mixer> bench;
  bench.device->set_reply<mixer_v2::INFO_t> (mixer_info ("M200", 15, 2));
  EXPECT_FALSE (identify (bench)->error);
  bench.device->set_reply<mixer_v2::RATE_t> (std::uint8_t (6));
  auto const out = new_outcome<mixer::RATE_t> ();
  bench.client->as<mixer> ().RATE (std::uint8_t (1),
                                   into<mixer::RATE_t> (out));
  ASSERT_TRUE (bench.run_until ([&] () { return out->calls != 0; }));
  EXPECT_FALSE (out->error) << out->error.message ();
  EXPECT_EQ (out->value, 6);
}

TEST (LumexProtocolClientLayers, Client_ARowTheLayerReplacedIsNotSentByTheBase)
{
  client_bench_t<mixer_v2, mixer> bench;
  bench.device->set_reply<mixer_v2::INFO_t> (mixer_info ("M200", 15, 2));
  // Before identify the base row goes out, with its own key.
  bench.device->set_behavior (tr::testing::behavior::silent ());
  EXPECT_TRUE (identify (bench)->error);
  bench.device->set_behavior (tr::testing::behavior::perfect ());
  EXPECT_FALSE (identify (bench)->error);
  std::uint64_t const sent = bench.requests ();
  auto const base = new_outcome<mixer::PING_t> ();
  bench.client->PING (into<mixer::PING_t> (base));
  bench.settle ();
  ASSERT_EQ (base->calls, 1);
  EXPECT_EQ (base->error, protocol_errc::not_supported);
  EXPECT_EQ (bench.requests (), sent);
  // The layer's row, its key and its type.
  bench.device->set_reply<mixer_v2::PING_t> (std::uint16_t (4000));
  auto const layer = new_outcome<mixer_v2::PING_t> ();
  bench.client->as<mixer_v2> ().PING (into<mixer_v2::PING_t> (layer));
  ASSERT_TRUE (bench.run_until ([&] () { return layer->calls != 0; }));
  ASSERT_FALSE (layer->error) << layer->error.message ();
  EXPECT_EQ (layer->value, 4000);
}

TEST (LumexProtocolClientLayers, Client_AReplyDecodesWithTheSessionOfIdentify)
{
  // The reply of SHAPE depends on a value the identity gives.
  client_bench_t<mixer_v1, mixer> bench;
  bench.device->set_reply<mixer_v1::INFO_t> (mixer_info ("M100", 3, 1, 1));
  std::vector<std::uint8_t> const wide = { 0x23, 0x34, 0x12 };
  std::vector<std::uint8_t> wire;
  ASSERT_TRUE (
      fr::slip::slip_codec ()
          .encode (layout::bytes_view_t (wide.data (), wide.size ()), wire)
          .has_value ());
  tr::testing::mock_stream_control *const control = bench.control.get ();
  // A watching handler: the device has no session value to make its default
  // reply with, so the only reply is this frame.
  bench.device->on<mixer_v1::SHAPE_t> (
      [control, wire] (layout::request_of<mixer_v1::SHAPE_t>::type const &)
        { control->feed (wire); });
  auto const before = new_outcome<mixer::SHAPE_t> ();
  bench.client->SHAPE (into<mixer::SHAPE_t> (before));
  ASSERT_TRUE (bench.run_until ([&] () { return before->calls != 0; }));
  // The form is not known yet: not the timeout, the decoder.
  EXPECT_EQ (before->error, protocol_errc::unknown_case);
  auto const out = identify (bench);
  ASSERT_FALSE (out->error) << out->error.message ();
  std::int64_t form = 0;
  ASSERT_TRUE (bench.client->session ().find ("mixer_form", form));
  EXPECT_EQ (form, 1);
  auto const after = new_outcome<mixer::SHAPE_t> ();
  bench.client->SHAPE (into<mixer::SHAPE_t> (after));
  ASSERT_TRUE (bench.run_until ([&] () { return after->calls != 0; }));
  ASSERT_FALSE (after->error) << after->error.message ();
  EXPECT_EQ (field::get<0> (after->value).discriminant, 1);
}

TEST (LumexProtocolClientLayers, Client_TheFirmwareOfTheIdentityGatesTheRows)
{
  client_bench_t<mixer_v1, mixer> bench;
  bench.device->set_reply<mixer_v1::OLD_t> (std::uint8_t (1));
  bench.device->set_reply<mixer_v1::NEW_t> (std::uint8_t (2));
  // Not identified yet: every row may be there.
  auto const unknown = new_outcome<mixer::NEW_t> ();
  bench.client->NEW (into<mixer::NEW_t> (unknown));
  ASSERT_TRUE (bench.run_until ([&] () { return unknown->calls != 0; }));
  EXPECT_FALSE (unknown->error) << unknown->error.message ();
  bench.device->set_reply<mixer_v1::INFO_t> (mixer_info ("M100", 5, 1));
  EXPECT_FALSE (identify (bench)->error);
  std::uint64_t const sent = bench.requests ();
  auto const missing = new_outcome<mixer::NEW_t> ();
  bench.client->NEW (into<mixer::NEW_t> (missing));
  bench.settle ();
  ASSERT_EQ (missing->calls, 1);
  EXPECT_EQ (missing->error, protocol_errc::not_supported);
  EXPECT_EQ (bench.requests (), sent);
  auto const there = new_outcome<mixer::OLD_t> ();
  bench.client->OLD (into<mixer::OLD_t> (there));
  ASSERT_TRUE (bench.run_until ([&] () { return there->calls != 0; }));
  EXPECT_FALSE (there->error) << there->error.message ();
}

TEST (LumexProtocolClientLayers, Client_IdentifyKeepsTheValuesItDoesNotName)
{
  client_bench_t<mixer_v1, mixer> bench;
  index::session_context_t session;
  session.set ("mine", 12);
  bench.client->set_session (session);
  bench.device->set_reply<mixer_v1::INFO_t> (mixer_info ("M100", 3, 1, 1));
  EXPECT_FALSE (identify (bench)->error);
  std::int64_t value = 0;
  EXPECT_TRUE (bench.client->session ().find ("mine", value));
  EXPECT_EQ (value, 12);
  EXPECT_EQ (bench.client->session ().layer (), "mixer_v1_t");
  // set_session keeps the layer.
  bench.client->set_session (index::session_context_t ());
  EXPECT_EQ (bench.client->session ().layer (), "mixer_v1_t");
  EXPECT_FALSE (bench.client->session ().find ("mine", value));
}

TEST (LumexProtocolClientLayers, Client_WithoutALayerSetTheClientKeepsItsLayer)
{
  // identify_with without layer_set: what the device says of a layer counts
  // for nothing, and as<own layer> opens.
  using solo = tables::solo_t;
  client_bench_t<solo> bench;
  bench.device->set_reply<solo::ASK_t> (std::uint8_t (2));
  auto const closed = new_outcome<solo::ASK_t> ();
  bench.client->as<solo> ().ASK (into<solo::ASK_t> (closed));
  bench.settle ();
  EXPECT_EQ (closed->error, protocol_errc::not_supported);
  auto const out = identify (bench);
  ASSERT_FALSE (out->error) << out->error.message ();
  EXPECT_EQ (out->value.layer, "solo_t");
  EXPECT_EQ (bench.client->session ().layer (), "solo_t");
  auto const open = new_outcome<solo::ASK_t> ();
  bench.client->as<solo> ().ASK (into<solo::ASK_t> (open));
  ASSERT_TRUE (bench.run_until ([&] () { return open->calls != 0; }));
  EXPECT_FALSE (open->error) << open->error.message ();
  EXPECT_EQ (open->value, 2);
}

TEST (LumexProtocolClientLayers,
      Client_OptionsOfTheApplicationStayForTheSameLayer)
{
  pr::client_options_t options = pr::client_options_for<mixer_v1> ();
  options.dialogue.defaults.retries = 2;
  client_bench_t<mixer_v1, mixer_v1> bench (options);
  bench.device->set_reply<mixer_v1::INFO_t> (mixer_info ("M100", 3, 1));
  EXPECT_FALSE (identify (bench)->error);
  bench.device->set_behavior (tr::testing::behavior::silent ());
  std::uint64_t const before = bench.client->statistics ().requests;
  auto const out = new_outcome<mixer_v1::RATE_t> ();
  bench.client->RATE (std::uint8_t (1), into<mixer_v1::RATE_t> (out));
  ASSERT_TRUE (bench.run_until ([&] () { return out->calls != 0; }));
  EXPECT_EQ (bench.client->statistics ().requests - before, 3u);
}

TEST (LumexProtocolClientLayers, Client_TheFeaturesFollowTheLayer)
{
  using plain = tables::plain_t;
  using featured = tables::featured_t;
  client_bench_t<plain, plain> bench;
  bench.advance (ms (1000));
  // The plain layer names no keepalive, no state, no watchdog.
  EXPECT_EQ (bench.client->statistics ().keepalives, 0u);
  EXPECT_EQ (bench.client->statistics ().watchdog_expiries, 0u);
  EXPECT_FALSE (
      bench.client->engine ().set_keepalive (pr::keepalive_generator_t ()));
  pr::reply_of<plain::WHO_t> who;
  field::get<0> (who) = 1;
  bench.device->set_reply<plain::WHO_t> (who);
  auto const out = identify (bench);
  ASSERT_FALSE (out->error) << out->error.message ();
  EXPECT_EQ (out->value.layer, "featured_t");
  // The keepalive of the layer runs, and answers keep the watchdog away.
  bench.advance (ms (250));
  EXPECT_EQ (bench.client->statistics ().keepalives, 2u);
  EXPECT_TRUE (
      bench.client->engine ().set_keepalive (pr::keepalive_generator_t ()));
  EXPECT_FALSE (bench.client->engine ().state_known ());
  // The row of state_from of the layer sets the state.
  std::vector<std::uint8_t> const state = { 0x03, 0x05 };
  std::vector<std::uint8_t> wire;
  ASSERT_TRUE (
      fr::slip::slip_codec ()
          .encode (layout::bytes_view_t (state.data (), state.size ()), wire)
          .has_value ());
  bench.control->feed (wire);
  bench.settle ();
  EXPECT_TRUE (bench.client->engine ().state_known ());
  EXPECT_EQ (bench.client->engine ().last_state (), 5);
  // The rate limit of the layer: the second request of two waits.
  std::uint64_t const delayed = bench.client->statistics ().rate_limited;
  auto const first_ask = new_outcome<featured::ASK_t> ();
  auto const second_ask = new_outcome<featured::ASK_t> ();
  bench.client->ASK (into<featured::ASK_t> (first_ask));
  bench.settle ();
  bench.client->ASK (into<featured::ASK_t> (second_ask));
  ASSERT_TRUE (bench.run_until ([&] () { return second_ask->calls != 0; }));
  EXPECT_GT (bench.client->statistics ().rate_limited, delayed);
  // The default timeout and retries of the layer: 150 ms, one more attempt.
  bench.device->set_behavior (tr::testing::behavior::silent ());
  auto const ask = new_outcome<featured::ASK_t> ();
  bench.client->ASK (into<featured::ASK_t> (ask));
  // The rate limit may delay each attempt a little.
  bench.advance (ms (299));
  EXPECT_EQ (ask->calls, 0);
  bench.advance (ms (100));
  ASSERT_EQ (ask->calls, 1);
  EXPECT_EQ (ask->error, protocol_errc::retries_exhausted);
  // The watchdog of the layer ends a device gone silent.
  bench.advance (ms (2000));
  EXPECT_EQ (bench.client->statistics ().watchdog_expiries, 1u);
}

TEST (LumexProtocolClientLayers, Client_ASubscriberFollowsTheLayer)
{
  client_bench_t<mixer_v2, mixer> bench;
  std::vector<int> levels;
  bench.client->on<mixer::LEVEL_t> (
      [&levels] (pr::reply_of<mixer::LEVEL_t> const &value)
        { levels.push_back (field::get<1> (value)); });
  bench.device->set_reply<mixer_v2::INFO_t> (mixer_info ("M200", 15, 2));
  EXPECT_FALSE (identify (bench)->error);
  bench.settle ();
  pr::reply_of<mixer::LEVEL_t> level;
  field::get<1> (level) = 3;
  EXPECT_TRUE (bench.device->start_stream<mixer_v2::LEVEL_t> (level));
  bench.advance (ms (150));
  EXPECT_EQ (levels.size (), 1u);
}

TEST (LumexProtocolClientLayers,
      Client_ASubscriberOfARowTheLayerReplacedWarnsOnce)
{
  // PING of the base has another key in mixer_v2: the row of the layer found
  // has the identifier but not the layout the subscriber decodes with. It
  // hears nothing, and the log says so once, not with every frame.
  log_capture_t const log;
  client_bench_t<mixer_v2, mixer> bench;
  int heard = 0;
  pr::subscription_t const made = bench.client->on<mixer::PING_t> (
      [&heard] (pr::reply_of<mixer::PING_t> const &) { ++heard; });
  EXPECT_NE (made.id, 0u);
  bench.device->set_reply<mixer_v2::INFO_t> (mixer_info ("M200", 15, 2));
  EXPECT_FALSE (identify (bench)->error);
  bench.settle ();
  for (int i = 0; i < 3; ++i)
    {
      bench.control->feed (wire_of ({ 0x42, 0x01, 0x00 }));
      bench.settle ();
    }
  EXPECT_EQ (heard, 0);
  EXPECT_EQ (log_capture_t::count_holding ("a frame of PING does not decode"),
             1u);
}

TEST (LumexProtocolClientLayers, Client_AsOnGivesARowOfTheLayerAfterIdentify)
{
  client_bench_t<mixer_v2, mixer> bench;
  int heard = 0;
  // Before identify the index has not the row: refused.
  pr::subscription_t const refused
      = bench.client->as<mixer_v2> ().on<mixer_v2::ONLY_V2_t> (
          [&heard] (pr::reply_of<mixer_v2::ONLY_V2_t> const &) { ++heard; });
  EXPECT_EQ (refused.id, 0u);
  bench.device->set_reply<mixer_v2::INFO_t> (mixer_info ("M200", 15, 2));
  EXPECT_FALSE (identify (bench)->error);
  bench.settle ();
  pr::subscription_t const made
      = bench.client->as<mixer_v2> ().on<mixer_v2::ONLY_V2_t> (
          [&heard] (pr::reply_of<mixer_v2::ONLY_V2_t> const &) { ++heard; });
  EXPECT_NE (made.id, 0u);
  std::vector<std::uint8_t> const frame = { 0x41, 0x01, 0x02 };
  std::vector<std::uint8_t> wire;
  ASSERT_TRUE (
      fr::slip::slip_codec ()
          .encode (layout::bytes_view_t (frame.data (), frame.size ()), wire)
          .has_value ());
  bench.control->feed (wire);
  bench.settle ();
  EXPECT_EQ (heard, 1);
}

// --- select_layer on the engine: what is under way follows the layer ---

namespace
{
using engine_bench_t = client_bench_t<mixer_v2, mixer>;

index::protocol_index_t const &
index_of_v1 ()
{
  return *index::protocol_index<mixer_v1> ();
}

index::protocol_index_t const &
index_of_v2 ()
{
  return *index::protocol_index<mixer_v2> ();
}

template <typename P, typename Row, typename... Args>
pr::exchange_t
exchange_of (Args const &...args)
{
  pr::exchange_t out;
  EXPECT_TRUE (
      (layout::encode_message<P, Row> (out.message, args...).has_value ()));
  out.row = index::protocol_index<P> ()->find_name (Row::identifier ());
  out.row_policy = true;
  return out;
}
} // namespace

TEST (LumexDialogueSelectLayer, GivenAQueuedRowTheLayerHasNot_ThenItEnds)
{
  engine_bench_t bench;
  pr::dialogue_engine &engine = bench.client->engine ();
  engine.select_layer (index_of_v1 (), dialogue::features_of<mixer_v1> ());
  bench.settle ();
  bench.device->set_behavior (tr::testing::behavior::silent ());
  // In flight: an exchange of a row of the first layer; queued behind it
  // another, and a row of the base.
  std::vector<std::shared_ptr<outcome_t<pr::reply_frame_t>>> outs;
  for (int i = 0; i < 3; ++i)
    outs.push_back (std::make_shared<outcome_t<pr::reply_frame_t>> ());
  record_t<pr::reply_frame_t> first = { outs[0] };
  record_t<pr::reply_frame_t> second = { outs[1] };
  record_t<pr::reply_frame_t> third = { outs[2] };
  engine.async_exchange (exchange_of<mixer_v1, mixer_v1::ONLY_V1_t> (), first);
  engine.async_exchange (exchange_of<mixer_v1, mixer_v1::ONLY_V1_t> (),
                         second);
  engine.async_exchange (exchange_of<mixer, mixer::RUN_t> (std::uint8_t (1)),
                         third);
  bench.settle ();
  EXPECT_EQ (outs[0]->calls, 0);
  engine.select_layer (index_of_v2 (), dialogue::features_of<mixer_v2> ());
  bench.settle ();
  EXPECT_EQ (outs[0]->error, protocol_errc::not_supported);
  EXPECT_EQ (outs[1]->error, protocol_errc::not_supported);
  // The base row goes on and starts: no reply expected, the write ends it.
  ASSERT_EQ (outs[2]->calls, 1);
  EXPECT_FALSE (outs[2]->error) << outs[2]->error.message ();
}

TEST (LumexDialogueSelectLayer,
      GivenAnExchangeInFlight_ThenItsReplyStillEndsIt)
{
  engine_bench_t bench;
  pr::dialogue_engine &engine = bench.client->engine ();
  bench.device->set_behavior (tr::testing::behavior::silent ());
  auto const out = std::make_shared<outcome_t<pr::reply_frame_t>> ();
  record_t<pr::reply_frame_t> done = { out };
  pr::exchange_t request
      = exchange_of<mixer, mixer::RATE_t> (std::uint8_t (1));
  engine.async_exchange (std::move (request), done);
  bench.settle ();
  engine.select_layer (index_of_v2 (), dialogue::features_of<mixer_v2> ());
  bench.settle ();
  EXPECT_EQ (out->calls, 0);
  std::vector<std::uint8_t> const frame = { 0x21, 0x07 };
  std::vector<std::uint8_t> wire;
  ASSERT_TRUE (
      fr::slip::slip_codec ()
          .encode (layout::bytes_view_t (frame.data (), frame.size ()), wire)
          .has_value ());
  bench.control->feed (wire);
  bench.settle ();
  ASSERT_EQ (out->calls, 1);
  EXPECT_FALSE (out->error) << out->error.message ();
  EXPECT_EQ (out->value, frame);
}

TEST (LumexDialogueSelectLayer, GivenALateReplyStillExpected_ThenItIsDropped)
{
  engine_bench_t bench;
  pr::dialogue_engine &engine = bench.client->engine ();
  bench.device->set_behavior (tr::testing::behavior::silent ());
  auto const out = std::make_shared<outcome_t<pr::reply_frame_t>> ();
  record_t<pr::reply_frame_t> done = { out };
  engine.async_exchange (exchange_of<mixer, mixer::RATE_t> (std::uint8_t (1)),
                         done);
  bench.advance (ms (250));
  ASSERT_EQ (out->calls, 1);
  EXPECT_EQ (out->error, protocol_errc::timeout);
  engine.select_layer (index_of_v2 (), dialogue::features_of<mixer_v2> ());
  bench.settle ();
  std::vector<std::uint8_t> const frame = { 0x21, 0x07 };
  std::vector<std::uint8_t> wire;
  ASSERT_TRUE (
      fr::slip::slip_codec ()
          .encode (layout::bytes_view_t (frame.data (), frame.size ()), wire)
          .has_value ());
  bench.control->feed (wire);
  bench.settle ();
  EXPECT_EQ (bench.client->statistics ().late_replies, 1u);
}

TEST (LumexDialogueSelectLayer, GivenAQueuedExchange_ThenItsReplyStillEndsIt)
{
  engine_bench_t bench;
  pr::dialogue_engine &engine = bench.client->engine ();
  bench.device->set_behavior (tr::testing::behavior::silent ());
  auto const first = std::make_shared<outcome_t<pr::reply_frame_t>> ();
  auto const queued = std::make_shared<outcome_t<pr::reply_frame_t>> ();
  record_t<pr::reply_frame_t> first_done = { first };
  record_t<pr::reply_frame_t> queued_done = { queued };
  engine.async_exchange (exchange_of<mixer, mixer::RATE_t> (std::uint8_t (1)),
                         first_done);
  engine.async_exchange (exchange_of<mixer, mixer::SHAPE_t> (), queued_done);
  bench.settle ();
  engine.select_layer (index_of_v2 (), dialogue::features_of<mixer_v2> ());
  bench.settle ();
  bench.control->feed (wire_of ({ 0x21, 0x07 }));
  bench.settle ();
  ASSERT_EQ (first->calls, 1);
  EXPECT_FALSE (first->error) << first->error.message ();
  EXPECT_EQ (queued->calls, 0);
  bench.control->feed (wire_of ({ 0x23, 0x09 }));
  bench.settle ();
  ASSERT_EQ (queued->calls, 1);
  EXPECT_FALSE (queued->error) << queued->error.message ();
}

TEST (LumexDialogueSelectLayer, GivenARunWaitingForAFrame_ThenItGoesOn)
{
  engine_bench_t bench;
  pr::dialogue_engine &engine = bench.client->engine ();
  lumex::core::protocol::dsl::scenario_step_t step;
  step.kind = lumex::core::protocol::dsl::scenario_step_kind::expect;
  step.row = "LEVEL";
  lumex::core::protocol::dsl::scenario_t scenario;
  scenario.steps.push_back (step);
  auto const out = std::make_shared<outcome_t<std::size_t>> ();
  record_t<std::size_t> done = { out };
  engine.async_run_scenario (scenario, dialogue::features_of<mixer> (), done);
  bench.settle ();
  engine.select_layer (index_of_v2 (), dialogue::features_of<mixer_v2> ());
  bench.settle ();
  EXPECT_EQ (out->calls, 0);
  bench.control->feed (wire_of ({ 0x12, 0x00, 0x01 }));
  bench.settle ();
  ASSERT_EQ (out->calls, 1);
  EXPECT_FALSE (out->error) << out->error.message ();
}

TEST (LumexDialogueSelectLayer, GivenAnEdgeSubscriber_ThenItFollowsTheLayer)
{
  engine_bench_t bench;
  pr::dialogue_engine &engine = bench.client->engine ();
  int edges = 0;
  pr::subscription_t const made
      = engine.on_edge (index::protocol_index<mixer> ()->find_name ("LEVEL"),
                        [&edges] (pr::edge_t const &) { ++edges; });
  EXPECT_NE (made.id, 0u);
  engine.select_layer (index_of_v2 (), dialogue::features_of<mixer_v2> ());
  bench.settle ();
  bench.control->feed (wire_of ({ 0x12, 0x00, 0x01 }));
  bench.control->feed (wire_of ({ 0x12, 0x01, 0x01 }));
  bench.settle ();
  EXPECT_EQ (edges, 1);
}

TEST (LumexDialogueSelectLayer,
      GivenASubscriberOfARowTheLayerHasNot_ThenItIsSilent)
{
  engine_bench_t bench;
  pr::dialogue_engine &engine = bench.client->engine ();
  engine.select_layer (index_of_v1 (), dialogue::features_of<mixer_v1> ());
  bench.settle ();
  int heard = 0;
  pr::subscription_t const made
      = engine.subscribe (index_of_v1 ().find_name ("ONLY_V1"),
                          [&heard] (pr::reply_frame_t const &) { ++heard; });
  EXPECT_NE (made.id, 0u);
  engine.select_layer (index_of_v2 (), dialogue::features_of<mixer_v2> ());
  bench.settle ();
  // Of no row now: no subscriber takes it for its own.
  bench.control->feed (wire_of ({ 0x40, 0x01 }));
  bench.settle ();
  EXPECT_EQ (heard, 0);
  EXPECT_EQ (bench.client->statistics ().unknown_frames, 1u);
  // The row is back with its layer.
  engine.select_layer (index_of_v1 (), dialogue::features_of<mixer_v1> ());
  bench.settle ();
  bench.control->feed (wire_of ({ 0x40, 0x01 }));
  bench.settle ();
  EXPECT_EQ (heard, 1);
}

TEST (LumexDialogueSelectLayer, GivenALayerOfMoreInFlight_ThenMoreGoOut)
{
  using plain = tables::plain_t;
  using featured = tables::featured_t;
  client_bench_t<plain, plain> bench;
  pr::dialogue_engine &engine = bench.client->engine ();
  bench.device->set_behavior (tr::testing::behavior::silent ());
  auto const out = std::make_shared<outcome_t<pr::reply_frame_t>> ();
  record_t<pr::reply_frame_t> done = { out };
  engine.async_exchange (exchange_of<plain, plain::ASK_t> (), done);
  engine.async_exchange (exchange_of<plain, plain::WHO_t> (), done);
  bench.settle ();
  EXPECT_EQ (bench.client->statistics ().requests, 1u);
  engine.select_layer (*index::protocol_index<featured> (),
                       dialogue::features_of<featured> ());
  // Past the rate limit of the layer; not past the timeout of the first.
  bench.advance (ms (40));
  EXPECT_EQ (bench.client->statistics ().requests, 2u);
}

TEST (LumexDialogueSelectLayer, GivenALayerWithAWindow_ThenALateReplyIsDropped)
{
  using plain = tables::plain_t;
  using windowed = tables::windowed_t;
  client_bench_t<plain, plain> bench;
  pr::dialogue_engine &engine = bench.client->engine ();
  engine.select_layer (*index::protocol_index<windowed> (),
                       dialogue::features_of<windowed> ());
  bench.settle ();
  bench.device->set_behavior (tr::testing::behavior::silent ());
  auto const out = std::make_shared<outcome_t<pr::reply_frame_t>> ();
  record_t<pr::reply_frame_t> done = { out };
  pr::exchange_t request = exchange_of<windowed, windowed::ORDERED_t> ();
  request.how = dialogue::correlation::by_order;
  engine.async_exchange (std::move (request), done);
  bench.advance (ms (1000));
  ASSERT_EQ (out->calls, 1);
  EXPECT_EQ (out->error, protocol_errc::timeout);
  // The next request waits for the window: 0 without the layer's option.
  auto const next = std::make_shared<outcome_t<pr::reply_frame_t>> ();
  record_t<pr::reply_frame_t> next_done = { next };
  std::uint64_t const sent = bench.client->statistics ().requests;
  pr::exchange_t second = exchange_of<windowed, windowed::ORDERED_t> ();
  second.how = dialogue::correlation::by_order;
  engine.async_exchange (std::move (second), next_done);
  bench.settle ();
  bench.control->feed (wire_of ({ 0x05, 0x01 }));
  bench.settle ();
  EXPECT_EQ (next->calls, 0);
  EXPECT_EQ (bench.client->statistics ().late_replies, 1u);
  EXPECT_LE (bench.client->statistics ().requests, sent + 1);
  engine.cancel ();
  bench.settle ();
}

TEST (LumexDialogueSelectLayer,
      GivenAKeepaliveWaitThatFiredAsTheLayerGoes_ThenNothing)
{
  using plain = tables::plain_t;
  using featured = tables::featured_t;
  client_bench_t<plain, featured> bench;
  pr::dialogue_engine &engine = bench.client->engine ();
  // The select is ahead on the strand, the wait fires behind it and cannot be
  // cancelled any more.
  engine.select_layer (*index::protocol_index<plain> (),
                       dialogue::features_of<plain> ());
  bench.clock.advance (ms (100));
  bench.settle ();
  EXPECT_EQ (bench.client->statistics ().keepalives, 0u);
}

TEST (LumexDialogueSelectLayer,
      GivenAWatchdogWaitThatFiredAsTheLayerGoes_ThenNothing)
{
  using plain = tables::plain_t;
  using featured = tables::featured_t;
  client_bench_t<plain, featured> bench;
  pr::dialogue_engine &engine = bench.client->engine ();
  engine.select_layer (*index::protocol_index<plain> (),
                       dialogue::features_of<plain> ());
  bench.clock.advance (ms (1000));
  bench.settle ();
  EXPECT_EQ (bench.client->statistics ().watchdog_expiries, 0u);
  EXPECT_TRUE (engine.link_is_up ());
}

TEST (LumexDialogueSelectLayer, GivenAStreamWatched_ThenItStaysWatched)
{
  engine_bench_t bench;
  bench.settle ();
  pr::reply_of<mixer::LEVEL_t> level;
  EXPECT_TRUE (bench.device->start_stream<mixer_v2::LEVEL_t> (level));
  bench.advance (ms (150));
  bench.client->engine ().select_layer (index_of_v2 (),
                                        dialogue::features_of<mixer_v2> ());
  bench.settle ();
  bench.device->stop_stream<mixer_v2::LEVEL_t> ();
  // Two intervals of silence: one gap, seen by the watch that carried over.
  bench.advance (ms (300));
  EXPECT_EQ (bench.client->statistics ().stream_gaps, 1u);
}

TEST (LumexDialogueSelectLayer, GivenTheLayerInUse_ThenNothingChanges)
{
  engine_bench_t bench;
  pr::dialogue_engine &engine = bench.client->engine ();
  index::protocol_index_t const *const before = &engine.index ();
  // client_options_for<mixer> gives these two: the same layer.
  engine.select_layer (*index::protocol_index<mixer> (),
                       dialogue::features_of<mixer> ());
  bench.settle ();
  EXPECT_EQ (&engine.index (), before);
  EXPECT_STREQ (engine.index ().protocol_name (), "mixer_t");
}

TEST (LumexDialogueSelectLayer,
      GivenAKeepaliveRowOfAnotherLayer_ThenItIsDropped)
{
  using plain = tables::plain_t;
  using featured = tables::featured_t;
  client_bench_t<plain, featured> bench;
  pr::dialogue_engine &engine = bench.client->engine ();
  int made = 0;
  EXPECT_TRUE (engine.set_keepalive (
      [&made] (layout::bytes_t &message,
               field::decode_context_t const &context) -> std::error_code
        {
          ++made;
          layout::encode_result_t const encoded
              = layout::encode_message<featured, featured::BEAT_t> (message,
                                                                    context);
          return encoded ? std::error_code () : encoded.error ();
        }));
  bench.advance (ms (150));
  EXPECT_EQ (made, 1);
  // A layer without a keepalive: the generator goes, the keepalive ends.
  engine.select_layer (*index::protocol_index<plain> (),
                       dialogue::features_of<plain> ());
  bench.settle ();
  std::uint64_t const started = bench.client->statistics ().keepalives;
  bench.advance (ms (500));
  EXPECT_EQ (bench.client->statistics ().keepalives, started);
  EXPECT_EQ (made, 1);
  // Back on the layer of the keepalive: its row needs no generator, and the
  // one that went stays gone.
  engine.select_layer (*index::protocol_index<featured> (),
                       dialogue::features_of<featured> ());
  bench.advance (ms (250));
  EXPECT_GT (bench.client->statistics ().keepalives, started);
  EXPECT_EQ (made, 1);
}

// --- what select_layer carries over, each part by itself ---

namespace
{
/// An exchange of WRITE, whose reply is the frame of CHANGE.
pr::exchange_t
write_exchange ()
{
  pr::exchange_t request = exchange_of<mixer, mixer::WRITE_t> (
      std::uint8_t (1), std::uint16_t (2));
  request.how = dialogue::correlation::by_reply_key;
  request.reply_row = index::protocol_index<mixer> ()->find_name ("CHANGE");
  EXPECT_NE (request.reply_row, nullptr);
  return request;
}

/// The frame of CHANGE that answers write_exchange ().
std::vector<std::uint8_t>
change_wire ()
{
  return wire_of ({ 0x30, 0x01, 0x02, 0x00 });
}
} // namespace

TEST (LumexDialogueSelectLayer,
      GivenAnExchangeInFlightWithAReplyRow_ThenTheFrameOfThatRowEndsIt)
{
  engine_bench_t bench;
  pr::dialogue_engine &engine = bench.client->engine ();
  bench.device->set_behavior (tr::testing::behavior::silent ());
  auto const out = std::make_shared<outcome_t<pr::reply_frame_t>> ();
  record_t<pr::reply_frame_t> done = { out };
  engine.async_exchange (write_exchange (), done);
  bench.settle ();
  engine.select_layer (index_of_v2 (), dialogue::features_of<mixer_v2> ());
  bench.settle ();
  EXPECT_EQ (out->calls, 0);
  bench.control->feed (change_wire ());
  bench.settle ();
  ASSERT_EQ (out->calls, 1);
  EXPECT_FALSE (out->error) << out->error.message ();
}

TEST (LumexDialogueSelectLayer,
      GivenAQueuedExchangeWithAReplyRow_ThenTheFrameOfThatRowEndsIt)
{
  engine_bench_t bench;
  pr::dialogue_engine &engine = bench.client->engine ();
  bench.device->set_behavior (tr::testing::behavior::silent ());
  auto const first = std::make_shared<outcome_t<pr::reply_frame_t>> ();
  auto const queued = std::make_shared<outcome_t<pr::reply_frame_t>> ();
  record_t<pr::reply_frame_t> first_done = { first };
  record_t<pr::reply_frame_t> queued_done = { queued };
  engine.async_exchange (exchange_of<mixer, mixer::RATE_t> (std::uint8_t (1)),
                         first_done);
  engine.async_exchange (write_exchange (), queued_done);
  bench.settle ();
  engine.select_layer (index_of_v2 (), dialogue::features_of<mixer_v2> ());
  bench.settle ();
  bench.control->feed (wire_of ({ 0x21, 0x07 }));
  bench.settle ();
  ASSERT_EQ (first->calls, 1);
  EXPECT_EQ (queued->calls, 0);
  bench.control->feed (change_wire ());
  bench.settle ();
  ASSERT_EQ (queued->calls, 1);
  EXPECT_FALSE (queued->error) << queued->error.message ();
}

TEST (LumexDialogueSelectLayer,
      GivenARowWithAMinimumInterval_ThenItsLastRequestCountsAtTheNextLayer)
{
  using plain = tables::plain_t;
  using paced_a = tables::paced_a_t;
  using paced_b = tables::paced_b_t;
  client_bench_t<plain, plain> bench;
  pr::dialogue_engine &engine = bench.client->engine ();
  bench.device->set_behavior (tr::testing::behavior::silent ());
  engine.select_layer (*index::protocol_index<paced_a> (),
                       dialogue::features_of<paced_a> ());
  bench.settle ();
  // Far from the start of the clock: a request time lost on the way would
  // pass for one long ago.
  bench.advance (ms (1000));
  auto const first = std::make_shared<outcome_t<pr::reply_frame_t>> ();
  record_t<pr::reply_frame_t> first_done = { first };
  engine.async_exchange (
      exchange_of<paced_a, paced_a::PACED_t> (std::uint8_t (1)), first_done);
  bench.settle ();
  EXPECT_EQ (first->calls, 1);
  EXPECT_EQ (bench.client->statistics ().requests, 1u);
  bench.advance (ms (50));
  engine.select_layer (*index::protocol_index<paced_b> (),
                       dialogue::features_of<paced_b> ());
  bench.settle ();
  auto const second = std::make_shared<outcome_t<pr::reply_frame_t>> ();
  record_t<pr::reply_frame_t> second_done = { second };
  engine.async_exchange (
      exchange_of<paced_b, paced_b::PACED_t> (std::uint8_t (2)), second_done);
  bench.settle ();
  // 200 ms after the first request, which was 50 ms ago.
  EXPECT_EQ (bench.client->statistics ().requests, 1u);
  bench.advance (ms (100));
  EXPECT_EQ (bench.client->statistics ().requests, 1u);
  bench.advance (ms (60));
  EXPECT_EQ (bench.client->statistics ().requests, 2u);
  EXPECT_EQ (second->calls, 1);
}

TEST (LumexDialogueSelectLayer,
      GivenASilenceAlreadyCounted_ThenItIsNotCountedAgain)
{
  engine_bench_t bench;
  bench.settle ();
  pr::reply_of<mixer::LEVEL_t> level;
  EXPECT_TRUE (bench.device->start_stream<mixer_v2::LEVEL_t> (level));
  bench.advance (ms (150));
  bench.device->stop_stream<mixer_v2::LEVEL_t> ();
  bench.advance (ms (300));
  EXPECT_EQ (bench.client->statistics ().stream_gaps, 1u);
  bench.client->engine ().select_layer (index_of_v2 (),
                                        dialogue::features_of<mixer_v2> ());
  bench.settle ();
  bench.advance (ms (1000));
  EXPECT_EQ (bench.client->statistics ().stream_gaps, 1u);
}

TEST (LumexDialogueSelectLayer,
      GivenALevelSeenBeforeTheLayer_ThenTheEdgeAfterItCounts)
{
  engine_bench_t bench;
  pr::dialogue_engine &engine = bench.client->engine ();
  int edges = 0;
  pr::subscription_t const made
      = engine.on_edge (index::protocol_index<mixer> ()->find_name ("LEVEL"),
                        [&edges] (pr::edge_t const &) { ++edges; });
  EXPECT_NE (made.id, 0u);
  bench.control->feed (wire_of ({ 0x12, 0x00, 0x01 }));
  bench.settle ();
  engine.select_layer (index_of_v2 (), dialogue::features_of<mixer_v2> ());
  bench.settle ();
  EXPECT_EQ (edges, 0);
  // The level the first frame set is the one this frame changes.
  bench.control->feed (wire_of ({ 0x12, 0x01, 0x01 }));
  bench.settle ();
  EXPECT_EQ (edges, 1);
}

TEST (LumexDialogueSelectLayer,
      GivenARunThatHeardARowBeforeTheLayer_ThenItsBranchFindsTheReply)
{
  engine_bench_t bench;
  pr::dialogue_engine &engine = bench.client->engine ();
  namespace pdsl = lumex::core::protocol::dsl;
  pdsl::scenario_t scenario;
  pdsl::scenario_step_t level;
  level.kind = pdsl::scenario_step_kind::expect;
  level.row = "LEVEL";
  scenario.steps.push_back (level);
  pdsl::scenario_step_t rate;
  rate.kind = pdsl::scenario_step_kind::expect;
  rate.row = "RATE";
  scenario.steps.push_back (rate);
  int decided = 0;
  std::size_t seen = 0;
  pdsl::scenario_step_t branch;
  branch.kind = pdsl::scenario_step_kind::branch;
  branch.row = "LEVEL";
  branch.label = "end";
  branch.decide = [&decided, &seen] (layout::bytes_view_t message,
                                     field::decode_context_t const &,
                                     bool &taken) -> std::error_code
    {
      ++decided;
      seen = message.size ();
      taken = false;
      return std::error_code ();
    };
  scenario.steps.push_back (branch);
  pdsl::scenario_step_t end;
  end.kind = pdsl::scenario_step_kind::label;
  end.label = "end";
  scenario.steps.push_back (end);
  auto const out = std::make_shared<outcome_t<std::size_t>> ();
  record_t<std::size_t> done = { out };
  engine.async_run_scenario (scenario, dialogue::features_of<mixer> (), done);
  bench.settle ();
  bench.control->feed (wire_of ({ 0x12, 0x00, 0x01 }));
  bench.settle ();
  EXPECT_EQ (out->calls, 0);
  engine.select_layer (index_of_v2 (), dialogue::features_of<mixer_v2> ());
  bench.settle ();
  bench.control->feed (wire_of ({ 0x21, 0x07 }));
  bench.settle ();
  ASSERT_EQ (out->calls, 1);
  EXPECT_FALSE (out->error) << out->error.message ();
  EXPECT_EQ (decided, 1);
  EXPECT_EQ (seen, 3u);
}

TEST (LumexDialogueSelectLayer,
      GivenARunWhoseNextStepWaitsForARow_ThenAFrameOfItDuringTheSendIsHeld)
{
  engine_bench_t bench;
  pr::dialogue_engine &engine = bench.client->engine ();
  bench.device->set_behavior (tr::testing::behavior::silent ());
  namespace pdsl = lumex::core::protocol::dsl;
  pdsl::scenario_t scenario;
  pdsl::scenario_step_t send;
  send.kind = pdsl::scenario_step_kind::send;
  send.row = "RATE";
  send.encode = [] (layout::bytes_t &message,
                    field::decode_context_t const &) -> std::error_code
    {
      layout::encode_result_t const encoded
          = layout::encode_message<mixer, mixer::RATE_t> (message,
                                                          std::uint8_t (1));
      return encoded ? std::error_code () : encoded.error ();
    };
  scenario.steps.push_back (send);
  pdsl::scenario_step_t level;
  level.kind = pdsl::scenario_step_kind::expect;
  level.row = "LEVEL";
  scenario.steps.push_back (level);
  auto const out = std::make_shared<outcome_t<std::size_t>> ();
  record_t<std::size_t> done = { out };
  engine.async_run_scenario (scenario, dialogue::features_of<mixer> (), done);
  bench.settle ();
  engine.select_layer (index_of_v2 (), dialogue::features_of<mixer_v2> ());
  bench.settle ();
  // The frame of the next step's row comes right before the reply that ends
  // this one.
  bench.control->feed (wire_of ({ 0x12, 0x00, 0x01 }));
  bench.settle ();
  EXPECT_EQ (out->calls, 0);
  bench.control->feed (wire_of ({ 0x21, 0x07 }));
  bench.settle ();
  ASSERT_EQ (out->calls, 1);
  EXPECT_FALSE (out->error) << out->error.message ();
}
