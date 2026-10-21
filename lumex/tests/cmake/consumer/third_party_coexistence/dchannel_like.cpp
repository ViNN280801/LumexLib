// The DChannel-like shared library of the coexistence fixture: its own copy of
// Asio and hidapi, used the way DChannel uses them.

#include <asio.hpp>
#include <hidapi.h>

extern "C" int
dchannel_like_run ()
{
  asio::io_context io;
  int ran = 0;
  asio::post (io, [&ran] () { ran = 1; });
  io.run ();

  hid_device_info *devices = hid_enumerate (0, 0);
  hid_free_enumeration (devices);
  return ran;
}
