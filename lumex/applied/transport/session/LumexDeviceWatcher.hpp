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
 * @brief `device_watcher`: polls the serial enumeration on a timer and
 * reports, per selector, the ports that arrived and the ports that went (spec
 * G1, hot-plug; no platform notification API, so it works the same on
 * Windows and Linux).
 * @details The watcher runs on its own strand of the given executor: the
 * first poll at once, then one every `period`. A selector added with `watch`
 * hears every matching port that arrives or goes (the ports present at its
 * first evaluation arrive then); one added with `watch_presence` hears
 * whether any matching port is present, at its first evaluation and at each
 * change, which is what a session's `presence_sink ()` takes:
 *
 * @code
 * watcher.watch_presence (selector, link.presence_sink ());
 * @endcode
 *
 * A new selector is evaluated at once against the last poll. Callbacks run on
 * the strand; an exception they throw is logged and dropped. The enumeration
 * runs on the watcher's strand too and blocks that thread for its duration
 * (the default one lists the ports without opening them, see
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

#include "lumex/applied/transport/session/LumexSessionSelectors.hpp"
#include "lumex/applied/transport/session/LumexSessionTypes.hpp"
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
typedef std::function<void (device_event_t const &)> device_callback_t;

class device_watcher_impl;

/// @brief Polls the serial ports (see the file description).
class device_watcher
{
public:
  /// @brief The handle of one `watch` / `watch_presence`.
  typedef std::uint64_t watch_id_t;

  /// @brief Starts polling every `period` (at least 1 ms) through
  /// `enumeration` (empty: `default_serial_enumeration ()`).
  explicit device_watcher (executor_type const &executor,
                           std::chrono::milliseconds period
                           = std::chrono::milliseconds (1000),
                           serial_enumeration_t enumeration
                           = serial_enumeration_t ());

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

  /// @brief Ends a watch (its callback is not called after the step that
  /// removes it; one already running may finish).
  void unwatch (watch_id_t id);

  /// @brief Polls now (the period starts again after it).
  void poll_now ();

  /// @brief Stops polling and drops every watch (any thread, idempotent).
  void stop () LUMEX_NOEXCEPT;

  /// @brief The polls done so far (the enumeration returned).
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
