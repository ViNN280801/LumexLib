// A pending read or write of a framed adapter whose handler owns the adapter
// (the "capture self" idiom) is a reference cycle: handler -> owner ->
// adapter -> hub -> handler, through a member of the hub that Asio's
// shutdown of an io_context does not reach. The shutdown asks the hub to
// destroy the handlers it keeps (an execution_context service of the
// transport), uncalled: the owner dies with them.

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <future>
#include <memory>
#include <mutex>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "LumexFramedTestSupport.hpp"
#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/framed/LumexFramedStream.hpp"
#include "lumex/applied/transport/stream/LumexByteStream.hpp"
#include "lumex/applied/transport/stream/LumexIoRunner.hpp"
#include "lumex/applied/transport/testing/LumexMockStream.hpp"

namespace
{
using namespace lumex_framed_test;

struct adapter_owner
{
  explicit adapter_owner (tr::stream::io_runner &runner)
      : mock (tr::testing::make_mock_stream (runner.get_executor (), "cycle")),
        link (std::move (mock.stream), slip_case::make (slip_case::limit ()),
              tr::framed::framed_options_t ())
  {
  }

  tr::testing::mock_stream mock;
  tr::framed::framed_stream<slip_case::codec_t> link;
};

// A device that opens and then never answers: a read or a write stays
// pending in it, as it does in a device that has stopped responding. Like
// every implementation that keeps a handler, it gives them up on the shutdown
// of the io_context.
class stalled_stream : public tr::stream::stream_impl
{
public:
  explicit stalled_stream (tr::executor_type const &executor)
      : tr::stream::stream_impl (executor, "stalled")
  {
  }

  void
  drop_kept_handlers () LUMEX_NOEXCEPT override
  {
    tr::io_handler_t read;
    tr::io_handler_t write;
    {
      std::lock_guard<std::mutex> const lock (mutex_);
      read = std::move (read_);
      write = std::move (write_);
    }
  }

  /// True once a write is pending in the device.
  bool
  write_is_pending () const
  {
    return write_pending_.load (std::memory_order_acquire);
  }

protected:
  void
  do_open (tr::status_handler_t handler) override
  {
    set_open (true);
    complete (std::move (handler), std::error_code ());
  }

  void
  do_read_some (tr::mutable_bytes_t, tr::io_handler_t handler) override
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    read_ = std::move (handler);
  }

  void
  do_write_some (tr::const_bytes_t, tr::io_handler_t handler) override
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    write_ = std::move (handler);
    write_pending_.store (true, std::memory_order_release);
  }

  void
  do_cancel () override
  {
  }

  void
  do_close () override
  {
    set_open (false);
  }

private:
  std::mutex mutex_;
  tr::io_handler_t read_;
  tr::io_handler_t write_;
  std::atomic<bool> write_pending_{ false };
};

struct stalled_owner
{
  explicit stalled_owner (tr::stream::io_runner &runner)
      : device (std::make_shared<stalled_stream> (runner.get_executor ())),
        link (tr::stream::byte_stream (device),
              slip_case::make (slip_case::limit ()),
              tr::framed::framed_options_t ())
  {
  }

  std::shared_ptr<stalled_stream> device;
  tr::framed::framed_stream<slip_case::codec_t> link;
};

// Waits until a write has reached the device (at most five seconds).
bool
wait_for_the_write (stalled_owner const &owner)
{
  for (int step = 0; step < 500 && !owner.device->write_is_pending (); ++step)
    std::this_thread::sleep_for (std::chrono::milliseconds (10));
  return owner.device->write_is_pending ();
}

// Waits until the strand of the adapter ran what was posted before.
template <typename Owner>
void
reach_the_hub (Owner &owner)
{
  std::promise<void> reached;
  asio::post (owner.link.transport ().impl ()->strand (),
              [&reached] () { reached.set_value (); });
  reached.get_future ().wait ();
}
} // namespace

TEST (
    LumexFramedHandlerCycle,
    GivenAFrameReadWhoseHandlerOwnsTheAdapter_WhenTheContextDies_ThenTheHandlerIsDroppedUncalled)
{
  std::weak_ptr<adapter_owner> weak;
  std::shared_ptr<std::atomic<int>> const calls
      = std::make_shared<std::atomic<int>> (0);
  {
    std::unique_ptr<tr::stream::io_runner> runner (
        new tr::stream::io_runner (2));
    std::shared_ptr<adapter_owner> owner
        = std::make_shared<adapter_owner> (*runner);
    weak = owner;
    ASSERT_TRUE (
        owner->link.transport ().open (tr::deadline_t (2000)).has_value ());
    owner->link.async_read_frame (
        [owner, calls] (std::error_code, tr::framed::frame_t) { ++*calls; });
    reach_the_hub (*owner);
    owner.reset ();
    runner->stop ();
    runner.reset ();
  }
  EXPECT_TRUE (weak.expired ())
      << "the handler the hub kept outlived the io_context";
  EXPECT_EQ (calls->load (), 0);
}

TEST (
    LumexFramedHandlerCycle,
    GivenARunningFrameWriteWhoseHandlerOwnsTheAdapter_WhenTheContextDies_ThenTheHandlerIsDroppedUncalled)
{
  std::weak_ptr<stalled_owner> weak;
  std::shared_ptr<std::atomic<int>> const calls
      = std::make_shared<std::atomic<int>> (0);
  {
    std::unique_ptr<tr::stream::io_runner> runner (
        new tr::stream::io_runner (2));
    std::shared_ptr<stalled_owner> owner
        = std::make_shared<stalled_owner> (*runner);
    weak = owner;
    ASSERT_TRUE (
        owner->link.transport ().open (tr::deadline_t (2000)).has_value ());
    std::vector<std::uint8_t> const payload (4, 0x11);
    owner->link.async_write_frame (tr::const_bytes_t (payload),
                                   [owner, calls] (std::error_code)
                                     { ++*calls; });
    ASSERT_TRUE (wait_for_the_write (*owner));
    owner.reset ();
    runner->stop ();
    runner.reset ();
  }
  EXPECT_TRUE (weak.expired ())
      << "the write the hub was running outlived the io_context";
  EXPECT_EQ (calls->load (), 0);
}

TEST (
    LumexFramedHandlerCycle,
    GivenAQueuedFrameWriteWhoseHandlerOwnsTheAdapter_WhenTheContextDies_ThenTheHandlerIsDroppedUncalled)
{
  std::weak_ptr<stalled_owner> weak;
  std::shared_ptr<std::atomic<int>> const calls
      = std::make_shared<std::atomic<int>> (0);
  {
    std::unique_ptr<tr::stream::io_runner> runner (
        new tr::stream::io_runner (2));
    std::shared_ptr<stalled_owner> owner
        = std::make_shared<stalled_owner> (*runner);
    weak = owner;
    ASSERT_TRUE (
        owner->link.transport ().open (tr::deadline_t (2000)).has_value ());
    std::vector<std::uint8_t> const payload (4, 0x22);
    // The first write stays in the device; the second waits behind it.
    owner->link.async_write_frame (tr::const_bytes_t (payload),
                                   [calls] (std::error_code) { ++*calls; });
    ASSERT_TRUE (wait_for_the_write (*owner));
    owner->link.async_write_frame (tr::const_bytes_t (payload),
                                   [owner, calls] (std::error_code)
                                     { ++*calls; });
    reach_the_hub (*owner);
    owner.reset ();
    runner->stop ();
    runner.reset ();
  }
  EXPECT_TRUE (weak.expired ())
      << "the write the hub had queued outlived the io_context";
  EXPECT_EQ (calls->load (), 0);
}
