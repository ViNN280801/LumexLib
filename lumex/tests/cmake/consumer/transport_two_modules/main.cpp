// An executable and a DLL that both link the static transport library and
// share one io_context (spec G6). The DLL connects a TCP stream on the
// executor of the executable's io_runner to a loopback listener of the
// executable, which accepts and closes the connection. Checks, by exit code
// (0 fine):
//   1 the DLL's read did not end with a transport error the executable
//     recognizes with is_transport_error (Asio's eof arrives as disconnected);
//   2 the two binaries share one category object (the library is static, so
//     each must have its own; then `==` holds only inside one binary);
//   3 the DLL's slot sees the executable's sink (each binary has its own);
//   4 a record of the DLL's stream reached the executable's sink;
//   5 the listener failed.

#include <atomic>
#include <cstdio>
#include <string>
#include <thread>

#include "Module.hpp"

namespace
{
namespace tr = lumex::applied::transport;

std::atomic<int> records_of_module (0);
std::string module_stream_name;

void
count_sink (tr::log::log_record_t const &record)
{
  if (!module_stream_name.empty () && module_stream_name == record.instance)
    ++records_of_module;
}
} // namespace

int
main ()
{
  tr::stream::io_runner runner (1);
  asio::ip::tcp::acceptor acceptor (
      runner.context (),
      asio::ip::tcp::endpoint (asio::ip::address_v4::loopback (), 0));
  std::uint16_t const port = acceptor.local_endpoint ().port ();
  module_stream_name = "127.0.0.1:" + std::to_string (port);

  tr::log::set_log_level (tr::log::log_level::trace);
  tr::log::set_log_sink (&count_sink);

  bool accepted = false;
  std::thread listener (
      [&acceptor, &accepted]
        {
          asio::ip::tcp::socket peer (acceptor.get_executor ());
          asio::error_code error;
          acceptor.accept (peer, error);
          accepted = !error;
          peer.shutdown (asio::ip::tcp::socket::shutdown_both, error);
          peer.close (error);
        });

  std::string name;
  std::error_code const error
      = fixture::read_until_closed (runner.get_executor (), port, name);
  listener.join ();
  if (!accepted)
    return 5;

  std::printf ("transport_two_modules: %s: %s [%s]\n", name.c_str (),
               error.message ().c_str (), error.category ().name ());
  if (!tr::error::is_transport_error (error,
                                      tr::error::transport_errc::disconnected))
    return 1;
  if (fixture::category_in_module () == &tr::error::transport_category ())
    return 2;
  if (fixture::sink_in_module () == &count_sink)
    return 3;
  if (records_of_module.load () != 0)
    return 4;
  std::printf ("transport_two_modules: one io_context, a category and a "
               "log slot per binary\n");
  return 0;
}
