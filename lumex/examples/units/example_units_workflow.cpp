// A workflow: values with units read from text are converted to the SI unit of
// their quantity, and the catalog of one domain is listed.
//
// Each line of the report is "<number> <unit>"; the unit is looked up in both
// scripts, so a Russian protocol and an English one are read by the same code.
// The bytes above 127 are UTF-8 written as escapes.
#include <cstddef>
#include <cstdlib>
#include <iomanip>
#include <iostream>
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

std::string
as_string (text_view_t text)
{
  return std::string (text.data (), text.size ());
}

// The unit that a value of the unit's kind is printed in: the coherent SI
// unit for the kinds listed here, the unit itself for the others.
prefixed_unit_t
print_unit_of (unit_id unit)
{
  prefixed_unit_t pair = { unit, prefix_id::none };
  switch (descriptor (unit).kind)
    {
    case quantity_kind::pressure:
      pair.unit = unit_id::pascal_unit;
      break;
    case quantity_kind::length:
      pair.unit = unit_id::metre;
      break;
    case quantity_kind::mass:
      pair.unit = unit_id::gram;
      pair.prefix = prefix_id::kilo;
      break;
    case quantity_kind::time:
      pair.unit = unit_id::second;
      break;
    case quantity_kind::energy:
      pair.unit = unit_id::joule;
      break;
    default:
      break;
    }
  return pair;
}

// Reads "<number> <unit>" and prints the value in the unit of its kind.
bool
read_line (char const *name, std::string const &line)
{
  char *end = nullptr;
  double const value = std::strtod (line.c_str (), &end);
  if (end == line.c_str ())
    {
      std::cout << name << ": no number in \"" << line << "\"\n";
      return false;
    }
  std::string unit_text (end);
  std::size_t const start = unit_text.find_first_not_of (' ');
  unit_text
      = start == std::string::npos ? std::string () : unit_text.substr (start);
  result_t<prefixed_unit_t, lookup_error> const unit
      = lookup (text_view_t (unit_text.data (), unit_text.size ()));
  if (!unit.has_value ())
    {
      std::cout << name << ": unknown unit \"" << unit_text << "\" ("
                << error_name (unit.error ()) << ")\n";
      return false;
    }
  prefixed_unit_t const target = print_unit_of (unit.value ().unit);
  result_t<double, conversion_error> const converted
      = convert (value, unit.value (), target);
  if (!converted.has_value ())
    {
      std::cout << name << ": " << value << " " << unit_text
                << " is not converted (" << error_name (converted.error ())
                << ")\n";
      return false;
    }
  result_t<symbol_buffer_t, format_error> const symbol
      = format_symbol (target, script::international);
  std::cout << name << ": " << std::setprecision (12) << value << " "
            << unit_text << " = " << converted.value () << " "
            << as_string (symbol.value ().view ()) << '\n';
  return true;
}
} // namespace

int
main ()
{
  std::cout << "== Values with units from text ==\n";
  check (read_line ("pressure", "101.325 kPa"), "kPa to Pa");
  check (read_line ("pressure", "760 mmHg"), "mmHg to Pa");
  check (read_line ("pressure", "1 \xD0\xB0\xD1\x82\xD0\xBC"),
         "the Russian atm is an input alias (no standard gives a symbol)");
  check (read_line ("length", "12.5 \xD0\xBC\xD0\xBA\xD0\xBC"),
         "the Russian mkm to m");
  check (read_line ("length", "3 ly"), "light years to m");
  check (read_line ("mass", "700 mg"), "mg to kg");
  check (read_line ("mass", "1 t"), "tonne to kg");
  check (read_line ("time", "1.5 h"), "hours to s");
  check (read_line ("energy", "2 kW\xC2\xB7h"), "kW.h to J");
  check (read_line ("energy", "5 eV"), "eV to J");
  check (!read_line ("level", "3 dB"), "a logarithmic unit is not converted");
  check (read_line ("rate", "50 Bq"),
         "a unit of another kind keeps its own unit");
  check (!read_line ("junk", "abc"), "text without a number");

  std::cout << "\n== The units of electromagnetism ==\n";
  std::size_t listed = 0;
  for (std::size_t index = 0; index < unit_count; ++index)
    {
      unit_descriptor_t const &unit = descriptor (unit_id_at (index));
      if (unit.unit_domain != domain::electromagnetism)
        continue;
      ++listed;
      std::cout << std::setw (40) << std::left << unit.name << " "
                << as_string (
                       canonical_symbol (unit.id, script::international))
                << '\n';
    }
  check (listed > 10,
         "the domain lists the SI units of electricity and magnetism");

  std::cout << (failures == 0 ? "\nAll checks passed.\n"
                              : "\nSome checks FAILED.\n");
  return failures == 0 ? 0 : 1;
}
