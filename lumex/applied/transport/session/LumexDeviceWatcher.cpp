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

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/log/LumexTransportLog.hpp"
#include "lumex/applied/transport/session/LumexDeviceWatcher.hpp"
#include "lumex/applied/transport/session/LumexSessionSelectors.hpp"

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
namespace
{
char const KWATCHER_COMPONENT[] = "transport.watcher";

bool
contains (std::vector<serial_port_info_t> const &ports,
          std::string const &path) LUMEX_NOEXCEPT
{
  for (std::size_t i = 0; i < ports.size (); ++i)
    if (ports[i].path == path)
      return true;
  return false;
}
} // namespace

/// The implementation: every member but the atomics is used on the strand.
class device_watcher_impl
    : public std::enable_shared_from_this<device_watcher_impl>
{
public:
  typedef device_watcher::watch_id_t watch_id_t;
  typedef asio::strand<executor_type> strand_type;

  device_watcher_impl (executor_type const &executor,
                       std::chrono::milliseconds period,
                       serial_enumeration_t enumeration)
      : strand_ (asio::make_strand (executor)), timer_ (strand_),
        period_ (period.count () > 0 ? period : std::chrono::milliseconds (1)),
        enumeration_ (enumeration ? std::move (enumeration)
                                  : default_serial_enumeration ()),
        stopped_ (false), next_id_ (1), polls_ (0), has_snapshot_ (false),
        wait_id_ (0)
  {
  }

  void
  start ()
  {
    request_poll ();
  }

  watch_id_t
  add (serial_selector_t const &selector, device_callback_t callback,
       presence_sink_t sink)
  {
    watch_id_t const id = next_id_.fetch_add (1, std::memory_order_relaxed);
    std::shared_ptr<entry> const added = std::make_shared<entry> ();
    added->id = id;
    added->selector = selector;
    added->callback = std::move (callback);
    added->sink = std::move (sink);
    std::shared_ptr<device_watcher_impl> const self = shared_from_this ();
    asio::post (strand_, [self, added] () { self->do_add (*added); });
    return id;
  }

  void
  remove (watch_id_t id)
  {
    std::shared_ptr<device_watcher_impl> const self = shared_from_this ();
    asio::post (strand_, [self, id] () { self->do_remove (id); });
  }

  void
  request_poll ()
  {
    std::shared_ptr<device_watcher_impl> const self = shared_from_this ();
    asio::post (strand_, [self] () { self->do_poll (); });
  }

  void
  stop ()
  {
    stopped_.store (true, std::memory_order_release);
    std::shared_ptr<device_watcher_impl> const self = shared_from_this ();
    asio::post (strand_, [self] () { self->do_stop (); });
  }

  std::uint64_t
  polls () const LUMEX_NOEXCEPT
  {
    return polls_.load (std::memory_order_acquire);
  }

private:
  struct entry
  {
    watch_id_t id = 0;
    serial_selector_t selector;
    device_callback_t callback;
    presence_sink_t sink;
    std::vector<serial_port_info_t> present;
    bool reported = false;
  };

  struct timer_done
  {
    std::shared_ptr<device_watcher_impl> self;
    unsigned wait_id;
    void
    operator() (std::error_code const &)
    {
      self->on_timer (wait_id);
    }
  };

  bool
  stopped () const LUMEX_NOEXCEPT
  {
    return stopped_.load (std::memory_order_acquire);
  }

  void
  do_add (entry const &added)
  {
    if (stopped ())
      return;
    entries_.push_back (added);
    if (has_snapshot_)
      evaluate (entries_.back ());
  }

  void
  do_remove (watch_id_t id)
  {
    for (std::vector<entry>::iterator it = entries_.begin ();
         it != entries_.end (); ++it)
      if (it->id == id)
        {
          entries_.erase (it);
          return;
        }
  }

  void
  do_poll ()
  {
    if (stopped ())
      return;
    std::vector<serial_port_info_t> ports;
    try
      {
        ports = enumeration_ ();
      }
    catch (std::exception const &failure)
      {
        LUMEX_TRANSPORT_LOG (warning, KWATCHER_COMPONENT, "serial",
                             std::string ("enumeration failed: ")
                                 + failure.what (),
                             log::log_bytes_t ());
        schedule ();
        return;
      }
    catch (...)
      {
        LUMEX_TRANSPORT_LOG (warning, KWATCHER_COMPONENT, "serial",
                             "enumeration failed", log::log_bytes_t ());
        schedule ();
        return;
      }
    ports_.swap (ports);
    has_snapshot_ = true;
    // By index: a callback cannot change entries_ (watch and unwatch post).
    for (std::size_t i = 0; i < entries_.size () && !stopped (); ++i)
      evaluate (entries_[i]);
    polls_.fetch_add (1, std::memory_order_acq_rel);
    schedule ();
  }

  void
  schedule ()
  {
    if (stopped ())
      return;
    unsigned const wait_id = ++wait_id_;
    timer_.expires_after (period_);
    timer_done done = { shared_from_this (), wait_id };
    timer_.async_wait (std::move (done));
  }

  void
  on_timer (unsigned wait_id)
  {
    // poll_now re-arms the timer with a newer wait_id_.
    if (wait_id != wait_id_)
      return;
    do_poll ();
  }

  void
  do_stop ()
  {
    ++wait_id_;
    timer_.cancel ();
    entries_.clear ();
  }

  void
  evaluate (entry &watched)
  {
    std::vector<serial_port_info_t> now;
    for (std::size_t i = 0; i < ports_.size (); ++i)
      if (matches (watched.selector, ports_[i]))
        now.push_back (ports_[i]);

    std::vector<device_event_t> events;
    for (std::size_t i = 0; i < watched.present.size (); ++i)
      if (!contains (now, watched.present[i].path))
        {
          device_event_t event;
          event.kind = device_event_kind::removed;
          event.port = watched.present[i];
          events.push_back (event);
        }
    for (std::size_t i = 0; i < now.size (); ++i)
      if (!contains (watched.present, now[i].path))
        {
          device_event_t event;
          event.kind = device_event_kind::arrived;
          event.port = now[i];
          events.push_back (event);
        }
    bool const was_present = !watched.present.empty ();
    bool const is_present = !now.empty ();
    bool const first = !watched.reported;
    watched.present.swap (now);
    watched.reported = true;

    if (watched.sink && (first || was_present != is_present))
      deliver_presence (watched, is_present);
    if (watched.callback)
      for (std::size_t i = 0; i < events.size () && !stopped (); ++i)
        deliver_event (watched, events[i]);
  }

  void
  deliver_presence (entry const &watched, bool present)
  {
    if (stopped ())
      return;
    try
      {
        watched.sink (present);
      }
    catch (...)
      {
        LUMEX_TRANSPORT_LOG (error, KWATCHER_COMPONENT,
                             describe (watched.selector).c_str (),
                             "presence sink threw", log::log_bytes_t ());
      }
  }

  void
  deliver_event (entry const &watched, device_event_t const &event)
  {
    LUMEX_TRANSPORT_LOG (
        info, KWATCHER_COMPONENT, describe (watched.selector).c_str (),
        std::string (event.kind == device_event_kind::arrived ? "arrived: "
                                                              : "removed: ")
            + event.port.path,
        log::log_bytes_t ());
    try
      {
        watched.callback (event);
      }
    catch (...)
      {
        LUMEX_TRANSPORT_LOG (error, KWATCHER_COMPONENT,
                             describe (watched.selector).c_str (),
                             "device callback threw", log::log_bytes_t ());
      }
  }

  strand_type strand_;
  asio::steady_timer timer_;
  std::chrono::milliseconds period_;
  serial_enumeration_t enumeration_;
  std::atomic<bool> stopped_;
  std::atomic<watch_id_t> next_id_;
  std::atomic<std::uint64_t> polls_;
  // The strand only.
  std::vector<entry> entries_;
  std::vector<serial_port_info_t> ports_;
  bool has_snapshot_;
  unsigned wait_id_;
};

