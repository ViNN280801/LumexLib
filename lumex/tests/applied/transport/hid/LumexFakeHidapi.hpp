// A fake hidapi behind the seam of hid_device
// (lumex/applied/transport/hid/detail/LumexHidapiSeam.hpp). The fake hands
// the reads exactly as the test gives the raw bytes (hidapi's convention:
// the report ID first only for a device that numbers its reports) and keeps
// the raw buffers of the writes, so a test sees the bytes on the hidapi
// boundary. Descriptors and reports are synthetic (vendor-defined usage
// page, made-up IDs and sizes): no real device is described. The fake also
// counts hidapi calls after the close (a reader that outlives its stop) and
// can block a read, a write or an open until the test releases it.

#ifndef LUMEX_TESTS_APPLIED_TRANSPORT_HID_FAKE_HIDAPI_HPP
#define LUMEX_TESTS_APPLIED_TRANSPORT_HID_FAKE_HIDAPI_HPP

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#include "lumex/applied/transport/hid/LumexHidDevice.hpp"
#include "lumex/applied/transport/hid/detail/LumexHidapiSeam.hpp"

namespace lumex_fake_hidapi
{
namespace hid = lumex::applied::transport::hid;
typedef std::vector<std::uint8_t> bytes;

// A vendor-defined device without numbered reports: one input report of
// `input` bytes and one output report of `output` bytes (up to 255 each).
inline bytes
unnumbered_descriptor (std::uint8_t input, std::uint8_t output)
{
  bytes d = { 0x06, 0x00,   0xFF, // Usage Page (vendor defined)
              0x09, 0x01,         // Usage
              0xA1, 0x01,         // Collection (Application)
              0x15, 0x00,         // Logical Minimum (0)
              0x26, 0xFF,   0x00, // Logical Maximum (255)
              0x75, 0x08,         // Report Size (8)
              0x95, input,        // Report Count
              0x09, 0x02,         // Usage
              0x81, 0x02,         // Input
              0x95, output,       // Report Count
              0x09, 0x03,         // Usage
              0x91, 0x02,         // Output
              0xC0 };             // End Collection
  return d;
}

// A vendor-defined device with numbered reports: report 1 (8 bytes in, 8
// bytes out), report 2 (16 bytes in, 4 bytes out), report 3 (input only, 2
// bytes).
inline bytes
numbered_descriptor ()
{
  bytes d = { 0x06, 0x00, 0xFF, 0x09, 0x01, 0xA1, 0x01, // vendor collection
              0x75, 0x08,                               // Report Size (8)
              0x85, 0x01,                               // Report ID (1)
              0x95, 0x08, 0x09, 0x02, 0x81, 0x02,       // 8 bytes in
              0x95, 0x08, 0x09, 0x03, 0x91, 0x02,       // 8 bytes out
              0x85, 0x02,                               // Report ID (2)
              0x95, 0x10, 0x09, 0x04, 0x81, 0x02,       // 16 bytes in
              0x95, 0x04, 0x09, 0x05, 0x91, 0x02,       // 4 bytes out
              0x85, 0x03,                               // Report ID (3)
              0x95, 0x02, 0x09, 0x06, 0x81, 0x02,       // 2 bytes in
              0xC0 };
  return d;
}

// The state of one fake device, shared by its handles and the test.
struct fake_device
{
  hid::hid_device_info_t info;
  bytes descriptor;

  std::mutex mutex;
  std::condition_variable changed;
  std::deque<bytes> reads;
  std::vector<bytes> writes;
  bool fail_next_read = false;
  bool fail_writes = false;
  bool hold_writes = false;
  bool hold_open = false;
  // A read waits for set_hold_reads (false) whatever its slice (a slice in
  // progress while the test stops the device).
  bool hold_reads = false;
  bool read_waiting = false;
  // Called on the writing thread at the start of every write.
  std::function<void ()> on_write;
  bool open_waiting = false;
  bool write_waiting = false;
  bool open = false;
  std::size_t opens = 0;
  std::size_t closes = 0;
  std::size_t read_calls = 0;
  std::size_t calls_after_close = 0;
  int last_slice = 0;
  bool closed_on_reading_thread = false;
  std::thread::id reading_thread;

