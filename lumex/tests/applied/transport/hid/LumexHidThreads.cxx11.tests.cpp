// The thread rules of hid_device and hid_runtime (lumex/applied/transport/hid)
// over the fake hidapi seam, after DChannel's zombie-thread pitfalls: two
// concurrent shutdowns, a write queued behind a busy executor ends by its
// deadline and by cancel, a report or a read failure after the stop never
// reaches the strand, stale results that reach it are dropped, a write and
// the close of the handle never overlap, no write after the stop, shutdown
// from a runtime thread, an open that completes during shutdown, and a
// process that exits with a device open. The devices are synthetic.

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <future>
#include <memory>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "LumexFakeHidapi.hpp"
#include "lumex/applied/transport/datagram/LumexMessageTransport.hpp"
#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/error/LumexTransportErrc.hpp"
#include "lumex/applied/transport/hid/LumexHidDevice.hpp"
#include "lumex/applied/transport/hid/detail/LumexHidRuntime.hpp"
#include "lumex/applied/transport/hid/detail/LumexHidapiSeam.hpp"
#include "lumex/applied/transport/stream/LumexIoRunner.hpp"

namespace
{
namespace tr = lumex::applied::transport;
namespace hid = tr::hid;
using lumex_fake_hidapi::bytes;
using lumex_fake_hidapi::fake_device;
using lumex_fake_hidapi::fake_handle;
using lumex_fake_hidapi::fake_seam;
using tr::deadline_t;
using tr::datagram::message_t;
using tr::error::transport_errc;
using tr::stream::io_runner;

typedef std::chrono::steady_clock clock_type;

message_t
message (std::uint8_t report_id, bytes const &payload)
{
  message_t result;
  result.report_id = report_id;
  result.payload = payload;
  return result;
}

template <typename Predicate>
bool
eventually (Predicate predicate)
{
  clock_type::time_point const end
      = clock_type::now () + std::chrono::seconds (5);
  while (!predicate ())
    {
      if (clock_type::now () > end)
        return false;
      std::this_thread::sleep_for (std::chrono::milliseconds (2));
    }
  return true;
}

long long
elapsed_ms (clock_type::time_point start)
{
  return static_cast<long long> (
      std::chrono::duration_cast<std::chrono::milliseconds> (clock_type::now ()
                                                             - start)
          .count ());
}

class LumexHidThreadsTest : public ::testing::Test
{
protected:
  LumexHidThreadsTest () : runner_ (2), seam_ (std::make_shared<fake_seam> ())
  {
  }

  void
  TearDown () override
  {
    EXPECT_TRUE (hid::hid_runtime::shutdown (deadline_t (5000)));
  }

  std::shared_ptr<fake_device>
  add (std::string const &path)
  {
    return seam_->add (path, 0x1234, 0x5678, path,
                       lumex_fake_hidapi::unnumbered_descriptor (8, 8));
  }

  hid::hid_device
  make (std::string const &path)
  {
    hid::hid_options_t options;
    options.path = path;
    return hid::detail::make_hid_device_with (seam_, runner_.get_executor (),
                                              options);
  }

  io_runner runner_;
  std::shared_ptr<fake_seam> seam_;
};

// Blocks the transport's strand: the handler of a receive runs on it.
struct strand_gate
{
  std::promise<void> entered;
  std::promise<void> release;
  std::shared_future<void> released;
  strand_gate () : released (release.get_future ().share ()) {}
};

void
block_strand (hid::hid_device &transport, fake_device &device,
              strand_gate &gate)
{
  transport.async_receive (
      [&gate] (std::error_code, message_t)
        {
          gate.entered.set_value ();
          gate.released.wait ();
        });
  device.push_raw ({ 0x01 });
  gate.entered.get_future ().wait ();
}

// The reader came back for its next slice: what it read before was posted.
void
wait_for_next_read (fake_device &device)
{
  std::size_t calls = 0;
  {
    std::lock_guard<std::mutex> const lock (device.mutex);
    calls = device.read_calls;
  }
  ASSERT_TRUE (device.wait_for ([&device, calls] ()
                                  { return device.read_calls > calls; }));
}
} // namespace

