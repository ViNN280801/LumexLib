// The deadline of a HID open (lumex/applied/transport/hid) over the fake
// hidapi seam: an open that waits for a thread of the runtime's executor is
// taken back by cancel (), close () and the deadline of a synchronous open,
// like a queued write; an open already inside hidapi (path lookup, open_path)
// ends at once with cancelled while its worker goes on, and the late result
// closes the handle it opened, starts no reader and changes nothing. An open
// that starts while an abandoned one still runs is independent of it. The
// runtime's shutdown and the destruction of the context while an abandoned
// open runs stay correct. The devices are synthetic.

#include <chrono>
#include <condition_variable>
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
using lumex_fake_hidapi::fake_seam;
using tr::deadline_t;
using tr::datagram::message_t;
using tr::error::transport_errc;
using tr::stream::io_runner;

using clock_type = std::chrono::steady_clock;

// What a synchronous call may take beyond its deadline: the cancel and the
// completion are two hops through the transport's strand, microseconds on an
// idle machine; the tests allow 400 ms for a loaded one.
long long const KSLACK_MS = 400;
// A held operation that the code under test should not wait for is freed by
// the test after this long, so that a defect shows as a late return and not
// as a hang.
long long const KBACKSTOP_MS = 1500;

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

// Runs `action` once after `milliseconds` (or at once when the test ends
// first) on a thread of its own.
class backstop
{
public:
  backstop (long long milliseconds, std::function<void ()> action)
      : action_ (std::move (action)), cancelled_ (false),
        thread_ ([this, milliseconds] () { run (milliseconds); })
  {
  }

  ~backstop ()
  {
    {
      std::lock_guard<std::mutex> const lock (mutex_);
      cancelled_ = true;
    }
    changed_.notify_all ();
    thread_.join ();
  }

  backstop (backstop const &) = delete;
  backstop &operator= (backstop const &) = delete;

private:
  void
  run (long long milliseconds)
  {
    std::unique_lock<std::mutex> lock (mutex_);
    if (!changed_.wait_for (lock, std::chrono::milliseconds (milliseconds),
                            [this] () { return cancelled_; }))
      {
        lock.unlock ();
        action_ ();
      }
  }

  std::function<void ()> action_;
  std::mutex mutex_;
  std::condition_variable changed_;
  bool cancelled_;
  std::thread thread_;
};

class LumexHidOpenDeadlineTest : public ::testing::Test
{
protected:
  LumexHidOpenDeadlineTest ()
      : runner_ (2), seam_ (std::make_shared<fake_seam> ())
  {
  }

  void
  TearDown () override
  {
    for (std::size_t i = 0; i < devices_.size (); ++i)
      {
        devices_[i]->set_hold_writes (false);
        devices_[i]->set_hold_open (false);
      }
    EXPECT_TRUE (hid::hid_runtime::shutdown (deadline_t (5000)));
  }

  std::shared_ptr<fake_device>
  add (std::string const &path)
  {
    std::shared_ptr<fake_device> const device
        = seam_->add (path, 0x1234, 0x5678, path,
                      lumex_fake_hidapi::unnumbered_descriptor (8, 8));
    devices_.push_back (device);
    return device;
  }

  hid::hid_device
  make (std::string const &path)
  {
    hid::hid_options_t options;
    options.path = path;
    return hid::detail::make_hid_device_with (seam_, runner_.get_executor (),
                                              options);
  }

  // Two writes stuck in hidapi hold both threads of the runtime's executor:
  // whatever is posted to it now waits.
  void
  stall_workers ()
  {
    a_ = add ("fake-a");
    b_ = add ("fake-b");
    first_.reset (new hid::hid_device (make ("fake-a")));
    second_.reset (new hid::hid_device (make ("fake-b")));
    ASSERT_TRUE (first_->open (deadline_t (2000)).has_value ());
    ASSERT_TRUE (second_->open (deadline_t (2000)).has_value ());
    a_->set_hold_writes (true);
    b_->set_hold_writes (true);
    stuck_a_ = first_->async_send (message (0, { 1 }), asio::use_future);
    stuck_b_ = second_->async_send (message (0, { 2 }), asio::use_future);
    ASSERT_TRUE (a_->wait_for ([this] () { return a_->write_waiting; }));
    ASSERT_TRUE (b_->wait_for ([this] () { return b_->write_waiting; }));
  }

