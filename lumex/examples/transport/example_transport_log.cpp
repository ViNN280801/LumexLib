// The log of lumex::transport. Part 1: the sink of the program, the level
// threshold, the component filter, the byte dump and the program's own
// records through LUMEX_TRANSPORT_LOG, on a TCP stream that talks to an echo
// peer on loopback (started here). Part 2 (when the library was built with the
// logger module, LUMEX_TRANSPORT_WITH_LOGGER): the same events routed into the
// LumexLogger file with log::install_lumex_logger_sink (), where the
// transport's threshold follows the logger: off while the logger is disabled,
// the logger's level once the trigger file appears, and so on at every
// change of the file. A program that logs only through the transport needs
// something that re-reads the trigger file: here a lumex_logger_watch_timer
// on the io_runner.
//
// The example writes a trigger file of its own name next to the executable
// (the logger reads it from there) and removes it, and the log file it
// opened, at the end. The directory is taken from argv[0]; when the logger
// does not come up there, part 2 says so and stops.
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <iterator>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "lumex/applied/transport/LumexTransport"

#if defined(LUMEX_TRANSPORT_WITH_LOGGER)
#include "lumex/applied/logger/LumexLogger"
#include "lumex/applied/transport/log/adapter/LumexTransportLoggerAdapter.hpp"
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
namespace tlog = lumex::applied::transport::log;

