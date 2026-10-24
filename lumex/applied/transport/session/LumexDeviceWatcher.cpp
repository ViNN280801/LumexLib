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

// `Info` is a serial_port_info_t or a hid::hid_device_info_t: both are
// identified by their path.
template <typename Info>
bool
contains (std::vector<Info> const &devices,
          std::string const &path) LUMEX_NOEXCEPT
{
  for (std::size_t i = 0; i < devices.size (); ++i)
    if (devices[i].path == path)
      return true;
  return false;
}

// What one poll changed for one selector.
template <typename Info> struct changes
{
  std::vector<Info> arrived;
  std::vector<Info> removed;
  bool was_present = false;
  bool is_present = false;
};

// The devices of `snapshot` that `selector` names against those it named
// before (`present`, replaced by the new set). `matches` is found by
// argument-dependent lookup (the serial selector's in this namespace, the
// HID selector's in hid).
template <typename Info, typename Selector>
changes<Info>
diff (Selector const &selector, std::vector<Info> const &snapshot,
      std::vector<Info> &present)
{
  // One device per path: hidapi lists a path once per top-level usage, and
  // a consumer that opens on arrival must open the path once (the first
  // matching record stands for it).
  std::vector<Info> now;
  for (std::size_t i = 0; i < snapshot.size (); ++i)
    if (matches (selector, snapshot[i]) && !contains (now, snapshot[i].path))
      now.push_back (snapshot[i]);

  changes<Info> result;
  for (std::size_t i = 0; i < present.size (); ++i)
    if (!contains (now, present[i].path))
      result.removed.push_back (present[i]);
  for (std::size_t i = 0; i < now.size (); ++i)
    if (!contains (present, now[i].path))
      result.arrived.push_back (now[i]);
  result.was_present = !present.empty ();
  result.is_present = !now.empty ();
  present.swap (now);
  return result;
}

std::string
describe_selector (serial_selector_t const &selector)
{
  return describe (selector);
}

#if defined(LUMEX_TRANSPORT_WITH_HID)
std::string
describe_selector (hid::hid_selector_t const &selector)
{
  return hid::describe (selector);
}
#endif

device_event_t
make_event (device_event_kind kind, serial_port_info_t const &port)
{
  device_event_t event;
  event.kind = kind;
  event.port = port;
  return event;
}

std::string const &
path_of (device_event_t const &event)
{
  return event.port.path;
}

#if defined(LUMEX_TRANSPORT_WITH_HID)
hid_device_event_t
make_event (device_event_kind kind, hid::hid_device_info_t const &device)
{
  hid_device_event_t event;
  event.kind = kind;
  event.device = device;
  return event;
}

std::string const &
path_of (hid_device_event_t const &event)
{
  return event.device.path;
}
#endif
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
                       serial_enumeration_t enumeration,
                       hid::hid_enumeration_t hid_enumeration)
      : strand_ (asio::make_strand (executor)), timer_ (strand_),
        period_ (period.count () > 0 ? period : std::chrono::milliseconds (1)),
        enumeration_ (enumeration ? std::move (enumeration)
                                  : default_serial_enumeration ()),
#if defined(LUMEX_TRANSPORT_WITH_HID)
        hid_enumeration_ (hid_enumeration ? std::move (hid_enumeration)
                                          : hid::default_hid_enumeration ()),
#endif
        stopped_ (false), next_id_ (1), polls_ (0), has_snapshot_ (false),
#if defined(LUMEX_TRANSPORT_WITH_HID)
        has_hid_snapshot_ (false),
#endif
        wait_id_ (0)
  {
#if !defined(LUMEX_TRANSPORT_WITH_HID)
    // Without the HID transport there is nothing to list.
    (void)hid_enumeration;
#endif
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
    std::shared_ptr<entry> const added = std::make_shared<entry> ();
    added->selector = selector;
    added->callback = std::move (callback);
    added->sink = std::move (sink);
    return post_add (added);
  }

