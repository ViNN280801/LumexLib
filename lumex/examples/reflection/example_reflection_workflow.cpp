#include <cstdint>
#include <iostream>

#include "lumex/core/reflection/LumexReflection"

using namespace lumex::core::reflection::var_info;

DEFINE_REFLECTED_ENUM (ExampleValve, std::uint8_t, (Load), (Inject), (Waste))

int
main ()
{
  std::cout
      << "=== Workflow: log a valve position with its source name ===\n\n";

  ExampleValve const position = ExampleValve::Inject;
  std::cout << LUMEX_VARINFO (position) << '\n';
  std::cout << "display=" << toString (position) << '\n';
  return 0;
}
