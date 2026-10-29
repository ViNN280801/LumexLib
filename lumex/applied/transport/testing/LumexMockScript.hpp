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
 * @file LumexMockScript.hpp
 * @brief `mock_options_t` and the observer types of the scripted mocks
 * (`mock_stream`, `mock_datagram`).
 * @details A mock made with `mock_options_t` takes the time of its scripted
 * deliveries (`feed_after`, `deliver_after`) from `options.timers`: with the
 * timer source of a `virtual_clock` they arrive when the test advances the
 * clock, with the default (the steady source) after the real delay. Use the
 * source of the clock that drives the rest of the test: a delivery waits on
 * the clock of the timer source it was made with. Available with
 * `LUMEX_TRANSPORT_WITH_TESTING`.
 */
#ifndef LUMEX_APPLIED_TRANSPORT_TESTING_MOCK_SCRIPT_HPP
#define LUMEX_APPLIED_TRANSPORT_TESTING_MOCK_SCRIPT_HPP

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "lumex/applied/transport/timing/LumexTimerSource.hpp"

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
/// @brief What a scripted mock is made with.
struct mock_options_t
{
  /// The name for logs.
  std::string name = "mock";
  /// The time of the scripted deliveries; never empty, steady by default.
  timing::timer_source timers;
};

/// @brief Sees every chunk a mock stream accepted, in order. Called on the
/// strand of the mock, outside its lock, after the write was recorded and its
/// completion posted: it may call the control of the mock (feed a reply,
/// hold, release, replace itself). It must not throw. A mock keeps the
/// observer until it is replaced by an empty one or the `io_context` is
/// destroyed: an observer that holds the mock's own control or stream is a
/// cycle until then (clear it with `on_write ({})`).
using write_observer_t
    = std::function<void (std::vector<std::uint8_t> const &chunk)>;
} // namespace testing
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_APPLIED_TRANSPORT_TESTING_MOCK_SCRIPT_HPP