namespace
{
using ms = std::chrono::milliseconds;

char const *
level_name (tlog::log_level level)
{
  switch (level)
    {
    case tlog::log_level::trace:
      return "trace";
    case tlog::log_level::debug:
      return "debug";
    case tlog::log_level::info:
      return "info";
    case tlog::log_level::warning:
      return "warning";
    case tlog::log_level::error:
      return "error";
    default:
      return "off";
    }
}

// The sink is a plain function (no state to hand it): the records it takes
// go to a list behind a lock. Every pointer of a record is valid only during
// the call, so the text is copied.
struct line_t
{
  tlog::log_level level;
  std::string text;
};

std::mutex &
lines_mutex ()
{
  static std::mutex mutex;
  return mutex;
}

std::vector<line_t> &
lines ()
{
  static std::vector<line_t> list;
  return list;
}

void
collect (tlog::log_record_t const &record)
{
  line_t line;
  line.level = record.level;
  line.text = std::string ("[") + record.component + ' ' + record.instance
              + "] " + record.message;
  if (!record.bytes.empty ())
    line.text += " | " + tlog::format_dump (record.bytes);
  std::lock_guard<std::mutex> const lock (lines_mutex ());
  lines ().push_back (line);
}

// Prints and forgets what the sink collected.
std::size_t
print_collected (char const *title)
{
  std::lock_guard<std::mutex> const lock (lines_mutex ());
  std::cout << "  " << title << ": " << lines ().size () << " record(s)\n";
  for (std::size_t i = 0; i < lines ().size (); ++i)
    std::cout << "    " << level_name (lines ()[i].level) << " "
              << lines ()[i].text << '\n';
  std::size_t const count = lines ().size ();
  lines ().clear ();
  return count;
}

// A peer that echoes one connection, on its own thread with plain Asio.
void
echo_one_connection (asio::ip::tcp::acceptor &acceptor)
{
  asio::ip::tcp::socket socket (acceptor.get_executor ());
  asio::error_code error;
  acceptor.accept (socket, error);
  std::vector<std::uint8_t> buffer (256);
  while (!error)
    {
      std::size_t const bytes
          = socket.read_some (asio::buffer (buffer), error);
      if (!error)
        asio::write (socket, asio::buffer (buffer.data (), bytes), error);
    }
}

// One exchange with a fresh echo peer: open, write, read, close. What the
// transport logs for it depends only on the threshold and the filter.
bool
exchange (tr::stream::io_runner &runner)
{
  asio::io_context peer_context;
  asio::ip::tcp::acceptor acceptor (
      peer_context,
      asio::ip::tcp::endpoint (asio::ip::address_v4::loopback (), 0));
  std::thread peer ([&acceptor] { echo_one_connection (acceptor); });
  tr::net::tcp_options_t options;
  options.host = "127.0.0.1";
  options.port = acceptor.local_endpoint ().port ();
  bool ok = false;
  {
    tr::stream::byte_stream stream
        = tr::net::make_tcp_stream (runner.get_executor (), options);
    if (stream.open (tr::deadline_t (3000)).has_value ())
      {
        std::string const request = "AT\r\n";
        std::vector<std::uint8_t> const bytes (request.begin (),
                                               request.end ());
        std::vector<std::uint8_t> reply (16);
        ok = stream
                 .write_all (tr::const_bytes_t (bytes), tr::deadline_t (1000))
                 .has_value ()
             && stream
                    .read_some (tr::mutable_bytes_t (reply),
                                tr::deadline_t (1000))
                    .has_value ();
      }
    stream.close ();
  }
  peer.join ();
  return ok;
}

bool
own_sink_levels_and_filters (tr::stream::io_runner &runner)
{
  std::cout << "--- 1. a sink of the program, the threshold and the filter\n";
  tlog::set_log_sink (&collect);

  // The default threshold is warning: a healthy exchange logs nothing.
  tlog::set_log_level (tlog::log_level::warning);
  if (!exchange (runner))
    return false;
  std::size_t const quiet = print_collected ("threshold warning");

  // From info on, open and close are visible; at trace every chunk too,
  // with its bytes (the dump: hex, then the text with control bytes named).
  tlog::set_log_level (tlog::log_level::info);
  if (!exchange (runner))
    return false;
  std::size_t const info = print_collected ("threshold info");

  tlog::set_log_level (tlog::log_level::trace);
  if (!exchange (runner))
    return false;
  std::size_t const trace = print_collected ("threshold trace");

  // A component filter: a record passes when its component starts with an
  // `allow` entry (all pass when `allow` is empty) and with no `deny` entry.
  std::vector<std::string> const none;
  std::vector<std::string> const only_tcp (1, "transport.tcp");
  tlog::set_component_filter (none, only_tcp);
  if (!exchange (runner))
    return false;
  std::size_t const denied = print_collected ("transport.tcp denied");
  tlog::set_component_filter (none, none);

  // The program's own records use the same sink, threshold and filter. The
  // message is built only when the record will be written.
  std::cout << "  log_enabled (info, \"app.client\"): "
            << (tlog::log_enabled (tlog::log_level::info, "app.client") ? "yes"
                                                                        : "no")
            << '\n';
  LUMEX_TRANSPORT_LOG (info, "app.client", "bench",
                       std::string ("exchange done"), tlog::log_bytes_t ());
  std::size_t const own = print_collected ("a record of the program");

  // The dump shows at most dump-limit bytes; the rest is only counted.
  tlog::set_dump_limit (4);
  std::vector<std::uint8_t> const wire
      = { 0x41, 0x54, 0x0D, 0x0A, 0x4F, 0x4B };
  std::cout << "  dump limited to 4 B: "
            << tlog::format_dump (
                   tlog::log_bytes_t (wire.data (), wire.size ()))
            << '\n';
  tlog::set_dump_limit (256);

  tlog::set_log_sink (nullptr); // back to the default: warning+ to std::cerr
  tlog::set_log_level (tlog::log_level::warning);
  return quiet == 0 && info > 0 && trace > info && denied < trace && own == 1;
}

#if defined(LUMEX_TRANSPORT_WITH_LOGGER)
using lumex::applied::logger::logger::LumexLogger;

char const *const KTRIGGER_NAME = "lumex_transport_example_trigger";

std::string
directory_of (std::string const &path)
{
  std::string::size_type const slash = path.find_last_of ("/\\");
  return slash == std::string::npos ? std::string (".")
                                    : path.substr (0, slash);
}

// The trigger file text in the format this build of the logger reads.
// Plain text first; HINT=false keeps the logger from writing its helper
// file next to the executable.
bool
trigger_text (std::string const &level, std::string &text)
{
#if defined(LUMEX_LOGGER_CONFIG_FORMAT_PLAIN_TEXT)
  text = "LEVEL=" + level + "\nHINT=false\n";
  return true;
#elif defined(LUMEX_LOGGER_CONFIG_FORMAT_INI)
  text = "[logger]\nLEVEL=" + level + "\nHINT=false\n";
  return true;
#elif defined(LUMEX_LOGGER_CONFIG_FORMAT_JSON)
  text = "{\n  \"logger\": {\n    \"LEVEL\": \"" + level
         + "\",\n    \"HINT\": \"false\"\n  }\n}\n";
  return true;
#elif defined(LUMEX_LOGGER_CONFIG_FORMAT_XML)
  text = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<logger>\n  <LEVEL>"
         + level + "</LEVEL>\n  <HINT>false</HINT>\n</logger>\n";
  return true;
#else
  (void)level;
  (void)text;
  return false; // the reserved YAML format cannot be read yet
#endif
}

bool
write_trigger (std::string const &path, std::string const &level)
{
  std::string text;
  if (!trigger_text (level, text))
    return false;
  std::ofstream out (path.c_str (),
                     std::ios::out | std::ios::trunc | std::ios::binary);
  out << text;
  out.close ();
  return !out.fail ();
}

std::string
read_file (std::string const &path)
{
  std::ifstream in (path.c_str (), std::ios::in | std::ios::binary);
  return std::string (std::istreambuf_iterator<char> (in),
                      std::istreambuf_iterator<char> ());
}

template <typename Condition>
bool
eventually (Condition condition)
{
  std::chrono::steady_clock::time_point const end
      = std::chrono::steady_clock::now () + std::chrono::seconds (5);
  while (!condition ())
    {
      if (std::chrono::steady_clock::now () >= end)
        return false;
      std::this_thread::sleep_for (ms (5));
    }
  return true;
}

// The transport lines of the logger file that came after the first `seen`
// ones, without the logger's timestamp and thread prefix (from the level on).
// Returns how many transport lines the file holds.
std::size_t
print_new_transport_lines (LumexLogger &logger, char const *title,
                           std::size_t seen)
{
  logger.flush ();
  std::string const text = read_file (logger.get_log_file_path ());
  std::cout << "  " << title << '\n';
  std::size_t found = 0;
  std::string::size_type begin = 0;
  while (begin < text.size ())
    {
      std::string::size_type end = text.find ('\n', begin);
      if (end == std::string::npos)
        end = text.size ();
      std::string const line = text.substr (begin, end - begin);
      begin = end + 1;
      std::string::size_type const mark = line.find ("[transport.");
      if (mark == std::string::npos || mark == 0)
        continue;
      if (++found <= seen)
        continue;
      std::string::size_type const level = line.rfind ('[', mark - 1);
      std::cout << "    "
                << line.substr (level == std::string::npos ? mark : level)
                << '\n';
    }
  if (found == seen)
    std::cout << "    (no new transport line)\n";
  return found;
}

bool
routed_into_the_logger (tr::stream::io_runner &runner,
                        std::string const &directory)
{
  std::cout << "--- 2. the transport log in the LumexLogger file\n";
  LumexLogger &logger = LumexLogger::get_instance ();
  std::string const trigger = directory + "/" + KTRIGGER_NAME;
  std::remove (trigger.c_str ());
  // A trigger file of this example's own name: the enable_logs of the
  // machine is not touched.
  logger.set_trigger_file_name (KTRIGGER_NAME);
  logger.force_reread ();

  // The sink goes into this binary's slot and the threshold follows the
  // logger: off while the logger is disabled, so no record is formatted.
  bool const follows = tlog::install_lumex_logger_sink ();
  std::cout << "  installed, the threshold follows the logger: "
            << (follows ? "yes" : "no") << ", threshold now "
            << level_name (tlog::get_log_level ()) << '\n';

  // The logger notices the new file only when something looks: a timer on
  // the io_runner does it for a program that writes to the logger only
  // through the transport. Destroy the timer before the runner.
  bool ok = false;
  {
    tlog::lumex_logger_watch_timer timer (runner, ms (20));
    ok = write_trigger (trigger, "trace")
         && eventually (
             [] { return tlog::get_log_level () == tlog::log_level::trace; });
    std::cout << "  trigger file with LEVEL=trace: threshold "
              << level_name (tlog::get_log_level ()) << '\n';
    if (!ok)
      {
        std::cout << "  the logger did not come up from " << trigger
                  << "; nothing more to show\n";
        std::remove (trigger.c_str ());
        tlog::uninstall_lumex_logger_sink ();
        logger.set_trigger_file_name ("enable_logs");
        return true;
      }
    if (!exchange (runner))
      ok = false;
    std::size_t const at_trace
        = print_new_transport_lines (logger, "in the log file at trace", 0);
    std::string const log_path = logger.get_log_file_path ();

    // The file changes, the threshold follows without a restart.
    ok = write_trigger (trigger, "warning")
         && eventually (
             [] { return tlog::get_log_level () == tlog::log_level::warning; })
         && ok;
    std::cout << "  trigger file with LEVEL=warning: threshold "
              << level_name (tlog::get_log_level ()) << '\n';
    exchange (runner);
    ok = print_new_transport_lines (logger, "added at warning", at_trace)
             == at_trace
         && at_trace > 0 && ok;

    // The file is gone: the logger is disabled, the threshold is off.
    std::remove (trigger.c_str ());
    ok = eventually (
             [] { return tlog::get_log_level () == tlog::log_level::off; })
         && ok;
    std::cout << "  trigger file removed: threshold "
              << level_name (tlog::get_log_level ()) << '\n';
    std::remove (log_path.c_str ());
  }

  tlog::uninstall_lumex_logger_sink (); // the default sink is back
  logger.set_trigger_file_name ("enable_logs");
  return ok;
}
#endif
} // namespace

int
main (int, char **argv)
{
  tr::stream::io_runner runner (1);
  if (!own_sink_levels_and_filters (runner))
    return 1;
#if defined(LUMEX_TRANSPORT_WITH_LOGGER)
  if (!routed_into_the_logger (runner, directory_of (argv[0])))
    return 1;
#else
  (void)argv;
  std::cout << "--- 2. built without the logger module (LUMEX_BUILD_LOGGER): "
               "no adapter to LumexLogger\n";
#endif
  return 0;
}
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
