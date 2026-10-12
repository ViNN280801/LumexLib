#include <iostream>
#include <string>

#include "lumex/core/optional/LumexOptional"

namespace
{
optional<std::string>
maybe_operator (bool logged_in)
{
  if (!logged_in)
    return nullopt;
  return optional<std::string> (std::string ("analyst.a"));
}
}

int
main ()
{
  std::cout << "=== Workflow: optional operator name on a run header ===\n\n";

  optional<std::string> const missing = maybe_operator (false);
  optional<std::string> const present = maybe_operator (true);
  std::cout << "anonymous=" << missing.value_or (std::string ("<unsigned>"))
            << '\n';
  std::cout << "signed=" << present.value_or (std::string ("<unsigned>"))
            << '\n';
  return 0;
}
