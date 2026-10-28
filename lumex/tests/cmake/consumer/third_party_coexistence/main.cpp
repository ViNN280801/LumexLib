// Runs Asio and hidapi from the executable (LumexLib's copies) and from the
// other shared library (its own copies) in one process, interleaved.

#include <cstdio>

#include <asio.hpp>
#include <hidapi.h>

extern "C" int other_library_run ();

int
main ()
{
  int ok = 0;
  for (int round = 0; round < 3; ++round)
    {
      asio::io_context io;
      int ran = 0;
      asio::post (io, [&ran] () { ran = 1; });
      io.run ();
      hid_device_info *devices = hid_enumerate (0, 0);
      hid_free_enumeration (devices);

      ok += ran + other_library_run ();
    }
  std::printf ("ok=%d\n", ok);
  return ok == 6 ? 0 : 1;
}
