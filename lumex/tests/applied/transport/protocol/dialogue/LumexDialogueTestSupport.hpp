// The bench of the dialogue engine tests: an io_context the test polls
// itself, a virtual clock, a mock stream framed with SLIP and a dialogue
// over the synthetic pump table of the vocabulary tests. The device side
// decodes the frames the dialogue writes and answers through a function of
// the test. No real device, its names, codes or values are in it.
#ifndef LUMEX_TESTS_APPLIED_TRANSPORT_PROTOCOL_DIALOGUE_TEST_SUPPORT_HPP
#define LUMEX_TESTS_APPLIED_TRANSPORT_PROTOCOL_DIALOGUE_TEST_SUPPORT_HPP

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/applied/transport/LumexTransport"
#include "lumex/core/framing/LumexFraming"
#include "lumex/core/protocol/LumexProtocol"
#include "lumex/tests/applied/transport/protocol/dsl/LumexDialogueTestTables.hpp"

namespace lumex_dialogue_test
{
namespace tr = lumex::applied::transport;
namespace pr = lumex::applied::transport::protocol;
namespace dialogue = lumex::applied::transport::protocol::dialogue;
namespace fr = lumex::core::framing;
namespace index = lumex::core::protocol::index;
namespace layout = lumex::core::protocol::layout;
namespace perror = lumex::core::protocol::error;
namespace tables = lumex_tests::dialogue_tables;
using pump = tables::pump_t;
using bytes_t = std::vector<std::uint8_t>;
using ms = std::chrono::milliseconds;
using perror::protocol_errc;
using tr::error::transport_errc;

/// Bytes from small integers.
inline bytes_t
bytes (std::initializer_list<int> values)
{
  bytes_t out;
  for (int value : values)
    out.push_back (static_cast<std::uint8_t> (value));
  return out;
}

/// The index of the pump table.
inline index::protocol_index_t const &
pump_index ()
{
  return *index::protocol_index<pump> ();
}

/// What one exchange ended with.
struct outcome_t
{
  int calls = 0;
  std::error_code error;
  bytes_t frame;
  /// The time of the clock when the handler ran.
  tr::timing::time_point_t at;
};

using outcome_ptr = std::shared_ptr<outcome_t>;

/// The completion of a test exchange: records the outcome.
struct record_t
{
  outcome_ptr out;
  tr::testing::virtual_clock *clock;

  void
  operator() (std::error_code error, pr::reply_frame_t frame)
  {
    ++out->calls;
    out->error = error;
    out->frame = std::move (frame);
    out->at = clock->now ();
  }
};

/// The row pointer of a row type in the pump index, null for void.
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

/// The exchange of `Row` with `args`, as the client fills it.
template <typename Row, typename... Args>
pr::exchange_t
make_exchange (Args const &...args)
{
  using traits = dialogue::row_traits_t<pump, Row>;
  pr::exchange_t out;
  layout::encode_result_t const encoded
      = layout::encode_message<pump, Row> (out.message, args...);
  EXPECT_TRUE (encoded.has_value ());
  out.row = row_of_t<Row>::get ();
  out.how = traits::how ();
  out.reply_row = row_of_t<typename traits::reply_row>::get ();
  out.matcher = traits::matcher ();
  out.reader = dialogue::reply_reader_of<pump, Row> ();
  return out;
}

/// A policy object for exchanges.
inline std::shared_ptr<pr::retry_policy_t const>
policy (std::uint32_t timeout_ms, std::uint32_t retries,
        pr::extra_retries_t extra = pr::extra_retries_t ())
{
  pr::retry_policy_t made;
  made.timeout_ms = timeout_ms;
  made.retries = retries;
  made.extra_retries = std::move (extra);
  return std::make_shared<pr::retry_policy_t const> (std::move (made));
}

/// One request the device received: its message and the time.
struct request_t
{
  bytes_t message;
  tr::timing::time_point_t at;
};

/// The bench (see the file description). Construct, then `start ()`.
class bench_t
{
public:
  explicit bench_t (pr::dialogue_options_t options = pr::dialogue_options_t ())
      : state_ (std::make_shared<device_state_t> ())
  {
    tr::testing::mock_options_t mock_options;
    mock_options.name = "pump";
    mock_options.timers = clock.source ();
    tr::testing::mock_stream mock = tr::testing::make_mock_stream (
        context.get_executor (), mock_options);
    control.reset (new tr::testing::mock_stream_control (mock.control));
    bool opened = false;
    mock.stream.async_open (
        [&opened] (std::error_code error)
          {
            EXPECT_FALSE (error);
            opened = true;
          });
    context.poll ();
    context.restart ();
    EXPECT_TRUE (opened);
    std::shared_ptr<device_state_t> const state = state_;
    tr::testing::virtual_clock *const at = &clock;
    control->on_write ([state, at] (bytes_t const &chunk)
                         { state->take (chunk, at->now ()); });
    options.timers = clock.source ();
    if (options.name == "dialogue")
      options.name = "pump";
    link_options_ = options;
    stream_.reset (new tr::stream::byte_stream (std::move (mock.stream)));
  }

