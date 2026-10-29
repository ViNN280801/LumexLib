// Device selectors and hot-plug of lumex::transport. A serial_selector_t names
// a device by what it is (the VID, PID and serial number of its USB adapter),
// not by where it is ("COM7" may come back as "COM9", "/dev/ttyUSB0" as
// "/dev/ttyUSB1"). A device_watcher polls the port list and tells a program,
// and through presence_sink () a session, when the device arrives or goes.
//
// The port list here is a fake bus owned by the example (the
// serial_enumeration_t seam of the selectors and of the watcher), so the
// example touches no port of the machine and its output does not depend on
// them. With no argument the real enumeration would be
// ss::default_serial_enumeration (), which lists the ports without opening
// them. The "device" behind the session is a mock stream.
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "lumex/applied/transport/LumexTransport"
#if defined(LUMEX_TRANSPORT_WITH_TESTING)
#include "lumex/applied/transport/testing/LumexMockStream.hpp"
#endif

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#pragma clang diagnostic ignored "-Wnrvo"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif
namespace tr = lumex::applied::transport;
namespace ss = tr::session;

namespace
{
using ms = std::chrono::milliseconds;

void
say (std::string const &line)
{
  static std::mutex mutex;
  std::lock_guard<std::mutex> const lock (mutex);
  std::cout << line << '\n';
}

// The transport log of this binary through the same lock as the example's
// own lines (warning and above go to std::cerr without a sink).
void
print_record (tr::log::log_record_t const &record)
{
  say (std::string ("  [log ") + record.component + ' ' + record.instance
       + "] " + record.message);
}

// The serial ports of a pretend machine. The enumeration seam may run on
// several threads (the watcher's strand, the open of a stream): the list is
// behind a lock.
class fake_bus
{
public:
  void
  plug (std::string const &path, std::string const &vid,
        std::string const &pid, std::string const &serial_number)
  {
    ss::serial_port_info_t port;
    port.path = path;
    port.vid = vid;
    port.pid = pid;
    port.serial_number = serial_number;
    std::lock_guard<std::mutex> const lock (mutex_);
    ports_.push_back (port);
  }

  void
  unplug (std::string const &path)
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    for (std::size_t i = 0; i < ports_.size (); ++i)
      if (ports_[i].path == path)
        {
          ports_.erase (ports_.begin () + static_cast<std::ptrdiff_t> (i));
          return;
        }
  }

  std::vector<ss::serial_port_info_t>
  list () const
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    return ports_;
  }

  ss::serial_enumeration_t
  enumeration ()
  {
    return std::bind (&fake_bus::list, this);
  }

private:
  mutable std::mutex mutex_;
  std::vector<ss::serial_port_info_t> ports_;
};

// What the callbacks of the watcher heard, and a wait for the n-th event.
class event_log
{
public:
  void
  add (std::string const &text)
  {
    {
      std::lock_guard<std::mutex> const lock (mutex_);
      ++count_;
    }
    say ("  " + text);
    changed_.notify_all ();
  }

  // True when `count` events happened within the limit.
  bool
  wait_for (std::size_t count, ms limit = ms (3000))
  {
    std::unique_lock<std::mutex> lock (mutex_);
    return changed_.wait_for (lock, limit,
                              [this, count] { return count_ >= count; });
  }

  std::size_t
  count () const
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    return count_;
  }

private:
  mutable std::mutex mutex_;
  std::condition_variable changed_;
  std::size_t count_ = 0;
};

void
show_selector (ss::serial_selector_t const &selector,
               std::vector<ss::serial_port_info_t> const &ports)
{
  std::ostringstream line;
  line << "  " << ss::describe (selector) << " -> ";
  std::string const found = ss::find_serial_port (selector, ports);
  if (found.empty ())
    line << "no port";
  else
    line << found;
  say (line.str ());
}

