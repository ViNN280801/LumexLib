#include <iostream>
#include <limits>

#include "lumex/core/utility/LumexUtility"

using namespace lumex::core::utility::demangle;
using namespace lumex::core::utility::numeric;
using namespace lumex::core::utility::process;

int
main ()
{
  std::cout
      << "=== Workflow: reject an oversized packet length safely ===\n\n";

  safe_comparator<unsigned int> received (50U);
  int const max_payload = 40;
  if (received.safe_compare (max_payload))
    std::cout << "accept length; pid=" << get_current_pid () << '\n';
  else
    std::cout << "reject length; type=" << lumDemangle (unsigned int)
              << " pid=" << get_current_pid () << '\n';

  // The three-way result works from C++11. Its types are named once:
  // strong_ordering_t for two integers, partial_ordering_t when a
  // floating-point value may be NaN. They are the classes of
  // LumexOrdering.hpp in every standard (from C++20 they convert to
  // std::strong_ordering and std::partial_ordering).
  strong_ordering_t const against_limit
      = safe_three_way_compare (received.get (), max_payload);
  partial_ordering_t const against_nan = safe_three_way_compare (
      max_payload, std::numeric_limits<double>::quiet_NaN ());
  std::cout << "three-way: length is "
            << (is_greater (against_limit) ? "above" : "within")
            << " the limit; against NaN it is "
            << (against_nan == partial_ordering_t::unordered ? "unordered"
                                                             : "ordered")
            << '\n';
  return 0;
}
