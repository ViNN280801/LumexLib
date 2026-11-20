// A walkthrough of lumex::units: the descriptor of a unit, the lookup of a
// symbol in both scripts, the formatting of a symbol, the exact ratio of two
// units, the conversion of a value and the degree Celsius.
//
// The symbols are UTF-8; the program prints the bytes above 127 as
// hexadecimal, so the output reads the same on a console that does not show
// UTF-8.
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "lumex/core/units/LumexUnits"

namespace
{
using namespace lumex::core::units;

int failures = 0;

void
check (bool condition, char const *what)
{
  std::cout << (condition ? "  ok   " : "  FAIL ") << what << '\n';
  if (!condition)
    ++failures;
}

// The bytes of a text as hexadecimal.
std::string
hex_bytes (text_view_t text)
{
  std::ostringstream out;
  out << std::hex << std::setfill ('0');
  for (std::size_t i = 0; i < text.size (); ++i)
    {
      if (i != 0)
        out << ' ';
      out << std::setw (2)
          << static_cast<unsigned int> (static_cast<unsigned char> (text[i]));
    }
  return out.str ();
}

std::string
as_string (text_view_t text)
{
  return std::string (text.data (), text.size ());
}

prefixed_unit_t
pair_of (unit_id unit, prefix_id prefix = prefix_id::none)
{
  prefixed_unit_t pair = { unit, prefix };
  return pair;
}

void
section (char const *title)
{
  std::cout << "\n== " << title << " ==\n";
}
} // namespace

