// hid_device (lumex/applied/transport/hid) with asio::use_awaitable under
// C++20, over the fake hidapi seam (synthetic device).

#include <cstdint>
#include <exception>
#include <future>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "LumexFakeHidapi.hpp"
#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/hid/LumexHidDevice.hpp"
#include "lumex/applied/transport/hid/detail/LumexHidapiSeam.hpp"
#include "lumex/applied/transport/stream/LumexIoRunner.hpp"

namespace tr = lumex::applied::transport;
namespace hid = tr::hid;

TEST (LumexHidDeviceAwaitableTest,
      GivenUseAwaitable_WhenOpenSendAndReceive_ThenTheReportArrives)
{
  tr::stream::io_runner runner (1);
  std::shared_ptr<lumex_fake_hidapi::fake_seam> const seam
      = std::make_shared<lumex_fake_hidapi::fake_seam> ();
  std::shared_ptr<lumex_fake_hidapi::fake_device> const device
      = seam->add ("fake-numbered", 0x1234, 0x5678, "N1",
                   lumex_fake_hidapi::numbered_descriptor ());
  hid::hid_options_t options;
  options.path = "fake-numbered";
  hid::hid_device transport = hid::detail::make_hid_device_with (
      seam, runner.get_executor (), options);
  tr::datagram::message_t received;
  std::promise<void> done;
  asio::co_spawn (
      runner.get_executor (),
      [&] () -> asio::awaitable<void>
        {
          co_await transport.async_open (asio::use_awaitable);
          tr::datagram::message_t request;
          request.report_id = 2;
          request.payload = { 1, 2 };
          co_await transport.async_send (request, asio::use_awaitable);
          device->push_raw ({ 0x01, 0x33 });
          received = co_await transport.async_receive (asio::use_awaitable);
        },
      [&] (std::exception_ptr failure)
        {
          if (failure)
            done.set_exception (failure);
          else
            done.set_value ();
        });
  done.get_future ().get ();
  EXPECT_EQ (received.report_id, 1);
  EXPECT_EQ (received.payload, (std::vector<std::uint8_t>{ 0x33 }));
  ASSERT_EQ (device->written ().size (), 1u);
  EXPECT_EQ (device->written ()[0],
             (std::vector<std::uint8_t>{ 2, 1, 2, 0, 0 }));
  transport.close ();
  EXPECT_TRUE (hid::hid_runtime::shutdown (tr::deadline_t (5000)));
}
