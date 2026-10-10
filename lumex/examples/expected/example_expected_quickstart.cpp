#include <iostream>
#include <string>

#include "lumex/core/expected/Expected"

namespace ex = lumex::core::expected::result;

using channel_t = ex::expected<int, std::string>;

channel_t
parse_channel (char const *text)
{
  if (text == nullptr || text[0] == '\0')
    return ex::failure (std::string ("empty"));
  if (text[0] < '0' || text[0] > '9')
    return ex::failure (std::string ("not a digit"));
  return ex::success (text[0] - '0');
}

int
main ()
{
  channel_t const good = parse_channel ("5");
  channel_t const bad = parse_channel ("x");

  std::cout << good.transform ([] (int v) { return v * 2; }).value_or (-1)
            << '\n';                 // 10
  std::cout << bad.error () << '\n'; // not a digit

  channel_t const recovered
      = bad.or_else ([] (std::string const &) { return channel_t (0); });
  std::cout << recovered.value () << '\n'; // 0

  try
    {
      std::cout << bad.value () << '\n'; // an error: throws
    }
  catch (lumex::core::expected::error::bad_expected_access<std::string> const
             &failed)
    {
      std::cout << "value () threw: " << failed.error ()
                << '\n'; // not a digit
    }
  return 0;
}
