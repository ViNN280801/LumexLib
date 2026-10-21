#include <vector>

#include "Module.hpp"

namespace fixture
{
namespace tr = lumex::applied::transport;

std::error_code
read_until_closed (tr::executor_type const &executor, std::uint16_t port,
                   std::string &name)
{
  tr::net::tcp_options_t options;
  options.host = "127.0.0.1";
  options.port = port;
  tr::stream::byte_stream stream
      = tr::net::make_tcp_stream (executor, options);
  name = stream.name ();
  tr::status_result_t const opened = stream.open (tr::deadline_t (10000));
  if (!opened.has_value ())
    return opened.error ();
  std::vector<std::uint8_t> buffer (64);
  for (;;)
    {
      tr::io_result_t const got = stream.read_some (
          tr::mutable_bytes_t (buffer), tr::deadline_t (10000));
      if (!got.has_value ())
        return got.error ();
    }
}

tr::log::log_sink_t
sink_in_module ()
{
  return tr::log::get_log_sink ();
}

std::error_category const *
category_in_module ()
{
  return &tr::error::transport_category ();
}
} // namespace fixture
