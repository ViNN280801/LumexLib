#include <iostream>
#include <string>

#include "lumex/core/string/LumexString"

using lumex::core::string::format::stringify;

int
main ()
{
  std::cout << "=== stringify many argument kinds ===\n\n";

  std::cout << "--- 1. Scalars ---\n";
  std::cout << "stringify() empty=\"" << stringify () << "\"\n";
  std::cout << "int=" << stringify (42) << '\n';
  std::cout << "double=" << stringify (1.25) << '\n';
  std::cout << "bool=" << stringify (true) << '\n';

  std::cout << "\n--- 2. Concatenation ---\n";
  std::cout << stringify ("channel=", 2, " flow=", 1.0, " ml/min") << '\n';

  std::cout << "\n--- 3. std::string ---\n";
  std::string const name ("HPLC-01");
  std::cout << stringify ("instrument=", name) << '\n';

  std::cout << "\n=== stringify example finished ===\n";
  return 0;
}
