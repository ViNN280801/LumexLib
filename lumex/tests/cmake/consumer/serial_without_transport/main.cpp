// The serial module without the transport module: enumerate the ports (the
// list may be empty) and print how many there are. Exit code 0.

#include <cstdio>
#include <string>
#include <vector>

#include "lumex/applied/serial/LumexSerialPort"

int
main ()
{
  std::vector<std::string> const ports
      = lumex::applied::serial::enumeration::enumerate_serial_port_names ();
  std::printf ("serial_without_transport: %u port(s)\n",
               static_cast<unsigned> (ports.size ()));
  return 0;
}