#if defined(LUMEX_TRANSPORT_WITH_HID)
  watch_id_t
  add (hid::hid_selector_t const &selector, hid_device_callback_t callback,
       presence_sink_t sink)
  {
    std::shared_ptr<entry> const added = std::make_shared<entry> ();
    added->is_hid = true;
    added->hid_selector = selector;
    added->hid_callback = std::move (callback);
    added->sink = std::move (sink);
    return post_add (added);
  }
#endif

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
#if defined(LUMEX_TRANSPORT_WITH_HID)
    bool is_hid = false;
    hid::hid_selector_t hid_selector;
    hid_device_callback_t hid_callback;
    std::vector<hid::hid_device_info_t> hid_present;
#endif
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

  watch_id_t
  post_add (std::shared_ptr<entry> const &added)
  {
    watch_id_t const id = next_id_.fetch_add (1, std::memory_order_relaxed);
    added->id = id;
    std::shared_ptr<device_watcher_impl> const self = shared_from_this ();
    asio::post (strand_, [self, added] () { self->do_add (*added); });
    return id;
  }

  void
  do_add (entry const &added)
  {
    if (stopped ())
      return;
    entries_.push_back (added);
#if defined(LUMEX_TRANSPORT_WITH_HID)
    if (added.is_hid)
      {
        // The first HID watch lists the devices itself (the polls skip the
        // HID enumeration while nobody watches it); a failed listing is
        // retried by the next poll.
        if (has_hid_snapshot_ || poll_hid ())
          evaluate_hid (entries_.back ());
        return;
      }
#endif
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
          break;
        }
#if defined(LUMEX_TRANSPORT_WITH_HID)
    // No watch left on the HID devices: the snapshot goes stale.
    if (!has_hid_entries ())
      has_hid_snapshot_ = false;
#endif
  }

  // False when the enumeration threw (logged): that poll is skipped.
  bool
  poll_serial ()
  {
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
        return false;
      }
    catch (...)
      {
        LUMEX_TRANSPORT_LOG (warning, KWATCHER_COMPONENT, "serial",
                             "enumeration failed", log::log_bytes_t ());
        return false;
      }
    ports_.swap (ports);
    has_snapshot_ = true;
    return true;
  }

#if defined(LUMEX_TRANSPORT_WITH_HID)
  bool
  has_hid_entries () const LUMEX_NOEXCEPT
  {
    for (std::size_t i = 0; i < entries_.size (); ++i)
      if (entries_[i].is_hid)
        return true;
    return false;
  }

  // As poll_serial, for the HID enumeration.
  bool
  poll_hid ()
  {
    std::vector<hid::hid_device_info_t> devices;
    try
      {
        devices = hid_enumeration_ ();
      }
    catch (std::exception const &failure)
      {
        LUMEX_TRANSPORT_LOG (warning, KWATCHER_COMPONENT, "hid",
                             std::string ("enumeration failed: ")
                                 + failure.what (),
                             log::log_bytes_t ());
        return false;
      }
    catch (...)
      {
        LUMEX_TRANSPORT_LOG (warning, KWATCHER_COMPONENT, "hid",
                             "enumeration failed", log::log_bytes_t ());
        return false;
      }
    hid_devices_.swap (devices);
    has_hid_snapshot_ = true;
    return true;
  }
#endif

  void
  do_poll ()
  {
    if (stopped ())
      return;
    bool const serial_listed = poll_serial ();
    bool hid_listed = false;
#if defined(LUMEX_TRANSPORT_WITH_HID)
    if (has_hid_entries ())
      hid_listed = poll_hid ();
#endif
    // By index: a callback cannot change entries_ (watch and unwatch post).
    for (std::size_t i = 0; i < entries_.size () && !stopped (); ++i)
      {
#if defined(LUMEX_TRANSPORT_WITH_HID)
        if (entries_[i].is_hid)
          {
            if (hid_listed)
              evaluate_hid (entries_[i]);
            continue;
          }
#endif
        if (serial_listed)
          evaluate (entries_[i]);
      }
    if (serial_listed || hid_listed)
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
    changes<serial_port_info_t> const found
        = diff (watched.selector, ports_, watched.present);
    report (watched, found, watched.selector, watched.callback);
  }

