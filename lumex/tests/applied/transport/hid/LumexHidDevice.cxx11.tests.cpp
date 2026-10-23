// hid_device (lumex/applied/transport/hid) over the fake hidapi seam: the
// report ID both ways for a numbered and an unnumbered device (the
// falsifier: either guess fails one of the two), padding, report_too_large,
// unknown_report_id, open by path and by VID/PID/serial, the descriptor
// failures, the reader thread (stop within a slice, the handle closed on the
// reader thread, no hidapi call after it), a device destroyed inside its own
// handler, hid_runtime (shutdown with a device open, the owner object, a
// write stuck in the driver), cancellation, the token styles, the
// statistics, the read failure and the report queue bound. The devices,
// descriptors and reports are synthetic.

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
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
using tr::datagram::message_result_t;
using tr::datagram::message_t;
using tr::error::transport_errc;
using tr::stream::io_runner;

// Synthetic IDs (no real product).
std::uint16_t const KVENDOR = 0x1234;
std::uint16_t const KPRODUCT = 0x5678;

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
eventually (Predicate predicate,
            std::chrono::milliseconds limit = std::chrono::milliseconds (5000))
{
  std::chrono::steady_clock::time_point const end
      = std::chrono::steady_clock::now () + limit;
  while (!predicate ())
    {
      if (std::chrono::steady_clock::now () > end)
        return false;
      std::this_thread::sleep_for (std::chrono::milliseconds (2));
    }
  return true;
}

bool
runtime_idle ()
{
  hid::hid_runtime_stats_t const stats = hid::hid_runtime::stats ();
  return stats.reader_threads == 0 && stats.pending_tasks == 0
         && stats.open_devices == 0;
}

class LumexHidDeviceTest : public ::testing::Test
{
protected:
  LumexHidDeviceTest () : runner_ (2), seam_ (std::make_shared<fake_seam> ())
  {
  }

  void
  TearDown () override
  {
    // Every test leaves the runtime as it found it.
    EXPECT_TRUE (hid::hid_runtime::shutdown (deadline_t (5000)));
    // open_devices drops when the strand runs the close that shutdown
    // posted: it can lag right after shutdown returns.
    EXPECT_TRUE (eventually (runtime_idle));
  }

  hid::hid_device
  make (hid::hid_options_t const &options)
  {
    return hid::detail::make_hid_device_with (seam_, runner_.get_executor (),
                                              options);
  }

  hid::hid_device
  make (std::string const &path)
  {
    hid::hid_options_t options;
    options.path = path;
    return make (options);
  }

  std::shared_ptr<fake_device>
  numbered (std::string const &path = "fake-numbered")
  {
    return seam_->add (path, KVENDOR, KPRODUCT, "N1",
                       lumex_fake_hidapi::numbered_descriptor ());
  }

  std::shared_ptr<fake_device>
  unnumbered (std::string const &path = "fake-unnumbered")
  {
    return seam_->add (path, KVENDOR, KPRODUCT, "U1",
                       lumex_fake_hidapi::unnumbered_descriptor (8, 8));
  }

  io_runner runner_;
  std::shared_ptr<fake_seam> seam_;
};
} // namespace

TEST_F (LumexHidDeviceTest,
        GivenANumberedDevice_WhenAReportArrives_ThenItCarriesItsReportId)
{
  std::shared_ptr<fake_device> const device = numbered ();
  hid::hid_device transport = make ("fake-numbered");
  ASSERT_TRUE (transport.open (deadline_t (2000)).has_value ());
  device->push_raw ({ 0x02, 0xAA, 0xBB });
  message_result_t const received = transport.receive (deadline_t (2000));
  ASSERT_TRUE (received.has_value ());
  EXPECT_EQ (received.value ().report_id, 2);
  EXPECT_EQ (received.value ().payload, (bytes{ 0xAA, 0xBB }));
}

TEST_F (LumexHidDeviceTest,
        GivenAnUnnumberedDevice_WhenAReportArrives_ThenNoByteIsTakenForAnId)
{
  std::shared_ptr<fake_device> const device = unnumbered ();
  hid::hid_device transport = make ("fake-unnumbered");
  ASSERT_TRUE (transport.open (deadline_t (2000)).has_value ());
  device->push_raw ({ 0x02, 0xAA, 0xBB });
  message_result_t const received = transport.receive (deadline_t (2000));
  ASSERT_TRUE (received.has_value ());
  EXPECT_EQ (received.value ().report_id, 0);
  EXPECT_EQ (received.value ().payload, (bytes{ 0x02, 0xAA, 0xBB }));
}