int
main ()
{
  section ("The descriptor of a unit");
  unit_descriptor_t const &pascal = descriptor (unit_id::pascal_unit);
  std::cout << pascal.name << ": "
            << as_string (canonical_symbol (unit_id::pascal_unit,
                                            script::international))
            << " / "
            << hex_bytes (
                   canonical_symbol (unit_id::pascal_unit, script::russian))
            << " (the Russian symbol, bytes), kg^"
            << int (pascal.dimension.exponent[1]) << " m^"
            << int (pascal.dimension.exponent[0]) << " s^"
            << int (pascal.dimension.exponent[2]) << ", domain "
            << domain_name (pascal.unit_domain) << '\n';
  check (pascal.scale.numerator == 1 && pascal.scale.exponent10 == 0,
         "the pascal is the coherent unit of pressure");
  unit_descriptor_t const &ev = descriptor (unit_id::electronvolt);
  std::cout << "eV: " << ev.scale.numerator << " / " << ev.scale.denominator
            << " * 10^" << ev.scale.exponent10
            << " J (1.602176634e-19 J, exact since 2018)\n";
  check (ev.factor == factor_kind::exact, "the electronvolt is exact");
  check (descriptor (unit_id::dalton).factor == factor_kind::measured,
         "the dalton is a measured value (CODATA 2022)");

  section ("Lookup of a symbol");
  result_t<prefixed_unit_t, lookup_error> const kilopascal
      = lookup (text_view_t ("kPa"));
  check (kilopascal.has_value ()
             && kilopascal.value ().unit == unit_id::pascal_unit
             && kilopascal.value ().prefix == prefix_id::kilo,
         "kPa is the pascal with the prefix kilo");
  // The Russian symbol of the micrometre, as UTF-8 bytes.
  result_t<prefixed_unit_t, lookup_error> const micrometre
      = lookup (text_view_t ("\xD0\xBC\xD0\xBA\xD0\xBC"));
  check (micrometre.has_value () && micrometre.value ().unit == unit_id::metre
             && micrometre.value ().prefix == prefix_id::micro,
         "the Russian mkm is the metre with the prefix micro");
  check (lookup (text_view_t ("\xC2\xB5m")).has_value ()
             && lookup (text_view_t ("um")).has_value (),
         "the micro sign U+00B5 and the letter u are both micro");
  result_t<prefixed_unit_t, lookup_error> const tesla
      = lookup (text_view_t ("\xD0\xA2\xD0\xBB"));
  check (tesla.has_value () && tesla.value ().unit == unit_id::tesla
             && tesla.value ().prefix == prefix_id::none,
         "the bare symbol wins: the Russian Tl is the tesla, not the "
         "teralitre");
  check (!lookup (text_view_t ("\xC2\xB5kg")).has_value (),
         "no compound prefixes: micro-kilogram is unknown");
  check (!lookup (text_view_t ("kmin")).has_value (),
         "the minute takes no prefix");
  result_t<prefixed_unit_t, lookup_error> const nothing
      = lookup (text_view_t ("xyz"));
  check (!nothing.has_value ()
             && nothing.error () == lookup_error::unknown_symbol,
         "an unknown symbol says so");

  section ("Formatting a symbol");
  result_t<symbol_buffer_t, format_error> const kilogram = format_symbol (
      pair_of (unit_id::gram, prefix_id::kilo), script::international);
  check (kilogram.has_value ()
             && as_string (kilogram.value ().view ()) == "kg",
         "the kilogram is the prefix kilo and the gram: kg");
  result_t<symbol_buffer_t, format_error> const russian_kilogram
      = format_symbol (pair_of (unit_id::gram, prefix_id::kilo),
                       script::russian);
  std::cout << "kg in Russian, bytes: "
            << hex_bytes (russian_kilogram.value ().view ()) << '\n';
  check (!format_symbol (pair_of (unit_id::minute, prefix_id::kilo),
                         script::international)
              .has_value (),
         "the minute does not take the prefix kilo");

  section ("The exact ratio and the conversion of a value");
  result_t<scale_t, conversion_error> const hour_in_minutes
      = ratio (pair_of (unit_id::hour), pair_of (unit_id::minute));
  check (hour_in_minutes.has_value ()
             && hour_in_minutes.value ().numerator == 6
             && hour_in_minutes.value ().exponent10 == 1,
         "1 h = 6 * 10^1 min exactly");
  result_t<double, conversion_error> const milligrams
      = convert (700.0, pair_of (unit_id::gram, prefix_id::milli),
                 pair_of (unit_id::gram));
  check (milligrams.has_value () && milligrams.value () == 0.7,
         "700 mg is exactly the double 0.7 g (the factor 1e-3 is a "
         "double-double and the product is rounded once)");
  // Many values of one pair: the converter is made once (the checks and the
  // exact factor), and `apply` converts a value in a few nanoseconds.
  result_t<converter_t, conversion_error> const milligram_to_gram
      = make_converter (pair_of (unit_id::gram, prefix_id::milli),
                        pair_of (unit_id::gram));
  check (milligram_to_gram.has_value ()
             && milligram_to_gram.value ().apply (700.0) == 0.7
             && milligram_to_gram.value () (1500.0) == 1.5,
         "a converter converts many values of one pair");
  result_t<double, conversion_error> const atmosphere
      = convert (1.0, pair_of (unit_id::standard_atmosphere),
                 pair_of (unit_id::pascal_unit));
  check (atmosphere.has_value () && atmosphere.value () == 101325.0,
         "1 atm = 101325 Pa");
  result_t<double, conversion_error> const mmhg
      = convert (760.0, pair_of (unit_id::millimetre_of_mercury),
                 pair_of (unit_id::pascal_unit, prefix_id::kilo));
  std::cout << "760 mmHg = " << std::setprecision (12) << mmhg.value ()
            << " kPa (the conventional mmHg is 133.322387415 Pa; the torr is "
               "101325 / 760 Pa)\n";
  check (convert (1.0, pair_of (unit_id::hertz), pair_of (unit_id::becquerel))
                 .error ()
             == conversion_error::different_quantity_kind,
         "hertz and becquerel are different quantities");
  check (convert (1.0, pair_of (unit_id::decibel), pair_of (unit_id::bel))
                 .error ()
             == conversion_error::no_si_value,
         "a logarithmic unit has no factor");

  section ("The degree Celsius");
  check (convert_point (25.0, pair_of (unit_id::degree_celsius),
                        pair_of (unit_id::kelvin))
                 .value ()
             == 298.15,
         "25 degrees Celsius is 298.15 K");
  check (convert_difference (10.0, pair_of (unit_id::degree_celsius),
                             pair_of (unit_id::kelvin))
                 .value ()
             == 10.0,
         "a difference of 10 degrees Celsius is 10 K");
  check (convert (25.0, pair_of (unit_id::degree_celsius),
                  pair_of (unit_id::kelvin))
                 .error ()
             == conversion_error::offset_unit,
         "convert refuses an affine unit: it needs a point or a difference");

  std::cout << (failures == 0 ? "\nAll checks passed.\n"
                              : "\nSome checks FAILED.\n");
  return failures == 0 ? 0 : 1;
}