  void
  free_workers ()
  {
    a_->set_hold_writes (false);
    b_->set_hold_writes (false);
  }

  bool
  workers_still_stuck ()
  {
    std::lock_guard<std::mutex> const lock_a (a_->mutex);
    std::lock_guard<std::mutex> const lock_b (b_->mutex);
    return a_->write_waiting && b_->write_waiting;
  }

  void
  finish_stalled_writes ()
  {
    free_workers ();
    stuck_a_.get ();
    stuck_b_.get ();
    EXPECT_TRUE (eventually (
        [] () { return hid::hid_runtime::stats ().pending_tasks == 0; }));
  }

  io_runner runner_;
  std::shared_ptr<fake_seam> seam_;
  std::vector<std::shared_ptr<fake_device>> devices_;
  std::shared_ptr<fake_device> a_;
  std::shared_ptr<fake_device> b_;
  std::unique_ptr<hid::hid_device> first_;
  std::unique_ptr<hid::hid_device> second_;
  std::future<void> stuck_a_;
  std::future<void> stuck_b_;
};

// The error an open ends with, as a future of the code (no exception travels
// between the threads: the exception_ptr of libstdc++ counts its references
// where ThreadSanitizer cannot see it).
template <typename Transport>
std::future<std::error_code>
open_async (Transport &transport)
{
  std::shared_ptr<std::promise<std::error_code>> const done
      = std::make_shared<std::promise<std::error_code>> ();
  transport.async_open ([done] (std::error_code error)
                          { done->set_value (error); });
  return done->get_future ();
}

std::error_code
error_of (std::future<std::error_code> &future)
{
  return future.get ();
}

// A device on a context nobody runs but the test (`poll`), so that the
// context can be destroyed, or a step kept waiting in it, while the
// runtime's threads still work for the device.
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

  // Runs the steps that are ready (a context that ran out of work is
  // stopped until it is restarted).
  void
  drive ()
  {
    context->restart ();
    context->poll ();
  }

  std::unique_ptr<asio::io_context> context;
  std::unique_ptr<hid::hid_device> transport;
  std::weak_ptr<tr::datagram::message_impl> impl;
};
} // namespace

// ---- an open that waits for a thread of the runtime -----------------------

TEST_F (
    LumexHidOpenDeadlineTest,
    GivenAnOpenQueuedBehindStuckWrites_WhenItsDeadlinePasses_ThenTimeoutAndNothingRunsLater)
{
  stall_workers ();
  std::shared_ptr<fake_device> const c = add ("fake-c");
  hid::hid_device third = make ("fake-c");
  // The readers of the two devices that are open.
  std::size_t const readers = hid::hid_runtime::stats ().reader_threads;
  // A defective open would wait for a worker: they are freed after the
  // backstop so that the test ends.
  backstop const guard (KBACKSTOP_MS, [this] () { free_workers (); });

  clock_type::time_point const start = clock_type::now ();
  tr::status_result_t const result = third.open (deadline_t (100));
  long long const took = elapsed_ms (start);

  ASSERT_FALSE (result.has_value ());
  EXPECT_TRUE (result.error () == transport_errc::timeout) << result.error ();
  EXPECT_LT (took, 100 + KSLACK_MS);
  EXPECT_TRUE (workers_still_stuck () || took >= KBACKSTOP_MS);
  EXPECT_EQ (third.statistics ().timeouts, 1u);

  finish_stalled_writes ();
  // The task that was queued finds the open taken back: no hidapi call, no
  // reader, no step.
  EXPECT_EQ (c->opens, 0u);
  EXPECT_FALSE (third.is_open ());
  EXPECT_EQ (hid::hid_runtime::stats ().reader_threads, readers);
  // The device opens normally afterwards.
  ASSERT_TRUE (third.open (deadline_t (2000)).has_value ());
  EXPECT_EQ (c->opens, 1u);
  EXPECT_TRUE (third.is_open ());
}

