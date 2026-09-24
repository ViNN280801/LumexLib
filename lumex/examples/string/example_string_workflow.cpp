#include <iostream>
#include <string>

#include "lumex/core/string/LumexString"

using lumex::core::string::format::stringify;

int
main ()
{
  std::cout << "=== Workflow: build a run label for a log line ===\n\n";

  std::string const batch ("B-104");
  int const vial = 12;
  std::string const label
      = stringify ("batch=", batch, " vial=", vial, " operator=lab");
  std::cout << label << '\n';
  return 0;
}
