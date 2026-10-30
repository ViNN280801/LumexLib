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
 * @file LumexMockDatagram.hpp
 * @brief `mock_datagram`: a `message_transport` driven by a test.
 * @details The control (thread-safe): `deliver` queues a message for
 * receives, `fail_*` makes the next operation fail, `disconnect` ends the
 * pending and later receives with `disconnected`, `sent` returns every message
 * sent. A receive waits for a message indefinitely; several receives may be
 * pending at once and are served in the order they were started.
 *
 * Scripting, as in `mock_stream`: `on_send` sees every accepted message on the
 * strand of the mock, `deliver_after` delivers after a delay of the mock's
 * timer source, `hold_next_open` keeps the next open pending,
 * `fail_send_after` lets some sends succeed and fails the next one,
 * `hold_sends` / `release_sends` keep sends pending (a frozen device) and let
 * them go in order. These settings belong to the script, not to a connection:
 * they outlive `close` and a new open. An observer that throws during
 * `release_sends` leaves the held sends that are left to a new flush step
 * before the exception goes on. Available with
 * `LUMEX_TRANSPORT_WITH_TESTING`.
 */
#ifndef LUMEX_APPLIED_TRANSPORT_TESTING_MOCK_DATAGRAM_HPP
#define LUMEX_APPLIED_TRANSPORT_TESTING_MOCK_DATAGRAM_HPP

#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

#include "lumex/applied/transport/datagram/LumexMessageTransport.hpp"
#include "lumex/applied/transport/testing/LumexMockScript.hpp"

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
namespace testing
{
class mock_datagram_impl;

/// @brief Sees every message a mock datagram transport accepted, in order
/// (the rules of `write_observer_t`).
using send_observer_t
    = std::function<void (datagram::message_t const &message)>;

/// @brief The script side of a mock datagram transport (thread-safe).
class mock_datagram_control
{
public:
  explicit mock_datagram_control (std::shared_ptr<mock_datagram_impl> impl);

  /// @brief Queues a message for receives.
  void deliver (datagram::message_t const &message);
  /// @brief The next open fails with `error`.
  void fail_open (std::error_code error);
  /// @brief The next receive fails with `error`.
  void fail_next_receive (std::error_code error);
  /// @brief The next send fails with `error`.
  void fail_next_send (std::error_code error);
  /// @brief The pending and later receives end with `disconnected` (messages
  /// delivered before are received first), and so do the sends held by
  /// `hold_sends`. Until the next open, sends fail with `disconnected` (they
  /// are not recorded) and messages delivered later are dropped.
  void disconnect ();
  /// @brief Every message sent so far.
  std::vector<datagram::message_t> sent () const;

  /// @brief Sets the observer of the accepted sends; an empty one removes it.
  void on_send (send_observer_t observer);
  /// @brief Queues `message` for receives after `delay` of the mock's timer
  /// source (the rules of `mock_stream_control::feed_after`).
  void deliver_after (std::chrono::milliseconds delay,
                      datagram::message_t const &message);
  /// @brief As `deliver_after`, but bound to the connection that is current
  /// now (every open and every close starts a new one): a delivery whose
  /// connection is gone is dropped, as are messages that come after a
  /// hang-up. (A plain `deliver_after` outlives a close and a reopen.)
  void deliver_after_on_link (std::chrono::milliseconds delay,
                              datagram::message_t const &message);
  /// @brief `disconnect ()` after `delay` of the mock's timer source;
  /// ordered with `deliver_after` by time and, for equal times, by the order
  /// of the calls. Bound to the connection that is current now: it never
  /// hangs up one opened later. A new open clears the disconnect.
  void disconnect_after (std::chrono::milliseconds delay);
  /// @brief Every open that starts before `duration` of the mock's timer
  /// source has passed from now fails with `error` (a cable that is plugged
  /// back later); a zero or negative duration clears the window. `fail_open`
  /// is served first.
  void fail_opens_for (std::chrono::milliseconds duration,
                       std::error_code error);
  /// @brief From now on sends stay pending (they are neither recorded nor
  /// completed) until `release_sends`, `cancel` or `close`; the latter two end
  /// them with `cancelled`.
  void hold_sends ();
  /// @brief Lets the held sends go, in the order they were started, on the
  /// strand of the mock (after the call returns, when the strand runs); later
  /// sends queue behind them until they are done.
  void release_sends ();
  /// @brief The next open does not complete until a cancel or a close ends it
  /// with `cancelled`.
  void hold_next_open ();
  /// @brief After `sends` more sends that succeed, the next send fails with
  /// `error`.
  void fail_send_after (std::size_t sends, std::error_code error);

private:
  std::shared_ptr<mock_datagram_impl> impl_;
};

/// @brief A mock message transport and its control.
struct mock_datagram
{
  datagram::message_transport transport;
  mock_datagram_control control;
};

/// @brief Makes a mock message transport on `executor` with the steady timer
/// source.
mock_datagram make_mock_datagram (executor_type const &executor,
                                  std::string const &name = "mock");

/// @brief Makes a mock message transport on `executor` with `options`.
mock_datagram make_mock_datagram (executor_type const &executor,
                                  mock_options_t const &options);
} // namespace testing
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_APPLIED_TRANSPORT_TESTING_MOCK_DATAGRAM_HPP
