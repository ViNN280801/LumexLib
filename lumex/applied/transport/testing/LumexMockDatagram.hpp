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
 * sent. A receive waits for a message indefinitely. Available with
 * `LUMEX_TRANSPORT_WITH_TESTING`.
 */
#ifndef LUMEX_APPLIED_TRANSPORT_TESTING_MOCK_DATAGRAM_HPP
#define LUMEX_APPLIED_TRANSPORT_TESTING_MOCK_DATAGRAM_HPP

#include <memory>
#include <string>
#include <system_error>
#include <vector>

#include "lumex/applied/transport/datagram/LumexMessageTransport.hpp"

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
  /// @brief The pending and later receives end with `disconnected`.
  void disconnect ();
  /// @brief Every message sent so far.
  std::vector<datagram::message_t> sent () const;

private:
  std::shared_ptr<mock_datagram_impl> impl_;
};

/// @brief A mock message transport and its control.
struct mock_datagram
{
  datagram::message_transport transport;
  mock_datagram_control control;
};

/// @brief Makes a mock message transport on `executor`.
mock_datagram make_mock_datagram (executor_type const &executor,
                                  std::string const &name = "mock");
} // namespace testing
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_APPLIED_TRANSPORT_TESTING_MOCK_DATAGRAM_HPP
