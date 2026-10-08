#include <iostream>
#include <string>

#include "lumex/core/expected/Expected"

// `unexpected` and `in_place` have no global alias (the MinGW runtime declares
// its own global `unexpected`): the using-directives bring in the module's.
using namespace lumex::core::expected::result;
using namespace lumex::core::expected::error;

namespace
{
expected<int, std::string>
parse_channel (char const *text)
{
  if (text == nullptr || text[0] == '\0')
    return expected<int, std::string> (unexpect, std::string ("empty"));
  if (text[0] < '0' || text[0] > '9')
    return expected<int, std::string> (unexpect, std::string ("not a digit"));
  return expected<int, std::string> (text[0] - '0');
}
}

int
main ()
{
  std::cout << "=== Expected success / error / observers ===\n\n";

  expected<int, std::string> ok (42);
  expected<int, std::string> err (unexpect, std::string ("boom"));

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
  expected<int, std::string> const ch0 = parse_channel ("2");
  expected<int, std::string> const ch1 = parse_channel ("");
  expected<int, std::string> const ch2 = parse_channel ("x");
  std::cout << "parse(\"2\")=" << ch0.value_or (-1) << '\n';
  std::cout << "parse(\"\") err=" << ch1.error_or (std::string ("?")) << '\n';
  std::cout << "parse(\"x\") err=" << ch2.error_or (std::string ("?")) << '\n';

  std::cout << "\n--- 4. Expected<void, E> ---\n";
  expected<void, std::string> done;
  expected<void, std::string> failed (unexpect, std::string ("busy"));
  std::cout << "void_ok=" << (done ? "yes" : "no")
            << " void_err=" << failed.error () << '\n';

  std::cout << "\n--- 5. success() / failure() factories ---\n";
  expected<int, std::string> const fromFactory = success (7);
  expected<int, std::string> const failedFactory
      = failure (std::string ("factory error"));
  expected<void, std::string> const voidFactory = success ();
  std::cout << "factory_ok=" << fromFactory.value ()
            << " factory_err=" << failedFactory.error ()
            << " factory_void=" << (voidFactory ? "yes" : "no") << '\n';

  std::cout << "\n--- 6. transform, and_then, or_else, transform_error ---\n";
  expected<int, std::string> const channel = parse_channel ("5");
  expected<int, std::string> const missing = parse_channel ("x");

  // A function that returns void gives expected<void, E>.
  int visited = 0;
  expected<void, std::string> const seen
      = channel.transform ([&visited] (int value) { visited = value; });

  // A function that returns an expected gives an expected of that expected:
  // the outer one says the channel parsed, the inner one that the port is
  // free.
  expected<expected<std::string, int>, std::string> const port
      = channel.transform (
          [] (int value) -> expected<std::string, int>
            {
              if (value > 3)
                return unexpected<int> (value);
              return std::string ("COM") + std::to_string (value);
            });

  expected<int, std::string> const chained = channel.and_then (
      [] (int value) { return parse_channel (value < 9 ? "9" : "1"); });
  expected<int, std::string> const recovered = missing.or_else (
      [] (std::string const &) { return expected<int, std::string> (0); });
  expected<int, int> const code = missing.transform_error (
      [] (std::string const &why) { return static_cast<int> (why.size ()); });
  std::cout << "seen=" << (seen ? "yes" : "no") << " visited=" << visited
            << " port_parsed=" << (port ? "yes" : "no")
            << " port_free=" << (port.value () ? "yes" : "no")
            << " port_busy_on=" << port.value ().error () << '\n';
  std::cout << "chained=" << chained.value_or (-1)
            << " recovered=" << recovered.value_or (-1)
            << " error_code=" << code.error () << '\n';

  std::cout << "\n--- 7. unexpected, conversion, comparison ---\n";
  // unexpected converts implicitly, like std::unexpected.
  expected<int, std::string> const refused
      = unexpected<std::string> (std::string ("refused"));
  expected<long, std::string> const wide = channel; // converting constructor
  std::cout << "refused=" << refused.error () << " wide=" << wide.value ()
            << " channel==5: " << (channel == 5)
            << " wide==5L: " << (wide == 5L) << " missing==unexpected: "
            << (missing
                == unexpected<std::string> (std::string ("not a digit")))
            << '\n';

  std::cout << "\n=== Expected example finished ===\n";
  return 0;
}