TEST_F (LumexHidDeviceTest,
        GivenANumberedDevice_WhenSending_ThenTheIdLeadsAndThePayloadIsPadded)
{
  std::shared_ptr<fake_device> const device = numbered ();
  hid::hid_device transport = make ("fake-numbered");
  ASSERT_TRUE (transport.open (deadline_t (2000)).has_value ());
  ASSERT_TRUE (transport.send (message (2, { 1, 2, 3 }), deadline_t (2000))
                   .has_value ());
  ASSERT_TRUE (
      transport
          .send (message (1, { 1, 2, 3, 4, 5, 6, 7, 8 }), deadline_t (2000))
          .has_value ());
  std::vector<bytes> const written = device->written ();
  ASSERT_EQ (written.size (), 2u);
  EXPECT_EQ (written[0], (bytes{ 2, 1, 2, 3, 0 }));
  EXPECT_EQ (written[1], (bytes{ 1, 1, 2, 3, 4, 5, 6, 7, 8 }));
}

TEST_F (LumexHidDeviceTest,
        GivenAnUnnumberedDevice_WhenSending_ThenAZeroIdLeadsAndPaddingFollows)
{
  std::shared_ptr<fake_device> const device = unnumbered ();
  hid::hid_device transport = make ("fake-unnumbered");
  ASSERT_TRUE (transport.open (deadline_t (2000)).has_value ());
  ASSERT_TRUE (
      transport.send (message (0, { 7, 8 }), deadline_t (2000)).has_value ());
  std::vector<bytes> const written = device->written ();
  ASSERT_EQ (written.size (), 1u);
  EXPECT_EQ (written[0], (bytes{ 0, 7, 8, 0, 0, 0, 0, 0, 0 }));
}

TEST_F (LumexHidDeviceTest,
        GivenAPayloadLargerThanTheReport_WhenSending_ThenReportTooLarge)
{
  std::shared_ptr<fake_device> const device = numbered ();
  std::shared_ptr<fake_device> const plain = unnumbered ();
  hid::hid_device transport = make ("fake-numbered");
  hid::hid_device other = make ("fake-unnumbered");
  ASSERT_TRUE (transport.open (deadline_t (2000)).has_value ());
  ASSERT_TRUE (other.open (deadline_t (2000)).has_value ());
  tr::status_result_t const too_large
      = transport.send (message (2, { 1, 2, 3, 4, 5 }), deadline_t (2000));
  ASSERT_FALSE (too_large.has_value ());
  EXPECT_TRUE (too_large.error () == transport_errc::report_too_large);
  tr::status_result_t const too_large_plain
      = other.send (message (0, bytes (9, 0x11)), deadline_t (2000));
  ASSERT_FALSE (too_large_plain.has_value ());
  EXPECT_TRUE (too_large_plain.error () == transport_errc::report_too_large);
  EXPECT_TRUE (device->written ().empty ());
  EXPECT_TRUE (plain->written ().empty ());
  EXPECT_EQ (
      transport.statistics ().errors_of (transport_errc::report_too_large),
      1u);
}

TEST_F (LumexHidDeviceTest,
        GivenAReportIdTheDeviceDoesNotHave_WhenSending_ThenUnknownReportId)
{
  std::shared_ptr<fake_device> const device = numbered ();
  std::shared_ptr<fake_device> const plain = unnumbered ();
  hid::hid_device transport = make ("fake-numbered");
  hid::hid_device other = make ("fake-unnumbered");
  ASSERT_TRUE (transport.open (deadline_t (2000)).has_value ());
  ASSERT_TRUE (other.open (deadline_t (2000)).has_value ());
  // 9: no such report; 3: an input report only; 0: the device numbers.
  std::uint8_t const ids[] = { 9, 3, 0 };
  for (std::size_t i = 0; i < 3; ++i)
    {
      tr::status_result_t const result
          = transport.send (message (ids[i], { 1 }), deadline_t (2000));
      ASSERT_FALSE (result.has_value ());
      EXPECT_TRUE (result.error () == transport_errc::unknown_report_id)
          << static_cast<int> (ids[i]);
    }
  // A device that numbers none has report 0 only.
  tr::status_result_t const plain_result
      = other.send (message (1, { 1 }), deadline_t (2000));
  ASSERT_FALSE (plain_result.has_value ());
  EXPECT_TRUE (plain_result.error () == transport_errc::unknown_report_id);
  EXPECT_TRUE (device->written ().empty ());
  EXPECT_TRUE (plain->written ().empty ());
}

