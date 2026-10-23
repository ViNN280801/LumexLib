/*
 * Copyright (c) 2026 Vladislav Semykin <vladislav.semykin@gmail.com>
 *
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of
 * charge, to any person obtaining a copy
 * of this software and associated
 * documentation files (the "Software"), to deal
 * in the Software without
 * restriction, including without limitation the rights
 * to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the
 * Software, and to permit persons to whom the Software is
 * furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice
 * and this permission notice shall be included in
 * all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT
 * WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO
 * THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH
 * THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/**
 * @file LumexHidDevice.hpp
 * @brief `hid_device`: a USB HID device as a `datagram::message_transport`
 * over hidapi (`LUMEX_TRANSPORT_WITH_HID`), its enumeration and the
 * `hid_runtime` that owns its threads.
 * @details One message is one report. `open` reads the report descriptor
 * (`analyze_report_descriptor`) and fails with `bad_report_descriptor` when
 * it cannot be read or analyzed; the report ID is never guessed.
 * `message_t::report_id` is explicit both ways: a received report of a
 * device that numbers its reports carries its ID, of a device that numbers
 * none ID 0; a sent message names the output report, which the device must
 * have (`unknown_report_id` otherwise; ID 0 for a device that numbers none).
 * An outgoing report is padded with zero bytes to the size of its output
 * report; a longer payload fails with `report_too_large` and sends nothing.
 * A received payload is the report as the device sent it, without the ID.
 *
 * Threads (hidapi blocks): every open device has a reader thread that calls
 * `hid_read_timeout` in slices of `hid_options_t::read_slice` (100 ms) and
 * posts each report to the transport's strand; it checks the stop flag
 * before and after every slice, so `close ()` ends it within one slice and a
 * report that arrives after the stop is dropped. The reader thread alone
 * closes the hidapi handle, after its last read. Open and writes run on a
 * small blocking executor (two threads at most), one write of a device at a
 * time, so the strand never blocks. A write that waits for a thread of the
 * executor ends with `cancelled` by `cancel ()`, `close ()` or the deadline
 * of a synchronous send and never reaches hidapi; a write stuck inside
 * hidapi (a driver that does not answer) occupies a thread, and the opens
 * and writes of the other devices queue behind it. The threads belong to the
 * module-wide `hid_runtime`: every open device holds a reference to it, and
 * its housekeeping thread joins every reader thread that ended, so no thread
 * ever joins itself (a device whose last reference goes away on its own
 * reader thread only sets the stop flag). `hid_runtime::shutdown (deadline)`
 * closes every device and joins every thread (concurrent calls run one
 * after the other); an owner object
 * (`hid_runtime runtime;` in `main`) does it when `main` returns. A process
 * that exits without either sets the stop flags of the remaining readers at
 * static destruction (after the destructors of the static objects created
 * after the first use of the HID transport: the consumer's `io_runner` must
 * outlive the open devices, or the program calls `shutdown` first) and does
 * not join them (no hidapi call follows the
 * flag; the operating system ends the threads). `hid_exit` is never called:
 * DChannel's hidapi copy may share the process.
 *
 * Cancellation, deadlines and the statistics follow `message_transport`; a
 * write that reached hidapi completes with its own result (hidapi cannot
 * cancel it), a queued one with `cancelled`. A failed read ends the reader:
 * the pending and every later receive complete with `disconnected` until
 * the device is closed and opened again.
 */
#ifndef LUMEX_APPLIED_TRANSPORT_HID_HID_DEVICE_HPP
#define LUMEX_APPLIED_TRANSPORT_HID_HID_DEVICE_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "lumex/applied/transport/datagram/LumexMessageTransport.hpp"
#include "lumex/applied/transport/hid/LumexHidReportDescriptor.hpp"
#include "lumex/applied/transport/stream/LumexStreamTypes.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#endif

