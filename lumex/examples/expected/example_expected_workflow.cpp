#include <iostream>
#include <string>

#include "lumex/core/expected/Expected"

using namespace lumex::core::expected::result;
using namespace lumex::core::expected::error;

namespace
{
Expected<double, std::string>
flow_or_error (double ml_min)
{
  if (ml_min <= 0.0)
    return Expected<double, std::string> (
        unexpect, std::string ("flow must be positive"));
  if (ml_min > 10.0)
    return Expected<double, std::string> (
        unexpect, std::string ("flow exceeds pump limit"));
  return Expected<double, std::string> (ml_min);
}
}

int
main ()
{
  std::cout << "=== Workflow: validate a pump flow before starting ===\n\n";

  double const candidates[] = { 1.0, 0.0, 12.5 };
  for (double const flow : candidates)
    {
      Expected<double, std::string> const result = flow_or_error (flow);
      if (result)
        std::cout << "accepted flow=" << result.value () << '\n';
      else
        std::cout << "rejected flow=" << flow << " reason=" << result.error ()
                  << '\n';
    }
  return 0;
}