TEST_F (LumexHidDeviceTest,
        GivenAnUnreadableOrBadDescriptor_WhenOpening_ThenBadReportDescriptor)
{
  std::shared_ptr<fake_device> const empty
      = seam_->add ("fake-empty", KVENDOR, KPRODUCT, "E1", bytes ());
  std::shared_ptr<fake_device> const truncated = seam_->add (
      "fake-truncated", KVENDOR, KPRODUCT, "T1", bytes{ 0x75, 0x08, 0x95 });
  hid::hid_device first = make ("fake-empty");
  hid::hid_device second = make ("fake-truncated");
  tr::status_result_t const unreadable = first.open (deadline_t (2000));
  tr::status_result_t const bad = second.open (deadline_t (2000));
  ASSERT_FALSE (unreadable.has_value ());
  ASSERT_FALSE (bad.has_value ());
  EXPECT_TRUE (unreadable.error () == transport_errc::bad_report_descriptor);
  EXPECT_TRUE (bad.error () == transport_errc::bad_report_descriptor);
  EXPECT_FALSE (first.is_open ());
  // The handle is closed again and no reader started.
  EXPECT_EQ (empty->closes, 1u);
  EXPECT_EQ (truncated->closes, 1u);
  EXPECT_EQ (empty->read_calls, 0u);
  EXPECT_EQ (hid::hid_runtime::stats ().reader_threads, 0u);
}

TEST_F (LumexHidDeviceTest,
        GivenVendorProductAndSerial_WhenOpening_ThenTheMatchingDeviceOpens)
{
  std::shared_ptr<fake_device> const first
      = seam_->add ("fake-a", KVENDOR, KPRODUCT, "S1",
                    lumex_fake_hidapi::unnumbered_descriptor (8, 8));
  std::shared_ptr<fake_device> const second
      = seam_->add ("fake-b", KVENDOR, KPRODUCT, "S2",
                    lumex_fake_hidapi::unnumbered_descriptor (8, 8));
  hid::hid_options_t options;
  options.vendor_id = KVENDOR;
  options.product_id = KPRODUCT;
  options.serial_number = "S2";
  hid::hid_device transport = make (options);
  ASSERT_TRUE (transport.open (deadline_t (2000)).has_value ());
  EXPECT_EQ (first->opens, 0u);
  EXPECT_EQ (second->opens, 1u);
  EXPECT_EQ (transport.name (), "hid 1234:5678");

  options.serial_number = "S9";
  hid::hid_device missing = make (options);
  tr::status_result_t const result = missing.open (deadline_t (2000));
  ASSERT_FALSE (result.has_value ());
  EXPECT_TRUE (result.error () == std::errc::no_such_device);
}

TEST_F (LumexHidDeviceTest,
        GivenAFilter_WhenEnumerating_ThenOnlyTheMatchingDevicesAreListed)
{
  seam_->add ("fake-a", KVENDOR, KPRODUCT, "S1", bytes ());
  seam_->add ("fake-b", KVENDOR, 0x0002, "S2", bytes ());
  seam_->add ("fake-c", 0x4321, KPRODUCT, "S3", bytes ());
  hid::hid_filter_t filter;
  EXPECT_EQ (hid::detail::enumerate_hid_devices_with (*seam_, filter).size (),
             3u);
  filter.vendor_id = KVENDOR;
  EXPECT_EQ (hid::detail::enumerate_hid_devices_with (*seam_, filter).size (),
             2u);
  filter.product_id = 0x0002;
  std::vector<hid::hid_device_info_t> const by_product
      = hid::detail::enumerate_hid_devices_with (*seam_, filter);
  ASSERT_EQ (by_product.size (), 1u);
  EXPECT_EQ (by_product[0].path, "fake-b");
  hid::hid_filter_t by_serial;
  by_serial.serial_number = "S3";
  ASSERT_EQ (
      hid::detail::enumerate_hid_devices_with (*seam_, by_serial).size (), 1u);
  hid::hid_filter_t by_path;
  by_path.path = "fake-a";
  std::vector<hid::hid_device_info_t> const found
      = hid::detail::enumerate_hid_devices_with (*seam_, by_path);
  ASSERT_EQ (found.size (), 1u);
  EXPECT_EQ (found[0].serial_number, "S1");
}

