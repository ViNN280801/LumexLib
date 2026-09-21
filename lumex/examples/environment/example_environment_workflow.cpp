#include <iostream>
#include <string>

#include "lumex/core/environment/LumexEnvironment"

using namespace lumex::core::environment::env;

int
main ()
{
  std::cout << "=== Workflow: optional feature flag for a lab session ===\n\n";

  char const *flag = "LUMEX_EXAMPLE_VERBOSE_SESSION";
  std::string const session
      = LumexEnvironment::get_or ("LUMEX_EXAMPLE_SESSION_ID", "local-dev");

  if (!LumexEnvironment::has (flag))
    LumexEnvironment::set (flag, "true");

  std::cout << "session_id=" << session
            << " verbose=" << (is_env_set (flag) ? "yes" : "no") << '\n';

  LumexEnvironment::set (flag, nullptr);
  return 0;
}