TEST_F (LumexHidThreadsTest,
        GivenTwoThreads_WhenBothShutDownWithADeviceOpen_ThenBothReturn)
{
  std::shared_ptr<fake_device> const device = add ("fake-a");
  for (int round = 0; round < 20; ++round)
    {
      hid::hid_device transport = make ("fake-a");
      ASSERT_TRUE (transport.open (deadline_t (2000)).has_value ());
      std::promise<bool> first;
      std::promise<bool> second;
      std::thread a (
          [&first] ()
            {
              first.set_value (hid::hid_runtime::shutdown (deadline_t (5000)));
            });
      std::thread b (
          [&second] ()
            {
              second.set_value (
                  hid::hid_runtime::shutdown (deadline_t (5000)));
            });
      std::future<bool> done_a = first.get_future ();
      std::future<bool> done_b = second.get_future ();
      // Before the fix the first call waited in join forever.
      ASSERT_EQ (done_a.wait_for (std::chrono::seconds (10)),
                 std::future_status::ready)
          << "round " << round;
      ASSERT_EQ (done_b.wait_for (std::chrono::seconds (10)),
                 std::future_status::ready)
          << "round " << round;
      a.join ();
      b.join ();
      EXPECT_TRUE (done_a.get ());
      EXPECT_TRUE (done_b.get ());
      hid::hid_runtime_stats_t const stats = hid::hid_runtime::stats ();
      EXPECT_EQ (stats.reader_threads, 0u);
      EXPECT_EQ (stats.worker_threads, 0u);
      EXPECT_FALSE (stats.housekeeping);
    }
}

TEST_F (LumexHidThreadsTest,
        GivenAWriteQueuedBehindStuckWrites_WhenItsDeadlinePasses_ThenTimeout)
{
  std::shared_ptr<fake_device> const a = add ("fake-a");
  std::shared_ptr<fake_device> const b = add ("fake-b");
  std::shared_ptr<fake_device> const c = add ("fake-c");
  hid::hid_device first = make ("fake-a");
  hid::hid_device second = make ("fake-b");
  hid::hid_device third = make ("fake-c");
  ASSERT_TRUE (first.open (deadline_t (2000)).has_value ());
  ASSERT_TRUE (second.open (deadline_t (2000)).has_value ());
  ASSERT_TRUE (third.open (deadline_t (2000)).has_value ());
  // Two writes stuck in the driver hold both threads of the executor.
  a->set_hold_writes (true);
  b->set_hold_writes (true);
  std::future<void> stuck_a
      = first.async_send (message (0, { 1 }), asio::use_future);
  std::future<void> stuck_b
      = second.async_send (message (0, { 2 }), asio::use_future);
  ASSERT_TRUE (a->wait_for ([&a] () { return a->write_waiting; }));
  ASSERT_TRUE (b->wait_for ([&b] () { return b->write_waiting; }));

  clock_type::time_point const start = clock_type::now ();
  tr::status_result_t const result
      = third.send (message (0, { 3 }), deadline_t (300));
  long long const took = elapsed_ms (start);
  ASSERT_FALSE (result.has_value ());
  EXPECT_TRUE (result.error () == transport_errc::timeout);
  EXPECT_LT (took, 1000);

  a->set_hold_writes (false);
  b->set_hold_writes (false);
  stuck_a.get ();
  stuck_b.get ();
  // The write taken back never reaches hidapi, and the device still works.
  ASSERT_TRUE (
      third.send (message (0, { 4 }), deadline_t (2000)).has_value ());
  std::vector<bytes> const written = c->written ();
  ASSERT_EQ (written.size (), 1u);
  EXPECT_EQ (written[0][1], 4);
}