TEST_F (LumexHidDeviceTest,
        GivenAnOpenDevice_WhenClosed_ThenTheReaderEndsWithinOneSlice)
{
  std::shared_ptr<fake_device> const device = unnumbered ();
  hid::hid_options_t options;
  options.path = "fake-unnumbered";
  options.read_slice = deadline_t (100);
  hid::hid_device transport = make (options);
  ASSERT_TRUE (transport.open (deadline_t (2000)).has_value ());
  ASSERT_TRUE (
      device->wait_for ([&device] () { return device->read_calls > 0; }));
  EXPECT_EQ (device->last_slice, 100);
  EXPECT_EQ (hid::hid_runtime::stats ().reader_threads, 1u);
  std::chrono::steady_clock::time_point const start
      = std::chrono::steady_clock::now ();
  transport.close ();
  ASSERT_TRUE (device->wait_for ([&device] () { return device->closes > 0; }));
  std::chrono::milliseconds const took
      = std::chrono::duration_cast<std::chrono::milliseconds> (
          std::chrono::steady_clock::now () - start);
  // One slice and the scheduling of the close.
  EXPECT_LT (took.count (), 400);
  EXPECT_TRUE (device->closed_on_reading_thread);
  EXPECT_TRUE (eventually (
      [] () { return hid::hid_runtime::stats ().reader_threads == 0; }));
  EXPECT_EQ (device->closes, 1u);
  EXPECT_EQ (device->calls_after_close, 0u);
}

TEST_F (LumexHidDeviceTest,
        GivenAReceiveHandler_WhenItDestroysItsDevice_ThenEverythingEndsClean)
{
  std::shared_ptr<fake_device> const device = unnumbered ();
  std::unique_ptr<hid::hid_device> holder (
      new hid::hid_device (make ("fake-unnumbered")));
  ASSERT_TRUE (holder->open (deadline_t (2000)).has_value ());
  std::promise<message_t> done;
  holder->async_receive (
      [&holder, &done] (std::error_code error, message_t received)
        {
          holder.reset ();
          if (error)
            done.set_exception (
                std::make_exception_ptr (std::system_error (error)));
          else
            done.set_value (received);
        });
  device->push_raw ({ 5, 6 });
  message_t const received = done.get_future ().get ();
  EXPECT_EQ (received.payload, (bytes{ 5, 6 }));
  ASSERT_TRUE (device->wait_for ([&device] () { return device->closes > 0; }));
  EXPECT_TRUE (eventually ([] () { return runtime_idle (); }));
  EXPECT_EQ (device->calls_after_close, 0u);
}

TEST_F (LumexHidDeviceTest,
        GivenAnOpenDevice_WhenTheRuntimeShutsDown_ThenAllThreadsAreJoined)
{
  std::shared_ptr<fake_device> const device = unnumbered ();
  hid::hid_device transport = make ("fake-unnumbered");
  ASSERT_TRUE (transport.open (deadline_t (2000)).has_value ());
  std::future<message_t> pending = transport.async_receive (asio::use_future);
  EXPECT_EQ (hid::hid_runtime::stats ().open_devices, 1u);
  ASSERT_TRUE (hid::hid_runtime::shutdown (deadline_t (2000)));
  hid::hid_runtime_stats_t const stats = hid::hid_runtime::stats ();
  EXPECT_EQ (stats.reader_threads, 0u);
  EXPECT_EQ (stats.worker_threads, 0u);
  EXPECT_FALSE (stats.housekeeping);
  EXPECT_EQ (device->closes, 1u);
  try
    {
      pending.get ();
      FAIL () << "the pending receive succeeded";
    }
  catch (std::system_error const &failure)
    {
      EXPECT_TRUE (failure.code () == transport_errc::cancelled);
    }
  EXPECT_TRUE (eventually (
      [] () { return hid::hid_runtime::stats ().open_devices == 0; }));
  tr::status_result_t const after
      = transport.send (message (0, { 1 }), deadline_t (2000));
  ASSERT_FALSE (after.has_value ());
  EXPECT_TRUE (after.error () == transport_errc::not_open);
  // The runtime starts again for the next open.
  ASSERT_TRUE (transport.open (deadline_t (2000)).has_value ());
  EXPECT_EQ (device->opens, 2u);
}

