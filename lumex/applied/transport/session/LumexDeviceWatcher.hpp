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
 * @file LumexDeviceWatcher.hpp
 * @brief `device_watcher`: polls the serial enumeration (and, with
 * `LUMEX_TRANSPORT_WITH_HID`, the HID enumeration) on a timer and reports,
 * per selector, the devices that arrived and the devices that went (spec G1,
 * hot-plug; no platform notification API, so it works the same on Windows
 * and Linux).
 * @details The watcher runs on its own strand of the given executor: the
 * first poll at once, then one every `period` of its options, on the clock
 * of their `timers` (`device_watcher_options_t`). A selector added with
 * `watch` hears every matching port that arrives or goes (the ports present at
 * its first evaluation arrive then); one added with `watch_presence` hears
 * whether any matching port is present, at its first evaluation and at each
 * change, which is what a session's `presence_sink ()` takes:
 *
 * @code
 * watcher.watch_presence (selector, link.presence_sink ());
 * @endcode
 *
 * A HID selector (`hid::hid_selector_t`) is watched the same way, with
 * `hid_device_event_t` records; a device that came back under another path
 * is a removal of the old path and an arrival of the new one. A HID path is
 * one device: hidapi on Linux and macOS lists a path once per top-level
 * usage, and the watcher reports one arrival and one removal per path (the
 * first matching record is the event's `device`), so a consumer that opens on
 * arrival opens once. Within one poll the removals of a selector come before
 * its arrivals. The HID enumeration (`hid::default_hid_enumeration ()`)
 * opens no device for use, but on Windows hidapi opens each interface with
 * zero access to read its attributes and strings, and it takes the same
 * process-wide mutex as `hid_open_path`: a poll can wait behind a slow open of
 * a HID device, and the watcher's strand with it. The serial listing runs on
 * every poll, the HID listing only while a HID selector is watched, in the
 * same poll; a failure of either skips only its own selectors for that
 * poll, and `polls ()` counts a poll when at least one of the listings it ran
 * returned. A new selector is evaluated at once against the last poll (the
 * first HID selector lists the HID devices itself). Callbacks run on the
 * strand; an exception they throw is logged and dropped. The enumeration runs
 * on the watcher's strand too and blocks that thread for its duration (the
 * default one lists the ports without opening them, see
 * `default_serial_enumeration ()`, so a poll does not touch the port a
 * session holds). An enumeration that throws skips that poll. The
 * destructor stops the watcher and returns; no callback runs after it (one
 * already running on another thread may still finish). A moved-from watcher
 * is empty: `watch` and `watch_presence` return 0 and the other members do
 * nothing.
 */
#ifndef LUMEX_APPLIED_TRANSPORT_SESSION_DEVICE_WATCHER_HPP
#define LUMEX_APPLIED_TRANSPORT_SESSION_DEVICE_WATCHER_HPP

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

#include "lumex/applied/transport/session/LumexSessionSelectors.hpp"
#include "lumex/applied/transport/session/LumexSessionTypes.hpp"
#include "lumex/applied/transport/stream/LumexStreamTypes.hpp"
#include "lumex/applied/transport/timing/LumexTimerSource.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