  ~bench_t ()
  {
    stop ();
    control->on_write (tr::testing::write_observer_t ());
    state_->respond = std::function<void (bytes_t const &)> ();
    settle ();
  }

  bench_t (bench_t const &) = delete;
  bench_t &operator= (bench_t const &) = delete;

  /// Makes the dialogue (the test may change `link_options ()` before).
  void
  start ()
  {
    pr::frame_link link (make_link ());
    the_dialogue.reset (new pr::dialogue_engine (
        std::move (link), pump_index (), link_options_));
    settle ();
  }

  /// Destroys the dialogue and runs what that posted.
  void
  stop ()
  {
    the_dialogue.reset ();
    settle ();
  }

  pr::dialogue_options_t &
  link_options ()
  {
    return link_options_;
  }

  /// Starts `request` and returns where its outcome will be.
  outcome_ptr
  start_exchange (pr::exchange_t request)
  {
    outcome_ptr const out = std::make_shared<outcome_t> ();
    record_t handler = { out, &clock };
    the_dialogue->async_exchange (std::move (request), std::move (handler));
    return out;
  }

  /// Runs the ready handlers without moving the clock.
  void
  settle ()
  {
    tr::testing::run_until (context, clock, [] () { return false; }, ms (0));
  }

  /// Runs and moves the clock until `done` or `budget` passed.
  bool
  run_until (std::function<bool ()> const &done, ms budget = ms (60000))
  {
    return tr::testing::run_until (context, clock, done, budget);
  }

  /// Moves the clock by `by` and runs what became ready.
  void
  advance (ms by)
  {
    tr::testing::run_until (context, clock, [] () { return false; }, by);
  }

  /// Sets how the device answers a request (on the strand of the mock).
  void
  on_request (std::function<void (bytes_t const &)> respond)
  {
    state_->respond = std::move (respond);
  }

  /// Sends `message` as one SLIP frame after `delay` (0: now).
  void
  reply (bytes_t const &message, ms delay = ms (0))
  {
    bytes_t wire;
    fr::slip::slip_codec const codec;
    EXPECT_TRUE (codec
                     .encode (fr::codec::bytes_view_t (message.data (),
                                                       message.size ()),
                              wire)
                     .has_value ());
    if (delay.count () == 0)
      control->feed (wire);
    else
      control->feed_after (delay, wire);
  }

  /// The requests the device received so far.
  std::vector<request_t>
  requests () const
  {
    return state_->requests;
  }

  asio::io_context context;
  tr::testing::virtual_clock clock;
  std::unique_ptr<tr::testing::mock_stream_control> control;
  std::unique_ptr<pr::dialogue_engine> the_dialogue;

private:
  struct device_state_t
  {
    fr::slip::slip_codec decoder;
    std::vector<request_t> requests;
    std::function<void (bytes_t const &)> respond;
    tr::timing::time_point_t now;

    void
    take (bytes_t const &chunk, tr::timing::time_point_t at)
    {
      now = at;
      decoder.decode (fr::codec::bytes_view_t (chunk.data (), chunk.size ()),
                      [this] (fr::codec::bytes_view_t frame)
                        {
                          request_t request
                              = { bytes_t (frame.begin (), frame.end ()),
                                  now };
                          requests.push_back (request);
                          if (respond)
                            respond (requests.back ().message);
                        });
    }
  };

  pr::frame_link
  make_link ()
  {
    tr::framed::framed_stream<fr::slip::slip_codec> framed (
        std::move (*stream_), fr::slip::slip_codec ());
    return pr::frame_link (std::move (framed));
  }

  std::shared_ptr<device_state_t> state_;
  std::unique_ptr<tr::stream::byte_stream> stream_;
  pr::dialogue_options_t link_options_;
};

/// Captures the records of the transport log of this binary while it lives,
/// at every level; restores the sink and the level afterwards.
class log_capture_t
{
public:
  struct record_t
  {
    tr::log::log_level level;
    std::string message;
  };

  log_capture_t ()
      : level_ (tr::log::get_log_level ()), sink_ (tr::log::get_log_sink ())
  {
    std::lock_guard<std::mutex> const lock (mutex ());
    records ().clear ();
    tr::log::set_log_sink (&log_capture_t::capture);
    tr::log::set_log_level (tr::log::log_level::trace);
  }

  ~log_capture_t ()
  {
    tr::log::set_log_sink (sink_);
    tr::log::set_log_level (level_);
  }

