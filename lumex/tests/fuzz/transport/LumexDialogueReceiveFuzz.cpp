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
    asio::post (strand_,
                asio::append (std::move (handler), std::error_code ()));
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
  switch (which % 14U)
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
    default:
      return make_exchange<pump::QUERY_NAMED_t> ();
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
};

struct complete_t
{
  std::shared_ptr<completion_t> state;
  void
  operator() (std::error_code, pr::reply_frame_t) const
  {
    ++state->calls;
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
  REQUIRE (stats.requests - stats.retries <= run.exchanges.size ());
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
  std::unique_ptr<hand_link_t> link (new hand_link_t (run.context));
  run.link = link.get ();
  run.engine.reset (new pr::dialogue_engine (
      pr::frame_link (std::shared_ptr<pr::frame_link_impl> (std::move (link))),
      pump_index (), options));
  run.settle ();

  while (script.more ())
    {
      std::uint8_t const op = script.next ();
      switch (op % 9U)
        {
        case 0:
        case 1:
          {
            unsigned const which = script.next ();
            std::uint8_t const argument = script.next ();
            std::uint8_t const how = script.next ();
            pr::exchange_t request = exchange_of (which, argument);
            if ((how & 0x0FU) != 0)
              {
                pr::retry_policy_t policy;
                policy.timeout_ms = 10U + (how & 0x0FU) * 20U;
                policy.retries = (how >> 4U) & 3U;
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
            run.exchanges.push_back (state);
            complete_t handler = { state };
            run.engine->async_exchange (std::move (request),
                                        std::move (handler));
            break;
          }
        case 2:
        case 3:
          {
            // A frame of random bytes (op 2) or of a key of the table and
            // random bytes (op 3).
            std::size_t length = script.next () % 12U;
            bytes_t frame;
            if (op % 9U == 3U)
              {
                frame.push_back (keys[script.next () % sizeof keys]);
                if (length > 0)
                  --length;
              }
            for (std::size_t i = 0; i < length; ++i)
              frame.push_back (script.next ());
            if (run.link->deliver (frame))
              ++run.delivered;
            break;
          }
        case 4:
          run.advance (ms (script.next () * 10U));
          break;
        case 5:
          run.engine->cancel ();
          break;
        case 6:
          run.link->lose ();
          break;
        case 7:
          {
            std::uint8_t const what = script.next ();
            if ((what & 1U) != 0)
              {
                if (run.subscribed)
                  break;
                run.subscribed = true;
                run_t *const owner = &run;
                run.engine->subscribe (row_of_t<pump::STATUS_t>::get (),
                                       [owner] (pr::reply_frame_t const &)
                                         { ++owner->heard_status; });
                run.engine->subscribe (row_of_t<pump::CHANGE_t>::get (),
                                       [owner] (pr::reply_frame_t const &)
                                         { ++owner->heard_change; });
              }
            else
              {
                run_t *const owner = &run;
                run.engine->on_unknown ([owner] (pr::reply_frame_t const &)
                                          { ++owner->heard_unknown; });
              }
            break;
          }
        default:
          break;
        }
      run.settle ();
    }

  // The clock runs on: every exchange ends (the timeouts are finite).
  for (int step = 0; step < 100; ++step)
    run.advance (ms (500));
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

// The scripts of the seed corpus.
std::vector<bytes_t>
seeds ()
{
  std::vector<bytes_t> result;
  // header, then ops: 0 which argument how; 2/3 frame; 4 advance; ...
  {
    // POLL, answered by a frame of its key.
    std::uint8_t const s[] = { 0x00, 0, 9, 0, 0, 3, 3, 16, 1, 0, 0, 0, 4, 20 };
    result.push_back (bytes_t (s, s + sizeof s));
  }
  {
    // START, answered by an acknowledgement; then a timeout and a retry.
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
    // PRIME (two phases), WRITE and its change report, a cancel.
    std::uint8_t const s[] = { 0x1A, 0, 2, 9, 0, 0, 5, 3, 4, 0, 3, 3, 2,  6,
                               0,    0, 5, 1, 0, 0, 3, 3, 2, 1, 0, 4, 30, 5 };
    result.push_back (bytes_t (s, s + sizeof s));
  }
  return result;
}
} // namespace

extern "C" int
LLVMFuzzerInitialize (int *argc, char ***argv)
{
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
