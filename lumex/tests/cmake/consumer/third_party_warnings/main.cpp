// Includes the third-party headers of the transport layer under the strictest
// warning flags of each compiler (see CMakeLists.txt); the build fails on any
// diagnostic they produce. Uses a little of each so templates instantiate.

#include <cstdio>

#include <asio.hpp>
#include <asio/spawn.hpp>
#include <boost/context/fiber.hpp>
#if !defined(FIXTURE_NO_HIDAPI)
#include <hidapi.h>
#endif

int
main ()
{
  asio::io_context io;
  int value = 0;
  asio::spawn (
      io,
      [&value] (asio::yield_context yield)
        {
          asio::post (yield);
          value = 1;
        },
      asio::detached);
  io.run ();

#if !defined(FIXTURE_NO_HIDAPI)
  if (hid_version () == nullptr)
    return 2;
#endif

  std::printf ("value=%d\n", value);
  return value == 1 ? 0 : 1;
}
