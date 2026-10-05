#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "lumex/applied/serial/LumexSerialPort"

using namespace lumex::applied::serial::enumeration;
using namespace lumex::applied::serial::port;
using namespace lumex::applied::serial::probe;

namespace
{
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
} // namespace

int
main ()
{
  std::cout << "=== Workflow: pick a non-Bluetooth COM name ===\n\n";

  std::vector<std::string> const names = enumerate_serial_port_names (true);
  if (names.empty ())
    {
      std::cout << "no serial ports reported\n";
      return 0;
    }

  std::string const chosen = names.front ();
  std::cout << "candidate=" << chosen << " bluetooth="
            << (is_bluetooth_enumerated_port (chosen) ? "yes" : "no") << '\n';
  std::cout << "not opening the port in this example\n";

  std::cout
      << "\n=== Workflow: bounded open of a deliberately wrong path ===\n";
  std::string const absent_path = resolve_serial_port_path (
      "USB9999",
      lumex::applied::serial::port::Constants::KSERIAL_PORT_CHANNEL_TYPE);
  SerialProber prober (absent_path, serial_port_settings_t ());

  serial_io_result_t const opened
      = prober.open (std::chrono::milliseconds (100));
  std::cout << "path=" << absent_path
            << " status=" << io_status_token (opened.status) << '\n';
  std::cout << "error=" << opened.system_error << '\n';

  prober.close ();
  prober.close (); // idempotent
  std::cout << "is_open_after_close=" << (prober.is_open () ? "true" : "false")
            << '\n';
  return 0;
}
