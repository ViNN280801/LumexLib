#include <iostream>

#include "lumex/core/utility/LumexUtility"

using namespace lumex::core::utility::demangle;
using namespace lumex::core::utility::numeric;
using namespace lumex::core::utility::process;

int
main ()
{
  std::cout
      << "=== Workflow: reject an oversized packet length safely ===\n\n";

  SafeComparator<unsigned int> received (50U);
  int const max_payload = 40;
  if (received.safe_compare (max_payload))
    std::cout << "accept length; pid=" << get_current_pid () << '\n';
  else
    std::cout << "reject length; type=" << lumDemangle (unsigned int)
              << " pid=" << get_current_pid () << '\n';
  return 0;
}
