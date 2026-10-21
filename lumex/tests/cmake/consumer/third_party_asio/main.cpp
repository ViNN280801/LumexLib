// Runs the vendored Asio: a posted handler, a steady_timer and a TCP round
// trip over the loopback. Returns 0 when every step happened.

#include <array>
#include <cstddef>
#include <cstdio>
#include <string>

#include <asio.hpp>

#if !defined(ASIO_STANDALONE) || !defined(ASIO_NO_DEPRECATED)
#error                                                                        \
    "lumex::3rdparty_asio must define ASIO_STANDALONE and ASIO_NO_DEPRECATED"
#endif

#if ASIO_VERSION != 103600
#error "lumex::3rdparty_asio is not Asio 1.36.0"
#endif

int
main ()
{
  asio::io_context io;
  int steps = 0;

  asio::post (io, [&steps] () { ++steps; });

  asio::steady_timer timer (io, std::chrono::milliseconds (10));
  timer.async_wait (
      [&steps] (asio::error_code const &ec)
        {
          if (!ec)
            ++steps;
        });

#if defined(ASIO_HAS_SERIAL_PORT)
  asio::serial_port port (io); // the type exists; no device is opened
  if (!port.is_open ())
    ++steps;
#else
  ++steps;
#endif

  asio::ip::tcp::acceptor acceptor (
      io, asio::ip::tcp::endpoint (asio::ip::address_v4::loopback (), 0));
  asio::ip::tcp::socket server (io);
  asio::ip::tcp::socket client (io);
  std::array<char, 4> received = { { 0, 0, 0, 0 } };
  std::string const message = "ping";

  acceptor.async_accept (
      server,
      [&] (asio::error_code const &ec)
        {
          if (ec)
            return;
          asio::async_read (server, asio::buffer (received),
                            [&] (asio::error_code const &rec, std::size_t n)
                              {
                                if (!rec && n == 4
                                    && std::string (received.data (), 4)
                                           == message)
                                  ++steps;
                              });
        });
  client.async_connect (acceptor.local_endpoint (),
                        [&] (asio::error_code const &ec)
                          {
                            if (ec)
                              return;
                            asio::async_write (
                                client, asio::buffer (message),
                                [&] (asio::error_code const &wec, std::size_t)
                                  {
                                    if (!wec)
                                      ++steps;
                                  });
                          });

  io.run ();
  std::printf ("steps=%d\n", steps);
  return steps == 5 ? 0 : 1;
}
