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

#include <memory>
#include <mutex>
#include <system_error>
#include <utility>

#include "lumex/applied/transport/datagram/LumexMessageTransport.hpp"
#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/error/LumexTransportErrc.hpp"
#include "lumex/applied/transport/session/detail/LumexSessionCore.hpp"
#include "lumex/applied/transport/session/detail/LumexSessionImpl.hpp"
#include "lumex/applied/transport/statistics/LumexTransportStatistics.hpp"
#include "lumex/applied/transport/stream/LumexByteStream.hpp"

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
namespace detail
{
namespace
{
// The completion of the open of one transport: on the session strand (its
// associated executor), it reports to the core. A plain handler given to the
// transport's public async_open, which erases it once. Weak: the transport,
// owned by the core, keeps it while it opens (REVIEW13 L1).
struct open_done
{
  std::weak_ptr<session_core> core;
  session_core::strand_type strand;
  unsigned generation;

  void
  operator() (std::error_code error)
  {
    std::shared_ptr<session_core> const alive = core.lock ();
    if (alive)
      alive->on_opened (generation, error);
  }

  typedef session_core::strand_type executor_type;
  executor_type
  get_executor () const LUMEX_NOEXCEPT
  {
    return strand;
  }
};
} // namespace

template <typename Transport>
session_impl<Transport>::session_impl (executor_type const &executor,
                                       factory_t factory,
                                       session_options_t options)
    : session_core (executor, std::move (options)),
      factory_ (std::move (factory))
{
}

template <typename Transport> session_impl<Transport>::~session_impl () {}

template <typename Transport>
statistics::statistics_t
session_impl<Transport>::transport_statistics () const
{
  std::lock_guard<std::mutex> const lock (statistics_mutex_);
  if (!statistics_)
    return statistics::statistics_t ();
  return statistics_->snapshot ();
}

template <typename Transport>
void
session_impl<Transport>::reset_transport_statistics ()
{
  std::lock_guard<std::mutex> const lock (statistics_mutex_);
  if (statistics_)
    statistics_->reset ();
}

template <typename Transport>
void
session_impl<Transport>::make_transport ()
{
  std::unique_ptr<Transport> made (new Transport (factory_ (executor ())));
  if (!made->impl ())
    throw std::system_error (
        make_error_code (error::transport_errc::invalid_endpoint),
        "the factory returned a moved-from transport");
  // The counters live in the transport's implementation: an aliasing pointer
  // keeps that alive for statistics () from any thread.
  std::shared_ptr<statistics::statistics_counters> const counters (
      made->impl (), &made->impl ()->counters ());
  {
    std::lock_guard<std::mutex> const lock (statistics_mutex_);
    statistics_ = counters;
  }
  current_ = std::move (made);
}

template <typename Transport>
void
session_impl<Transport>::open_transport (unsigned generation)
{
  open_done done = { shared_from_this (), strand (), generation };
  current_->async_open (std::move (done));
}

template <typename Transport>
void
session_impl<Transport>::drop_transport () LUMEX_NOEXCEPT
{
  if (!current_)
    return;
  current_->close ();
  current_.reset ();
}

template <typename Transport>
void
session_impl<Transport>::cancel_transport () LUMEX_NOEXCEPT
{
  if (current_)
    current_->cancel ();
}

template class session_impl<stream::byte_stream>;
template class session_impl<datagram::message_transport>;
} // namespace detail
} // namespace session
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
