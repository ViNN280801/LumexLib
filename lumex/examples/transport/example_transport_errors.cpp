// Errors and access problems of lumex::transport: what transport_errc holds
// and how it compares, what an operating system error looks like and how
// format_system_error () spells it, and how an application tells its user
// "no access to the port" on Linux (permission_denied: the user is not in the
// port's group, the udev rule is missing) and on Windows (access denied means
// the port is busy). Nothing here needs a device, root or the network
// beyond loopback: the examples open a port that does not exist, a temporary
// file whose mode forbids the open (POSIX; skipped for root, which ignores
// the mode) and a loopback port nobody listens on. The listing of ports with
// a denied one is a made-up list: the real one, with a state per port, comes
// from enumerate_serial_ports_detailed (), which opens every port and so is
// left out of the examples.
//
// The udev rules that fix a permission_denied for good, and how to install
// them, are described in lumex/examples/transport/udev/README.md (installed
// as share/LumexLib/udev/README.md).
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <string>
#include <system_error>
#include <vector>

#if !defined(_WIN32)
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include "lumex/applied/transport/LumexTransport"
#include "lumex/core/utility/os/LumexSystemError.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-libc-call"
#pragma clang diagnostic ignored "-Wnrvo"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif
namespace tr = lumex::applied::transport;
namespace en = lumex::applied::serial::enumeration;

namespace
{
using tr::error::transport_errc;

struct named_errc
{
  transport_errc value;
  char const *name;
};

// Every value of transport_errc with the name it has in the code.
named_errc const KALL_ERRORS[]
    = { { transport_errc::timeout, "timeout" },
        { transport_errc::cancelled, "cancelled" },
        { transport_errc::closed, "closed" },
        { transport_errc::not_open, "not_open" },
        { transport_errc::disconnected, "disconnected" },
        { transport_errc::would_deadlock, "would_deadlock" },
        { transport_errc::invalid_endpoint, "invalid_endpoint" },
        { transport_errc::report_too_large, "report_too_large" },
        { transport_errc::bad_report_descriptor, "bad_report_descriptor" },
        { transport_errc::permission_denied, "permission_denied" },
        { transport_errc::unknown_report_id, "unknown_report_id" } };

void
the_error_codes ()
{
  std::cout << "--- 1. transport_errc\n";
  for (std::size_t i = 0; i < sizeof KALL_ERRORS / sizeof KALL_ERRORS[0]; ++i)
    {
      std::error_code const code
          = tr::error::make_error_code (KALL_ERRORS[i].value);
      std::cout << "  " << KALL_ERRORS[i].name << ": " << code.message ()
                << '\n';
    }

  // Three values mean what a standard condition means, so code that does not
  // know the transport can still test them.
  std::error_code const timeout
      = tr::error::make_error_code (transport_errc::timeout);
  std::error_code const denied
      = tr::error::make_error_code (transport_errc::permission_denied);
  std::cout << "  timeout == std::errc::timed_out: "
            << (timeout == std::errc::timed_out ? "yes" : "no")
            << ", permission_denied == std::errc::permission_denied: "
            << (denied == std::errc::permission_denied ? "yes" : "no") << '\n';

  // An error code made in another binary (a DLL that links the static
  // transport library has its own category object) compares through the
  // name of the category.
  std::cout
      << "  is_transport_error (timeout, timeout): "
      << (tr::error::is_transport_error (timeout, transport_errc::timeout)
              ? "yes"
              : "no")
      << ", (timeout, cancelled): "
      << (tr::error::is_transport_error (timeout, transport_errc::cancelled)
              ? "yes"
              : "no")
      << '\n';
}

// What the application tells its user when a port cannot be opened. Short,
// in the user's terms, with the one action that helps. The transport's own
// log record (level error, component transport.serial) carries the technical
// detail: the node, its owner, group and mode.
std::string
explain_open_failure (std::string const &port, std::error_code const &error)
{
  if (error == transport_errc::permission_denied)
    // POSIX only: the open failed with EACCES or EPERM.
    return "You may not open " + port
           + ". Add your user to the group that owns the port (usually "
             "dialout), or install the udev rule from "
             "share/LumexLib/udev, and log in again.";
  if (error == transport_errc::timeout)
    return port
           + " did not answer in time (a Bluetooth port that is paired "
             "but not connected?).";
  if (error == transport_errc::invalid_endpoint)
    return "No port matches " + port + ".";
  if (error == std::errc::no_such_file_or_directory
      || error == std::errc::no_such_device)
    return port + " is not there. Is the device plugged in?";
#if defined(_WIN32)
  // Windows reports ERROR_ACCESS_DENIED (5) for a COM port that another
  // program holds: the library leaves it a system error, it is not a
  // permission problem there.
  if (error.category () == std::system_category () && error.value () == 5)
    return port + " is in use by another program. Close it and try again.";
#endif
  // Anything else: the system's own words, with the code.
  return port + ": "
         + lumex::core::utility::os::format_system_error (error.value ());
}

// The log record the transport writes when an open fails.
std::mutex &
records_mutex ()
{
  static std::mutex mutex;
  return mutex;
}

std::vector<std::string> &
error_records ()
{
  static std::vector<std::string> records;
  return records;
}

void
keep_errors (tr::log::log_record_t const &record)
{
  if (record.level != tr::log::log_level::error)
    return;
  std::lock_guard<std::mutex> const lock (records_mutex ());
  error_records ().push_back (std::string (record.component) + ": "
                              + record.message);
}

tr::status_result_t
open_serial (tr::stream::io_runner &runner, std::string const &path)
{
  tr::serial::serial_stream_options_t options;
  options.path = path;
  tr::stream::byte_stream stream
      = tr::serial::make_serial_stream (runner.get_executor (), options);
  return stream.open (tr::deadline_t (3000));
}

void
open_errors_of_a_missing_port (tr::stream::io_runner &runner)
{
  std::cout << "--- 2. a port that does not exist\n";
  std::string const path = "/dev/lumex-example-no-such-port";
  tr::status_result_t const result = open_serial (runner, path);
  if (result.has_value ())
    {
      std::cout << "  opened?!\n";
      return;
    }
  // A system error stays what the system said: its category is the system
  // category and the value is errno (POSIX) or GetLastError () (Windows).
  std::error_code const error = result.error ();
  std::cout << "  category '" << error.category ().name () << "', value "
            << error.value () << ", message '" << error.message () << "'\n"
            << "  format_system_error: '"
            << lumex::core::utility::os::format_system_error (error.value ())
            << "'\n"
            << "  for the user: " << explain_open_failure (path, error)
            << '\n';
}

#if !defined(_WIN32)
// A file in the temporary directory whose mode forbids the open, standing in
// for a device node the user may not use. Removed by the destructor.
class forbidden_node
{
public:
  forbidden_node ()
  {
    char const *const dir = std::getenv ("TMPDIR");
    path_ = std::string (dir != nullptr && dir[0] != '\0' ? dir : "/tmp")
            + "/lumex_example_XXXXXX";
    int const fd = ::mkstemp (&path_[0]);
    if (fd < 0)
      {
        path_.clear ();
        return;
      }
    ::close (fd);
    if (::chmod (path_.c_str (), 0) != 0)
      {
        ::unlink (path_.c_str ());
        path_.clear ();
      }
  }

