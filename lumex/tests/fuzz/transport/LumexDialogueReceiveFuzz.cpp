// libFuzzer target of the receive path of the dialogue engine
// (lumex/applied/transport/protocol/dialogue): an input is a script for a
// dialogue_engine over a link driven by hand (frame_link_impl) on the
// virtual clock, with the synthetic pump table of the dialogue tests. The
// script starts exchanges of the rows of the table (every correlation and
// reply kind) with their own timeouts and retries, feeds frames (random
// bytes, or the key of a row and random bytes), moves the clock, cancels,
// subscribes, and ends reads with an error. The invariants:
//   - the handler of an exchange runs once, never twice, and when the clock
//     has run on no exchange is left pending;
//   - every frame the engine took is counted once, as a reply, a late reply,
//     an unknown frame, an event or an unexpected reply;
//   - the counters agree: one request per write, retries within requests,
//     exhausted exchanges within the exchanges started, negative
//     acknowledgements within the replies, the subscribers heard within the
//     events counted.
// A script whose header has its top bit set runs with the features of the
// table (features_of): the state, rate limits, edges, gaps, the watchdog, and
// ops for the state and the keepalive.
// Only the public API is used (dialogue_engine, frame_link, frame_link_impl,
// exchange_t, reply_reader_of). Anything else stops the run. Built with
// LUMEX_BUILD_FUZZERS (Clang only); CTest runs it for a short bounded time
// under the label fuzz. LLVMFuzzerInitialize writes the seed corpus
// (scripts of synthetic exchanges and replies) into the corpus directory
// named on the command line.

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <string>
#include <system_error>
#include <tuple>
#include <vector>

#include "lumex/applied/transport/error/LumexTransportErrc.hpp"
#include "lumex/applied/transport/protocol/LumexTransportProtocol.hpp"
#include "lumex/applied/transport/testing/LumexVirtualClock.hpp"
#include "lumex/core/protocol/LumexProtocol"
#include "lumex/tests/applied/transport/protocol/dsl/LumexDialogueTestTables.hpp"

