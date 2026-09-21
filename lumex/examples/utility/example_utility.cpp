#include <iostream>
#include <typeinfo>
#include <vector>

#include "lumex/core/utility/LumexUtility"

using namespace lumex::core::utility::demangle;
using namespace lumex::core::utility::numeric;

int
main ()
{
  std::cout << "=== Utility: demangle, OS macros, SafeComparator ===\n\n";

  std::cout << "--- 1. lumDemangle / demangle_type_name ---\n";
  std::cout << "demangle(vector<int>)=" << lumDemangle (std::vector<int>)
            << '\n';
  std::cout << "demangle_type_name="
            << demangle_type_name (typeid (std::vector<int>).name ()) << '\n';

  std::cout << "\n--- 2. OS compile-time flags ---\n";
#if LUMEX_OS_WINDOWS
  std::cout << "LUMEX_OS_WINDOWS=1\n";
#elif LUMEX_OS_LINUX
  std::cout << "LUMEX_OS_LINUX=1\n";
#else
  std::cout << "other OS\n";
#endif

  std::cout << "\n--- 3. SafeComparator across signed/unsigned ---\n";
  SafeComparator<unsigned char> packet (200);
  int const limit = 300;
  std::cout << "200u8 >= 300=" << (packet.safe_compare (limit) ? "yes" : "no")
            << " 200u8 < 300=" << (packet.safe_less (limit) ? "yes" : "no")
            << " 200u8 <= 200="
            << (packet.safe_less_equal (200) ? "yes" : "no") << '\n';

  std::cout << "\n=== Utility example finished ===\n";
  return 0;
}
