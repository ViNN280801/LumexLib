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
 * @file LumexCountingHandler.hpp
 * @brief `counting_handler`: the wrapper the base classes put around a
 * completion handler to count the result before the handler runs.
 * @details Internal to the transport library. The wrapper is a move-only
 * function object that holds the counters by `shared_ptr` (a handler may
 * outlive the stream) and the handler it wraps; asio::associator
 * specialisations forward every association (executor, allocator,
 * cancellation slot, immediate executor) of the wrapped handler, so the user
 * handler still runs on its own executor and allocator. The base classes wrap
 * the handler the user gave, before it is type-erased.
 */
#ifndef LUMEX_APPLIED_TRANSPORT_DETAIL_COUNTING_HANDLER_HPP
#define LUMEX_APPLIED_TRANSPORT_DETAIL_COUNTING_HANDLER_HPP

#include <cstddef>
#include <memory>
#include <system_error>
#include <utility>

#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/statistics/LumexTransportStatistics.hpp"
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
namespace detail
{
/**
 * @brief Counts a completion through `Recorder`, then calls `Handler`.
 * @tparam Handler The wrapped handler (move-only is fine).
 * @tparam Recorder A function object `void (statistics::statistics_counters &,
 * std::error_code const &, Args const &...)` that counts the result.
 */
template <typename Handler, typename Recorder> struct counting_handler
{
  std::shared_ptr<statistics::statistics_counters> counters;
  Recorder recorder;
  Handler handler;

  template <typename... Args>
  void
  operator() (std::error_code error, Args &&...args)
  {
    recorder (*counters, error, args...);
    std::move (handler) (error, std::forward<Args> (args)...);
  }
};

/// @brief Wraps `handler`; the result is move-only like the handler. Call it
/// with the handler as the user gave it: wrapping before the one type erasure
/// (`io_handler_t (count_with (...))`) keeps a single
/// `any_completion_handler`, whose allocator is the user's own. Wrapping an
/// erased handler would forward the erased handler's allocator, a view into an
/// object the outer erasure destroys first.
template <typename Recorder, typename Handler>
counting_handler<Handler, Recorder>
count_with (std::shared_ptr<statistics::statistics_counters> const &counters,
            Recorder const &recorder, Handler handler)
{
  counting_handler<Handler, Recorder> wrapped
      = { counters, recorder, std::move (handler) };
  return wrapped;
}

/// @brief Counts an open.
struct open_recorder
{
  void
  operator() (statistics::statistics_counters &counters,
              std::error_code const &error) const LUMEX_NOEXCEPT
  {
    counters.add_open (error);
  }
};

/// @brief Counts a read.
struct read_recorder
{
  void
  operator() (statistics::statistics_counters &counters,
              std::error_code const &error,
              std::size_t bytes) const LUMEX_NOEXCEPT
  {
    counters.add_read (error, bytes);
  }
};

/// @brief Counts a write (also the bytes written before an error).
struct write_recorder
{
  void
  operator() (statistics::statistics_counters &counters,
              std::error_code const &error,
              std::size_t bytes) const LUMEX_NOEXCEPT
  {
    counters.add_write (error, bytes);
  }
};

/// @brief Counts a message receive by the payload of the message (any type
/// with a `payload` container).
struct receive_recorder
{
  template <typename Message>
  void
  operator() (statistics::statistics_counters &counters,
              std::error_code const &error,
              Message const &message) const LUMEX_NOEXCEPT
  {
    counters.add_receive (error, message.payload.size ());
  }
};

/// @brief Counts a message send by the payload size taken at the start.
struct send_recorder
{
  std::size_t bytes;
  void
  operator() (statistics::statistics_counters &counters,
              std::error_code const &error) const LUMEX_NOEXCEPT
  {
    counters.add_send (error, bytes);
  }
};
} // namespace detail
} // namespace transport
} // namespace applied
} // namespace lumex

namespace asio
{
template <template <typename, typename> class Associator, typename Handler,
          typename Recorder, typename DefaultCandidate>
struct associator<
    Associator,
    lumex::applied::transport::detail::counting_handler<Handler, Recorder>,
    DefaultCandidate> : Associator<Handler, DefaultCandidate>
{
  typedef lumex::applied::transport::detail::counting_handler<Handler,
                                                              Recorder>
      wrapper_type;

  static typename Associator<Handler, DefaultCandidate>::type
  get (wrapper_type const &wrapper) LUMEX_NOEXCEPT
  {
    return Associator<Handler, DefaultCandidate>::get (wrapper.handler);
  }

  static auto
  get (wrapper_type const &wrapper,
       DefaultCandidate const &candidate) LUMEX_NOEXCEPT
      -> decltype (Associator<Handler, DefaultCandidate>::get (wrapper.handler,
                                                               candidate))
  {
    return Associator<Handler, DefaultCandidate>::get (wrapper.handler,
                                                       candidate);
  }
};

// An already type-erased handler (a completion token may be an
// any_completion_handler, which Asio passes through unchanged) keeps its
// executor and cancellation slot, but its allocator is not forwarded: it is a
// view into the erased handler, which the outer erasure destroys before it
// frees its own storage. The default candidate is returned instead.
template <typename... Signatures, typename Recorder, typename DefaultCandidate>
struct associator<associated_allocator,
                  lumex::applied::transport::detail::counting_handler<
                      any_completion_handler<Signatures...>, Recorder>,
                  DefaultCandidate>
{
  typedef DefaultCandidate type;

  static type
  get (lumex::applied::transport::detail::counting_handler<
       any_completion_handler<Signatures...>, Recorder> const &) LUMEX_NOEXCEPT
  {
    return type ();
  }

  static DefaultCandidate const &
  get (lumex::applied::transport::detail::counting_handler<
           any_completion_handler<Signatures...>, Recorder> const &,
       DefaultCandidate const &candidate) LUMEX_NOEXCEPT
  {
    return candidate;
  }
};
} // namespace asio

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_APPLIED_TRANSPORT_DETAIL_COUNTING_HANDLER_HPP
