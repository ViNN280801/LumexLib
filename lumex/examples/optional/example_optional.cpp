#include <iostream>
#include <string>

#include "lumex/core/optional/LumexOptional"

int
main ()
{
  std::cout << "=== optional observers and modifiers ===\n\n";

  std::cout << "--- 1. Empty / filled ---\n";
  optional<int> empty;
  optional<int> filled (7);
  std::cout << "empty.has_value=" << (empty.has_value () ? "yes" : "no")
            << " filled=" << filled.value () << '\n';

  std::cout << "\n--- 2. nullopt construction and comparison ---\n";
  optional<int> cleared = nullopt;
  std::cout << "cleared==nullopt=" << (cleared == nullopt ? "yes" : "no")
            << " filled!=nullopt=" << (filled != nullopt ? "yes" : "no")
            << '\n';

  std::cout << "\n--- 3. value_or / reset / emplace ---\n";
  std::cout << "empty.value_or(-1)=" << empty.value_or (-1) << '\n';
  filled.reset ();
  std::cout << "after reset has_value=" << (filled.has_value () ? "yes" : "no")
            << '\n';
  filled.emplace (99);
  std::cout << "after emplace=" << *filled << '\n';

  std::cout << "\n--- 4. Strings ---\n";
  optional<std::string> name;
  name.emplace ("HPLC-01");
  std::cout << "name=" << name.value_or (std::string ("<none>")) << '\n';
  name = nullopt;
  std::cout << "name after nullopt=" << name.value_or (std::string ("<none>"))
            << '\n';

  std::cout << "\n=== Optional example finished ===\n";
  return 0;
}
