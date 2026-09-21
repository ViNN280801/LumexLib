#include <iostream>

#include "lumex/core/utility/LumexUtility"

using namespace lumex::core::utility::demangle;
using namespace lumex::core::utility::numeric;

int
main ()
{
  std::cout
      << "=== Workflow: reject an oversized packet length safely ===\n\n";

  SafeComparator<unsigned int> received (50U);
  int const max_payload = 40;
  if (received.safe_compare (max_payload))
    std::cout << "accept length\n";
  else
    std::cout << "reject length; type=" << lumDemangle (unsigned int) << '\n';
  return 0;
}
