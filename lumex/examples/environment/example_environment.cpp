#include <iostream>
#include <string>

#include "lumex/core/environment/LumexEnvironment"

using namespace lumex::core::environment::env;

namespace
{
char const *kMarker = "LUMEX_EXAMPLE_MARKER";
}

int
main ()
{
  std::cout << "=== Environment get / set / has / truthy ===\n\n";

  LumexEnvironment &env = LumexEnvironment::instance ();

  std::cout << "--- 1. Instance read with fallback ---\n";
  std::string const path = env.get_environment_variable_or ("PATH", "");
  std::cout << "PATH length=" << path.size () << '\n';

  std::cout << "\n--- 2. Static get + operator bool ---\n";
  LumexEnvironment::EnvResult const lang = LumexEnvironment::get ("LANG");
  if (lang)
    std::cout << "LANG=" << lang.value << '\n';
  else
    std::cout << "LANG missing error_code=" << lang.error_code
              << " fallback=" << lang.get_value_or ("<unset>") << '\n';

  std::cout << "\n--- 3. Set, has, overwrite=false ---\n";
  bool const first = LumexEnvironment::set (kMarker, "1");
  bool const blocked = LumexEnvironment::set (kMarker, "should_not_stick",
                                              /*overwrite=*/false);
  std::cout << "set ok=" << (first ? "yes" : "no")
            << " overwrite_false=" << (blocked ? "yes" : "no")
            << " has=" << (LumexEnvironment::has (kMarker) ? "yes" : "no")
            << " value=" << LumexEnvironment::get_or (kMarker, "") << '\n';

  std::cout << "\n--- 4. is_truthy / is_env_set ---\n";
  std::cout << "is_truthy(" << kMarker
            << ")=" << (LumexEnvironment::is_truthy (kMarker) ? "yes" : "no")
            << " is_env_set=" << (is_env_set (kMarker) ? "yes" : "no") << '\n';
  LumexEnvironment::set (kMarker, "0");
  std::cout << "after set 0 is_truthy="
            << (LumexEnvironment::is_truthy (kMarker) ? "yes" : "no") << '\n';

  std::cout << "\n--- 5. Unset with nullptr ---\n";
  LumexEnvironment::set (kMarker, nullptr);
  std::cout << "after unset has="
            << (LumexEnvironment::has (kMarker) ? "yes" : "no") << '\n';

  std::cout << "\n=== Environment example finished ===\n";
  return 0;
}
