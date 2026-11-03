// The tables and the bench of the tests of the features of the dialogue engine
// (streams, keepalive, watchdog, edges, rate limits, state, scenarios). An
// invented "tank": no real device, its names, codes or values are in it.
#ifndef LUMEX_TESTS_APPLIED_TRANSPORT_PROTOCOL_DIALOGUE_FEATURE_SUPPORT_HPP
#define LUMEX_TESTS_APPLIED_TRANSPORT_PROTOCOL_DIALOGUE_FEATURE_SUPPORT_HPP

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <new>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/applied/transport/LumexTransport"
#include "lumex/core/protocol/LumexProtocol"
#include "lumex/tests/applied/transport/protocol/dialogue/LumexDialogueTestSupport.hpp"

namespace lumex_dialogue_feature_test
{
namespace dt = lumex_dialogue_test;
namespace tr = dt::tr;
namespace pr = dt::pr;
namespace dialogue = dt::dialogue;
namespace index = dt::index;
namespace layout = dt::layout;
using dt::bytes;
using dt::bytes_t;
using dt::log_capture_t;
using dt::manual_link_t;
using dt::ms;
using dt::outcome_ptr;
using dt::outcome_t;
using dt::protocol_errc;
using dt::record_t;
using dt::transport_errc;
using tr::log::log_level;

enum class tank_state : std::uint8_t
{
  idle,
  filling,
  draining,
  fault
};

struct tank_codec_t
{
};

LUMEX_PROTOCOL_LAYOUT (tank_status_t, (u8_, enum_<u8_, tank_state>, u16_le_),
                       ("valve", "state", "level"));

// The rows of every table of this file: keys in the first byte.
// clang-format off
#define TANK_ROWS(X, X_AS) \
  X (ACK, 0x06, "Accepted", (), (), (), ()) \
  X (CLOCK, 0x11, "Set the clock", (u32_le_), ("seconds"), (), (idempotent)) \
  X (STATUS, 0x12, "State", (), (), (tank_status_t), \
     (stream (100), edge_event<0, 0, 1> (), edge_event<0, 1, 0> ())) \
  X (LEVEL, 0x13, "Level", (), (), (u16_le_), (stream (500))) \
  X (ENABLE_STATUS, 0x21, "Turn on the state", (), (), (u8_), \
     (enables_stream<STATUS_t> ())) \
  X (ENABLE_LEVEL, 0x22, "Turn on the level", (), (), (u8_), \
     (enables_stream<LEVEL_t> ())) \
  X (FILL, 0x31, "Fill", (u8_), ("amount"), (u8_), \
     (accepted_in (tank_state::idle))) \
  X (DRAIN, 0x32, "Drain", (u8_), ("amount"), (u8_), \
     (accepted_in (tank_state::filling, tank_state::draining))) \
  X (QUERY, 0x33, "Query", (), (), (u8_), ()) \
  X (SLOW, 0x34, "One in 50 ms", (), (), (u8_), (min_interval (50))) \
  X (ERASE, 0x35, "Erase", (u8_), ("block"), (u8_), (dangerous)) \
  X (RETRIED, 0x36, "Sent again after a NAK", (), (), \
     (ack_nak<u8_, u16_le_>), (min_interval (50), retries (1))) \
  X (ORDERED, 0x37, "Answered by order", (), (), (u8_), \
     (correlate (correlation::by_order))) \
  X (TIMED, 0x38, "A time of its own", (), (), (u8_), \
     (timeout (77), retries (2))) \
  X (NOTHING, 0x39, "No reply", (), (), (), ()) \
  X (QUICK, 0x3A, "One in 10 ms", (), (), (u8_), (min_interval (10))) \
  X (ORDERED_SLOW, 0x3B, "By order, one in 50 ms", (), (), (u8_), \
     (correlate (correlation::by_order), min_interval (50))) \
  X (CHANGED, 0x3C, "A value changed", (), (), (u8_), ()) \
  X (SET_VALUE, 0x3D, "Answered by the change", (u8_), ("value"), (), \
     (reply_row<CHANGED_t> ())) \
  X (RETRIED_QUICK, 0x3E, "Sent again after a NAK, one in 10 ms", (), (), \
     (ack_nak<u8_, u16_le_>), (min_interval (10))) \
  X (NAK, 0x15, "Refused", (), (), (u8_), ()) \
  X (STOP, 0x40, "Stop, acknowledged by rows", (), (), \
     (ack_nak<ACK_t, NAK_t>), ()) \
  X (PRIME, 0x41, "Prime, acknowledged by a row", (u16_le_), ("volume"), \
     (two_phase<ACK_t, u16_le_>), ()) \
  X (ECHOED, 0x42, "Set, echoed by order", (u8_), ("value"), \
     (echo_set<u8_>), (correlate (correlation::by_order))) \
  X (ZEROED, 0x43, "Set, zero refuses, by order", (u8_), ("value"), \
     (echo_zero_on_error<u8_>), (correlate (correlation::by_order))) \
  X (STATED, 0x44, "Set, the state instead of a refusal, by order", (u8_), \
     ("value"), (state_instead_of_refusal<u8_>), \
     (correlate (correlation::by_order)))
// clang-format on

/// Every protocol option of the engine; the row of the keepalive has a field.
struct tank_header_t
{
  using lumex_vocabulary = dialogue::vocabulary_t;
  static LUMEX_CONSTEXPR std::size_t key_width = 1;