TEST_F (
    LumexHidDeviceTest,
    GivenTheOwnerObject_WhenMainEndsWithADeviceOpen_ThenTheThreadsAreJoined)
{
  std::shared_ptr<fake_device> const device = unnumbered ();
  {
    // As in main: the device outlives the owner object.
    hid::hid_device transport = make ("fake-unnumbered");
    hid::hid_runtime runtime;
    ASSERT_TRUE (transport.open (deadline_t (2000)).has_value ());
    EXPECT_EQ (hid::hid_runtime::stats ().reader_threads, 1u);
  }
  hid::hid_runtime_stats_t const stats = hid::hid_runtime::stats ();
  EXPECT_EQ (stats.reader_threads, 0u);
  EXPECT_EQ (stats.worker_threads, 0u);
  EXPECT_FALSE (stats.housekeeping);
  EXPECT_EQ (device->closes, 1u);
  EXPECT_EQ (device->calls_after_close, 0u);
}

TEST_F (
    LumexHidDeviceTest,
    GivenAStoppedExecutor_WhenTheRuntimeShutsDown_ThenTheStopFlagEndsTheReader)
{
  // As at process exit: the io_context of the device no longer runs, so the
  // close that shutdown posts never runs; the stop flag alone ends the
  // reader.
  std::shared_ptr<fake_device> const device = unnumbered ();
  asio::io_context context;
  asio::executor_work_guard<asio::io_context::executor_type> work
      = asio::make_work_guard (context);
  std::thread thread ([&context] () { context.run (); });
  {
    hid::hid_device transport
        = hid::detail::make_hid_device_with (seam_, context.get_executor (),
                                             [] ()
                                               {
                                                 hid::hid_options_t options;
                                                 options.path
                                                     = "fake-unnumbered";
                                                 return options;
                                               }());
    ASSERT_TRUE (transport.open (deadline_t (2000)).has_value ());
    work.reset ();
    context.stop ();
    thread.join ();
    EXPECT_TRUE (hid::hid_runtime::shutdown (deadline_t (2000)));
    EXPECT_EQ (hid::hid_runtime::stats ().reader_threads, 0u);
    EXPECT_EQ (device->closes, 1u);
  }
  // The device's posted close dies with the context.
}

TEST_F (LumexHidDeviceTest,
        GivenAWriteStuckInTheDriver_WhenShuttingDown_ThenFalseUntilItEnds)
{
  std::shared_ptr<fake_device> const device = unnumbered ();
  hid::hid_device transport = make ("fake-unnumbered");
  ASSERT_TRUE (transport.open (deadline_t (2000)).has_value ());
  device->set_hold_writes (true);
  std::future<void> sent
      = transport.async_send (message (0, { 1 }), asio::use_future);
  ASSERT_TRUE (
      device->wait_for ([&device] () { return device->write_waiting; }));
  EXPECT_FALSE (hid::hid_runtime::shutdown (deadline_t (150)));
  device->set_hold_writes (false);
  sent.get ();
  EXPECT_TRUE (hid::hid_runtime::shutdown (deadline_t (2000)));
  EXPECT_EQ (device->written ().size (), 1u);
}