  // A report as hidapi hands it (the ID first only when numbered).
  void
  push_raw (bytes const &raw)
  {
    std::lock_guard<std::mutex> const lock (mutex);
    reads.push_back (raw);
    changed.notify_all ();
  }

  void
  fail_read ()
  {
    std::lock_guard<std::mutex> const lock (mutex);
    fail_next_read = true;
    changed.notify_all ();
  }

  void
  set_hold_writes (bool hold)
  {
    std::lock_guard<std::mutex> const lock (mutex);
    hold_writes = hold;
    changed.notify_all ();
  }

  void
  set_hold_reads (bool hold)
  {
    std::lock_guard<std::mutex> const lock (mutex);
    hold_reads = hold;
    changed.notify_all ();
  }

  void
  set_hold_open (bool hold)
  {
    std::lock_guard<std::mutex> const lock (mutex);
    hold_open = hold;
    changed.notify_all ();
  }

  // Waits until `predicate` holds (true) or 5 s pass (false).
  template <typename Predicate>
  bool
  wait_for (Predicate predicate)
  {
    std::unique_lock<std::mutex> lock (mutex);
    return changed.wait_for (lock, std::chrono::seconds (5), predicate);
  }

  std::vector<bytes>
  written ()
  {
    std::lock_guard<std::mutex> const lock (mutex);
    return writes;
  }
};

class fake_handle final : public hid::detail::hidapi_handle
{
public:
  explicit fake_handle (std::shared_ptr<fake_device> device)
      : device_ (std::move (device))
  {
  }

  int
  read_timeout (std::uint8_t *data, std::size_t size, int milliseconds,
                std::error_code &error) override
  {
    std::unique_lock<std::mutex> lock (device_->mutex);
    ++device_->read_calls;
    device_->last_slice = milliseconds;
    device_->reading_thread = std::this_thread::get_id ();
    device_->changed.notify_all ();
    if (!device_->open)
      {
        ++device_->calls_after_close;
        error = std::make_error_code (std::errc::bad_file_descriptor);
        return -1;
      }
    if (device_->hold_reads)
      {
        device_->read_waiting = true;
        device_->changed.notify_all ();
        device_->changed.wait (lock,
                               [this] () { return !device_->hold_reads; });
        device_->read_waiting = false;
      }
    device_->changed.wait_for (
        lock, std::chrono::milliseconds (milliseconds), [this] ()
          { return !device_->reads.empty () || device_->fail_next_read; });
    if (device_->fail_next_read)
      {
        device_->fail_next_read = false;
        error = std::make_error_code (std::errc::io_error);
        return -1;
      }
    if (device_->reads.empty ())
      return 0;
    bytes const raw = device_->reads.front ();
    device_->reads.pop_front ();
    device_->changed.notify_all ();
    std::size_t const n = std::min (size, raw.size ());
    if (n > 0)
      std::memcpy (data, raw.data (), n);
    return static_cast<int> (n);
  }

  int
  write (std::uint8_t const *data, std::size_t size,
         std::error_code &error) override
  {
    std::function<void ()> hook;
    {
      std::lock_guard<std::mutex> const guard (device_->mutex);
      hook = device_->on_write;
    }
    if (hook)
      hook ();
    std::unique_lock<std::mutex> lock (device_->mutex);
    if (!device_->open)
      {
        ++device_->calls_after_close;
        error = std::make_error_code (std::errc::bad_file_descriptor);
        return -1;
      }
    device_->write_waiting = true;
    device_->changed.notify_all ();
    device_->changed.wait (lock, [this] () { return !device_->hold_writes; });
    device_->write_waiting = false;
    if (device_->fail_writes)
      {
        error = std::make_error_code (std::errc::io_error);
        return -1;
      }
    device_->writes.push_back (bytes (data, data + size));
    device_->changed.notify_all ();
    return static_cast<int> (size);
  }