  template <typename P>
  using dialogue_options
      = dialogue::options<dialogue::keepalive<typename P::CLOCK_t, 1000>,
                          dialogue::watchdog_ms<2500>,
                          dialogue::state_from<typename P::STATUS_t, 1>,
                          dialogue::default_timeout_ms<400>,
                          dialogue::default_retries<1>,
                          dialogue::late_reply_window_ms<30>>;
};

/// The state and no time-driven option.
struct quiet_header_t
{
  using lumex_vocabulary = dialogue::vocabulary_t;
  static LUMEX_CONSTEXPR std::size_t key_width = 1;

  template <typename P>
  using dialogue_options
      = dialogue::options<dialogue::state_from<typename P::STATUS_t, 1>>;
};

/// A rate limit over every row.
struct rated_header_t
{
  using lumex_vocabulary = dialogue::vocabulary_t;
  static LUMEX_CONSTEXPR std::size_t key_width = 1;
  using dialogue_options = dialogue::options<dialogue::min_interval_ms<40>,
                                             dialogue::max_in_flight<1>>;
};

/// Two requests in flight: a rate limit of one row must not hold another.
struct wide_header_t
{
  using lumex_vocabulary = dialogue::vocabulary_t;
  static LUMEX_CONSTEXPR std::size_t key_width = 1;
};

/// The state in a reply of one field, on a row without edges.
struct one_field_header_t
{
  using lumex_vocabulary = dialogue::vocabulary_t;
  static LUMEX_CONSTEXPR std::size_t key_width = 1;

  template <typename P>
  using dialogue_options
      = dialogue::options<dialogue::state_from<typename P::QUERY_t, 0>>;
};

/// A keepalive row without fields needs no generator.
struct beat_header_t
{
  using lumex_vocabulary = dialogue::vocabulary_t;
  static LUMEX_CONSTEXPR std::size_t key_width = 1;

  template <typename P>
  using dialogue_options
      = dialogue::options<dialogue::keepalive<typename P::NOTHING_t, 200>>;
};

/// A watchdog and nothing that could keep the link talking.
struct watched_header_t
{
  using lumex_vocabulary = dialogue::vocabulary_t;
  static LUMEX_CONSTEXPR std::size_t key_width = 1;
  using dialogue_options = dialogue::options<dialogue::watchdog_ms<300>>;
};

// The table of the keepalive with a field; no watchdog in these tests.
struct clock_header_t
{
  using lumex_vocabulary = dialogue::vocabulary_t;
  static LUMEX_CONSTEXPR std::size_t key_width = 1;