void
selectors_against_a_port_list ()
{
  say ("--- 1. selectors against a list of ports");
  fake_bus bus;
  bus.plug ("/dev/ttyUSB0", "1234", "0042", "BENCH-01");
  bus.plug ("/dev/ttyUSB1", "1234", "0042", "BENCH-02");
  bus.plug ("/dev/ttyACM0", "ABCD", "0001", "");
  std::vector<ss::serial_port_info_t> const ports = bus.list ();

  // The first matching port wins: with only the IDs the two adapters of one
  // model are the same device, with the serial number they are not.
  ss::serial_selector_t by_model;
  by_model.vid = "1234";
  by_model.pid = "0042";
  show_selector (by_model, ports);

  ss::serial_selector_t by_adapter = by_model;
  by_adapter.serial_number = "bench-02"; // compared without case
  show_selector (by_adapter, ports);

  ss::serial_selector_t hex_prefix;
  hex_prefix.vid = "0xabcd"; // "ABCD" and "0xabcd" are the same ID
  show_selector (hex_prefix, ports);

  ss::serial_selector_t by_path;
  by_path.path = "/dev/ttyUSB1"; // with a path nothing else is looked at
  by_path.vid = "FFFF";
  show_selector (by_path, ports);

  ss::serial_selector_t absent;
  absent.vid = "9999";
  show_selector (absent, ports);

  // A selector without any field names no port, not every port.
  ss::serial_selector_t empty;
  say ("  an empty selector matches the first port: "
       + std::string (ss::matches (empty, ports[0]) ? "yes" : "no"));

  ss::usb_identity_t const identity = ss::usb_identity (ports[1]);
  say ("  usb_identity of " + ports[1].path + ": VID " + identity.vid + " PID "
       + identity.pid + " SN " + identity.serial_number);
}

// A selector by identity makes a stream that looks the port up at each open:
// no match is a failed attempt (invalid_endpoint), not a crash. The stream
// finds a port, then the open of its node fails here, because the node of
// the pretend machine does not exist.
void
the_factory_looks_the_port_up_at_each_open ()
{
  say ("--- 2. the factory finds the port at every open");
  fake_bus bus;
  tr::stream::io_runner runner (1);
  ss::serial_selector_t selector;
  selector.vid = "1234";
  selector.pid = "0042";
  ss::stream_session::factory_t const factory
      = ss::make_serial_factory (selector, bus.enumeration ());

  {
    tr::stream::byte_stream stream = factory (runner.get_executor ());
    tr::status_result_t const result = stream.open (tr::deadline_t (2000));
    say ("  nothing plugged in: "
         + (result.has_value () ? std::string ("opened")
                                : result.error ().message ()));
  }
  bus.plug ("/dev/lumex-example-no-such-node", "1234", "0042", "BENCH-01");
  {
    tr::stream::byte_stream stream = factory (runner.get_executor ());
    tr::status_result_t const result = stream.open (tr::deadline_t (2000));
    say ("  plugged in, node missing: "
         + (result.has_value () ? std::string ("opened")
                                : result.error ().message ()));
  }
}

#if defined(LUMEX_TRANSPORT_WITH_TESTING)
// A session whose device is a mock stream (the mock transports are built with
// LUMEX_TRANSPORT_WITH_TESTING).
class mock_device_factory
{
public:
  explicit mock_device_factory (std::shared_ptr<std::size_t> const &calls)
      : calls_ (calls)
  {
  }

  tr::stream::byte_stream
  operator() (tr::executor_type const &executor) const
  {
    ++*calls_;
    return tr::testing::make_mock_stream (executor, "bench").stream;
  }

private:
  std::shared_ptr<std::size_t> calls_;
};

void
the_watcher_reports_arrivals_and_departures ()
{
  say ("--- 3. device_watcher: arrived and removed");
  fake_bus bus;
  tr::stream::io_runner runner (1);
  event_log events;

  ss::serial_selector_t selector;
  selector.vid = "1234";
  selector.pid = "0042";

  // The first poll runs at once, then one every period; poll_now () polls
  // without waiting for the period.
  ss::device_watcher_options_t watching;
  watching.period = ms (20);
  watching.enumeration = bus.enumeration ();
  ss::device_watcher watcher (runner.get_executor (), watching);
  watcher.watch (
      selector,
      [&events] (ss::device_event_t const &event)
        {
          events.add (std::string (event.kind == ss::device_event_kind::arrived
                                       ? "arrived: "
                                       : "removed: ")
                      + event.port.path);
        });

  bus.plug ("/dev/ttyUSB0", "1234", "0042", "BENCH-01");
  bus.plug ("/dev/ttyACM0", "ABCD", "0001", ""); // not this selector's
  watcher.poll_now ();
  events.wait_for (1);
  bus.unplug ("/dev/ttyUSB0");
  watcher.poll_now ();
  events.wait_for (2);
  // A device that comes back under another name is a removal of the old
  // name and an arrival of the new one.
  bus.plug ("/dev/ttyUSB3", "1234", "0042", "BENCH-01");
  watcher.poll_now ();
  events.wait_for (3);
  say ("  events heard: " + std::to_string (events.count ())
       + ", polls counted: "
       + std::string (watcher.polls () >= 3 ? "yes" : "no"));
}

