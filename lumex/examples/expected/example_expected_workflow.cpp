#include <iostream>
#include <string>

#include "lumex/core/expected/Expected"

using namespace lumex::core::expected::result;
using namespace lumex::core::expected::error;

namespace
{
expected<double, std::string>
flow_or_error (double ml_min)
{
  if (ml_min <= 0.0)
    return expected<double, std::string> (
        unexpect, std::string ("flow must be positive"));
  if (ml_min > 10.0)
    return expected<double, std::string> (
        unexpect, std::string ("flow exceeds pump limit"));
  return expected<double, std::string> (ml_min);
}

/// Turns a validated flow into the pump speed. Any failure keeps its text.
expected<int, std::string>
speed_for (double ml_min)
{
  return flow_or_error (ml_min).transform (
      [] (double flow) { return static_cast<int> (flow * 60.0); });
}
}

int
main ()
{
  std::cout << "=== Workflow: validate a pump flow before starting ===\n\n";

  double const candidates[] = { 1.0, 0.0, 12.5 };
  for (double const flow : candidates)
    {
      expected<double, std::string> const result = flow_or_error (flow);
      if (result)
        std::cout << "accepted flow=" << result.value () << '\n';
      else
        std::cout << "rejected flow=" << flow << " reason=" << result.error ()
                  << '\n';
    }

  std::cout << "\n--- the same checks as a pipeline ---\n";
  int started = 0;
  for (double const flow : candidates)
    {
      // transform turns a good flow into a speed, then or_else logs a refusal
      // and passes it on, and the last transform (its function returns void)
      // only counts the start, so a refused flow is never counted.
      expected<void, std::string> const start
          = speed_for (flow)
                .or_else (
                    [flow] (std::string const &why)
                      {
                        std::cout << "flow=" << flow << " refused: " << why
                                  << '\n';
                        return expected<int, std::string> (unexpect, why);
                      })
                .transform (
                    [&started] (int speed)
                      {
                        ++started;
                        std::cout << "speed=" << speed << " started\n";
                      });
      if (!start)
        std::cout << "  not started: " << start.error () << '\n';
    }
  std::cout << "started " << started << " of 3\n";
  return 0;
}