TEST_F (
    LumexHidOpenDeadlineTest,
    GivenAnOpenQueuedBehindStuckWrites_WhenCancelled_ThenItEndsCancelledBeforeTheWorkersFree)
{
  stall_workers ();
  std::shared_ptr<fake_device> const c = add ("fake-c");
  std::weak_ptr<tr::datagram::message_impl> impl;
  {
    hid::hid_device third = make ("fake-c");
    impl = third.impl ();
    backstop const guard (KBACKSTOP_MS, [this] () { free_workers (); });
    std::future<std::error_code> opened = open_async (third);
    third.cancel ();
    ASSERT_EQ (opened.wait_for (std::chrono::milliseconds (1000)),
               std::future_status::ready);
    EXPECT_TRUE (workers_still_stuck ());
    EXPECT_TRUE (error_of (opened) == transport_errc::cancelled);
    finish_stalled_writes ();
    EXPECT_EQ (c->opens, 0u);
    EXPECT_FALSE (third.is_open ());
  }
  // The open taken back let go of what it pinned: the device is freed with
  // its owner.
  EXPECT_TRUE (eventually ([&impl] () { return impl.expired (); }));
}

TEST_F (
    LumexHidOpenDeadlineTest,
    GivenAnOpenQueuedBehindStuckWrites_WhenClosed_ThenItEndsCancelledBeforeTheWorkersFree)
{
  stall_workers ();
  std::shared_ptr<fake_device> const c = add ("fake-c");
  std::weak_ptr<tr::datagram::message_impl> impl;
  {
    hid::hid_device third = make ("fake-c");
    impl = third.impl ();
    backstop const guard (KBACKSTOP_MS, [this] () { free_workers (); });
    std::future<std::error_code> opened = open_async (third);
    third.close ();
    ASSERT_EQ (opened.wait_for (std::chrono::milliseconds (1000)),
               std::future_status::ready);
    EXPECT_TRUE (workers_still_stuck ());
    EXPECT_TRUE (error_of (opened) == transport_errc::cancelled);
    finish_stalled_writes ();
    EXPECT_EQ (c->opens, 0u);
    EXPECT_FALSE (third.is_open ());
  }
  EXPECT_TRUE (eventually ([&impl] () { return impl.expired (); }));
}

// ---- an open already inside hidapi -----------------------------------------

TEST_F (
    LumexHidOpenDeadlineTest,
    GivenAnOpenInsideOpenPath_WhenItsDeadlinePasses_ThenTimeoutAndTheLateHandleIsClosed)
{
  std::shared_ptr<fake_device> const device = add ("fake-a");
  device->set_hold_open (true);
  hid::hid_device transport = make ("fake-a");
  backstop const guard (KBACKSTOP_MS,
                        [&device] () { device->set_hold_open (false); });

  clock_type::time_point const start = clock_type::now ();
  tr::status_result_t const result = transport.open (deadline_t (100));
  long long const took = elapsed_ms (start);

  ASSERT_FALSE (result.has_value ());
  EXPECT_TRUE (result.error () == transport_errc::timeout) << result.error ();
  EXPECT_LT (took, 100 + KSLACK_MS);
  {
    // The worker is still inside open_path when the caller has its answer.
    std::lock_guard<std::mutex> const lock (device->mutex);
    EXPECT_TRUE (device->open_waiting || took >= KBACKSTOP_MS);
  }
  device->push_raw ({ 0x11 });

  device->set_hold_open (false);
  // The late handle is closed by the worker that opened it; no reader reads
  // from it and the device never becomes open.
  ASSERT_TRUE (
      device->wait_for ([&device] () { return device->closes == 1; }));
  EXPECT_TRUE (eventually (
      [] () { return hid::hid_runtime::stats ().pending_tasks == 0; }));
  EXPECT_EQ (device->opens, 1u);
  EXPECT_EQ (device->read_calls, 0u);
  EXPECT_FALSE (device->open);
  EXPECT_FALSE (transport.is_open ());
  EXPECT_EQ (hid::hid_runtime::stats ().reader_threads, 0u);
  EXPECT_EQ (hid::detail::stale_events (transport), 0u);

  // A later open works, and the report the abandoned open did not read is
  // still there.
  ASSERT_TRUE (transport.open (deadline_t (2000)).has_value ());
  EXPECT_EQ (device->opens, 2u);
  tr::datagram::message_result_t const report
      = transport.receive (deadline_t (2000));
  ASSERT_TRUE (report.has_value ());
  EXPECT_EQ (report.value ().payload, (bytes{ 0x11 }));
}

