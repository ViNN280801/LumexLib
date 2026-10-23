// The transport without the framing module: the umbrella header does not
// include the framed adapters, the macro is not defined, and a mock stream
// still answers a read.

#include <cstdint>
#include <iostream>
#include <system_error>
#include <vector>

#include "lumex/applied/transport/LumexTransport"
#include "lumex/applied/transport/testing/LumexMockStream.hpp"

#if defined(LUMEX_TRANSPORT_WITH_FRAMING)
#error "LUMEX_TRANSPORT_WITH_FRAMING is defined without the framing module"
#endif
#if defined(LUMEX_APPLIED_TRANSPORT_FRAMED_FRAMED_STREAM_HPP)
#error "the umbrella includes the framed adapters without the framing module"
#endif

int
main ()
{
  namespace tr = lumex::applied::transport;
  tr::stream::io_runner runner (1);
  tr::testing::mock_stream mock
      = tr::testing::make_mock_stream (runner.get_executor (), "plain");
  if (!mock.stream.open (tr::deadline_t (1000)).has_value ())
    return 1;
  mock.control.feed (std::vector<std::uint8_t> (3, std::uint8_t (7)));
  std::vector<std::uint8_t> buffer (3);
  tr::io_result_t const read = mock.stream.read_some (
      tr::mutable_bytes_t (buffer), tr::deadline_t (1000));
  bool const fine = read.has_value () && read.value () == 3 && buffer[0] == 7;
  std::cout << "transport_without_framing: " << (fine ? "fine" : "FAILED")
            << '\n';
  return fine ? 0 : 1;
}