#if defined(LUMEX_TRANSPORT_WITH_HID)
  void
  evaluate_hid (entry &watched)
  {
    changes<hid::hid_device_info_t> const found
        = diff (watched.hid_selector, hid_devices_, watched.hid_present);
    report (watched, found, watched.hid_selector, watched.hid_callback);
  }
#endif

  template <typename Info, typename Selector, typename Callback>
  void
  report (entry &watched, changes<Info> const &found, Selector const &selector,
          Callback const &callback)
  {
    bool const first = !watched.reported;
    watched.reported = true;
    if (watched.sink && (first || found.was_present != found.is_present))
      deliver_presence (watched, describe_selector (selector),
                        found.is_present);
    if (!callback)
      return;
    for (std::size_t i = 0; i < found.removed.size () && !stopped (); ++i)
      deliver_event (
          callback, describe_selector (selector),
          make_event (device_event_kind::removed, found.removed[i]));
    for (std::size_t i = 0; i < found.arrived.size () && !stopped (); ++i)
      deliver_event (
          callback, describe_selector (selector),
          make_event (device_event_kind::arrived, found.arrived[i]));
  }

  void
  deliver_presence (entry const &watched, std::string const &name,
                    bool present)
  {
    if (stopped ())
      return;
    try
      {
        watched.sink (present);
      }
    catch (...)
      {
        LUMEX_TRANSPORT_LOG (error, KWATCHER_COMPONENT, name.c_str (),
                             "presence sink threw", log::log_bytes_t ());
      }
  }

  template <typename Callback, typename Event>
  void
  deliver_event (Callback const &callback, std::string const &name,
                 Event const &event)
  {
    LUMEX_TRANSPORT_LOG (info, KWATCHER_COMPONENT, name.c_str (),
                         std::string (event.kind == device_event_kind::arrived
                                          ? "arrived: "
                                          : "removed: ")
                             + path_of (event),
                         log::log_bytes_t ());
    try
      {
        callback (event);
      }
    catch (...)
      {
        LUMEX_TRANSPORT_LOG (error, KWATCHER_COMPONENT, name.c_str (),
                             "device callback threw", log::log_bytes_t ());
      }
  }

  strand_type strand_;
  asio::steady_timer timer_;
  std::chrono::milliseconds period_;
  serial_enumeration_t enumeration_;
#if defined(LUMEX_TRANSPORT_WITH_HID)
  hid::hid_enumeration_t hid_enumeration_;
#endif
  std::atomic<bool> stopped_;
  std::atomic<watch_id_t> next_id_;
  std::atomic<std::uint64_t> polls_;
  // The strand only.
  std::vector<entry> entries_;
  std::vector<serial_port_info_t> ports_;
  bool has_snapshot_;
#if defined(LUMEX_TRANSPORT_WITH_HID)
  std::vector<hid::hid_device_info_t> hid_devices_;
  bool has_hid_snapshot_;
#endif
  unsigned wait_id_;
};

device_watcher::device_watcher (executor_type const &executor,
                                std::chrono::milliseconds period,
                                serial_enumeration_t enumeration,
                                hid::hid_enumeration_t hid_enumeration)
    : impl_ (std::make_shared<device_watcher_impl> (
          executor, period, std::move (enumeration),
          std::move (hid_enumeration)))
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

#if defined(LUMEX_TRANSPORT_WITH_HID)
device_watcher::watch_id_t
device_watcher::watch (hid::hid_selector_t const &selector,
                       hid_device_callback_t callback)
{
  if (!impl_)
    return 0;
  return impl_->add (selector, std::move (callback), presence_sink_t ());
}

device_watcher::watch_id_t
device_watcher::watch_presence (hid::hid_selector_t const &selector,
                                presence_sink_t sink)
{
  if (!impl_)
    return 0;
  return impl_->add (selector, hid_device_callback_t (), std::move (sink));
}
#endif

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