  int
  report_descriptor (std::uint8_t *data, std::size_t size,
                     std::error_code &error) override
  {
    std::lock_guard<std::mutex> const lock (device_->mutex);
    if (device_->descriptor.empty ())
      {
        error = std::make_error_code (std::errc::io_error);
        return -1;
      }
    std::size_t const n = std::min (size, device_->descriptor.size ());
    std::memcpy (data, device_->descriptor.data (), n);
    return static_cast<int> (n);
  }

  std::string
  last_error_text () override
  {
    return "fake hidapi error";
  }

  void
  close () override
  {
    std::lock_guard<std::mutex> const lock (device_->mutex);
    if (!device_->open)
      ++device_->calls_after_close;
    device_->open = false;
    ++device_->closes;
    device_->closed_on_reading_thread
        = device_->reading_thread == std::this_thread::get_id ();
    device_->changed.notify_all ();
  }

private:
  std::shared_ptr<fake_device> device_;
};

class fake_seam final : public hid::detail::hidapi_seam
{
public:
  std::shared_ptr<fake_device>
  add (std::string const &path, std::uint16_t vendor_id,
       std::uint16_t product_id, std::string const &serial,
       bytes const &descriptor, int interface_number = -1,
       std::uint16_t usage_page = 0, std::uint16_t usage = 0)
  {
    // The whole record is filled before the device is listed: a watcher
    // thread copies it as soon as the lock is released.
    std::shared_ptr<fake_device> const device
        = std::make_shared<fake_device> ();
    device->info.path = path;
    device->info.vendor_id = vendor_id;
    device->info.product_id = product_id;
    device->info.serial_number = serial;
    device->info.interface_number = interface_number;
    device->info.usage_page = usage_page;
    device->info.usage = usage;
    device->descriptor = descriptor;
    std::lock_guard<std::mutex> const lock (mutex_);
    devices_.push_back (device);
    return device;
  }

  // The device is unplugged: the enumeration no longer lists it and its
  // path cannot be opened (a handle already open stays valid).
  void
  remove (std::shared_ptr<fake_device> const &device)
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    for (std::size_t i = 0; i < devices_.size (); ++i)
      if (devices_[i] == device)
        {
          devices_.erase (devices_.begin () + static_cast<std::ptrdiff_t> (i));
          return;
        }
  }

  // The device is plugged in again: listed again, under `path`.
  void
  plug (std::shared_ptr<fake_device> const &device, std::string const &path)
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    device->info.path = path;
    for (std::size_t i = 0; i < devices_.size (); ++i)
      if (devices_[i] == device)
        return;
    devices_.push_back (device);
  }

  std::vector<hid::hid_device_info_t>
  enumerate (std::uint16_t vendor_id, std::uint16_t product_id) override
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    std::vector<hid::hid_device_info_t> found;
    for (std::size_t i = 0; i < devices_.size (); ++i)
      if ((vendor_id == 0 || devices_[i]->info.vendor_id == vendor_id)
          && (product_id == 0 || devices_[i]->info.product_id == product_id))
        found.push_back (devices_[i]->info);
    return found;
  }

  std::unique_ptr<hid::detail::hidapi_handle>
  open_path (std::string const &path, std::error_code &error) override
  {
    std::shared_ptr<fake_device> device;
    {
      std::lock_guard<std::mutex> const lock (mutex_);
      for (std::size_t i = 0; i < devices_.size (); ++i)
        if (devices_[i]->info.path == path)
          device = devices_[i];
    }
    if (!device)
      {
        error = std::make_error_code (std::errc::no_such_device);
        return std::unique_ptr<hid::detail::hidapi_handle> ();
      }
    std::unique_lock<std::mutex> lock (device->mutex);
    device->open_waiting = true;
    device->changed.notify_all ();
    device->changed.wait (lock, [&device] () { return !device->hold_open; });
    device->open_waiting = false;
    device->open = true;
    ++device->opens;
    return std::unique_ptr<hid::detail::hidapi_handle> (
        new fake_handle (device));
  }

private:
  std::mutex mutex_;
  std::vector<std::shared_ptr<fake_device>> devices_;
};
} // namespace lumex_fake_hidapi

#endif // !LUMEX_TESTS_APPLIED_TRANSPORT_HID_FAKE_HIDAPI_HPP