TEST_F (LumexHidThreadsTest,
        GivenAWriteQueuedBehindStuckWrites_WhenCancelled_ThenItEndsCancelled)
{
  std::shared_ptr<fake_device> const a = add ("fake-a");
  std::shared_ptr<fake_device> const b = add ("fake-b");
  std::shared_ptr<fake_device> const c = add ("fake-c");
  hid::hid_device first = make ("fake-a");
  hid::hid_device second = make ("fake-b");
  hid::hid_device third = make ("fake-c");
  ASSERT_TRUE (first.open (deadline_t (2000)).has_value ());
  ASSERT_TRUE (second.open (deadline_t (2000)).has_value ());
  ASSERT_TRUE (third.open (deadline_t (2000)).has_value ());
  a->set_hold_writes (true);
  b->set_hold_writes (true);
  std::future<void> stuck_a
      = first.async_send (message (0, { 1 }), asio::use_future);
  std::future<void> stuck_b
      = second.async_send (message (0, { 2 }), asio::use_future);
  ASSERT_TRUE (a->wait_for ([&a] () { return a->write_waiting; }));
  ASSERT_TRUE (b->wait_for ([&b] () { return b->write_waiting; }));
  std::future<void> queued
      = third.async_send (message (0, { 3 }), asio::use_future);
  third.cancel ();
  ASSERT_EQ (queued.wait_for (std::chrono::seconds (2)),
             std::future_status::ready);
  try
    {
      queued.get ();
      FAIL () << "the queued write was not taken back";
    }
  catch (std::system_error const &failure)
    {
      EXPECT_TRUE (failure.code () == transport_errc::cancelled);
    }
  a->set_hold_writes (false);
  b->set_hold_writes (false);
  stuck_a.get ();
  stuck_b.get ();
  EXPECT_TRUE (eventually (
      [] () { return hid::hid_runtime::stats ().pending_tasks == 0; }));
  EXPECT_TRUE (c->written ().empty ());
}

TEST_F (LumexHidThreadsTest,
        GivenAReportArrivingAfterTheStop_WhenTheSliceEnds_ThenItIsNotPosted)
{
  std::shared_ptr<fake_device> const device = add ("fake-a");
  hid::hid_device transport = make ("fake-a");
  ASSERT_TRUE (transport.open (deadline_t (2000)).has_value ());
  device->set_hold_reads (true);
  ASSERT_TRUE (
      device->wait_for ([&device] () { return device->read_waiting; }));
  device->push_raw ({ 0x42 });
  transport.close ();
  ASSERT_TRUE (eventually ([&transport] () { return !transport.is_open (); }));
  // The slice in progress ends with a report after the stop flag.
  device->set_hold_reads (false);
  ASSERT_TRUE (device->wait_for ([&device] () { return device->closes > 0; }));
  // A strand round trip: whatever the reader posted before it closed the
  // handle has run.
  std::future<message_t> after = transport.async_receive (asio::use_future);
  EXPECT_THROW (after.get (), std::system_error);
  EXPECT_EQ (hid::detail::stale_events (transport), 0u);
  EXPECT_EQ (device->calls_after_close, 0u);
}

TEST_F (LumexHidThreadsTest,
        GivenAReportPostedBeforeAQueuedClose_WhenTheStrandRuns_ThenItIsDropped)
{
  std::shared_ptr<fake_device> const device = add ("fake-a");
  hid::hid_device transport = make ("fake-a");
  ASSERT_TRUE (transport.open (deadline_t (2000)).has_value ());
  strand_gate gate;
  block_strand (transport, *device, gate);
  // The close waits on the strand; the reader posts a report behind it.
  transport.close ();
  device->push_raw ({ 0x42 });
  ASSERT_TRUE (
      device->wait_for ([&device] () { return device->reads.empty (); }));
  wait_for_next_read (*device);
  gate.release.set_value ();
  EXPECT_TRUE (
      eventually ([&transport] ()
                    { return hid::detail::stale_events (transport) == 1u; }));
  // Reopened, the device never hands the old connection's report to a
  // reader. The fake keeps one open flag per device: the old reader must
  // have closed its handle first.
  ASSERT_TRUE (device->wait_for ([&device] () { return device->closes > 0; }));
  ASSERT_TRUE (transport.open (deadline_t (2000)).has_value ());
  tr::datagram::message_result_t const next
      = transport.receive (deadline_t (200));
  ASSERT_FALSE (next.has_value ());
  EXPECT_TRUE (next.error () == transport_errc::timeout) << next.error ();
}

