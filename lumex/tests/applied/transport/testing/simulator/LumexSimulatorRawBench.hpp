// The bare mock benches of the device simulator tests: a mock stream or a
// mock datagram transport with a simulator on it, no dialogue engine; the
// test writes request frames and reads what the device sent.
#ifndef LUMEX_TESTS_APPLIED_TRANSPORT_TESTING_SIMULATOR_RAW_BENCH_HPP
#define LUMEX_TESTS_APPLIED_TRANSPORT_TESTING_SIMULATOR_RAW_BENCH_HPP

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/applied/transport/LumexTransport"
#include "lumex/core/framing/LumexFraming"
#include "lumex/tests/applied/transport/testing/simulator/LumexSimulatorTestTables.hpp"

namespace lumex_simulator_test
{
namespace tr = lumex::applied::transport;
namespace fr = lumex::core::framing;
namespace field = lumex::core::protocol::field;
namespace layout = lumex::core::protocol::layout;
namespace tables = lumex_tests::simulator_tables;
using lab = tables::lab_t;
using bytes_t = std::vector<std::uint8_t>;
using ms = std::chrono::milliseconds;
using device_t = tr::testing::device_simulator<lab>;
using tables::bytes;

inline bytes_t
framed (bytes_t const &payload)
{
  bytes_t wire;
  fr::slip::slip_codec const codec;
  EXPECT_TRUE (
      codec
          .encode (fr::codec::bytes_view_t (payload.data (), payload.size ()),
                   wire)
          .has_value ());
  return wire;
}

// The frames the device sent, from the bytes read so far.
inline std::vector<bytes_t>
frames_of (bytes_t const &wire)
{
  std::vector<bytes_t> out;
  fr::slip::slip_codec codec;
  codec.decode (fr::codec::bytes_view_t (wire.data (), wire.size ()),
                [&out] (fr::codec::bytes_view_t frame)
                  { out.push_back (bytes_t (frame.begin (), frame.end ())); });
  return out;
}

inline void
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

// A mock stream with a simulator on it; the test writes request frames and
// reads what the device sent.
class raw_bench_t
{
public:
  raw_bench_t ()
  {
    tr::testing::mock_options_t options;
    options.timers = clock.source ();
    mock.reset (new tr::testing::mock_stream (
        tr::testing::make_mock_stream (context.get_executor (), options)));
    mock->stream.async_open ([] (std::error_code) {});
    drain (context);
    device.reset (
        new device_t (mock->control, fr::slip::slip_codec (), clock));
    read_again ();
    drain (context);
  }

  ~raw_bench_t ()
  {
    stopped = true;
    mock->stream.close ();
    device.reset ();
    drain (context);
  }

  // Writes `wire` as one write.
  void
  write (bytes_t const &wire)
  {
    std::shared_ptr<bytes_t> const kept = std::make_shared<bytes_t> (wire);
    mock->stream.async_write_all (
        tr::const_bytes_t (kept->data (), kept->size ()),
        [kept] (std::error_code error, std::size_t) { EXPECT_FALSE (error); });
    drain (context);
  }

  void
  advance (ms by)
  {
    tr::testing::run_until (context, clock, [] () { return false; }, by);
  }

  std::vector<bytes_t>
  replies () const
  {
    return frames_of (received);
  }

  asio::io_context context;
  tr::testing::virtual_clock clock;
  std::unique_ptr<tr::testing::mock_stream> mock;
  std::unique_ptr<device_t> device;
  bytes_t received;
  bool stopped = false;

private:
  void
  read_again ()
  {
    mock->stream.async_read_some (
        tr::mutable_bytes_t (chunk, sizeof (chunk)),
        [this] (std::error_code error, std::size_t count)
          {
            if (error || stopped)
              return;
            received.insert (received.end (), chunk, chunk + count);
            read_again ();
          });
  }

  std::uint8_t chunk[512];
};

// A mock datagram transport with a simulator on it.
class datagram_bench_t
{
public:
  explicit datagram_bench_t (std::uint8_t report_id)
  {
    tr::testing::mock_options_t options;
    options.timers = clock.source ();
    mock.reset (new tr::testing::mock_datagram (
        tr::testing::make_mock_datagram (context.get_executor (), options)));
    mock->transport.async_open ([] (std::error_code) {});
    drain (context);
    device.reset (new device_t (mock->control, fr::slip::slip_codec (),
                                report_id, clock));
    receive_again ();
    drain (context);
  }

  ~datagram_bench_t ()
  {
    stopped = true;
    mock->transport.close ();
    device.reset ();
    drain (context);
  }

  void
  send (std::uint8_t report_id, bytes_t const &payload)
  {
    tr::datagram::message_t message;
    message.report_id = report_id;
    message.payload = payload;
    mock->transport.async_send (std::move (message), [] (std::error_code error)
                                  { EXPECT_FALSE (error); });
    drain (context);
  }

  asio::io_context context;
  tr::testing::virtual_clock clock;
  std::unique_ptr<tr::testing::mock_datagram> mock;
  std::unique_ptr<device_t> device;
  std::vector<tr::datagram::message_t> received;
  bool stopped = false;

private:
  void
  receive_again ()
  {
    mock->transport.async_receive (
        [this] (std::error_code error, tr::datagram::message_t message)
          {
            if (error || stopped)
              return;
            received.push_back (std::move (message));
            receive_again ();
          });
  }
};

inline bytes_t
info_request ()
{
  return bytes ({ 0x10 });
}
} // namespace lumex_simulator_test

#endif // !LUMEX_TESTS_APPLIED_TRANSPORT_TESTING_SIMULATOR_RAW_BENCH_HPP
