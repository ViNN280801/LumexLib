#include <iostream>
#include <string>

#include "lumex/core/expected/Expected"

using namespace lumex::core::expected::result;
using namespace lumex::core::expected::error;

namespace
{
Expected<int, std::string>
parse_channel (char const *text)
{
  if (text == nullptr || text[0] == '\0')
    return Expected<int, std::string> (unexpect, std::string ("empty"));
  if (text[0] < '0' || text[0] > '9')
    return Expected<int, std::string> (unexpect, std::string ("not a digit"));
  return Expected<int, std::string> (text[0] - '0');
}
}

int
main ()
{
  std::cout << "=== Expected success / error / observers ===\n\n";

  Expected<int, std::string> ok (42);
  Expected<int, std::string> err (unexpect, std::string ("boom"));

  std::cout << "--- 1. has_value / bool / value ---\n";
  std::cout << "ok.has_value=" << (ok.has_value () ? "yes" : "no")
            << " bool=" << (ok ? "yes" : "no") << " value=" << ok.value ()
            << '\n';
  std::cout << "err.has_value=" << (err.has_value () ? "yes" : "no")
            << " error=" << err.error () << '\n';

  std::cout << "\n--- 2. value_or / error_or ---\n";
  std::cout << "err.value_or(-1)=" << err.value_or (-1)
            << " ok.error_or(\"none\")=" << ok.error_or (std::string ("none"))
            << '\n';

  std::cout << "\n--- 3. Factory-style parse ---\n";
  Expected<int, std::string> const ch0 = parse_channel ("2");
  Expected<int, std::string> const ch1 = parse_channel ("");
  Expected<int, std::string> const ch2 = parse_channel ("x");
  std::cout << "parse(\"2\")=" << ch0.value_or (-1) << '\n';
  std::cout << "parse(\"\") err=" << ch1.error_or (std::string ("?")) << '\n';
  std::cout << "parse(\"x\") err=" << ch2.error_or (std::string ("?")) << '\n';

  std::cout << "\n--- 4. Expected<void, E> ---\n";
  Expected<void, std::string> done;
  Expected<void, std::string> failed (unexpect, std::string ("busy"));
  std::cout << "void_ok=" << (done ? "yes" : "no")
            << " void_err=" << failed.error () << '\n';

  std::cout << "\n=== Expected example finished ===\n";
  return 0;
}
