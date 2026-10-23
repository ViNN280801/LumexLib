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

#include <chrono>
#include <cmath>
#include <cstdint>
#include <system_error>

#include "lumex/applied/transport/error/LumexTransportErrc.hpp"
#include "lumex/applied/transport/session/LumexSessionTypes.hpp"

#if defined(__clang__)
// The library's clang-cl set: -Wall there is -Weverything (as in the other
// modules' sources).
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#if __has_warning("-Wunsafe-buffer-usage-in-libc-call")
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-libc-call"
#endif
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wswitch-enum"
#if __has_warning("-Wnrvo")
#pragma clang diagnostic ignored "-Wnrvo"
#endif
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#endif

namespace lumex
{
namespace applied
{
namespace transport
{
namespace session
{
using error::transport_errc;

char const *
to_string (channel_state state) LUMEX_NOEXCEPT
{
  switch (state)
    {
    case channel_state::closed:
      return "closed";
    case channel_state::connecting:
      return "connecting";
    case channel_state::open:
      return "open";
    case channel_state::reconnecting:
      return "reconnecting";
    case channel_state::faulted:
      return "faulted";
    default:
      return "unknown";
    }
}

std::chrono::milliseconds
reconnect_delay (reconnect_policy_t const &policy,
                 std::uint32_t failed_attempt, double random01) LUMEX_NOEXCEPT
{
  double const max_delay
      = policy.max_delay.count () > 0
            ? static_cast<double> (policy.max_delay.count ())
            : 0.0;
  double const initial
      = policy.initial_delay.count () > 0
            ? static_cast<double> (policy.initial_delay.count ())
            : 0.0;
  // A multiplier below 1 (or NaN) counts as 1.
  double const multiplier = policy.multiplier >= 1.0 ? policy.multiplier : 1.0;
  double const exponent
      = failed_attempt > 1 ? static_cast<double> (failed_attempt - 1) : 0.0;

  // initial == 0 stays 0 (0 * inf would be NaN); an overflow to inf ends at
  // max_delay.
  double base
      = initial > 0.0 ? initial * std::pow (multiplier, exponent) : 0.0;
  if (!(base <= max_delay))
    base = max_delay;

  double jitter = policy.jitter >= 0.0 ? policy.jitter : 0.0;
  if (!(jitter <= 1.0))
    jitter = 1.0;
  double random = random01 >= 0.0 ? random01 : 0.0;
  if (!(random <= 1.0))
    random = 1.0;

  double delay = base * (1.0 + jitter * (2.0 * random - 1.0));
  if (!(delay >= 0.0))
    delay = 0.0;
  if (delay > max_delay)
    delay = max_delay;
  return std::chrono::milliseconds (
      static_cast<std::chrono::milliseconds::rep> (std::floor (delay + 0.5)));
}

bool
is_link_lost (std::error_code const &error) LUMEX_NOEXCEPT
{
  if (!error)
    return false;
  return !error::is_transport_error (error, transport_errc::cancelled)
         && !error::is_transport_error (error, transport_errc::timeout)
         && !error::is_transport_error (error, transport_errc::would_deadlock)
         && !error::is_transport_error (error,
                                        transport_errc::report_too_large)
         && !error::is_transport_error (error,
                                        transport_errc::unknown_report_id);
}
} // namespace session
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
