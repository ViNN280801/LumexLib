// lumex/tests/core/units/api/LumexUnitsRouteMix.cxx11.tests.cpp
//
// Units built with different routes of the product (the fused multiply-add,
// Dekker's two-product, the product in integers) linked into one program: the
// routes are in inline namespaces of their own, so every unit keeps its own
// `converter_t` and its own `apply`, and the linker cannot keep one copy of an
// inline function for both. This unit has the route of the suite; the two
// helper files are compiled with the exact and the two-product routes.
#include <cstdint>
#include <cstring>
#include <string>
#include <typeinfo>

#include <gtest/gtest.h>

#include "lumex/core/units/LumexUnits"

double lumex_test_units_exact_unit_apply (double value);
std::string lumex_test_units_exact_unit_type_name ();
double lumex_test_units_two_product_unit_apply (double value);
std::string lumex_test_units_two_product_unit_type_name ();
std::string lumex_test_units_exact_unit_helper_type_name ();
std::string lumex_test_units_two_product_unit_helper_type_name ();

TEST (
    LumexUnitsRouteMixTest,
    GivenUnitsOfDifferentRoutes_WhenLinkedTogether_ThenEachKeepsItsOwnConverterType)
{
  using namespace lumex::core::units;
  std::string const own = typeid (converter_t).name ();
  std::string const exact = lumex_test_units_exact_unit_type_name ();
  std::string const two_product
      = lumex_test_units_two_product_unit_type_name ();
  EXPECT_NE (std::string::npos, exact.find ("route_exact")) << exact;
  EXPECT_NE (std::string::npos, two_product.find ("route_two_product"))
      << two_product;
  EXPECT_NE (exact, two_product);
  // the helpers of the products (the integers of the factor, the split) are
  // in the namespace of the route too
  std::string const own_helper
      = typeid (lumex::core::units::route_detail::big_t).name ();
  std::string const exact_helper
      = lumex_test_units_exact_unit_helper_type_name ();
  std::string const two_product_helper
      = lumex_test_units_two_product_unit_helper_type_name ();
  EXPECT_NE (std::string::npos, exact_helper.find ("route_exact"))
      << exact_helper;
  EXPECT_NE (std::string::npos, two_product_helper.find ("route_two_product"))
      << two_product_helper;
  EXPECT_NE (exact_helper, two_product_helper);
  EXPECT_NE (std::string::npos, own_helper.find ("route_"));
#if LUMEX_UNITS_ROUTE == LUMEX_UNITS_ROUTE_FUSED
  EXPECT_NE (std::string::npos, own.find ("route_fused")) << own;
  EXPECT_NE (own, exact);
  EXPECT_NE (own, two_product);
#elif LUMEX_UNITS_ROUTE == LUMEX_UNITS_ROUTE_TWO_PRODUCT
  EXPECT_NE (std::string::npos, own.find ("route_two_product")) << own;
  EXPECT_EQ (own, two_product);
  EXPECT_NE (own, exact);
#else
  EXPECT_NE (std::string::npos, own.find ("route_exact")) << own;
  EXPECT_EQ (own, exact);
  EXPECT_NE (own, two_product);
#endif
}

TEST (
    LumexUnitsRouteMixTest,
    GivenUnitsOfDifferentRoutes_WhenApplied_ThenEveryRouteGivesTheCorrectlyRoundedValue)
{
  using namespace lumex::core::units;
  converter_t const milli = make_converter (make_scale (-3, 1, 1, 0));
  for (double const x : { 1.0, 700.0, 0.1, 123456.789, 1e150, 1e-150 })
    {
      double const expected = milli.apply (x);
      EXPECT_EQ (expected, lumex_test_units_exact_unit_apply (x)) << x;
      EXPECT_EQ (expected, lumex_test_units_two_product_unit_apply (x)) << x;
    }
  EXPECT_EQ (0.7, lumex_test_units_exact_unit_apply (700.0));
  EXPECT_EQ (0.7, lumex_test_units_two_product_unit_apply (700.0));
}
