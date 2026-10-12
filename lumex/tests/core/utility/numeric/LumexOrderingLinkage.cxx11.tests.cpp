// LumexOrderingLinkage.cxx11.tests.cpp
// The second translation unit of the ordering suites: it includes
// LumexOrdering.hpp on its own and hands out the addresses of two named
// values, so that LumexOrdering.cxx11.tests.cpp can check that both units
// refer to one object. Before C++17 the constants are defined in the header
// with a link-once attribute (or `inline`); a plain definition would be a
// second definition of the symbol, and the suite would not link.

#include "lumex/core/utility/numeric/LumexOrdering.hpp"

using namespace lumex::core::utility::numeric;

namespace lumex_ordering_test
{
strong_ordering const *address_of_less_in_second_unit ();
partial_ordering const *address_of_unordered_in_second_unit ();

strong_ordering const *
address_of_less_in_second_unit ()
{
  return &strong_ordering::less;
}

partial_ordering const *
address_of_unordered_in_second_unit ()
{
  return &partial_ordering::unordered;
}
} // namespace lumex_ordering_test
