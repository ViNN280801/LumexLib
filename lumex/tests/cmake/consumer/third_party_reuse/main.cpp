// The consumer's own lumex_3rdparty_asio target must be the one in use.

#include <asio.hpp>
#include <boost/context/fiber.hpp>

#if !defined(FIXTURE_CONSUMER_OWNS_ASIO)
#error "lumex_setup_asio () replaced the consumer's lumex_3rdparty_asio target"
#endif

int
main ()
{
  asio::io_context io;
  int ran = 0;
  asio::post (io, [&ran] () { ran = 1; });
  io.run ();
  return ran == 1 ? 0 : 1;
}