TEST_F (
    LumexHidOpenDeadlineTest,
    GivenAnOpenInsideOpenPath_WhenCancelled_ThenItEndsCancelledAtOnceAndTheDeviceIsFreed)
{
  std::shared_ptr<fake_device> const device = add ("fake-a");
  device->set_hold_open (true);
  std::weak_ptr<tr::datagram::message_impl> impl;
  {
    hid::hid_device transport = make ("fake-a");
    impl = transport.impl ();
    backstop const guard (KBACKSTOP_MS,
                          [&device] () { device->set_hold_open (false); });
    std::future<std::error_code> opened = open_async (transport);
    ASSERT_TRUE (
        device->wait_for ([&device] () { return device->open_waiting; }));
    transport.cancel ();
    ASSERT_EQ (opened.wait_for (std::chrono::milliseconds (1000)),
               std::future_status::ready);
    {
      std::lock_guard<std::mutex> const lock (device->mutex);
      EXPECT_TRUE (device->open_waiting);
    }
    EXPECT_TRUE (error_of (opened) == transport_errc::cancelled);
    device->set_hold_open (false);
    ASSERT_TRUE (
        device->wait_for ([&device] () { return device->closes == 1; }));
    EXPECT_FALSE (transport.is_open ());
    EXPECT_EQ (device->read_calls, 0u);
  }
  // The abandoned open's piece of work ended when its late result arrived:
  // the device is freed with its owner.
  EXPECT_TRUE (eventually ([&impl] () { return impl.expired (); }));
}

TEST_F (
    LumexHidOpenDeadlineTest,
    GivenAnOpenInsideOpenPath_WhenClosed_ThenItEndsCancelledAtOnceAndTheLateHandleIsClosed)
{
  std::shared_ptr<fake_device> const device = add ("fake-a");
  device->set_hold_open (true);
  hid::hid_device transport = make ("fake-a");
  backstop const guard (KBACKSTOP_MS,
                        [&device] () { device->set_hold_open (false); });
  std::future<std::error_code> opened = open_async (transport);
  ASSERT_TRUE (
      device->wait_for ([&device] () { return device->open_waiting; }));
  transport.close ();
  ASSERT_EQ (opened.wait_for (std::chrono::milliseconds (1000)),
             std::future_status::ready);
  {
    std::lock_guard<std::mutex> const lock (device->mutex);
    EXPECT_TRUE (device->open_waiting);
  }
  EXPECT_TRUE (error_of (opened) == transport_errc::cancelled);
  device->set_hold_open (false);
  ASSERT_TRUE (
      device->wait_for ([&device] () { return device->closes == 1; }));
  EXPECT_FALSE (transport.is_open ());
  EXPECT_EQ (device->read_calls, 0u);
  EXPECT_TRUE (eventually (
      [] () { return hid::hid_runtime::stats ().reader_threads == 0; }));
}

// The finder of the device hangs (resolve_path runs on the open's worker).
TEST_F (
    LumexHidOpenDeadlineTest,
    GivenAHangingPathResolver_WhenTheDeadlinePasses_ThenTimeoutAndNothingIsOpened)
{
  std::shared_ptr<fake_device> const device = add ("fake-a");
  std::promise<void> entered;
  std::promise<void> release;
  std::shared_future<void> const released = release.get_future ().share ();
  hid::hid_options_t options;
  options.path = "unused";
  options.resolve_path = [&entered, released] ()
    {
      entered.set_value ();
      released.wait ();
      return std::string ("fake-a");
    };
  hid::hid_device transport = hid::detail::make_hid_device_with (
      seam_, runner_.get_executor (), options);
  std::once_flag once;
  backstop const guard (
      KBACKSTOP_MS, [&release, &once] ()
        { std::call_once (once, [&release] () { release.set_value (); }); });

  clock_type::time_point const start = clock_type::now ();
  tr::status_result_t const result = transport.open (deadline_t (100));
  long long const took = elapsed_ms (start);

  ASSERT_FALSE (result.has_value ());
  EXPECT_TRUE (result.error () == transport_errc::timeout) << result.error ();
  EXPECT_LT (took, 100 + KSLACK_MS);
  std::call_once (once, [&release] () { release.set_value (); });
  EXPECT_TRUE (eventually (
      [] () { return hid::hid_runtime::stats ().pending_tasks == 0; }));
  // The worker finds the open abandoned when the finder returns: it does not
  // open the device it found.
  EXPECT_EQ (device->opens, 0u);
  EXPECT_FALSE (transport.is_open ());
}

