// lumex/tests/core/units/LumexUnitsGlobalNames.cxx11.tests.cpp
//
// Including the units umbrella adds no name to the global namespace, and the
// platform headers that define macros with common names (<windows.h>:
// min, max, small, interface, pascal) do not break it. This file declares
// global names spelled like the module's and uses them unqualified: a header
// that writes `using namespace lumex::core::units;` at file scope makes every
// such use ambiguous, and this file stops compiling.
#include <string>

#if defined(_WIN32)
#include <windows.h>
#endif

#include <gtest/gtest.h>

#include "lumex/core/units/LumexUnits"

struct own_name_t
{
};

// the names of the data header (data/LumexUnitsData.hpp)
enum class unit_id
{
  mine
};
enum class prefix_id
{
  mine
};
enum class script
{
  mine
};
enum class domain
{
  mine
};
enum class quantity_kind
{
  mine
};
struct scale_t : own_name_t
{
};
struct dimension_t : own_name_t
{
};
struct symbol_t : own_name_t
{
};
struct unit_descriptor_t : own_name_t
{
};
struct prefix_descriptor_t : own_name_t
{
};

// the names of the API header (api/LumexUnitsApi.hpp)
struct prefixed_unit_t : own_name_t
{
};
struct converter_t : own_name_t
{
};
enum class lookup_error
{
  mine
};
enum class conversion_error
{
  mine
};

static int
lookup (int value)
{
  return value + 1;
}
static int
convert (int value)
{
  return value + 2;
}
static int
ratio (int value)
{
  return value + 3;
}
static int
descriptor (int value)
{
  return value + 4;
}
static int
format_symbol (int value)
{
  return value + 5;
}
static int
scale_of (int value)
{
  return value + 6;
}
static int
canonical_symbol (int value)
{
  return value + 7;
}
static int
make_converter (int value)
{
  return value + 8;
}
static int
scale_factor (int value)
{
  return value + 9;
}

TEST (LumexUnitsGlobalNamesTest,
      GivenOwnGlobalNames_WhenTheUmbrellaIsIncluded_ThenTheyAreNotAmbiguous)
{
  EXPECT_EQ (unit_id::mine, unit_id::mine);
  EXPECT_EQ (prefix_id::mine, prefix_id::mine);
  EXPECT_EQ (script::mine, script::mine);
  EXPECT_EQ (domain::mine, domain::mine);
  EXPECT_EQ (quantity_kind::mine, quantity_kind::mine);
  EXPECT_EQ (lookup_error::mine, lookup_error::mine);
  EXPECT_EQ (conversion_error::mine, conversion_error::mine);
  EXPECT_EQ (2, lookup (1));
  EXPECT_EQ (3, convert (1));
  EXPECT_EQ (4, ratio (1));
  EXPECT_EQ (5, descriptor (1));
  EXPECT_EQ (6, format_symbol (1));
  EXPECT_EQ (7, scale_of (1));
  EXPECT_EQ (8, canonical_symbol (1));
  EXPECT_EQ (9, make_converter (1));
  EXPECT_EQ (10, scale_factor (1));
  EXPECT_EQ (1u, sizeof (scale_t));
  EXPECT_EQ (1u, sizeof (dimension_t));
  EXPECT_EQ (1u, sizeof (symbol_t));
  EXPECT_EQ (1u, sizeof (unit_descriptor_t));
  EXPECT_EQ (1u, sizeof (prefix_descriptor_t));
  EXPECT_EQ (1u, sizeof (prefixed_unit_t));
  EXPECT_EQ (1u, sizeof (converter_t));
}

TEST (LumexUnitsGlobalNamesTest,
      GivenTheNamespace_WhenQualified_ThenTheModuleIsKnownAsLumexCoreUnits)
{
  // The umbrella makes the data and the api visible as lumex::core::units.
  namespace units = lumex::core::units;
  units::prefixed_unit_t const pair
      = { units::unit_id::pascal_unit, units::prefix_id::kilo };
  EXPECT_EQ (units::unit_id::pascal_unit, pair.unit);
  EXPECT_TRUE (
      units::lookup (lumex::core::string_view::view::lumex_string_view ("kPa"))
          .has_value ());
  EXPECT_EQ (105u, units::unit_count);
  EXPECT_EQ (105u, lumex::core::units::unit_count);
}

TEST (
    LumexUnitsGlobalNamesTest,
    GivenCatalogNames_WhenPlatformMacrosAreDefined_ThenTheEnumeratorsStillCompile)
{
  // Every enumerator is spelled in this unit with the macros of the platform
  // headers in effect (see the include above).
  using lumex::core::units::unit_id;
  unit_id const all[]
      = { unit_id::pascal_unit, unit_id::bar,  unit_id::minute, unit_id::hour,
          unit_id::day,         unit_id::var,  unit_id::one,    unit_id::are,
          unit_id::foot,        unit_id::gram, unit_id::second };
  EXPECT_EQ (11u, sizeof (all) / sizeof (all[0]));
}