TEST_F (
    LumexHidThreadsTest,
    GivenAReadFailurePostedBeforeAQueuedClose_WhenTheStrandRuns_ThenDropped)
{
  std::shared_ptr<fake_device> const device = add ("fake-a");
  hid::hid_device transport = make ("fake-a");
  ASSERT_TRUE (transport.open (deadline_t (2000)).has_value ());
  strand_gate gate;
  block_strand (transport, *device, gate);
  transport.close ();
  device->fail_read ();
  // The reader posted the failure and closed the handle on its way out.
  ASSERT_TRUE (device->wait_for ([&device] () { return device->closes > 0; }));
  gate.release.set_value ();
  EXPECT_TRUE (
      eventually ([&transport] ()
                    { return hid::detail::stale_events (transport) == 1u; }));
  // Reopened, a receive waits for a report: the old connection's failure
  // does not end it with disconnected.
  ASSERT_TRUE (transport.open (deadline_t (2000)).has_value ());
  tr::datagram::message_result_t const next
      = transport.receive (deadline_t (200));
  ASSERT_FALSE (next.has_value ());
  EXPECT_TRUE (next.error () == transport_errc::timeout) << next.error ();
}

TEST (LumexHidConnectionTest,
      GivenAWriteInProgress_WhenTheReaderClosesTheHandle_ThenTheCloseWaits)
{
  std::shared_ptr<fake_device> const device = std::make_shared<fake_device> ();
  device->open = true;
  std::shared_ptr<hid::detail::hid_connection> const connection
      = std::make_shared<hid::detail::hid_connection> (
          std::unique_ptr<hid::detail::hidapi_handle> (
              new fake_handle (device)),
          false, 9, 100);
  device->set_hold_writes (true);
  std::error_code written;
  std::thread writer (
      [&connection, &written] ()
        {
          std::string text;
          written = connection->write (bytes (9, 0), text);
        });
  ASSERT_TRUE (
      device->wait_for ([&device] () { return device->write_waiting; }));
  connection->request_stop ();
  std::thread closer ([&connection] () { connection->close_handle (); });
  std::this_thread::sleep_for (std::chrono::milliseconds (150));
  {
    std::lock_guard<std::mutex> const lock (device->mutex);
    EXPECT_EQ (device->closes, 0u) << "the handle closed under a write";
  }
  device->set_hold_writes (false);
  writer.join ();
  closer.join ();
  EXPECT_FALSE (written);
  EXPECT_EQ (device->closes, 1u);
  EXPECT_EQ (device->written ().size (), 1u);
  EXPECT_EQ (device->calls_after_close, 0u);
}

TEST (LumexHidConnectionTest,
      GivenAStoppedConnection_WhenWriting_ThenClosedAndNothingIsWritten)
{
  std::shared_ptr<fake_device> const device = std::make_shared<fake_device> ();
  device->open = true;
  hid::detail::hid_connection connection (
      std::unique_ptr<hid::detail::hidapi_handle> (new fake_handle (device)),
      false, 9, 100);
  connection.request_stop ();
  std::string text;
  std::error_code const error = connection.write (bytes (9, 0), text);
  EXPECT_TRUE (error == transport_errc::closed);
  EXPECT_TRUE (device->written ().empty ());
}

TEST_F (LumexHidThreadsTest,
        GivenARuntimeThread_WhenItCallsShutdown_ThenFalseAtOnce)
{
  std::shared_ptr<fake_device> const device = add ("fake-a");
  hid::hid_device transport = make ("fake-a");
  ASSERT_TRUE (transport.open (deadline_t (2000)).has_value ());
  bool result = true;
  long long took = -1;
  device->on_write = [&result, &took] ()
    {
      // On a thread of the runtime's executor: waiting would wait for
      // itself.
      clock_type::time_point const start = clock_type::now ();
      result = hid::hid_runtime::shutdown (deadline_t (1000));
      took = elapsed_ms (start);
    };
  ASSERT_TRUE (
      transport.send (message (0, { 1 }), deadline_t (5000)).has_value ());
  {
    std::lock_guard<std::mutex> const lock (device->mutex);
    device->on_write = std::function<void ()> ();
  }
  EXPECT_FALSE (result);
  EXPECT_LT (took, 300);
  EXPECT_TRUE (transport.is_open ());
}

