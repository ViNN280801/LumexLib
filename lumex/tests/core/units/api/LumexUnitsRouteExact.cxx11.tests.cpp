// lumex/tests/core/units/api/LumexUnitsRouteExact.cxx11.tests.cpp
//
// A unit of the route test (see LumexUnitsRouteMix.cxx11.tests.cpp): this file
// is compiled with the route of the product in integers, whatever the route of
// the rest of the suite, and answers through two plain functions. It has no
// test of its own.
#undef LUMEX_UNITS_ROUTE
#define LUMEX_UNITS_ROUTE 3

#include <string>
#include <typeinfo>

#include "lumex/core/units/LumexUnits"

double
lumex_test_units_exact_unit_apply (double value)
{
  using namespace lumex::core::units;
  return make_converter (make_scale (-3, 1, 1, 0)).apply (value);
}

std::string
lumex_test_units_exact_unit_type_name ()
{
  return typeid (lumex::core::units::converter_t).name ();
}

// The type of a helper of the products: it is in the namespace of the route
// as well as the converter, so no copy of it is shared with another route.
std::string
lumex_test_units_exact_unit_helper_type_name ()
{
  return typeid (lumex::core::units::route_detail::big_t).name ();
}