  log_capture_t (log_capture_t const &) = delete;
  log_capture_t &operator= (log_capture_t const &) = delete;

  /// The messages of `level` of the dialogue's component.
  static std::vector<std::string>
  messages (tr::log::log_level level)
  {
    std::lock_guard<std::mutex> const lock (mutex ());
    std::vector<std::string> out;
    for (std::size_t i = 0; i < records ().size (); ++i)
      if (records ()[i].level == level)
        out.push_back (records ()[i].message);
    return out;
  }

  /// How many messages of `level` hold `needle`.
  static std::size_t
  count (tr::log::log_level level, std::string const &needle)
  {
    std::vector<std::string> const all = messages (level);
    std::size_t found = 0;
    for (std::size_t i = 0; i < all.size (); ++i)
      if (all[i].find (needle) != std::string::npos)
        ++found;
    return found;
  }

private:
  static std::mutex &
  mutex ()
  {
    static std::mutex instance;
    return instance;
  }

  static std::vector<record_t> &
  records ()
  {
    static std::vector<record_t> instance;
    return instance;
  }

  static void
  capture (tr::log::log_record_t const &item)
  {
    if (std::string (item.component) != "transport.dialogue")
      return;
    std::lock_guard<std::mutex> const lock (mutex ());
    record_t entry;
    entry.level = item.level;
    entry.message = item.message;
    records ().push_back (entry);
  }

  tr::log::log_level level_;
  tr::log::log_sink_t sink_;
};

/**
 * A link the test drives by hand: a write completes at once (posted), a read
 * waits until the test delivers a frame, and the completion of a delivery
 * is posted to the strand at the moment of the call, so that a test can
 * order it against a timer that the virtual clock ends afterwards.
 */
class manual_link_t : public pr::frame_link_impl
{
public:
  explicit manual_link_t (asio::io_context &context)
      : strand_ (
            asio::make_strand (tr::executor_type (context.get_executor ()))),
        name_ ("manual")
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
    std::lock_guard<std::mutex> const lock (mutex_);
    reads_.push_back (std::move (handler));
  }

  void
  start_write (std::shared_ptr<pr::link_frame_t const> frame,
               tr::status_handler_t handler) override
  {
    {
      std::lock_guard<std::mutex> const lock (mutex_);
      if (throw_on_write_)
        throw std::bad_alloc ();
      written_.push_back (*frame);
    }
    asio::post (strand_,
                asio::append (std::move (handler), std::error_code ()));
  }

  void
  cancel () LUMEX_NOEXCEPT override
  {
    end_reads ();
  }

  void
  close () LUMEX_NOEXCEPT override
  {
    end_reads ();
  }

  /// Hands `frame` to the oldest pending read (posted); false without one.
  bool
  deliver (bytes_t const &frame)
  {
    pr::link_read_handler_t handler;
    {
      std::lock_guard<std::mutex> const lock (mutex_);
      if (reads_.empty ())
        return false;
      handler = std::move (reads_.front ());
      reads_.erase (reads_.begin ());
    }
    asio::post (strand_, asio::append (std::move (handler), std::error_code (),
                                       pr::link_frame_t (frame)));
    return true;
  }

  /// Ends the pending reads with `cancelled` while the link stays open (a
  /// link of one's own that cancels its reads, a session between two
  /// connections).
  void
  cancel_reads ()
  {
    end_reads ();
  }

  /// The next writes throw `std::bad_alloc` (or stop throwing).
  void
  throw_on_write (bool on)
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    throw_on_write_ = on;
  }

  /// How many reads wait.
  std::size_t
  pending_reads () const
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    return reads_.size ();
  }

  std::vector<bytes_t>
  written () const
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    return written_;
  }

private:
  void
  end_reads () LUMEX_NOEXCEPT
  {
    std::vector<pr::link_read_handler_t> ended;
    {
      std::lock_guard<std::mutex> const lock (mutex_);
      ended.swap (reads_);
    }
    for (std::size_t i = 0; i < ended.size (); ++i)
      asio::post (strand_,
                  asio::append (std::move (ended[i]),
                                make_error_code (transport_errc::cancelled),
                                pr::link_frame_t ()));
  }

  strand_type strand_;
  std::string name_;
  mutable std::mutex mutex_;
  std::vector<pr::link_read_handler_t> reads_;
  std::vector<bytes_t> written_;
  bool throw_on_write_ = false;
};

/// The key of a request message (its first byte in the pump table).
inline int
key_of (bytes_t const &message)
{
  return message.empty () ? -1 : message[0];
}
} // namespace lumex_dialogue_test

#endif // !LUMEX_TESTS_APPLIED_TRANSPORT_PROTOCOL_DIALOGUE_TEST_SUPPORT_HPP