void
presence_drives_a_session ()
{
  say ("--- 4. presence_sink (): the watcher tells a session");
  fake_bus bus;
  tr::stream::io_runner runner (1);
  std::shared_ptr<std::size_t> const calls = std::make_shared<std::size_t> (0);

  ss::session_options_t options;
  options.name = "bench";
  options.policy.initial_delay = ms (20);
  options.policy.jitter = 0.0;
  options.on_state_change = [] (ss::state_change_t const &change)
    {
      say ("  state " + std::string (ss::to_string (change.previous)) + " -> "
           + ss::to_string (change.current));
    };
  ss::stream_session link (runner.get_executor (), mock_device_factory (calls),
                           options);

  ss::serial_selector_t selector;
  selector.vid = "1234";
  selector.pid = "0042";
  ss::device_watcher_options_t watching;
  watching.period = ms (20);
  watching.enumeration = bus.enumeration ();
  ss::device_watcher watcher (runner.get_executor (), watching);
  event_log presence;
  watcher.watch_presence (
      selector, [&presence] (bool present)
        { presence.add (present ? "device present" : "device gone"); });
  // A second watch of the same selector feeds the session. The session needs
  // no watcher to work: without one it reconnects by its policy, blind to
  // whether the device is there.
  watcher.watch_presence (selector, link.presence_sink ());

  bus.plug ("/dev/ttyUSB0", "1234", "0042", "BENCH-01");
  watcher.poll_now ();
  presence.wait_for (1);
  tr::status_result_t const opened = link.open (tr::deadline_t (2000));
  say ("  open: "
       + (opened.has_value () ? std::string ("ok")
                              : opened.error ().message ()));

  // The device is pulled out: the watcher says so, the session drops its
  // transport and stops trying until the device is back.
  bus.unplug ("/dev/ttyUSB0");
  watcher.poll_now ();
  presence.wait_for (2);
  // Several attempts would fit in this time at the policy's delay (20 ms).
  std::this_thread::sleep_for (ms (150));
  say ("  while the device is gone: "
       + std::string (ss::to_string (link.state ())) + ", factory calls "
       + std::to_string (*calls));

  bus.plug ("/dev/ttyUSB0", "1234", "0042", "BENCH-01");
  watcher.poll_now ();
  presence.wait_for (3);
  ss::state_snapshot_t seen = link.state_snapshot ();
  std::chrono::steady_clock::time_point const end
      = std::chrono::steady_clock::now () + ms (3000);
  while (seen.state != ss::channel_state::open
         && std::chrono::steady_clock::now () < end)
    {
      std::future<ss::state_snapshot_t> next
          = link.async_wait_state_change (seen, asio::use_future);
      if (next.wait_until (end) != std::future_status::ready)
        break;
      seen = next.get ();
    }
  say ("  device back: " + std::string (ss::to_string (link.state ()))
       + ", factory calls " + std::to_string (*calls));
}
#endif
} // namespace

int
main ()
{
  tr::log::set_log_sink (&print_record);
  selectors_against_a_port_list ();
  the_factory_looks_the_port_up_at_each_open ();
  the_watcher_reports_arrivals_and_departures ();
#if defined(LUMEX_TRANSPORT_WITH_TESTING)
  presence_drives_a_session ();
#else
  say ("--- 4. built without the mock transports "
       "(LUMEX_TRANSPORT_WITH_TESTING): the session part is left out");
#endif
  return 0;
}
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
