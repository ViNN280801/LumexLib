#include <cstddef>
#include <iostream>
#include <string>

#include "lumex/core/optional/LumexOptional"

namespace
{
optional<int>
parse_port (std::string const &text)
{
  if (text.empty ()
      || text.find_first_not_of ("0123456789") != std::string::npos)
    return nullopt;
  return optional<int> (std::stoi (text));
}

optional<int>
default_port ()
{
  return optional<int> (502);
}
}

int
main ()
{
  // A literal converts to the optional, and so does a string to be assigned.
  optional<std::string> setting = "COM3";
  setting = "COM4";

  // transform maps the value, and_then chains a step that may be empty,
  // or_else supplies a fallback; none of them runs on an empty optional.
  optional<std::string> const configured = std::string ("1502");
  optional<int> const port = configured.and_then (parse_port);
  optional<std::size_t> const digits = configured.transform (
      [] (std::string const &text) { return text.size (); });
  optional<int> const fallback
      = optional<std::string> ().and_then (parse_port).or_else (default_port);

  std::cout << "setting=" << *setting << '\n';
  std::cout << "port=" << port.value_or (0)
            << " digits=" << digits.value_or (0)
            << " fallback=" << fallback.value_or (0) << '\n';
  return 0;
}
