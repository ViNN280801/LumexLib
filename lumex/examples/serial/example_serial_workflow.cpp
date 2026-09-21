#include <iostream>
#include <string>
#include <vector>

#include "lumex/applied/serial/LumexSerialPort"

using namespace lumex::applied::serial::enumeration;

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
  return 0;
}