TEST_F (LumexHidThreadsTest,
        GivenAnOpenFinishingDuringShutdown_WhenItStartsTheReader_ThenClosed)
{
  std::shared_ptr<fake_device> const device = add ("fake-a");
  device->set_hold_open (true);
  hid::hid_device transport = make ("fake-a");
  std::future<void> opened = transport.async_open (asio::use_future);
  ASSERT_TRUE (
      device->wait_for ([&device] () { return device->open_waiting; }));
  std::promise<bool> shut;
  std::thread stopper (
      [&shut] ()
        { shut.set_value (hid::hid_runtime::shutdown (deadline_t (3000))); });
  // Shutdown stops accepting readers and waits for the open in progress.
  ASSERT_TRUE (
      eventually ([] () { return !hid::hid_runtime::stats ().accepting; }));
  device->set_hold_open (false);
  try
    {
      opened.get ();
      FAIL () << "a reader started during shutdown";
    }
  catch (std::system_error const &failure)
    {
      EXPECT_TRUE (failure.code () == transport_errc::closed);
    }
  stopper.join ();
  EXPECT_TRUE (shut.get_future ().get ());
  EXPECT_TRUE (device->wait_for ([&device] () { return device->closes > 0; }));
  EXPECT_EQ (device->read_calls, 0u);
}

// A device on a context nobody runs but the test (`poll`), so that the
// context can be destroyed while the runtime's threads still work for the
// device.
struct manual_device
{
  manual_device (std::shared_ptr<fake_seam> const &seam,
                 std::string const &path)
      : context (new asio::io_context)
  {
    hid::hid_options_t options;
    options.path = path;
    transport.reset (new hid::hid_device (hid::detail::make_hid_device_with (
        seam, context->get_executor (), options)));
    impl = transport->impl ();
  }

  std::unique_ptr<asio::io_context> context;
  std::unique_ptr<hid::hid_device> transport;
  std::weak_ptr<tr::datagram::message_impl> impl;
};

// The runtime's thread opens the device and the context goes meanwhile. The
// thread posts nothing to the destroyed strand, is not the last owner of the
// device, and closes the handle it opened.
TEST_F (LumexHidThreadsTest,
        GivenAnOpenInProgress_WhenTheContextIsDestroyed_ThenNothingIsPosted)
{
  std::shared_ptr<fake_device> const device = add ("fake-a");
  device->set_hold_open (true);
  manual_device held (seam_, "fake-a");
  held.transport->async_open ([] (std::error_code)
                                { ADD_FAILURE () << "the open handler ran"; });
  held.context->poll ();
  ASSERT_TRUE (
      device->wait_for ([&device] () { return device->open_waiting; }));

  held.transport.reset ();
  held.context.reset ();
  // The shutdown of the context let the device go: the thread holds none.
  EXPECT_TRUE (held.impl.expired ());

  device->set_hold_open (false);
  EXPECT_TRUE (device->wait_for ([&device] () { return device->closes > 0; }));
  EXPECT_EQ (device->read_calls, 0u);
  EXPECT_TRUE (eventually (
      [] () { return hid::hid_runtime::stats ().reader_threads == 0; }));
}

// The open finished and its step waits in a context that is destroyed
// without running it: the step owns the connection, so the reader stops and
// the handle is closed.
TEST_F (
    LumexHidThreadsTest,
    GivenAPostedOpenStepThatNeverRuns_WhenTheContextIsDestroyed_ThenTheHandleIsClosed)
{
  std::shared_ptr<fake_device> const device = add ("fake-a");
  manual_device held (seam_, "fake-a");
  held.transport->async_open ([] (std::error_code)
                                { ADD_FAILURE () << "the open handler ran"; });
  held.context->poll ();
  ASSERT_TRUE (device->wait_for ([&device] () { return device->opens > 0; }));
  ASSERT_TRUE (eventually (
      [] () { return hid::hid_runtime::stats ().pending_tasks == 0; }));
  // The reader waits for the release finish_open gives.
  EXPECT_EQ (hid::hid_runtime::stats ().reader_threads, 1u);
  EXPECT_EQ (device->closes, 0u);

  held.transport.reset ();
  held.context.reset ();
  EXPECT_TRUE (device->wait_for ([&device] () { return device->closes > 0; }));
  EXPECT_TRUE (eventually (
      [] () { return hid::hid_runtime::stats ().reader_threads == 0; }));
  EXPECT_TRUE (held.impl.expired ());
  EXPECT_EQ (device->read_calls, 0u);
}