TEST_F (
    LumexHidDeviceTest,
    GivenAPendingReceive_WhenCancelled_ThenItEndsCancelledAndTheDeviceStays)
{
  std::shared_ptr<fake_device> const device = unnumbered ();
  hid::hid_device transport = make ("fake-unnumbered");
  ASSERT_TRUE (transport.open (deadline_t (2000)).has_value ());
  std::future<message_t> pending = transport.async_receive (asio::use_future);
  transport.cancel ();
  try
    {
      pending.get ();
      FAIL () << "the cancelled receive succeeded";
    }
  catch (std::system_error const &failure)
    {
      EXPECT_TRUE (failure.code () == transport_errc::cancelled);
    }
  EXPECT_TRUE (transport.is_open ());
  device->push_raw ({ 4 });
  message_result_t const next = transport.receive (deadline_t (2000));
  ASSERT_TRUE (next.has_value ());
  EXPECT_EQ (next.value ().payload, (bytes{ 4 }));
  EXPECT_EQ (transport.statistics ().cancellations, 1u);
}

TEST_F (LumexHidDeviceTest,
        GivenAQueuedSendBehindAWrite_WhenCancelled_ThenOnlyTheQueuedOneEnds)
{
  std::shared_ptr<fake_device> const device = unnumbered ();
  hid::hid_device transport = make ("fake-unnumbered");
  ASSERT_TRUE (transport.open (deadline_t (2000)).has_value ());
  device->set_hold_writes (true);
  std::future<void> first
      = transport.async_send (message (0, { 1 }), asio::use_future);
  ASSERT_TRUE (
      device->wait_for ([&device] () { return device->write_waiting; }));
  std::future<void> second
      = transport.async_send (message (0, { 2 }), asio::use_future);
  transport.cancel ();
  try
    {
      second.get ();
      FAIL () << "the queued send was not cancelled";
    }
  catch (std::system_error const &failure)
    {
      EXPECT_TRUE (failure.code () == transport_errc::cancelled);
    }
  device->set_hold_writes (false);
  // The write in progress reached hidapi: it ends with its own result.
  first.get ();
  std::vector<bytes> const written = device->written ();
  ASSERT_EQ (written.size (), 1u);
  EXPECT_EQ (written[0][1], 1);
}

TEST_F (LumexHidDeviceTest,
        GivenACancellationSlot_WhenItEmits_ThenThePendingReceiveIsCancelled)
{
  unnumbered ();
  hid::hid_device transport = make ("fake-unnumbered");
  ASSERT_TRUE (transport.open (deadline_t (2000)).has_value ());
  asio::cancellation_signal signal;
  std::promise<std::error_code> done;
  transport.async_receive (asio::bind_cancellation_slot (
      signal.slot (),
      [&done] (std::error_code error, message_t) { done.set_value (error); }));
  std::this_thread::sleep_for (std::chrono::milliseconds (20));
  // The signal is emitted on the transport's executor (asio's rule).
  asio::post (transport.get_executor (), [&signal] ()
                { signal.emit (asio::cancellation_type::terminal); });
  EXPECT_TRUE (done.get_future ().get () == transport_errc::cancelled);
}

TEST_F (LumexHidDeviceTest,
        GivenCallbacks_WhenOpenSendAndReceive_ThenTheReportArrives)
{
  std::shared_ptr<fake_device> const device = numbered ();
  hid::hid_device transport = make ("fake-numbered");
  std::promise<std::error_code> done;
  message_t received;
  transport.async_open (
      [&] (std::error_code error)
        {
          if (error)
            {
              done.set_value (error);
              return;
            }
          transport.async_send (
              message (1, { 9 }),
              [&] (std::error_code sent)
                {
                  if (sent)
                    {
                      done.set_value (sent);
                      return;
                    }
                  device->push_raw ({ 0x01, 0x42 });
                  transport.async_receive (
                      [&] (std::error_code error2, message_t m)
                        {
                          received = m;
                          done.set_value (error2);
                        });
                });
        });
  std::error_code const error = done.get_future ().get ();
  ASSERT_FALSE (error) << error.message ();
  EXPECT_EQ (received.report_id, 1);
  EXPECT_EQ (received.payload, (bytes{ 0x42 }));
}