  template <typename P>
  using dialogue_options
      = dialogue::options<dialogue::keepalive<typename P::CLOCK_t, 1000>>;
};
LUMEX_PROTOCOL_DEFINE (clocked_t, tank_codec_t, clock_header_t, TANK_ROWS);

// The table whose keepalive row answers.
struct answered_header_t
{
  using lumex_vocabulary = dialogue::vocabulary_t;
  static LUMEX_CONSTEXPR std::size_t key_width = 1;

  template <typename P>
  using dialogue_options
      = dialogue::options<dialogue::keepalive<typename P::QUERY_t, 100>>;
};
LUMEX_PROTOCOL_DEFINE (answered_t, tank_codec_t, answered_header_t, TANK_ROWS);

LUMEX_PROTOCOL_DEFINE (tank_t, tank_codec_t, tank_header_t, TANK_ROWS);
LUMEX_PROTOCOL_DEFINE (quiet_t, tank_codec_t, quiet_header_t, TANK_ROWS);
LUMEX_PROTOCOL_DEFINE (one_field_t, tank_codec_t, one_field_header_t,
                       TANK_ROWS);
LUMEX_PROTOCOL_DEFINE (rated_t, tank_codec_t, rated_header_t, TANK_ROWS);
LUMEX_PROTOCOL_DEFINE (wide_t, tank_codec_t, wide_header_t, TANK_ROWS);
LUMEX_PROTOCOL_DEFINE (beat_t, tank_codec_t, beat_header_t, TANK_ROWS);
LUMEX_PROTOCOL_DEFINE (watched_t, tank_codec_t, watched_header_t, TANK_ROWS);

/// The bytes of a STATUS frame.
inline bytes_t
status_frame (int valve, tank_state state = tank_state::idle, int level = 0)
{
  return bytes ({ 0x12, valve, static_cast<int> (state), level & 0xFF,
                  (level >> 8) & 0xFF });
}

/// The exchange of `Row` of layer `P` with `args`, as the client fills it.
template <typename P, typename Row, typename... Args>
pr::exchange_t
make_exchange (Args const &...args)
{
  using traits = dialogue::row_traits_t<P, Row>;
  index::protocol_index_t const &table = *index::protocol_index<P> ();
  pr::exchange_t out;
  layout::encode_result_t const encoded
      = layout::encode_message<P, Row> (out.message, args...);
  EXPECT_TRUE (encoded.has_value ());
  out.row = table.find_name (Row::identifier ());
  out.how = traits::how ();
  out.matcher = traits::matcher ();
  out.reader = dialogue::reply_reader_of<P, Row> ();
  return out;
}

/// The row of a table by its type.
template <typename P, typename Row>
index::row_t const *
row_of ()
{
  return index::protocol_index<P> ()->find_name (Row::identifier ());
}

/// A timer source over a virtual clock whose next waits can be made to throw.
class flaky_timers_t : public tr::timing::timer_source_impl
{
public:
  explicit flaky_timers_t (tr::timing::timer_source inner)
      : inner_ (std::move (inner)), throwing_ (std::make_shared<bool> (false))
  {
  }

  void
  set_throwing (bool on)
  {
    *throwing_ = on;
  }

  tr::timing::time_point_t
  now () const override
  {
    return inner_.now ();
  }

  std::shared_ptr<tr::timing::timer_impl>
  make_timer (tr::executor_type const &executor) const override
  {
    return std::make_shared<flaky_timer_t> (
        inner_.make_timer (executor).impl (), throwing_);
  }

private:
  class flaky_timer_t : public tr::timing::timer_impl
  {
  public:
    flaky_timer_t (std::shared_ptr<tr::timing::timer_impl> inner,
                   std::shared_ptr<bool> throwing)
        : inner_ (std::move (inner)), throwing_ (std::move (throwing))
    {
    }

    tr::executor_type
    get_executor () const override
    {
      return inner_->get_executor ();
    }

    tr::timing::time_point_t
    now () const override
    {
      return inner_->now ();
    }

    void
    start_wait (tr::timing::time_point_t at,
                tr::timing::wait_handler_t handler) override
    {
      if (*throwing_)
        throw std::bad_alloc ();
      inner_->start_wait (at, std::move (handler));
    }

