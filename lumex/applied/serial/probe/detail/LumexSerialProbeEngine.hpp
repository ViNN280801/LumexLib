/*
 * Copyright (c) 2026 Vladislav Semykin <vladislav.semykin@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/**
 * @file LumexSerialProbeEngine.hpp
 * @brief Internal deadline engine of the serial prober.
 *
 * @details Header-only `detail::run_probe`: it opens the port when needed,
 * purges the buffers, writes the request and reads until the caller's
 * predicate confirms the response, the response cap is reached or the
 * deadline expires. The public contract lives in `LumexSerialProber.hpp`.
 * Consumers must not include this header.
 */
#ifndef LUMEX_APPLIED_SERIAL_PROBE_DETAIL_SERIAL_PROBE_ENGINE_HPP
#define LUMEX_APPLIED_SERIAL_PROBE_DETAIL_SERIAL_PROBE_ENGINE_HPP

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#if __has_warning("-Wunsafe-buffer-usage-in-libc-call")
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-libc-call"
#endif
#endif

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

#include "lumex/applied/serial/probe/LumexSerialProber.hpp"
#include "lumex/applied/serial/probe/detail/LumexSerialTransport.hpp"
#include "lumex/core/utility/macros/LumexConstantMacros.hpp"

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace applied
{
namespace serial
{
namespace probe
{
namespace detail
{
/**
 * @brief Read chunk of the probe loop, in bytes.
 */
LUMEX_CONST_NUM std::size_t KPROBE_READ_CHUNK_BYTES = 256U;

/**
 * @brief Milliseconds elapsed since `start`.
 * @param start Monotonic time point the probe started at.
 */
inline std::chrono::milliseconds
elapsed_since (std::chrono::steady_clock::time_point start) LUMEX_NOEXCEPT
{
  return std::chrono::duration_cast<std::chrono::milliseconds> (
      std::chrono::steady_clock::now () - start);
}

/**
 * @brief Runs one bounded probe over `transport`.
 *
 * @details Steps: bounded open when the transport is closed (`open_timeout`
 * or `open_failed` stop the call); purge; write the request (`io_error`
 * when it does not leave in full); read loop until the predicate confirms
 * the response (`responded`), the cap is reached or the deadline expires
 * (`incomplete` when bytes arrived, `no_response` when none did); an OS
 * read error yields `io_error` with the bytes collected so far.
 *
 * @param transport Open or closed port transport; the engine opens it when
 * `is_open ()` is false and keeps it open afterwards.
 * @param request Request bytes; an empty vector skips the write step.
 * @param response_complete Predicate over the accumulated response, called
 * after every read chunk and never with an empty vector.
 * @param deadline Overall budget for open, purge, write and reads.
 * @param max_response_bytes Response cap; values below 1 are clamped to 1.
 * @return The probe outcome with the collected bytes, the elapsed time and
 * the formatted platform error.
 */
inline serial_probe_result_t
run_probe (serial_transport_t &transport,
           std::vector<std::uint8_t> const &request,
           std::function<bool (std::vector<std::uint8_t> const &)> const
               &response_complete,
           std::chrono::milliseconds deadline, std::size_t max_response_bytes)
{
  std::chrono::steady_clock::time_point const start
      = std::chrono::steady_clock::now ();

  auto remaining = [start, deadline] () -> std::chrono::milliseconds
    {
      std::chrono::milliseconds const used = elapsed_since (start);
      return deadline > used ? deadline - used : std::chrono::milliseconds (0);
    };

  serial_probe_result_t result;

  if (!transport.is_open ())
    {
      transport_result_t const opened = transport.open (remaining ());
      if (opened.status != transport_status::ok)
        {
          result.status = opened.status == transport_status::deadline_exceeded
                              ? serial_probe_status::open_timeout
                              : serial_probe_status::open_failed;
          result.system_error = opened.system_error;
          result.elapsed = elapsed_since (start);
          return result;
        }
    }

  transport.purge ();

  if (!request.empty ())
    {
      transport_result_t const written
          = transport.write (request.data (), request.size (), remaining ());
      if (written.status != transport_status::ok)
        {
          result.status = serial_probe_status::io_error;
          result.system_error = written.system_error;
          result.elapsed = elapsed_since (start);
          return result;
        }
    }

  std::size_t const cap = max_response_bytes < 1U ? 1U : max_response_bytes;
  std::vector<std::uint8_t> chunk (KPROBE_READ_CHUNK_BYTES);

  for (;;)
    {
      if (result.response.size () >= cap)
        {
          result.status = serial_probe_status::incomplete;
          break;
        }

      std::size_t const space = cap - result.response.size ();
      std::size_t const capacity
          = space < chunk.size () ? space : chunk.size ();

      transport_result_t const got
          = transport.read (chunk.data (), capacity, remaining ());

      if (got.status == transport_status::failed)
        {
          result.status = serial_probe_status::io_error;
          result.system_error = got.system_error;
          break;
        }

      // `deadline_exceeded` carries no bytes; `ok` with zero bytes means no
      // progress. Both stop the loop instead of spinning, and the outcome
      // depends on what has been collected so far.
      if (got.status == transport_status::deadline_exceeded || got.bytes == 0U)
        {
          result.status = result.response.empty ()
                              ? serial_probe_status::no_response
                              : serial_probe_status::incomplete;
          break;
        }

      std::size_t const count = got.bytes < capacity ? got.bytes : capacity;
      result.response.insert (result.response.end (), chunk.begin (),
                              chunk.begin ()
                                  + static_cast<std::ptrdiff_t> (count));

      if (response_complete (result.response))
        {
          result.status = serial_probe_status::responded;
          break;
        }
    }

  result.elapsed = elapsed_since (start);
  return result;
}

} // namespace detail
} // namespace probe
} // namespace serial
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_APPLIED_SERIAL_PROBE_DETAIL_SERIAL_PROBE_ENGINE_HPP