namespace
{
namespace tr = lumex::applied::transport;
namespace pr = lumex::applied::transport::protocol;
namespace dialogue = lumex::applied::transport::protocol::dialogue;
namespace index = lumex::core::protocol::index;
namespace layout = lumex::core::protocol::layout;
namespace tables = lumex_tests::dialogue_tables;
using pump = tables::pump_t;
using bytes_t = std::vector<std::uint8_t>;
using ms = std::chrono::milliseconds;

void
fail (char const *expression, int line)
{
  std::fprintf (stderr, "check failed at line %d: %s\n", line, expression);
  std::abort ();
}

#define REQUIRE(condition)                                                    \
  do                                                                          \
    {                                                                         \
      if (!(condition))                                                       \
        fail (#condition, __LINE__);                                          \
    }                                                                         \
  while (false)

// A link the script drives: a write completes at once (posted), a read waits
// until the script delivers a frame or an error.
class hand_link_t : public pr::frame_link_impl
{
public:
  explicit hand_link_t (asio::io_context &context)
      : strand_ (
            asio::make_strand (tr::executor_type (context.get_executor ()))),
        name_ ("hand")
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
    return true;
  }

  void
  start_read (pr::link_read_handler_t handler) override
  {
    reads_.push_back (std::move (handler));
  }

  void
  start_write (std::shared_ptr<pr::link_frame_t const>,
               tr::status_handler_t handler) override
  {
    ++writes;
    std::error_code error;
    if (fail_writes > 0)
      {
        --fail_writes;
        error = make_error_code (tr::error::transport_errc::disconnected);
      }
    asio::post (strand_, asio::append (std::move (handler), error));
  }

  void
  cancel () LUMEX_NOEXCEPT override
  {
    end_reads (make_error_code (tr::error::transport_errc::cancelled));
  }

  void
  close () LUMEX_NOEXCEPT override
  {
    end_reads (make_error_code (tr::error::transport_errc::closed));
  }

  // Hands `frame` to the oldest pending read; false without one.
  bool
  deliver (bytes_t const &frame)
  {
    if (reads_.empty ())
      return false;
    pr::link_read_handler_t handler = std::move (reads_.front ());
    reads_.erase (reads_.begin ());
    asio::post (strand_, asio::append (std::move (handler), std::error_code (),
                                       pr::link_frame_t (frame)));
    return true;
  }

  // Ends the oldest pending read with an error of the link.
  void
  lose ()
  {
    if (reads_.empty ())
      return;
    pr::link_read_handler_t handler = std::move (reads_.front ());
    reads_.erase (reads_.begin ());
    asio::post (strand_,
                asio::append (
                    std::move (handler),
                    make_error_code (tr::error::transport_errc::disconnected),
                    pr::link_frame_t ()));
  }

  std::uint64_t writes = 0;
  unsigned fail_writes = 0;

private:
  void
  end_reads (std::error_code error) LUMEX_NOEXCEPT
  {
    std::vector<pr::link_read_handler_t> ended;
    ended.swap (reads_);
    for (std::size_t i = 0; i < ended.size (); ++i)
      asio::post (strand_, asio::append (std::move (ended[i]), error,
                                         pr::link_frame_t ()));
  }

  strand_type strand_;
  std::string name_;
  std::vector<pr::link_read_handler_t> reads_;
};

index::protocol_index_t const &
pump_index ()
{
  return *index::protocol_index<pump> ();
}

template <typename Row> struct row_of_t
{
  static index::row_t const *
  get ()
  {
    return pump_index ().find_name (Row::identifier ());
  }
};

template <> struct row_of_t<void>
{
  static index::row_t const *
  get ()
  {
    return nullptr;
  }
};

template <typename Row, typename... Args>
pr::exchange_t
make_exchange (Args const &...args)
{
  using traits = dialogue::row_traits_t<pump, Row>;
  pr::exchange_t out;
  layout::encode_result_t const encoded
      = layout::encode_message<pump, Row> (out.message, args...);
  REQUIRE (encoded.has_value ());
  out.row = row_of_t<Row>::get ();
  out.how = traits::how ();
  out.reply_row = row_of_t<typename traits::reply_row>::get ();
  out.matcher = traits::matcher ();
  out.reader = dialogue::reply_reader_of<pump, Row> ();
  return out;
}

// The rows a script starts, one per reply kind and correlation, with the
// argument byte of the script.
pr::exchange_t
exchange_of (unsigned which, std::uint8_t argument)
{
  switch (which % 19U)
    {
    case 0:
      return make_exchange<pump::START_t> (argument);
    case 1:
      return make_exchange<pump::STOP_t> ();
    case 2:
      return make_exchange<pump::PRIME_t> (
          static_cast<std::uint16_t> (argument * 257U));
    case 3:
      return make_exchange<pump::SET_FLOW_t> (
          static_cast<std::uint16_t> (argument * 3U));
    case 4:
      return make_exchange<pump::SET_LIMIT_t> (argument);
    case 5:
      return make_exchange<pump::WRITE_t> (
          argument, static_cast<std::uint16_t> (argument * 5U));
    case 6:
      return make_exchange<pump::READ_t> (argument);
    case 7:
      return make_exchange<pump::QUERY_t> ();
    case 8:
      return make_exchange<pump::TOUCH_t> ();
    case 9:
      return make_exchange<pump::POLL_t> ();
    case 10:
      return make_exchange<pump::INFO_t> ();
    case 11:
      return make_exchange<pump::BOTH_t> ();
    case 12:
      return make_exchange<pump::WRITE_NAMED_t> (argument);
    case 13:
      return make_exchange<pump::QUERY_NAMED_t> ();
    case 14:
      return make_exchange<pump::SET_MODE_t> (
          static_cast<tables::pump_state> (argument % 3U));
    case 15:
      return make_exchange<pump::SET_STATE_t> (
          static_cast<tables::pump_state> (argument % 3U));
    case 16:
      return make_exchange<pump::SET_GAIN_t> (static_cast<double> (argument)
                                              / 100.0);
    case 17:
      return make_exchange<pump::SET_RATE_t> (static_cast<float> (argument));
    default:
      return make_exchange<pump::CLOCK_t> (
          static_cast<std::uint32_t> (argument) * 1000U);
    }
}

// The keys of the rows of the pump table.
std::uint8_t const keys[]
    = { 0x06, 0x15, 0x30, 0x10, 0x11, 0x12, 0x01, 0x02, 0x03, 0x04, 0x05, 0x07,
        0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x16, 0x17, 0x18 };

// One exchange of the script: how many times its handler ran.
struct completion_t
{
  int calls = 0;
  std::error_code error;
  bool has_frame = false;
  unsigned which = 0;
  std::shared_ptr<asio::cancellation_signal> signal
      = std::make_shared<asio::cancellation_signal> ();
};

// Exchanges that ended without an error, per row of exchange_of; printed at
// exit when LUMEX_FUZZ_STATS is set.
std::uint64_t ok_by_row[19];
std::uint64_t started_by_row[19];

struct complete_t
{
  std::shared_ptr<completion_t> state;
  void
  operator() (std::error_code error, pr::reply_frame_t frame) const
  {
    ++state->calls;
    state->error = error;
    state->has_frame = frame.size () != 0;
    REQUIRE (state->calls == 1);
  }
};

// extra_retries: two more attempts in all.
struct two_more_t
{
  std::shared_ptr<int> left;
  std::uint32_t
  operator() (index::session_context_t const &, std::error_code const &) const
  {
    if (*left == 0)
      return 0;
    --*left;
    return 1;
  }
};

class script_t
{
public:
  script_t (std::uint8_t const *data, std::size_t size)
      : data_ (data), size_ (size)
  {
  }

