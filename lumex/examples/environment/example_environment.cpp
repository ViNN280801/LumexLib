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

  lumex_environment &env = lumex_environment::instance ();

  std::cout << "--- 1. Instance read with fallback ---\n";
  std::string const path = env.get_environment_variable_or ("PATH", "");
  std::cout << "PATH length=" << path.size () << '\n';

  std::cout << "\n--- 2. Static get + operator bool ---\n";
  lumex_environment::EnvResult const lang = lumex_environment::get ("LANG");
  if (lang)
    std::cout << "LANG=" << lang.value << '\n';
  else
    std::cout << "LANG missing error_code=" << lang.error_code
              << " fallback=" << lang.get_value_or ("<unset>") << '\n';

  std::cout << "\n--- 3. Set, has, overwrite=false ---\n";
  bool const first = lumex_environment::set (kMarker, "1");
  bool const blocked = lumex_environment::set (kMarker, "should_not_stick",
                                               /*overwrite=*/false);
  std::cout << "set ok=" << (first ? "yes" : "no")
            << " overwrite_false=" << (blocked ? "yes" : "no")
            << " has=" << (lumex_environment::has (kMarker) ? "yes" : "no")
            << " value=" << lumex_environment::get_or (kMarker, "") << '\n';

  std::cout << "\n--- 4. is_truthy / is_env_set ---\n";
  std::cout << "is_truthy(" << kMarker
            << ")=" << (lumex_environment::is_truthy (kMarker) ? "yes" : "no")
            << " is_env_set=" << (is_env_set (kMarker) ? "yes" : "no") << '\n';
  lumex_environment::set (kMarker, "0");
  std::cout << "after set 0 is_truthy="
            << (lumex_environment::is_truthy (kMarker) ? "yes" : "no") << '\n';

  std::cout << "\n--- 5. Unset with nullptr ---\n";
  lumex_environment::set (kMarker, nullptr);
  std::cout << "after unset has="
            << (lumex_environment::has (kMarker) ? "yes" : "no") << '\n';

  std::cout << "\n=== Environment example finished ===\n";
  return 0;
}
