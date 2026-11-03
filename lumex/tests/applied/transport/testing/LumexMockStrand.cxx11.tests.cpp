// The strand of a mock, from its control: what a simulator makes its timers
// for, so that they run beside the observer of the mock.

#include <cstddef>
#include <cstdint>
#include <system_error>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/applied/transport/LumexTransport"

namespace
{
namespace tr = lumex::applied::transport;
using bytes_t = std::vector<std::uint8_t>;
} // namespace

TEST (LumexMockStrand,
      GivenAMockStream_WhenAskedForItsStrand_ThenTheObserverRunsOnIt)
{
  asio::io_context context;
  tr::testing::mock_stream mock
      = tr::testing::make_mock_stream (context.get_executor ());
  mock.stream.async_open ([] (std::error_code) {});
  context.poll ();
  context.restart ();
  tr::testing::mock_stream_control control = mock.control;
  EXPECT_FALSE (control.get_strand ().running_in_this_thread ());
  bool inside = false;
  bool ran = false;
  control.on_write (
      [&] (bytes_t const &)
        {
          inside = control.get_strand ().running_in_this_thread ();
          ran = true;
        });
  bytes_t const data (3, 0x55);
  mock.stream.async_write_all (tr::const_bytes_t (data.data (), data.size ()),
                               [] (std::error_code, std::size_t) {});
  context.poll ();
  EXPECT_TRUE (ran);
  EXPECT_TRUE (inside);
  control.on_write (tr::testing::write_observer_t ());
}

TEST (LumexMockStrand,
      GivenAMockDatagram_WhenAskedForItsStrand_ThenTheObserverRunsOnIt)
{
  asio::io_context context;
  tr::testing::mock_datagram mock
      = tr::testing::make_mock_datagram (context.get_executor ());
  mock.transport.async_open ([] (std::error_code) {});
  context.poll ();
  context.restart ();
  tr::testing::mock_datagram_control control = mock.control;
  EXPECT_FALSE (control.get_strand ().running_in_this_thread ());
  bool inside = false;
  bool ran = false;
  control.on_send (
      [&] (tr::datagram::message_t const &)
        {
          inside = control.get_strand ().running_in_this_thread ();
          ran = true;
        });
  tr::datagram::message_t message;
  message.payload.assign (2, 0x11);
  mock.transport.async_send (message, [] (std::error_code) {});
  context.poll ();
  EXPECT_TRUE (ran);
  EXPECT_TRUE (inside);
  control.on_send (tr::testing::send_observer_t ());
}