TEST_F (LumexHidDeviceTest,
        GivenUseFuture_WhenOpenSendAndReceive_ThenTheReportArrives)
{
  std::shared_ptr<fake_device> const device = numbered ();
  hid::hid_device transport = make ("fake-numbered");
  transport.async_open (asio::use_future).get ();
  transport.async_send (message (2, { 1, 2 }), asio::use_future).get ();
  device->push_raw ({ 0x03, 0x10, 0x20 });
  message_t const received = transport.async_receive (asio::use_future).get ();
  EXPECT_EQ (received.report_id, 3);
  EXPECT_EQ (received.payload, (bytes{ 0x10, 0x20 }));
  EXPECT_EQ (device->written ().size (), 1u);
}

TEST_F (LumexHidDeviceTest,
        GivenYieldContext_WhenOpenSendAndReceive_ThenTheReportArrives)
{
#if defined(LUMEX_TRANSPORT_WITH_SPAWN)
  std::shared_ptr<fake_device> const device = numbered ();
  hid::hid_device transport = make ("fake-numbered");
  std::promise<std::error_code> done;
  message_t received;
  asio::spawn (
      runner_.get_executor (),
      [&] (asio::yield_context yield)
        {
          std::error_code error;
          transport.async_open (yield[error]);
          if (!error)
            transport.async_send (message (1, { 5 }), yield[error]);
          if (!error)
            {
              device->push_raw ({ 0x02, 0x07 });
              received = transport.async_receive (yield[error]);
            }
          done.set_value (error);
        },
      [] (std::exception_ptr failure)
        {
          if (failure)
            std::rethrow_exception (failure);
        });
  std::error_code const error = done.get_future ().get ();
  ASSERT_FALSE (error) << error.message ();
  EXPECT_EQ (received.report_id, 2);
  EXPECT_EQ (received.payload, (bytes{ 0x07 }));
#else
  GTEST_SKIP () << "LUMEX_TRANSPORT_WITH_SPAWN is off";
#endif
}

TEST_F (LumexHidDeviceTest,
        GivenTraffic_WhenAskingForStatistics_ThenMessagesAndBytesAreCounted)
{
  std::shared_ptr<fake_device> const device = unnumbered ();
  hid::hid_device transport = make ("fake-unnumbered");
  ASSERT_TRUE (transport.open (deadline_t (2000)).has_value ());
  device->push_raw ({ 1, 2, 3 });
  ASSERT_TRUE (transport.receive (deadline_t (2000)).has_value ());
  ASSERT_TRUE (
      transport.send (message (0, { 4, 5 }), deadline_t (2000)).has_value ());
  tr::statistics::statistics_t const stats = transport.statistics ();
  EXPECT_EQ (stats.opens, 1u);
  EXPECT_EQ (stats.messages_received, 1u);
  EXPECT_EQ (stats.messages_sent, 1u);
  // Payload bytes, without the ID and the padding.
  EXPECT_EQ (stats.bytes_read, 3u);
  EXPECT_EQ (stats.bytes_written, 2u);
  EXPECT_EQ (stats.total_errors (), 0u);
}

TEST_F (LumexHidDeviceTest,
        GivenAFailedRead_WhenReceiving_ThenDisconnectedUntilReopened)
{
  std::shared_ptr<fake_device> const device = unnumbered ();
  hid::hid_device transport = make ("fake-unnumbered");
  ASSERT_TRUE (transport.open (deadline_t (2000)).has_value ());
  std::future<message_t> pending = transport.async_receive (asio::use_future);
  device->fail_read ();
  try
    {
      pending.get ();
      FAIL () << "the receive survived a failed read";
    }
  catch (std::system_error const &failure)
    {
      EXPECT_TRUE (failure.code () == transport_errc::disconnected);
    }
  message_result_t const again = transport.receive (deadline_t (2000));
  ASSERT_FALSE (again.has_value ());
  EXPECT_TRUE (again.error () == transport_errc::disconnected);
  // The reader closed the handle on its way out.
  ASSERT_TRUE (device->wait_for ([&device] () { return device->closes > 0; }));
  transport.close ();
  ASSERT_TRUE (transport.open (deadline_t (2000)).has_value ());
  device->push_raw ({ 8 });
  EXPECT_TRUE (transport.receive (deadline_t (2000)).has_value ());
}