#if defined(LUMEX_TRANSPORT_WITH_HID)
#include "lumex/applied/transport/hid/LumexHidSelectors.hpp"
#else
// The constructor takes a HID enumeration in every build, so that the class
// does not change with the option; without the HID transport the type is only
// named (its record is never defined, the argument is ignored).
namespace lumex
{
namespace applied
{
namespace transport
{
namespace hid
{
struct hid_device_info_t;
using hid_enumeration_t = std::function<std::vector<hid_device_info_t> ()>;
} // namespace hid
} // namespace transport
} // namespace applied
} // namespace lumex
#endif

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
namespace session
{
/// @brief What happened to a port.
enum class device_event_kind : int
{
  arrived,
  removed
};

/// @brief A port that arrived or went.
struct device_event_t
{
  device_event_kind kind = device_event_kind::arrived;
  serial_port_info_t port;
};

/// @brief Called on the watcher's strand for each event of its selector.
using device_callback_t = std::function<void (device_event_t const &)>;

#if defined(LUMEX_TRANSPORT_WITH_HID)
/// @brief A HID interface that arrived or went.
struct hid_device_event_t
{
  device_event_kind kind = device_event_kind::arrived;
  hid::hid_device_info_t device;
};

/// @brief Called on the watcher's strand for each event of a HID selector.
using hid_device_callback_t = std::function<void (hid_device_event_t const &)>;
#endif

class device_watcher_impl;

/// @brief The settings of a `device_watcher`.
struct device_watcher_options_t
{
  /// The time between two polls (at least 1 ms).
  std::chrono::milliseconds period = std::chrono::milliseconds (1000);
  /// The serial listing; empty: `default_serial_enumeration ()`.
  serial_enumeration_t enumeration;
  /// The HID listing; empty: `hid::default_hid_enumeration ()` (ignored
  /// without `LUMEX_TRANSPORT_WITH_HID`).
  hid::hid_enumeration_t hid_enumeration;
  /// The clock of the period; the default is `timing::steady_timer_source ()`
  /// (real time), a test gives the source of a `testing::virtual_clock`.
  timing::timer_source timers;
};

/// @brief Polls the serial ports and the HID devices (see the file
/// description).
class device_watcher
{
public:
  /// @brief The handle of one `watch` / `watch_presence`.
  using watch_id_t = std::uint64_t;

  /// @brief Starts polling with `options` (see `device_watcher_options_t`;
  /// the defaults poll the serial ports once a second in real time).
  explicit device_watcher (executor_type const &executor,
                           device_watcher_options_t options
                           = device_watcher_options_t ());

  /// @brief Stops; no callback runs after it returns.
  ~device_watcher ();

  device_watcher (device_watcher const &) = delete;
  device_watcher &operator= (device_watcher const &) = delete;
  device_watcher (device_watcher &&other) LUMEX_NOEXCEPT;
  device_watcher &operator= (device_watcher &&other) LUMEX_NOEXCEPT;

  /// @brief Reports each port `selector` names when it arrives or goes (the
  /// id is never 0).
  watch_id_t watch (serial_selector_t const &selector,
                    device_callback_t callback);

  /// @brief Reports whether a port `selector` names is present (the id is
  /// never 0).
  watch_id_t watch_presence (serial_selector_t const &selector,
                             presence_sink_t sink);

  // The HID overloads exist only with the HID transport: `hid_selector_t` and
  // the event record live in hid/, which is neither compiled nor installed
  // without it. The class does not change with the macro (one shared pointer,
  // the same constructor); the macro is a public definition of the transport
  // target, so a consumer always sees the library's setting.
#if defined(LUMEX_TRANSPORT_WITH_HID)
  /// @brief Reports each HID interface `selector` names when it arrives or
  /// goes (the id is never 0).
  watch_id_t watch (hid::hid_selector_t const &selector,
                    hid_device_callback_t callback);

  /// @brief Reports whether a HID interface `selector` names is present (the
  /// id is never 0).
  watch_id_t watch_presence (hid::hid_selector_t const &selector,
                             presence_sink_t sink);
#endif

  /// @brief Ends a watch (its callback is not called after the step that
  /// removes it; one already running may finish).
  void unwatch (watch_id_t id);

  /// @brief Polls now (the period starts again after it).
  void poll_now ();

  /// @brief Stops polling and drops every watch (any thread, idempotent).
  void stop () LUMEX_NOEXCEPT;

  /// @brief The polls done so far (at least one of the listings returned; a
  /// poll whose every listing threw is not counted).
  std::uint64_t polls () const LUMEX_NOEXCEPT;

private:
  std::shared_ptr<device_watcher_impl> impl_;
};
} // namespace session
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_APPLIED_TRANSPORT_SESSION_DEVICE_WATCHER_HPP
