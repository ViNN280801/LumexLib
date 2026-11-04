// The benches of the tests of the generated client: a device simulator on a
// mock stream framed with SLIP and a client<P> over it, on a virtual clock the
// test moves (client_bench_t), or with a runner on another thread and the real
// clock for the calls that block (thread_bench_t). No real device, its names,
// codes or values are in them.
#ifndef LUMEX_TESTS_APPLIED_TRANSPORT_PROTOCOL_CLIENT_TEST_SUPPORT_HPP
#define LUMEX_TESTS_APPLIED_TRANSPORT_PROTOCOL_CLIENT_TEST_SUPPORT_HPP

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/applied/transport/LumexTransport"
#include "lumex/core/framing/LumexFraming"
#include "lumex/core/protocol/LumexProtocol"
#include "lumex/tests/applied/transport/protocol/client/LumexClientTestTables.hpp"

namespace lumex_client_test
{
namespace tr = lumex::applied::transport;
namespace pr = lumex::applied::transport::protocol;
namespace dialogue = lumex::applied::transport::protocol::dialogue;
namespace fr = lumex::core::framing;
namespace index = lumex::core::protocol::index;
namespace layout = lumex::core::protocol::layout;
namespace field = lumex::core::protocol::field;
namespace tables = lumex_tests::client_tables;
using ms = std::chrono::milliseconds;
using lumex::core::protocol::error::protocol_errc;
using tr::error::transport_errc;
using mixer = tables::mixer_t;
using mixer_v1 = tables::mixer_v1_t;
using mixer_v2 = tables::mixer_v2_t;

/// What one call ended with.
template <typename Value> struct outcome_t
{
  int calls = 0;
  std::error_code error;
  Value value;

  outcome_t () : value () {}
};

/// The completion of a test call: records the outcome.
template <typename Value> struct record_t
{
  std::shared_ptr<outcome_t<Value>> out;

  void
  operator() (std::error_code error, Value value)
  {
    ++out->calls;
    out->error = error;
    out->value = std::move (value);
  }
};

template <typename Row> using outcome_of = outcome_t<pr::reply_of<Row>>;

/// A new outcome and its handler.
template <typename Row>
std::shared_ptr<outcome_of<Row>>
new_outcome ()
{
  return std::make_shared<outcome_of<Row>> ();
}

template <typename Row>
record_t<pr::reply_of<Row>>
into (std::shared_ptr<outcome_of<Row>> const &out)
{
  record_t<pr::reply_of<Row>> made = { out };
  return made;
}

/// The frame of `payload` as the SLIP codec of the benches writes it.
inline std::vector<std::uint8_t>
wire_of (std::vector<std::uint8_t> const &payload)
{
  std::vector<std::uint8_t> wire;
  EXPECT_TRUE (
      fr::slip::slip_codec ()
          .encode (layout::bytes_view_t (payload.data (), payload.size ()),
                   wire)
          .has_value ());
  return wire;
}

/// What the identity row of the mixer says.
inline pr::reply_of<mixer::INFO_t>
mixer_info (char const *model, int firmware, int layer, int form = 0)
{
  pr::reply_of<mixer::INFO_t> info;
  std::string const text (model);
  field::get<0> (info).assign (field::text_view (text.data (), text.size ()));
  field::get<1> (info) = static_cast<std::uint16_t> (firmware);
  field::get<2> (info) = static_cast<std::uint8_t> (layer);
  field::get<3> (info) = static_cast<std::uint8_t> (form);
  return info;
}

/// The messages the transport logged at `level`.
class log_capture_t
{
public:
  log_capture_t ()
      : level_ (tr::log::get_log_level ()), sink_ (tr::log::get_log_sink ())
  {
    std::lock_guard<std::mutex> const lock (mutex ());
    lines ().clear ();
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

  /// Whether a line holds `text`.
  static bool
  any_holds (std::string const &text)
  {
    return count_holding (text) != 0;
  }

  /// How many lines hold `text`.
  static std::size_t
  count_holding (std::string const &text)
  {
    std::lock_guard<std::mutex> const lock (mutex ());
    std::size_t count = 0;
    for (std::size_t i = 0; i < lines ().size (); ++i)
      if (lines ()[i].find (text) != std::string::npos)
        ++count;
    return count;
  }

private:
  static std::mutex &
  mutex ()
  {
    static std::mutex made;
    return made;
  }

  static std::vector<std::string> &
  lines ()
  {
    static std::vector<std::string> made;
    return made;
  }

  static void
  capture (tr::log::log_record_t const &record)
  {
    std::lock_guard<std::mutex> const lock (mutex ());
    lines ().push_back (record.message);
  }

  tr::log::log_level level_;
  tr::log::log_sink_t sink_;
};

/// A device of layer `Device` and a client of layer `Client` on a virtual
/// clock; the test polls the context itself.
template <typename Device, typename Client = Device> class client_bench_t
{
public:
  using client_type = pr::client<Client>;

  explicit client_bench_t (pr::client_options_t options
                           = pr::client_options_for<Client> ())
  {
    tr::testing::mock_options_t mock_options;
    mock_options.timers = clock.source ();
    tr::testing::mock_stream mock = tr::testing::make_mock_stream (
        context.get_executor (), mock_options);
    control.reset (new tr::testing::mock_stream_control (mock.control));
    mock.stream.async_open ([] (std::error_code) {});
    context.poll ();
    context.restart ();
    device.reset (new tr::testing::device_simulator<Device> (
        mock.control, fr::slip::slip_codec (), clock));
    options.dialogue.timers = clock.source ();
    client.reset (new client_type (
        pr::frame_link (tr::framed::framed_stream<fr::slip::slip_codec> (
            std::move (mock.stream), fr::slip::slip_codec ())),
        options));
    settle ();
  }

  ~client_bench_t ()
  {
    client.reset ();
    settle ();
    device.reset ();
    settle ();
  }

  client_bench_t (client_bench_t const &) = delete;
  client_bench_t &operator= (client_bench_t const &) = delete;

  /// Runs the ready handlers without moving the clock.
  void
  settle ()
  {
    tr::testing::run_until (context, clock, [] () { return false; }, ms (0));
  }

  /// Moves the clock by `by` and runs what became ready.
  void
  advance (ms by)
  {
    tr::testing::run_until (context, clock, [] () { return false; }, by);
  }

  /// Runs and moves the clock until `done` or `budget` passed.
  bool
  run_until (std::function<bool ()> const &done, ms budget = ms (60000))
  {
    return tr::testing::run_until (context, clock, done, budget);
  }

  /// The requests the device saw.
  std::uint64_t
  requests () const
  {
    return device->statistics ().requests;
  }

  asio::io_context context;
  tr::testing::virtual_clock clock;
  std::unique_ptr<tr::testing::mock_stream_control> control;
  std::unique_ptr<tr::testing::device_simulator<Device>> device;
  std::unique_ptr<client_type> client;
};

/// The same with a runner on another thread: the engine has the real clock,
/// or `virtual_timers` for a test that moves the row's timeouts itself.
template <typename Device, typename Client = Device> class thread_bench_t
{
public:
  using client_type = pr::client<Client>;

  explicit thread_bench_t (bool virtual_timers = false)
      : guard (asio::make_work_guard (context))
  {
    tr::testing::mock_stream mock
        = tr::testing::make_mock_stream (context.get_executor ());
    control.reset (new tr::testing::mock_stream_control (mock.control));
    device.reset (new tr::testing::device_simulator<Device> (
        mock.control, fr::slip::slip_codec (), clock));
    mock.stream.async_open ([] (std::error_code) {});
    context.poll ();
    context.restart ();
    pr::client_options_t options = pr::client_options_for<Client> ();
    if (virtual_timers)
      options.dialogue.timers = clock.source ();
    client.reset (new client_type (
        pr::frame_link (tr::framed::framed_stream<fr::slip::slip_codec> (
            std::move (mock.stream), fr::slip::slip_codec ())),
        options));
    runner = std::thread ([this] () { context.run (); });
  }

  ~thread_bench_t ()
  {
    client.reset ();
    device.reset ();
    stop ();
  }

  thread_bench_t (thread_bench_t const &) = delete;
  thread_bench_t &operator= (thread_bench_t const &) = delete;

  /// Ends the runner: what is queued stays unrun.
  void
  stop ()
  {
    guard.reset ();
    context.stop ();
    if (runner.joinable ())
      runner.join ();
  }

  /// Runs `body` on the runner thread and waits for it.
  template <typename Body>
  void
  on_runner (Body body)
  {
    std::promise<void> done;
    asio::post (context,
                [&] ()
                  {
                    body ();
                    done.set_value ();
                  });
    done.get_future ().wait ();
  }

  asio::io_context context;
  asio::executor_work_guard<asio::io_context::executor_type> guard;
  tr::testing::virtual_clock clock;
  std::unique_ptr<tr::testing::mock_stream_control> control;
  std::unique_ptr<tr::testing::device_simulator<Device>> device;
  std::unique_ptr<client_type> client;
  std::thread runner;
};
} // namespace lumex_client_test

#endif // !LUMEX_TESTS_APPLIED_TRANSPORT_PROTOCOL_CLIENT_TEST_SUPPORT_HPP