// ---- an open that starts while an abandoned one still runs -----------------

TEST_F (
    LumexHidOpenDeadlineTest,
    GivenAnAbandonedOpenStillInsideHidapi_WhenTheDeviceOpensAgain_ThenOnlyTheNewOpenCounts)
{
  std::shared_ptr<fake_device> const device = add ("fake-a");
  device->set_hold_open (true);
  hid::hid_device transport = make ("fake-a");
  backstop const guard (KBACKSTOP_MS * 2,
                        [&device] () { device->set_hold_open (false); });
  std::future<std::error_code> first = open_async (transport);
  ASSERT_TRUE (
      device->wait_for ([&device] () { return device->open_waiting; }));
  transport.cancel ();
  ASSERT_EQ (first.wait_for (std::chrono::milliseconds (1000)),
             std::future_status::ready);
  EXPECT_TRUE (error_of (first) == transport_errc::cancelled);

  // The new open takes the other thread of the executor and waits in
  // open_path too; the old one has not returned.
  std::future<std::error_code> second = open_async (transport);
  ASSERT_TRUE (
      device->wait_for ([&device] () { return device->open_waiters == 2; }));
  device->set_hold_open (false);
  ASSERT_EQ (second.wait_for (std::chrono::seconds (5)),
             std::future_status::ready);
  EXPECT_FALSE (error_of (second));
  EXPECT_TRUE (transport.is_open ());
  // The late handle of the first is closed, the second one lives and reads.
  ASSERT_TRUE (
      device->wait_for ([&device] () { return device->closes == 1; }));
  EXPECT_EQ (device->opens, 2u);
  EXPECT_TRUE (device->open);
  device->push_raw ({ 0x22 });
  tr::datagram::message_result_t const report
      = transport.receive (deadline_t (2000));
  ASSERT_TRUE (report.has_value ());
  EXPECT_EQ (report.value ().payload, (bytes{ 0x22 }));
  EXPECT_EQ (hid::detail::stale_events (transport), 0u);
  EXPECT_EQ (hid::hid_runtime::stats ().reader_threads, 1u);
}

// The cancel arrives on the strand after the worker finished everything but
// before the step that carries the result: the step finds the open ended.
TEST_F (
    LumexHidOpenDeadlineTest,
    GivenAFinishedOpenWhoseStepFollowsACancel_WhenTheStrandRuns_ThenTheStepIsDroppedAndTheHandleClosed)
{
  std::shared_ptr<fake_device> const device = add ("fake-a");
  device->set_hold_open (true);
  manual_device held (seam_, "fake-a");
  std::error_code seen;
  int calls = 0;
  held.transport->async_open (
      [&seen, &calls] (std::error_code error)
        {
          seen = error;
          ++calls;
        });
  held.drive ();
  ASSERT_TRUE (
      device->wait_for ([&device] () { return device->open_waiting; }));
  // Queued on the strand, not run: the context is driven by the test.
  held.transport->cancel ();
  device->set_hold_open (false);
  // The worker found the open not abandoned yet: it opened the device,
  // started the reader and posted its step behind the cancel.
  ASSERT_TRUE (eventually (
      [&device] ()
        {
          std::lock_guard<std::mutex> const lock (device->mutex);
          return device->opens == 1;
        }));
  ASSERT_TRUE (eventually (
      [] ()
        {
          hid::hid_runtime_stats_t const stats = hid::hid_runtime::stats ();
          return stats.pending_tasks == 0 && stats.reader_threads == 1;
        }));
  EXPECT_EQ (calls, 0);

  held.drive ();
  EXPECT_EQ (calls, 1);
  EXPECT_TRUE (seen == transport_errc::cancelled) << seen;
  EXPECT_FALSE (held.transport->is_open ());
  // The reader never got its release: it stopped and closed the handle.
  ASSERT_TRUE (
      device->wait_for ([&device] () { return device->closes == 1; }));
  EXPECT_TRUE (eventually (
      [] () { return hid::hid_runtime::stats ().reader_threads == 0; }));
  EXPECT_EQ (device->read_calls, 0u);
  EXPECT_EQ (hid::detail::stale_events (*held.transport), 0u);
  held.transport.reset ();
  held.drive ();
  EXPECT_TRUE (eventually ([&held] () { return held.impl.expired (); }));
}