TEST_F (LumexHidDeviceTest,
        GivenAFullReportQueue_WhenAnotherReportArrives_ThenTheOldestIsDropped)
{
  std::shared_ptr<fake_device> const device = unnumbered ();
  hid::hid_options_t options;
  options.path = "fake-unnumbered";
  options.max_queued_reports = 2;
  hid::hid_device transport = make (options);
  ASSERT_TRUE (transport.open (deadline_t (2000)).has_value ());
  device->push_raw ({ 1 });
  device->push_raw ({ 2 });
  device->push_raw ({ 3 });
  ASSERT_TRUE (
      device->wait_for ([&device] () { return device->reads.empty (); }));
  // The strand takes the last report after the fake handed it on.
  std::this_thread::sleep_for (std::chrono::milliseconds (200));
  message_result_t const first = transport.receive (deadline_t (2000));
  message_result_t const second = transport.receive (deadline_t (2000));
  ASSERT_TRUE (first.has_value ());
  ASSERT_TRUE (second.has_value ());
  EXPECT_EQ (first.value ().payload, (bytes{ 2 }));
  EXPECT_EQ (second.value ().payload, (bytes{ 3 }));
}

TEST_F (LumexHidDeviceTest,
        GivenAnOpenInProgress_WhenClosed_ThenItEndsCancelledAndTheHandleCloses)
{
  std::shared_ptr<fake_device> const device = unnumbered ();
  device->set_hold_open (true);
  hid::hid_device transport = make ("fake-unnumbered");
  std::future<void> opened = transport.async_open (asio::use_future);
  ASSERT_TRUE (
      device->wait_for ([&device] () { return device->open_waiting; }));
  transport.close ();
  device->set_hold_open (false);
  try
    {
      opened.get ();
      FAIL () << "the open survived the close";
    }
  catch (std::system_error const &failure)
    {
      EXPECT_TRUE (failure.code () == transport_errc::cancelled);
    }
  EXPECT_FALSE (transport.is_open ());
  ASSERT_TRUE (device->wait_for ([&device] () { return device->closes > 0; }));
  EXPECT_EQ (device->read_calls, 0u);
}

TEST_F (LumexHidDeviceTest,
        GivenAnOpenDevice_WhenAskingForTheLayout_ThenTheDescriptorsSizes)
{
  numbered ();
  hid::hid_device transport = make ("fake-numbered");
  EXPECT_TRUE (hid::opened_report_layout (transport).input_reports.empty ());
  ASSERT_TRUE (transport.open (deadline_t (2000)).has_value ());
  hid::report_layout_t const layout = hid::opened_report_layout (transport);
  EXPECT_TRUE (layout.numbered);
  EXPECT_EQ (layout.output_size (2), 4u);
  transport.close ();
  EXPECT_TRUE (eventually (
      [&transport] ()
        {
          return hid::opened_report_layout (transport).input_reports.empty ();
        }));
}

TEST (LumexHidapiStringTest,
      GivenHidapiStrings_WhenConvertedToUtf8_ThenTheCodePointsAreKept)
{
  EXPECT_EQ (hid::detail::wide_to_utf8 (nullptr), "");
  EXPECT_EQ (hid::detail::wide_to_utf8 (L"SN-01"), "SN-01");
  // U+0416 (two bytes) and U+20AC (three bytes).
  wchar_t const mixed[] = { 0x0416, 0x20AC, 0 };
  EXPECT_EQ (hid::detail::wide_to_utf8 (mixed), "\xD0\x96\xE2\x82\xAC");
#if defined(_WIN32)
  // U+1F600 as a surrogate pair; an unpaired high surrogate.
  wchar_t const pair[] = { 0xD83D, 0xDE00, 0 };
  wchar_t const lone[] = { 0xD83D, 0x41, 0 };
  EXPECT_EQ (hid::detail::wide_to_utf8 (pair), "\xF0\x9F\x98\x80");
  EXPECT_EQ (hid::detail::wide_to_utf8 (lone), "\xEF\xBF\xBD"
                                               "A");
#else
  wchar_t const wide[] = { static_cast<wchar_t> (0x1F600), 0 };
  wchar_t const lone[] = { static_cast<wchar_t> (0xD83D), 0x41, 0 };
  EXPECT_EQ (hid::detail::wide_to_utf8 (wide), "\xF0\x9F\x98\x80");
  EXPECT_EQ (hid::detail::wide_to_utf8 (lone), "\xEF\xBF\xBD"
                                               "A");
#endif
}