// What the runtime's threads pin while they work for a device is let go when
// the work ends, so a device that was opened, written to and read from is
// freed with its owner.
TEST_F (LumexHidThreadsTest,
        GivenAnOpenedDevice_WhenItIsDestroyed_ThenTheImplementationIsFreed)
{
  std::shared_ptr<fake_device> const device = add ("fake-a");
  std::weak_ptr<tr::datagram::message_impl> impl;
  {
    hid::hid_device transport = make ("fake-a");
    impl = transport.impl ();
    ASSERT_TRUE (transport.open (deadline_t (2000)).has_value ());
    ASSERT_TRUE (
        transport.send (message (0, { 1 }), deadline_t (5000)).has_value ());
    device->push_raw ({ 0x07 });
    ASSERT_TRUE (transport.receive (deadline_t (2000)).has_value ());
    EXPECT_FALSE (impl.expired ());
  }
  EXPECT_TRUE (eventually ([&impl] () { return impl.expired (); }));
  EXPECT_TRUE (device->wait_for ([&device] () { return device->closes > 0; }));
}

namespace
{
// The fake device of the exit test, never destroyed (threads still run at
// exit).
fake_device *exit_device = nullptr;
// The reference that keeps it alive. It is never released, on purpose; held
// by a global it stays reachable, so LeakSanitizer does not report the child
// process of the death test at its exit.
std::shared_ptr<fake_device> *exit_device_owner = nullptr;

// Constructed before the runtime's sentinel, so destroyed after it: checks
// that the stop flag set at static destruction ended the reader without a
// hidapi call (no new read, no close); otherwise exit code 3.
struct exit_checker
{
  ~exit_checker ()
  {
    if (exit_device == nullptr)
      return;
    // The reads stop: no new slice for 350 ms (a slice in progress, 100 ms,
    // may still end; a loaded machine may delay the last one). A reader that
    // ignores the flag starts a slice every 100 ms and never gets there.
    clock_type::time_point const end
        = clock_type::now () + std::chrono::seconds (3);
    std::size_t last = calls ();
    clock_type::time_point stable_since = clock_type::now ();
    bool stable = false;
    while (!stable && clock_type::now () < end)
      {
        std::this_thread::sleep_for (std::chrono::milliseconds (50));
        std::size_t const now_calls = calls ();
        if (now_calls != last)
          {
            last = now_calls;
            stable_since = clock_type::now ();
          }
        else
          stable = clock_type::now () - stable_since
                   >= std::chrono::milliseconds (350);
      }
    std::lock_guard<std::mutex> const lock (exit_device->mutex);
    // No hidapi close after the flag either.
    if (!stable || exit_device->closes != 0)
      std::_Exit (3);
  }

  static std::size_t
  calls ()
  {
    std::lock_guard<std::mutex> const lock (exit_device->mutex);
    return exit_device->read_calls;
  }
};

void
exit_with_device_open ()
{
  static exit_checker const checker;
  std::shared_ptr<fake_seam> const seam = std::make_shared<fake_seam> ();
  std::shared_ptr<fake_device> const device
      = seam->add ("fake-exit", 0x1234, 0x5678, "X",
                   lumex_fake_hidapi::unnumbered_descriptor (8, 8));
  // Kept alive past exit on purpose.
  exit_device_owner = new std::shared_ptr<fake_device> (device);
  exit_device = device.get ();
  io_runner *const runner = new io_runner (1);
  hid::hid_options_t options;
  options.path = "fake-exit";
  hid::hid_device *const transport
      = new hid::hid_device (hid::detail::make_hid_device_with (
          seam, runner->get_executor (), options));
  if (!transport->open (deadline_t (2000)).has_value ())
    std::_Exit (2);
  std::exit (0);
}
} // namespace

TEST (
    LumexHidExitDeathTest,
    GivenADeviceOpenAtExit_WhenTheProcessExits_ThenTheReaderStopsWithoutHidapi)
{
  EXPECT_EXIT (exit_with_device_open (), ::testing::ExitedWithCode (0), "");
}
