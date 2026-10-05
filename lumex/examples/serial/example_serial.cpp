#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "lumex/applied/serial/LumexSerialPort"

using namespace lumex::applied::serial::enumeration;
using namespace lumex::applied::serial::probe;

namespace
{
// Absent path used by the bounded-open section; no scan can create it, so
// the example never opens a real port.
std::string
absent_port_path ()
{
#if defined(_WIN32)
  return "\\\\.\\LUMEXNOPE";
#else
  return "/dev/ttyLUMEXNOPE";
#endif
}

char const *
io_status_token (serial_io_status status)
{
  switch (status)
    {
    case serial_io_status::ok:
      return "ok";
    case serial_io_status::deadline_exceeded:
      return "deadline_exceeded";
    case serial_io_status::failed:
      return "failed";
    case serial_io_status::not_open:
      return "not_open";
    }
  return "unknown";
}

char const *
probe_status_token (serial_probe_status status)
{
  switch (status)
    {
    case serial_probe_status::responded:
      return "responded";
    case serial_probe_status::incomplete:
      return "incomplete";
    case serial_probe_status::no_response:
      return "no_response";
    case serial_probe_status::open_timeout:
      return "open_timeout";
    case serial_probe_status::open_failed:
      return "open_failed";
    case serial_probe_status::io_error:
      return "io_error";
    }
  return "unknown";
}
} // namespace

int
main ()
{
  std::cout << "=== Serial port names and Bluetooth filter ===\n\n";

  std::cout << "--- 1. Names, keep Bluetooth ---\n";
  std::vector<std::string> const all = enumerate_serial_port_names (false);
  std::cout << "serial_ports=" << all.size () << '\n';
  for (std::string const &name : all)
    {
      std::cout << "  " << name << " bluetooth="
                << (is_bluetooth_enumerated_port (name) ? "yes" : "no")
                << '\n';
    }

  std::cout << "\n--- 2. Names, filter Bluetooth ---\n";
  std::vector<std::string> const filtered = enumerate_serial_port_names (true);
  std::cout << "filtered_ports=" << filtered.size () << '\n';

  std::cout << "\n--- 3. print_serial_ports_info from names only ---\n";
  std::vector<serial_port_info_t> infos;
  infos.reserve (filtered.size ());
  for (std::string const &name : filtered)
    {
      serial_port_info_t info;
      info.path = name;
      info.state = serial_port_state::available;
      infos.push_back (info);
    }
  print_serial_ports_info (std::cout, infos);

  std::cout << "\n--- 4. Bounded open of an absent port ---\n";
  SerialProber prober (absent_port_path (), serial_port_settings_t ());
  std::cout << "is_open=" << (prober.is_open () ? "true" : "false") << '\n';

  serial_read_result_t const read_result
      = prober.read (std::chrono::milliseconds (10));
  std::cout << "read_before_open=" << io_status_token (read_result.status)
            << '\n';

  serial_io_result_t const opened
      = prober.open (std::chrono::milliseconds (100));
  std::cout << "open_absent=" << io_status_token (opened.status)
            << " error=" << opened.system_error << '\n';

  serial_probe_result_t const probed = prober.probe (
      std::vector<std::uint8_t> (1U, 0x41U),
      [] (std::vector<std::uint8_t> const &) { return true; },
      std::chrono::milliseconds (100));
  std::cout << "probe_absent=" << probe_status_token (probed.status)
            << " elapsed_ms=" << probed.elapsed.count () << '\n';

  prober.close ();
  std::cout << "after_close_is_open=" << (prober.is_open () ? "true" : "false")
            << '\n';

  std::cout << "\n=== Serial example finished ===\n";
  return 0;
}
