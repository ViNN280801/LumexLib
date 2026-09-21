#include <iostream>
#include <string>
#include <vector>

#include "lumex/applied/serial/LumexSerialPort"

using namespace lumex::applied::serial::enumeration;

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

  std::cout << "\n=== Serial example finished ===\n";
  return 0;
}