  ~forbidden_node ()
  {
    if (!path_.empty ())
      ::unlink (path_.c_str ());
  }

  forbidden_node (forbidden_node const &) = delete;
  forbidden_node &operator= (forbidden_node const &) = delete;

  std::string const &
  path () const
  {
    return path_;
  }

private:
  std::string path_;
};
#endif

bool
no_access_to_the_port (tr::stream::io_runner &runner)
{
  std::cout << "--- 3. no access to the port\n";
#if defined(_WIN32)
  // Access denied on Windows is a busy port. A directory stands in for it:
  // CreateFile on a directory without the backup flag fails with
  // ERROR_ACCESS_DENIED, which the library does not turn into
  // permission_denied.
  char const *const root = std::getenv ("SystemRoot");
  if (root == nullptr)
    {
      std::cout << "  SystemRoot is not set: nothing to show\n";
      return true;
    }
  std::string const path = root;
  std::string const shown = "COM3 (stand-in)";
#else
  if (::geteuid () == 0)
    {
      std::cout << "  running as root, which ignores the mode of a file: "
                   "nothing to show\n";
      return true;
    }
  forbidden_node const node;
  if (node.path ().empty ())
    {
      std::cout << "  cannot create the temporary node: nothing to show\n";
      return true;
    }
  std::string const &path = node.path ();
  std::string const shown = "/dev/ttyUSB0 (stand-in)";
#endif
  tr::log::set_log_sink (&keep_errors);
  tr::log::set_log_level (tr::log::log_level::error);
  tr::status_result_t const result = open_serial (runner, path);
  tr::log::set_log_sink (nullptr);
  tr::log::set_log_level (tr::log::log_level::warning);
  if (result.has_value ())
    {
      std::cout << "  the open succeeded?!\n";
      return false;
    }
  std::error_code const error = result.error ();
  std::cout << "  error: category '" << error.category ().name ()
            << "', permission_denied: "
            << (error == transport_errc::permission_denied ? "yes" : "no")
            << '\n'
            << "  the application tells its user:\n    "
            << explain_open_failure (shown, error) << '\n';
  // The transport's own record is for the program's log, not for the user:
  // it names the node, its owner, group and mode, and then says how to fix
  // it. The example prints only the advice (the part after the first "; "):
  // the owner is a name of this machine.
  std::lock_guard<std::mutex> const lock (records_mutex ());
  std::cout << "  records at level error: " << error_records ().size ()
            << '\n';
  for (std::size_t i = 0; i < error_records ().size (); ++i)
    {
      std::string const &record = error_records ()[i];
      std::string::size_type const advice = record.find ("; ");
      if (advice != std::string::npos)
        std::cout << "  the advice in the record: "
                  << record.substr (advice + 2) << '\n';
    }
#if defined(_WIN32)
  return error.category () == std::system_category ()
         && !(error == transport_errc::permission_denied);
#else
  return error == transport_errc::permission_denied
         && error == std::errc::permission_denied;
#endif
}

en::serial_port_info_t
port_with (std::string const &path, en::serial_port_state state,
           std::string const &note)
{
  en::serial_port_info_t port;
  port.path = path;
  port.state = state;
  port.system_error = note;
  return port;
}

void
a_listing_with_a_denied_port ()
{
  std::cout << "--- 4. a port listing with a denied port (a made-up list)\n";
  std::vector<en::serial_port_info_t> ports;
  ports.push_back (
      port_with ("ttyUSB0", en::serial_port_state::available, ""));
  ports.push_back (
      port_with ("ttyUSB1", en::serial_port_state::denied,
                 lumex::core::utility::os::format_system_error (EACCES)));
  ports.push_back (port_with ("ttyS0", en::serial_port_state::free, ""));

  // The library's own table groups the ports by state; denied has a section
  // of its own, and only on POSIX.
  en::print_serial_ports_info (std::cout, ports);

  // A program that offers the ports to its user marks the denied ones and
  // says what to do, instead of hiding them.
  std::cout << "\n  for the user:\n";
  for (std::size_t i = 0; i < ports.size (); ++i)
    {
      std::cout << "    " << ports[i].path << " ("
                << en::serial_port_state_to_string (ports[i].state) << ")";
      if (ports[i].state == en::serial_port_state::denied)
        std::cout << " - you may not open it; see the udev README";
      std::cout << '\n';
    }
}

bool
a_refused_connection (tr::stream::io_runner &runner)
{
  std::cout << "--- 5. a TCP port nobody listens on\n";
  std::uint16_t port = 0;
  {
    // The port number of a listener that is closed again at once.
    asio::io_context context;
    asio::ip::tcp::acceptor acceptor (
        context,
        asio::ip::tcp::endpoint (asio::ip::address_v4::loopback (), 0));
    port = acceptor.local_endpoint ().port ();
  }
  tr::net::tcp_options_t options;
  options.host = "127.0.0.1";
  options.port = port;
  tr::stream::byte_stream stream
      = tr::net::make_tcp_stream (runner.get_executor (), options);
  tr::status_result_t const result = stream.open (tr::deadline_t (3000));
  if (result.has_value ())
    {
      std::cout << "  connected?! something listens on the port now\n";
      return true;
    }
  std::error_code const error = result.error ();
  std::cout << "  refused: '" << error.message () << "', "
            << (error.category () == std::system_category ()
                    ? "a system error, kept as it came"
                    : "a transport error")
            << '\n';

  // Reading from a stream that never opened is a program error, and says so.
  std::vector<std::uint8_t> buffer (8);
  tr::io_result_t const read
      = stream.read_some (tr::mutable_bytes_t (buffer), tr::deadline_t (100));
  std::cout << "  read on the closed stream: " << read.error ().message ()
            << " (not_open: "
            << (read.error () == transport_errc::not_open ? "yes" : "no")
            << ")\n";
  return read.error () == transport_errc::not_open;
}
} // namespace

int
main ()
{
  tr::stream::io_runner runner (1);
  the_error_codes ();
  open_errors_of_a_missing_port (runner);
  if (!no_access_to_the_port (runner))
    return 1;
  a_listing_with_a_denied_port ();
  if (!a_refused_connection (runner))
    return 1;
  return 0;
}
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
