// The interface between the executable and the library of the fixture.
#ifndef LUMEX_TESTS_TRANSPORT_TWO_MODULES_HPP
#define LUMEX_TESTS_TRANSPORT_TWO_MODULES_HPP

#include <cstdint>
#include <string>
#include <system_error>

#include "lumex/applied/transport/LumexTransport"

#if defined(_WIN32)
#if defined(TRANSPORT_FIXTURE_BUILDING)
#define TRANSPORT_FIXTURE_API __declspec (dllexport)
#else
#define TRANSPORT_FIXTURE_API __declspec (dllimport)
#endif
#else
#define TRANSPORT_FIXTURE_API __attribute__ ((visibility ("default")))
#endif

namespace fixture
{
// Connects a TCP stream made inside the library, on `executor`, to
// 127.0.0.1:`port` and reads until the read fails; returns that error (or the
// open's). `name` receives the stream's log name.
TRANSPORT_FIXTURE_API std::error_code
read_until_closed (lumex::applied::transport::executor_type const &executor,
                   std::uint16_t port, std::string &name);

// The log sink of the library's own slot (null: the default sink).
TRANSPORT_FIXTURE_API lumex::applied::transport::log::log_sink_t
sink_in_module ();

// The address of the transport category as the library sees it.
TRANSPORT_FIXTURE_API std::error_category const *category_in_module ();
} // namespace fixture

#endif // !LUMEX_TESTS_TRANSPORT_TWO_MODULES_HPP
