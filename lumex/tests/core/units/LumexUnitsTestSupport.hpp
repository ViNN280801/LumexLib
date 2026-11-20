// lumex/tests/core/units/LumexUnitsTestSupport.hpp
//
// Helpers of the units tests: the names of the enumerations (written here, in
// the order of the enumerators, so that a reordered enumeration fails the
// tests that compare them with the oracle), the parsing of the oracle's
// symbols and lists, and a few conversions between strings and the enumerators
// of the catalog.
#ifndef LUMEX_TESTS_CORE_UNITS_TEST_SUPPORT_HPP
#define LUMEX_TESTS_CORE_UNITS_TEST_SUPPORT_HPP

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/units/LumexUnits"
#include "lumex/tests/core/units/oracle/LumexUnitsOracleData.hpp"

namespace lumex_test_units
{
using namespace lumex::core::units;

static char const *const domain_names[] = { "space_and_time",
                                            "mechanics",
                                            "thermodynamics",
                                            "electromagnetism",
                                            "light_and_radiation",
                                            "acoustics",
                                            "physical_chemistry",
                                            "atomic_and_nuclear_physics",
                                            "general" };
static_assert (sizeof (domain_names) / sizeof (domain_names[0])
                   == static_cast<std::size_t> (domain::general) + 1,
               "domain_names follows the enumerators of domain");

static char const *const standing_names[]
    = { "si_base",   "si_special", "accepted", "relative_log",
        "temporary", "annex",      "cgpm" };
static_assert (sizeof (standing_names) / sizeof (standing_names[0])
                   == static_cast<std::size_t> (standing::cgpm) + 1,
               "standing_names follows the enumerators of standing");

static char const *const system_names[] = { "si", "non_si", "cgs" };
static_assert (sizeof (system_names) / sizeof (system_names[0])
                   == static_cast<std::size_t> (unit_system::cgs) + 1,
               "system_names follows the enumerators of unit_system");

static char const *const factor_names[] = { "exact", "measured", "none" };
static_assert (sizeof (factor_names) / sizeof (factor_names[0])
                   == static_cast<std::size_t> (factor_kind::none) + 1,
               "factor_names follows the enumerators of factor_kind");

static char const *const kind_names[] = { "length",
                                          "mass",
                                          "time",
                                          "electric_current",
                                          "thermodynamic_temperature",
                                          "celsius_temperature",
                                          "amount_of_substance",
                                          "luminous_intensity",
                                          "plane_angle",
                                          "solid_angle",
                                          "frequency",
                                          "force",
                                          "pressure",
                                          "energy",
                                          "power",
                                          "electric_charge",
                                          "voltage",
                                          "capacitance",
                                          "resistance",
                                          "conductance",
                                          "magnetic_flux",
                                          "magnetic_flux_density",
                                          "inductance",
                                          "luminous_flux",
                                          "illuminance",
                                          "activity",
                                          "absorbed_dose",
                                          "dose_equivalent",
                                          "catalytic_activity",
                                          "volume",
                                          "optical_power",
                                          "area",
                                          "apparent_power",
                                          "reactive_power",
                                          "dimensionless_ratio",
                                          "logarithmic_level",
                                          "loudness_level",
                                          "frequency_interval",
                                          "linear_density",
                                          "velocity",
                                          "acceleration",
                                          "rotational_frequency",
                                          "dynamic_viscosity",
                                          "kinematic_viscosity",
                                          "resistivity",
                                          "magnetomotive_force",
                                          "magnetic_field_strength",
                                          "exposure",
                                          "luminance" };
static_assert (sizeof (kind_names) / sizeof (kind_names[0])
                   == static_cast<std::size_t> (quantity_kind::luminance) + 1,
               "kind_names follows the enumerators of quantity_kind");

static char const *const prefix_names[]
    = { "quecto", "ronto", "yocto", "zepto", "atto",  "femto",
        "pico",   "nano",  "micro", "milli", "centi", "deci",
        "deca",   "hecto", "kilo",  "mega",  "giga",  "tera",
        "peta",   "exa",   "zetta", "yotta", "ronna", "quetta" };
static_assert (sizeof (prefix_names) / sizeof (prefix_names[0])
                   == static_cast<std::size_t> (prefix_id::quetta) + 1,
               "prefix_names follows the enumerators of prefix_id");

inline std::vector<std::string>
split_list (std::string const &text, char separator = '|')
{
  std::vector<std::string> parts;
  if (text.empty ())
    return parts;
  std::size_t begin = 0;
  for (;;)
    {
      std::size_t const end = text.find (separator, begin);
      if (end == std::string::npos)
        {
          parts.push_back (text.substr (begin));
          return parts;
        }
      parts.push_back (text.substr (begin, end - begin));
      begin = end + 1;
    }
}

/// A symbol of the oracle: a trailing '!' means that it takes no prefix.
struct oracle_symbol_t
{
  std::string text;
  bool prefixable;
};

inline oracle_symbol_t
parse_oracle_symbol (std::string text)
{
  oracle_symbol_t parsed;
  parsed.prefixable = true;
  if (!text.empty () && text[text.size () - 1] == '!')
    {
      text.erase (text.size () - 1);
      parsed.prefixable = false;
    }
  parsed.text = text;
  return parsed;
}

inline std::string
text_of (symbol_t const &symbol)
{
  return std::string (symbol.text, symbol.size);
}

inline std::string
text_of (text_view_t view)
{
  return std::string (view.data (), view.size ());
}

inline text_view_t
view_of (std::string const &text)
{
  return text_view_t (text.data (), text.size ());
}

/// The unit whose enumerator is spelled `name`; a failure when none is.
inline unit_id
unit_from_name (std::string const &name)
{
  for (std::size_t index = 0; index < unit_count; ++index)
    {
      if (name == descriptor (unit_id_at (index)).name)
        return unit_id_at (index);
    }
  ADD_FAILURE () << "no unit named " << name;
  return unit_id ();
}

inline prefix_id
prefix_from_name (std::string const &name)
{
  if (name == "none")
    return prefix_id::none;
  for (std::size_t index = 0; index < prefix_count; ++index)
    {
      if (name == prefix_names[index])
        return static_cast<prefix_id> (index);
    }
  ADD_FAILURE () << "no prefix named " << name;
  return prefix_id::none;
}

/// The prefix set of an oracle spec: all, none, or names separated by '|'.
inline std::uint32_t
mask_of_spec (std::string const &spec)
{
  if (spec == "all")
    return all_prefixes_mask;
  if (spec == "none")
    return 0;
  std::uint32_t mask = 0;
  for (std::string const &name : split_list (spec))
    mask |= prefix_bit (prefix_from_name (name));
  return mask;
}

/// One symbol of a unit in a script: canonical or alias, with its flag.
struct unit_symbol_t
{
  std::string text;
  bool prefixable;
  bool canonical;
};

/// The canonical symbol (when not empty) and the aliases of a unit in a
/// script, in the order of the catalog.
inline std::vector<unit_symbol_t>
symbols_of (unit_id id, script in)
{
  std::vector<unit_symbol_t> symbols;
  symbol_t const &canonical = in == script::international
                                  ? descriptor (id).international
                                  : descriptor (id).russian;
  if (canonical.size != 0)
    {
      unit_symbol_t entry
          = { text_of (canonical), canonical.prefixable, true };
      symbols.push_back (entry);
    }
  for (std::size_t index = 0; index < alias_count; ++index)
    {
      alias_descriptor_t const &alias = alias_at (index);
      if (alias.unit == id && alias.symbol_script == in)
        {
          unit_symbol_t entry
              = { text_of (alias.symbol), alias.symbol.prefixable, false };
          symbols.push_back (entry);
        }
    }
  return symbols;
}

/// The canonical symbol and the aliases of a prefix in a script.
inline std::vector<std::string>
prefix_symbols_of (prefix_id id, script in)
{
  std::vector<std::string> symbols;
  symbols.push_back (text_of (in == script::international
                                  ? descriptor (id).international
                                  : descriptor (id).russian));
  for (std::size_t index = 0; index < prefix_alias_count; ++index)
    {
      prefix_alias_descriptor_t const &alias = prefix_alias_at (index);
      if (alias.prefix == id && alias.symbol_script == in)
        symbols.push_back (text_of (alias.symbol));
    }
  return symbols;
}

inline char const *
script_name (script in)
{
  return in == script::international ? "international" : "russian";
}

/// The distance of two doubles in units in the last place (both finite and
/// of one sign).
inline std::uint64_t
ulp_distance (double left, double right)
{
  std::uint64_t a = 0;
  std::uint64_t b = 0;
  std::memcpy (&a, &left, sizeof (a));
  std::memcpy (&b, &right, sizeof (b));
  return a > b ? a - b : b - a;
}
} // namespace lumex_test_units

#endif // LUMEX_TESTS_CORE_UNITS_TEST_SUPPORT_HPP