// ---- the runtime and the context while an abandoned open runs --------------

TEST_F (
    LumexHidOpenDeadlineTest,
    GivenAnAbandonedOpenStillRunning_WhenTheRuntimeShutsDown_ThenFalseAtTheDeadlineAndTrueAfterwards)
{
  std::shared_ptr<fake_device> const device = add ("fake-a");
  device->set_hold_open (true);
  hid::hid_device transport = make ("fake-a");
  std::future<std::error_code> opened = open_async (transport);
  ASSERT_TRUE (
      device->wait_for ([&device] () { return device->open_waiting; }));
  transport.cancel ();
  ASSERT_EQ (opened.wait_for (std::chrono::seconds (2)),
             std::future_status::ready);

  clock_type::time_point const start = clock_type::now ();
  bool const stopped = hid::hid_runtime::shutdown (deadline_t (200));
  long long const took = elapsed_ms (start);
  // The worker is inside hidapi: shutdown reports it and goes back to
  // accepting work.
  EXPECT_FALSE (stopped);
  EXPECT_LT (took, 200 + KSLACK_MS);
  EXPECT_TRUE (hid::hid_runtime::stats ().accepting);

  device->set_hold_open (false);
  ASSERT_TRUE (
      device->wait_for ([&device] () { return device->closes == 1; }));
  EXPECT_TRUE (hid::hid_runtime::shutdown (deadline_t (3000)));
  hid::hid_runtime_stats_t const stats = hid::hid_runtime::stats ();
  EXPECT_EQ (stats.reader_threads, 0u);
  EXPECT_EQ (stats.worker_threads, 0u);
  EXPECT_EQ (stats.pending_tasks, 0u);
  EXPECT_EQ (device->read_calls, 0u);
}

TEST_F (
    LumexHidOpenDeadlineTest,
    GivenAnAbandonedOpenStillRunning_WhenTheContextIsDestroyed_ThenNothingIsPostedAndTheHandleIsClosed)
{
  std::shared_ptr<fake_device> const device = add ("fake-a");
  device->set_hold_open (true);
  manual_device held (seam_, "fake-a");
  int calls = 0;
  std::error_code seen;
  held.transport->async_open (
      [&seen, &calls] (std::error_code error)
        {
          seen = error;
          ++calls;
        });
  held.drive ();
  ASSERT_TRUE (
      device->wait_for ([&device] () { return device->open_waiting; }));
  held.transport->cancel ();
  held.drive ();
  EXPECT_EQ (calls, 1);
  EXPECT_TRUE (seen == transport_errc::cancelled) << seen;

  held.transport.reset ();
  held.context.reset ();
  // The context's shutdown let the device go; the worker holds none.
  EXPECT_TRUE (held.impl.expired ());

  device->set_hold_open (false);
  EXPECT_TRUE (
      device->wait_for ([&device] () { return device->closes == 1; }));
  EXPECT_EQ (device->read_calls, 0u);
  EXPECT_TRUE (eventually (
      [] () { return hid::hid_runtime::stats ().pending_tasks == 0; }));
  EXPECT_EQ (hid::hid_runtime::stats ().reader_threads, 0u);
}

TEST_F (
    LumexHidOpenDeadlineTest,
    GivenAnOpenQueuedBehindStuckWrites_WhenCancelledAndTheContextIsDestroyed_ThenTheTaskTakenBackPostsNothing)
{
  stall_workers ();
  std::shared_ptr<fake_device> const c = add ("fake-c");
  manual_device held (seam_, "fake-c");
  std::size_t const readers = hid::hid_runtime::stats ().reader_threads;
  held.transport->async_open ([] (std::error_code) {});
  held.drive ();
  held.transport->cancel ();
  held.drive ();
  held.transport.reset ();
  held.context.reset ();
  EXPECT_TRUE (held.impl.expired ());
  finish_stalled_writes ();
  EXPECT_EQ (c->opens, 0u);
  EXPECT_EQ (hid::hid_runtime::stats ().reader_threads, readers);
}