namespace lumex
{
namespace applied
{
namespace transport
{
namespace hid
{
/// @brief A HID device is a message transport.
typedef datagram::message_transport hid_device;

/// @brief One HID interface found by `enumerate_hid_devices`.
struct hid_device_info_t
{
  /// The platform path that `hid_options_t::path` takes.
  std::string path;
  std::uint16_t vendor_id = 0;
  std::uint16_t product_id = 0;
  /// The strings in UTF-8; empty when the device has none.
  std::string serial_number;
  std::string manufacturer;
  std::string product;
  /// bcdDevice.
  std::uint16_t release_number = 0;
  std::uint16_t usage_page = 0;
  std::uint16_t usage = 0;
  /// The USB interface number; -1 when unknown.
  int interface_number = -1;
};

/// @brief Which devices `enumerate_hid_devices` lists; an empty or zero
/// field matches any device.
struct hid_filter_t
{
  std::uint16_t vendor_id = 0;
  std::uint16_t product_id = 0;
  /// Compared with the UTF-8 serial number.
  std::string serial_number;
  /// Compared with the platform path.
  std::string path;
};

/// @brief What `make_hid_device` opens and how.
struct hid_options_t
{
  /// The platform path (`hid_device_info_t::path`). When empty, `open`
  /// takes the first device of the enumeration that matches `vendor_id`,
  /// `product_id` and `serial_number` (an empty or zero field matches any)
  /// and fails with `std::errc::no_such_device` when none does.
  std::string path;
  std::uint16_t vendor_id = 0;
  std::uint16_t product_id = 0;
  std::string serial_number;
  /// The instance name in the logs; empty: the path, or `hid vvvv:pppp`.
  std::string name;
  /// The slice of the reader's `hid_read_timeout`: `close ()` ends the
  /// reader within one slice.
  deadline_t read_slice = deadline_t (100);
  /// Reports kept while no receive is pending; past it the oldest is
  /// dropped with a warning in the log.
  std::size_t max_queued_reports = 256;
};

/// @brief The HID devices that match `filter` (the strings are compared
/// after the conversion to UTF-8). Enumeration does not open the devices: a
/// device the user may not open is listed too.
std::vector<hid_device_info_t>
enumerate_hid_devices (hid_filter_t const &filter = hid_filter_t ());

/// @brief A HID device on `executor` (closed; call `open`).
hid_device make_hid_device (executor_type const &executor,
                            hid_options_t const &options);

/// @brief The report layout of an open HID device (any thread); an empty
/// layout when `device` is closed or is no HID device.
report_layout_t opened_report_layout (hid_device const &device);

/// @brief Counters of the `hid_runtime` (for tests and diagnostics).
struct hid_runtime_stats_t
{
  /// Open devices (each holds a reference). It drops when a device's strand
  /// runs its close, so right after `shutdown` returns it can still lag.
  std::size_t open_devices = 0;
  /// Reader threads started and not yet joined.
  std::size_t reader_threads = 0;
  /// Threads of the blocking executor.
  std::size_t worker_threads = 0;
  /// Tasks queued or running on the blocking executor.
  std::size_t pending_tasks = 0;
  /// True while the housekeeping thread runs.
  bool housekeeping = false;
  /// False while `shutdown` runs (or after the exit sentinel): no reader
  /// starts, an open that completes then fails with `closed`.
  bool accepting = true;
};

/**
 * @brief The owner of the threads of every `hid_device` (see the file
 * description). Hold one in `main` so that the threads are joined before
 * `main` returns; the static functions work without one.
 */
class hid_runtime
{
public:
  /// @brief The deadline the destructor gives `shutdown`.
  static deadline_t
  default_shutdown_deadline () LUMEX_NOEXCEPT
  {
    return deadline_t (2000);
  }

  hid_runtime ();
  /// @brief `shutdown (default_shutdown_deadline ())`.
  ~hid_runtime ();

  hid_runtime (hid_runtime const &) = delete;
  hid_runtime &operator= (hid_runtime const &) = delete;

  /**
   * @brief Closes every open device (its pending operations end with
   * `cancelled` once its executor runs them) and joins every thread of the
   * runtime within `deadline`. Devices can be opened again afterwards.
   * @return True when every thread ended and was joined; false when the
   * deadline passed first (a write stuck in the driver, for example; the
   * threads are joined later) or when called from a thread of the runtime.
   */
  static bool shutdown (deadline_t deadline);

  /// @brief A snapshot of the counters.
  static hid_runtime_stats_t stats ();
};
} // namespace hid
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_APPLIED_TRANSPORT_HID_HID_DEVICE_HPP