  bool
  more () const
  {
    return at_ < size_;
  }

  std::uint8_t
  next ()
  {
    return at_ < size_ ? data_[at_++] : 0;
  }

private:
  std::uint8_t const *data_;
  std::size_t size_;
  std::size_t at_ = 0;
};

struct run_t
{
  asio::io_context context;
  tr::testing::virtual_clock clock;
  hand_link_t *link = nullptr;
  std::unique_ptr<pr::dialogue_engine> engine;
  std::vector<std::shared_ptr<completion_t>> exchanges;
  // Frames handed to the engine, and the subscribers' calls.
  std::uint64_t delivered = 0;
  std::uint64_t heard_status = 0;
  std::uint64_t heard_change = 0;
  std::uint64_t heard_unknown = 0;
  // One subscription per row: the calls counted are of one handler each.
  bool subscribed = false;
  bool cancelled = false;
  bool lost = false;
  bool unknown_set = false;
  bool closed = false;
  std::vector<pr::subscription_t> subscriptions;
  std::uint64_t allowed = 0;
  std::uint32_t default_attempts = 1;

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
};

void
check_counters (run_t &run)
{
  pr::dialogue_statistics_t const stats = run.engine->statistics ();
  REQUIRE (stats.requests == run.link->writes);
  REQUIRE (stats.retries <= stats.requests);
  REQUIRE (stats.requests - stats.retries - stats.keepalives
           <= run.exchanges.size ());
  // A negative acknowledgement is a reply to its attempt.
  REQUIRE (stats.naks <= stats.replies);
  REQUIRE (stats.retries_exhausted <= run.exchanges.size ());
  REQUIRE (stats.timeouts >= stats.retries_exhausted);
  // Every frame the engine took ended in exactly one counter.
  REQUIRE (stats.late_replies + stats.unknown_frames + stats.events
               + stats.unexpected_replies + stats.replies
           == run.delivered);
  REQUIRE (run.heard_status + run.heard_change
           <= stats.events + stats.unexpected_replies);
  REQUIRE (run.heard_unknown <= stats.unknown_frames);
  std::uint64_t exhausted = 0, succeeded = 0, naks = 0;
  for (std::size_t i = 0; i < run.exchanges.size (); ++i)
    {
      std::error_code const e = run.exchanges[i]->error;
      if (e
          == lumex::core::protocol::error::make_error_code (
              lumex::core::protocol::error::protocol_errc::retries_exhausted))
        ++exhausted;
      if (e
          == lumex::core::protocol::error::make_error_code (
              lumex::core::protocol::error::protocol_errc::nak))
        ++naks;
      if (!e)
        {
          // A row without a reply (18) ends with its write.
          if (run.exchanges[i]->which != 18U)
            ++succeeded;
          ++ok_by_row[run.exchanges[i]->which];
        }
      if (e == make_error_code (tr::error::transport_errc::cancelled))
        REQUIRE (run.cancelled);
      if (e
          == lumex::core::protocol::error::make_error_code (
              lumex::core::protocol::error::protocol_errc::link_lost))
        REQUIRE (run.lost);
    }
  REQUIRE (naks <= stats.naks);
  REQUIRE (exhausted == stats.retries_exhausted);
  REQUIRE (succeeded <= stats.replies);
  REQUIRE (stats.requests - stats.keepalives <= run.allowed);
}

// Hands `frame` to the engine and checks that exactly one counter moved, and
// that the handler of the frame was called when there is one.
void
deliver (run_t &run, bytes_t const &frame)
{
  pr::dialogue_statistics_t const a = run.engine->statistics ();
  std::uint64_t const heard = run.heard_status + run.heard_change;
  std::uint64_t const unknown = run.heard_unknown;
  if (!run.link->deliver (frame))
    return;
  ++run.delivered;
  run.settle ();
  pr::dialogue_statistics_t const b = run.engine->statistics ();
  REQUIRE ((b.late_replies - a.late_replies)
               + (b.unknown_frames - a.unknown_frames) + (b.events - a.events)
               + (b.unexpected_replies - a.unexpected_replies)
               + (b.replies - a.replies)
           == 1U);
  if (run.unknown_set && b.unknown_frames != a.unknown_frames)
    REQUIRE (run.heard_unknown == unknown + 1U);
  if (run.subscribed && !frame.empty ()
      && (frame[0] == 0x12 || frame[0] == 0x30)
      && (b.events != a.events
          || b.unexpected_replies != a.unexpected_replies))
    REQUIRE (run.heard_status + run.heard_change == heard + 1U);
}

void
play (std::uint8_t const *data, std::size_t size)
{
  script_t script (data, size);
  run_t run;
  pr::dialogue_options_t options;
  std::uint8_t const header = script.next ();
  options.max_in_flight = 1U + (header & 3U) % 3U;
  options.queue_bound = (header & 4U) != 0 ? 3 : 64;
  options.late_reply_window_ms = (header & 8U) != 0 ? 100 : 0;
  options.defaults.timeout_ms = 50U + (header >> 4U) * 10U;
  options.defaults.retries = (header >> 6U) & 1U;
  options.timers = run.clock.source ();
  // The features of the table: the state, the rate limits, the edges, the
  // watchdog (a link it ends is a lost one).
  bool const features = (header & 0x80U) != 0;
  if (features)
    {
      options.features = dialogue::features_of<pump> ();
      run.lost = true;
    }
  std::unique_ptr<hand_link_t> link (new hand_link_t (run.context));
  run.link = link.get ();
  run.engine.reset (new pr::dialogue_engine (
      pr::frame_link (std::shared_ptr<pr::frame_link_impl> (std::move (link))),
      pump_index (), options));
  run.default_attempts = 1U + options.defaults.retries;
  run.settle ();

  while (script.more ())
    {
      std::uint8_t const op = script.next ();
      switch (op % 16U)
        {
        case 0:
        case 1:
          {
            unsigned const which = script.next ();
            std::uint8_t const argument = script.next ();
            std::uint8_t const how = script.next ();
            pr::exchange_t request = exchange_of (which, argument);
            std::uint64_t attempts = run.default_attempts;
            if ((how & 0x0FU) != 0)
              {
                pr::retry_policy_t policy;
                policy.timeout_ms = 10U + (how & 0x0FU) * 20U;
                policy.retries = (how >> 4U) & 3U;
                attempts
                    = 1U + policy.retries + ((how & 0x80U) != 0 ? 2U : 0U);
                if ((how & 0x80U) != 0)
                  {
                    two_more_t more = { std::make_shared<int> (2) };
                    policy.extra_retries = more;
                  }
                request.policy
                    = std::make_shared<pr::retry_policy_t const> (policy);
              }
            std::shared_ptr<completion_t> state
                = std::make_shared<completion_t> ();
            state->which = which % 19U;
            ++started_by_row[state->which];
            run.allowed += attempts;
            run.exchanges.push_back (state);
            complete_t handler = { state };
            run.engine->async_exchange (
                std::move (request),
                asio::bind_cancellation_slot (state->signal->slot (),
                                              std::move (handler)));
            break;
          }
        case 2:
        case 3:
          {
            // A frame of random bytes (op 2) or of a key of the table and
            // random bytes (op 3).
            std::size_t length = script.next () % 12U;
            bytes_t frame;
            if (op % 16U == 3U)
              {
                frame.push_back (keys[script.next () % sizeof keys]);
                if (length > 0)
                  --length;
              }
            for (std::size_t i = 0; i < length; ++i)
              frame.push_back (script.next ());
            deliver (run, frame);
            break;
          }
        case 4:
          run.advance (ms (script.next () * 10U));
          break;
        case 5:
          run.cancelled = true;
          run.engine->cancel ();
          break;
        case 6:
          run.lost = true;
          run.link->lose ();
          break;
        case 7:
          {
            std::uint8_t const what = script.next ();
            if ((what & 1U) != 0)
              {
                if (run.subscribed)
                  break;
                run_t *const owner = &run;
                pr::subscription_t const status = run.engine->subscribe (
                    row_of_t<pump::STATUS_t>::get (),
                    [owner] (pr::reply_frame_t const &)
                      { ++owner->heard_status; });
                pr::subscription_t const change = run.engine->subscribe (
                    row_of_t<pump::CHANGE_t>::get (),
                    [owner] (pr::reply_frame_t const &)
                      { ++owner->heard_change; });
                // A dialogue that was closed refuses them.
                REQUIRE ((status.id != 0) == !run.closed);
                REQUIRE ((change.id != 0) == !run.closed);
                if (!run.closed)
                  {
                    run.subscribed = true;
                    run.subscriptions.push_back (status);
                    run.subscriptions.push_back (change);
                  }
              }
            else
              {
                run_t *const owner = &run;
                run.unknown_set = !run.closed;
                run.engine->on_unknown ([owner] (pr::reply_frame_t const &)
                                          { ++owner->heard_unknown; });
              }
            break;
          }
        case 8:
          // Ends the subscriptions.
          for (std::size_t i = 0; i < run.subscriptions.size (); ++i)
            REQUIRE (run.engine->unsubscribe (run.subscriptions[i])
                     == !run.closed);
          run.subscriptions.clear ();
          run.subscribed = false;
          break;
        case 9:
          {
            // A frame of exactly the bytes of the script.
            std::size_t length = script.next () % 16U;
            bytes_t frame;
            for (std::size_t i = 0; i < length; ++i)
              frame.push_back (script.next ());
            deliver (run, frame);
            break;
          }
        case 10:
          // The next write fails.
          run.link->fail_writes = 1;
          run.lost = true;
          break;
        case 11:
          {
            // The cancellation slot of one exchange.
            std::uint8_t const which = script.next ();
            if (!run.exchanges.empty ())
              {
                run.cancelled = true;
                run.exchanges[which % run.exchanges.size ()]->signal->emit (
                    asio::cancellation_type::terminal);
              }
            break;
          }
        case 12:
          run.closed = true;
          run.cancelled = true;
          run.engine->close ();
          break;
        case 13:
          // The last known state, as the table's accepted_in reads it.
          if ((script.next () & 1U) != 0)
            run.engine->set_state (script.next () % 3);
          else
            run.engine->clear_state ();
          break;
        case 14:
          // The keepalive of the table (with the features); its requests are
          // counted apart.
          run.engine->set_keepalive (dialogue::keepalive_from<pump> (
              [] () { return std::make_tuple (std::uint32_t (1)); }));
          break;
        default:
          break;
        }
      run.settle ();
    }

  // The clock runs on: every exchange ends (the timeouts are finite).
  {
    run_t *const r = &run;
    tr::testing::run_until (
        run.context, run.clock,
        [r] ()
          {
            for (std::size_t i = 0; i < r->exchanges.size (); ++i)
              if (r->exchanges[i]->calls == 0)
                return false;
            return true;
          },
        ms ((run.allowed + run.exchanges.size () + 1U) * 6000U));
  }
  for (std::size_t i = 0; i < run.exchanges.size (); ++i)
    REQUIRE (run.exchanges[i]->calls == 1);
  check_counters (run);

  // Closing ends nothing twice.
  run.engine->close ();
  run.settle ();
  run.engine.reset ();
  run.settle ();
  for (std::size_t i = 0; i < run.exchanges.size (); ++i)
    REQUIRE (run.exchanges[i]->calls == 1);
}

// A script of one exchange of row `which` answered by the frames `replies`
// (each as exactly its bytes), then the clock moves on.
bytes_t
row_script (unsigned which, std::uint8_t argument,
            std::vector<bytes_t> const &replies)
{
  bytes_t out;
  out.push_back (0x00);
  out.push_back (0);
  out.push_back (static_cast<std::uint8_t> (which));
  out.push_back (argument);
  out.push_back (0);
  for (std::size_t i = 0; i < replies.size (); ++i)
    {
      out.push_back (9);
      out.push_back (static_cast<std::uint8_t> (replies[i].size ()));
      out.insert (out.end (), replies[i].begin (), replies[i].end ());
    }
  out.push_back (4);
  out.push_back (10);
  return out;
}

bytes_t
frame_of (std::initializer_list<int> values)
{
  bytes_t out;
  for (int value : values)
    out.push_back (static_cast<std::uint8_t> (value));
  return out;
}

// The scripts of the seed corpus: one per row with the frame that answers it
// the way its reply kind wants (the echo rows answer with the request itself),
// and a few of the other ops.
std::vector<bytes_t>
seeds ()
{
  std::vector<bytes_t> result;
  std::vector<bytes_t> none;
  std::vector<bytes_t> one (1);
  // 0 START (ack_nak<u8, refusal>): the acknowledgement.
  one[0] = frame_of ({ 0x01, 0x00 });
  result.push_back (row_script (0, 3, one));
  // 1 STOP (ack_nak<ACK, NAK>): ACK.
  one[0] = frame_of ({ 0x06 });
  result.push_back (row_script (1, 0, one));
  // 2 PRIME (two_phase<ACK, u16>): ACK, then the completion.
  {
    std::vector<bytes_t> two;
    two.push_back (frame_of ({ 0x06 }));
    two.push_back (frame_of ({ 0x03, 0x34, 0x12 }));
    result.push_back (row_script (2, 1, two));
  }
  // 3, 4, 14, 15, 16, 17: echo kinds, the reply is the request.
  static unsigned const echoes[] = { 3, 4, 14, 15, 16, 17 };
  for (std::size_t i = 0; i < sizeof echoes / sizeof echoes[0]; ++i)
    {
      pr::exchange_t const request = exchange_of (echoes[i], 2);
      one[0] = request.message;
      result.push_back (row_script (echoes[i], 2, one));
    }
  // 5 WRITE and 12 WRITE_NAMED: the change report of the row.
  one[0] = frame_of ({ 0x30, 0x04, 0x14, 0x00 });
  result.push_back (row_script (5, 4, one));
  one[0] = frame_of ({ 0x30, 0x04, 0x00, 0x00 });
  result.push_back (row_script (12, 4, one));
  // 6 READ (by_order), 7 QUERY, 9 POLL, 13 QUERY_NAMED: the reply row.
  one[0] = frame_of ({ 0x09, 0x01, 0x00 });
  result.push_back (row_script (6, 1, one));
  one[0] = frame_of ({ 0x0A, 0x01 });
  result.push_back (row_script (7, 0, one));
  one[0] = frame_of ({ 0x0C, 0x01 });
  result.push_back (row_script (9, 0, one));
  one[0] = frame_of ({ 0x0E, 0x01 });
  result.push_back (row_script (13, 0, one));
  // 8 TOUCH (ack_nak<nothing, NAK>): the acknowledgement.
  one[0] = frame_of ({ 0x0B });
  result.push_back (row_script (8, 0, one));
  // 10 INFO: the model text and the firmware.
  one[0] = frame_of ({ 0x10, 'A', 'B', 'C', 0, 0, 0, 0, 0, 0x02, 0x01 });
  result.push_back (row_script (10, 0, one));
  // 11 BOTH (ack_nak<u8, i8>).
  one[0] = frame_of ({ 0x0D, 0x01 });
  result.push_back (row_script (11, 0, one));
  // 18 CLOCK: no reply, it ends with the write.
  result.push_back (row_script (18, 5, none));
  {
    // START refused (NAK), a timeout and a retry.
    std::uint8_t const s[]
        = { 0x01, 0, 0, 7, 0, 3, 2, 0, 0, 4, 50, 0, 1, 0, 0, 4, 200 };
    result.push_back (bytes_t (s, s + sizeof s));
  }
  {
    // Two exchanges in flight, subscribers, an unknown frame, a lost read.
    std::uint8_t const s[]
        = { 0x02, 7, 1,  0, 1, 0, 3, 10, 0, 0, 0,   7, 0, 2, 5, 1, 0,  0,
            3,    3, 18, 1, 2, 3, 4, 6,  2, 3, 255, 1, 2, 0, 5, 4, 100 };
    result.push_back (bytes_t (s, s + sizeof s));
  }
  {
    // A cancelled exchange, a write that fails, a close and an exchange after.
    std::uint8_t const s[]
        = { 0x00, 0,  9, 0, 0, 0, 7,  0, 0, 0, 10, 0, 0, 8, 0, 0,
            0,    11, 0, 7, 1, 8, 12, 0, 9, 0, 0,  7, 1, 4, 20 };
    result.push_back (bytes_t (s, s + sizeof s));
  }
  // The same with the features of the table, and the ops of the state and the
  // keepalive in front.
  std::size_t const without = result.size ();
  for (std::size_t i = 0; i < without; ++i)
    {
      bytes_t script = result[i];
      script[0] = 0x80;
      script.insert (script.begin () + 1, { 14, 13, 1, 1 });
      result.push_back (script);
    }
  return result;
}

void
print_statistics ()
{
  for (unsigned i = 0; i < 19; ++i)
    std::fprintf (stderr, "row %u: %llu of %llu ended without an error\n", i,
                  static_cast<unsigned long long> (ok_by_row[i]),
                  static_cast<unsigned long long> (started_by_row[i]));
}
} // namespace

extern "C" int
LLVMFuzzerInitialize (int *argc, char ***argv)
{
  if (std::getenv ("LUMEX_FUZZ_STATS") != nullptr)
    std::atexit (&print_statistics);
  for (int i = 1; i < *argc; ++i)
    {
      std::string const argument ((*argv)[i]);
      if (argument.empty () || argument[0] == '-')
        continue;
      std::vector<bytes_t> const all = seeds ();
      for (std::size_t n = 0; n < all.size (); ++n)
        {
          std::ofstream file (argument + "/seed_" + std::to_string (n)
                                  + ".bin",
                              std::ios::binary);
          if (!file)
            break;
          file.write (reinterpret_cast<char const *> (all[n].data ()),
                      static_cast<std::streamsize> (all[n].size ()));
        }
      break;
    }
  return 0;
}

extern "C" int
LLVMFuzzerTestOneInput (std::uint8_t const *data, std::size_t size)
{
  play (data, size);
  return 0;
}