device_watcher::device_watcher (executor_type const &executor,
                                std::chrono::milliseconds period,
                                serial_enumeration_t enumeration)
    : impl_ (std::make_shared<device_watcher_impl> (executor, period,
                                                    std::move (enumeration)))
{
  impl_->start ();
}

device_watcher::~device_watcher () { stop (); }

device_watcher::device_watcher (device_watcher &&other) LUMEX_NOEXCEPT
    : impl_ (std::move (other.impl_))
{
}

device_watcher &
device_watcher::operator= (device_watcher &&other) LUMEX_NOEXCEPT
{
  if (this != &other)
    {
      stop ();
      impl_ = std::move (other.impl_);
    }
  return *this;
}

device_watcher::watch_id_t
device_watcher::watch (serial_selector_t const &selector,
                       device_callback_t callback)
{
  if (!impl_)
    return 0;
  return impl_->add (selector, std::move (callback), presence_sink_t ());
}

device_watcher::watch_id_t
device_watcher::watch_presence (serial_selector_t const &selector,
                                presence_sink_t sink)
{
  if (!impl_)
    return 0;
  return impl_->add (selector, device_callback_t (), std::move (sink));
}

void
device_watcher::unwatch (watch_id_t id)
{
  if (impl_)
    impl_->remove (id);
}

void
device_watcher::poll_now ()
{
  if (impl_)
    impl_->request_poll ();
}

void
device_watcher::stop () LUMEX_NOEXCEPT
{
  if (!impl_)
    return;
  try
    {
      impl_->stop ();
    }
  catch (...)
    {
    }
}

std::uint64_t
device_watcher::polls () const LUMEX_NOEXCEPT
{
  return impl_ ? impl_->polls () : 0;
}
} // namespace session
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
