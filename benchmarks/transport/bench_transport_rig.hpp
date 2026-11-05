// bench_transport_rig.hpp
// A device simulator and a client<P> on a mock stream framed with SLIP, the
// way the tests of the generated client build them (see
// lumex/tests/applied/transport/protocol/client/LumexClientTestSupport.hpp),
// for the dialogue and the device benchmarks. Two rigs:
//
//   polled_rig_t   one thread: the benchmark polls the io_context and moves
//                  the virtual clock itself. The device always runs on the
//                  virtual clock; the engine runs on it too unless
//                  `real_timers` asks for real Asio timers.
//   threaded_rig_t a thread runs the io_context (real timers in the engine,
//                  the device on a virtual clock that stands still), for the
//                  synchronous calls that block the caller.
#ifndef LUMEX_BENCHMARKS_TRANSPORT_RIG_HPP
#define LUMEX_BENCHMARKS_TRANSPORT_RIG_HPP

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include "bench_transport_tables.hpp"
#include "lumex/applied/transport/LumexTransport"
#include "lumex/core/framing/LumexFraming"
#include "lumex/core/protocol/LumexProtocol"

namespace bench_transport
{
namespace tr = lumex::applied::transport;
namespace pr = lumex::applied::transport::protocol;
namespace fr = lumex::core::framing;
using ms = std::chrono::milliseconds;
using protocol_type = bench_protocol_t;
using client_type = pr::client<protocol_type>;
using device_type = tr::testing::device_simulator<protocol_type>;

[[noreturn]] inline void
die (char const *what)
{
  std::fprintf (stderr, "bench: %s\n", what);
  std::exit (1);
}

/// The SLIP frame that carries the sample reply of `Row`.
template <typename Row>
std::vector<std::uint8_t>
sample_reply_frame ()
{
  namespace layout = lumex::core::protocol::layout;
  layout::bytes_t message;
  layout::sample_reply_message<protocol_type, Row> (message);
  std::vector<std::uint8_t> frame;
  fr::slip::slip_codec codec;
  codec.encode (layout::bytes_view_t (message.data (), message.size ()),
                frame);
  return frame;
}

/// The pieces of one device behind a mock stream and its client; built on the
/// thread that owns the executor, before it runs.
struct link_t
{
  std::unique_ptr<device_type> device;
  std::unique_ptr<client_type> client;
  /// Without a client the stream stays here, for a reader of the benchmark.
  std::shared_ptr<tr::stream::byte_stream> raw;
};

/// Makes a device and a client on `executor`. The mock and the device use
/// `clock`; the engine uses it when `engine_on_clock`.
inline link_t
make_link (tr::executor_type const &executor,
           tr::testing::virtual_clock &clock, bool engine_on_clock,
           bool with_client = true)
{
  tr::testing::mock_options_t mock_options;
  mock_options.timers = clock.source ();
  tr::testing::mock_stream mock
      = tr::testing::make_mock_stream (executor, mock_options);
  mock.stream.async_open ([] (std::error_code) {});
  link_t link;
  link.device.reset (
      new device_type (mock.control, fr::slip::slip_codec (), clock));
  if (with_client)
    {
      pr::client_options_t options = pr::client_options_for<protocol_type> ();
      if (engine_on_clock)
        options.dialogue.timers = clock.source ();
      link.client.reset (new client_type (
          pr::frame_link (tr::framed::framed_stream<fr::slip::slip_codec> (
              std::move (mock.stream), fr::slip::slip_codec ())),
          options));
    }
  else
    link.raw
        = std::make_shared<tr::stream::byte_stream> (std::move (mock.stream));
  return link;
}

class polled_rig_t
{
public:
  explicit polled_rig_t (bool real_timers)
  {
    link_ = make_link (context_.get_executor (), clock_, !real_timers);
    settle ();
  }

  ~polled_rig_t ()
  {
    link_.client.reset ();
    settle ();
    link_.device.reset ();
    settle ();
  }

  polled_rig_t (polled_rig_t const &) = delete;
  polled_rig_t &operator= (polled_rig_t const &) = delete;

  /// Runs the ready handlers.
  void
  settle ()
  {
    for (;;)
      {
        if (context_.stopped ())
          context_.restart ();
        if (context_.poll () == 0)
          return;
      }
  }

  /// Runs until `done ()`; a wait of the virtual clock that is due is ended.
  template <typename Done>
  void
  run_until (Done const &done)
  {
    for (std::uint64_t spins = 0; !done (); ++spins)
      {
        if (spins > 50000000)
          die ("an exchange never ended");
        if (context_.stopped ())
          context_.restart ();
        if (context_.poll () == 0 && !done ())
          clock_.advance (ms (0));
      }
  }

  device_type &
  device ()
  {
    return *link_.device;
  }

  client_type &
  client ()
  {
    return *link_.client;
  }

private:
  asio::io_context context_;
  tr::testing::virtual_clock clock_;
  link_t link_;
};

class threaded_rig_t
{
public:
  threaded_rig_t () : guard_ (asio::make_work_guard (context_))
  {
    link_ = make_link (context_.get_executor (), clock_, false);
    runner_ = std::thread ([this] () { context_.run (); });
  }

  ~threaded_rig_t ()
  {
    link_.client.reset ();
    link_.device.reset ();
    guard_.reset ();
    context_.stop ();
    if (runner_.joinable ())
      runner_.join ();
  }

  threaded_rig_t (threaded_rig_t const &) = delete;
  threaded_rig_t &operator= (threaded_rig_t const &) = delete;

  device_type &
  device ()
  {
    return *link_.device;
  }

  client_type &
  client ()
  {
    return *link_.client;
  }

private:
  asio::io_context context_;
  asio::executor_work_guard<asio::io_context::executor_type> guard_;
  tr::testing::virtual_clock clock_;
  link_t link_;
  std::thread runner_;
};
} // namespace bench_transport

#endif // !LUMEX_BENCHMARKS_TRANSPORT_RIG_HPP