    std::size_t
    cancel () LUMEX_NOEXCEPT override
    {
      return inner_->cancel ();
    }

  private:
    std::shared_ptr<tr::timing::timer_impl> inner_;
    std::shared_ptr<bool> throwing_;
  };

  tr::timing::timer_source inner_;
  std::shared_ptr<bool> throwing_;
};

/**
 * A dialogue over a `manual_link_t` on a virtual clock, with the features of
 * layer `P`: the test delivers each frame and each read failure itself, so it
 * can order them against a timer the clock ended.
 */
template <typename P> class manual_bench_t
{
public:
  manual_bench_t () : link (std::make_shared<manual_link_t> (context))
  {
    options = dialogue::options_for<P> ();
    options.name = "tank";
    options.timers = clock.source ();
  }

  ~manual_bench_t ()
  {
    stop ();
    clock.advance (ms (0));
    settle ();
  }

  manual_bench_t (manual_bench_t const &) = delete;
  manual_bench_t &operator= (manual_bench_t const &) = delete;

  void
  start ()
  {
    engine.reset (new pr::dialogue_engine (
        pr::frame_link (link), *index::protocol_index<P> (), options));
    settle ();
  }

  void
  stop ()
  {
    engine.reset ();
    settle ();
  }

  /// Runs the ready handlers; the clock stands.
  void
  settle ()
  {
    context.restart ();
    context.poll ();
    context.restart ();
  }

  /// Moves the clock by `by`, wait by wait, running the handlers.
  void
  advance (ms by)
  {
    tr::testing::run_until (context, clock, [] () { return false; }, by);
  }

  /// Runs and moves the clock until `done`.
  bool
  run_until (std::function<bool ()> const &done, ms budget = ms (60000))
  {
    return tr::testing::run_until (context, clock, done, budget);
  }

  /// Delivers `frame` and runs it.
  void
  deliver (bytes_t const &frame)
  {
    ASSERT_TRUE (link->deliver (frame));
    settle ();
  }

  /// Starts `request` and returns where its outcome will be.
  outcome_ptr
  start_exchange (pr::exchange_t request)
  {
    outcome_ptr const out = std::make_shared<outcome_t> ();
    record_t handler = { out, &clock };
    engine->async_exchange (std::move (request), std::move (handler));
    return out;
  }

  /// The keys of the requests written so far.
  std::vector<int>
  keys () const
  {
    std::vector<int> out;
    std::vector<bytes_t> const written = link->written ();
    for (std::size_t i = 0; i < written.size (); ++i)
      out.push_back (dt::key_of (written[i]));
    return out;
  }

  /// How many frames of `key` were written.
  std::size_t
  count (int key) const
  {
    std::vector<int> const all = keys ();
    std::size_t found = 0;
    for (std::size_t i = 0; i < all.size (); ++i)
      if (all[i] == key)
        ++found;
    return found;
  }

  asio::io_context context;
  tr::testing::virtual_clock clock;
  std::shared_ptr<manual_link_t> link;
  pr::dialogue_options_t options;
  std::unique_ptr<pr::dialogue_engine> engine;
};

/// A generator that gives no message.
inline pr::keepalive_generator_t
tank_keepalive_none ()
{
  return [] (layout::bytes_t &,
             lumex::core::protocol::field::decode_context_t const &)
    { return std::error_code (); };
}

/// What a sequence (a scenario, the enabling of streams) ended with.
struct sequence_outcome_t
{
  int calls = 0;
  std::error_code error;
  std::size_t step = 0;
};

using sequence_outcome_ptr = std::shared_ptr<sequence_outcome_t>;

/// The completion of a sequence in a test.
struct sequence_record_t
{
  sequence_outcome_ptr out;

  void
  operator() (std::error_code error, std::size_t step)
  {
    ++out->calls;
    out->error = error;
    out->step = step;
  }
};
} // namespace lumex_dialogue_feature_test

#endif // !LUMEX_TESTS_APPLIED_TRANSPORT_PROTOCOL_DIALOGUE_FEATURE_SUPPORT_HPP
