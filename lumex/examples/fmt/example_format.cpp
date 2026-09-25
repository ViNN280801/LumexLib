// example_format.cpp
// LumexFormat, part 1: replacement fields and the whole specification
// mini-language [[fill]align][sign][#][0][width][.precision][L][type] for
// every built-in type. Other examples: example_format_api.cpp (output
// functions), example_format_ranges.cpp, example_format_chrono.cpp,
// example_format_custom.cpp (user types), example_format_workflow.cpp.
#include <cstdint>
#include <iostream>
#include <limits>
#include <locale>
#include <string>
#if __cplusplus >= 201703L
#include <string_view>
#endif

#include "lumex/core/fmt/LumexFormat"
#include "lumex/core/string_view/view/LumexStringView.hpp"

namespace fmt = lumex::core::fmt;

namespace
{
/** A locale with grouping by three and a decimal comma, for `L`. */
class GroupedNumbers : public std::numpunct<char>
{
protected:
  char
  do_decimal_point () const override
  {
    return ',';
  }

  char
  do_thousands_sep () const override
  {
    return '\'';
  }

  std::string
  do_grouping () const override
  {
    return "\3";
  }

  std::string
  do_truename () const override
  {
    return "yes";
  }

  std::string
  do_falsename () const override
  {
    return "no";
  }
};

void
show (std::string const &text)
{
  std::cout << "  [" << text << "]\n";
}
} // namespace

int
main ()
{
  std::cout << "=== LumexFormat: fields and specifications ===\n";

  std::cout << "\n--- 1. Replacement fields ---\n";
  show (fmt::format ("{} + {} = {}", 2, 3, 5));               // automatic
  show (fmt::format ("{1} before {0}, {1} again", "b", "a")); // manual
  show (fmt::format ("{name} = {value}", fmt::arg ("name", "flow"),
                     fmt::arg ("value", 1.25))); // named
  show (fmt::format ("{} and {named}", 1, fmt::arg ("named", 2)));
  show (fmt::format ("{{literal}} and {{{}}}", 7)); // escaped braces
  show (fmt::format ("{0:}", 42));                  // empty specification

  std::cout << "\n--- 2. Fill and alignment ---\n";
  show (fmt::format ("{:<8}|{:>8}|{:^8}", "left", "right", "mid"));
  show (fmt::format ("{:*<8}|{:->8}|{:_^8}", "a", "b", "c"));
  show (fmt::format ("{:\xD0\xB6^9}", "utf-8")); // multi-byte fill
  show (fmt::format ("{:6}|{:6}", 42, "text"));  // default: numbers right

  std::cout << "\n--- 3. Sign, alternate form, zero padding ---\n";
  show (fmt::format ("{:+} {:+} {:-} {: } {: }", 5, -5, 5, 5, -5));
  show (fmt::format ("{:+}", 42u)); // a sign on unsigned is valid
  show (fmt::format ("{:#b} {:#B} {:#o} {:#x} {:#X}", 10, 10, 10, 255, 255));
  show (
      fmt::format ("{:08} {:+08} {:#010x} {:08.3f}", -42, 42, 255, -3.14159));
  show (fmt::format ("{:<08}", 42)); // an explicit alignment disables 0

  std::cout << "\n--- 4. Width and precision, static and dynamic ---\n";
  show (fmt::format ("{:10.3f}", 3.14159));
  show (fmt::format ("{:{}.{}f}", 3.14159, 10, 2));    // automatic arguments
  show (fmt::format ("{0:{1}.{2}f}", 3.14159, 10, 4)); // indexed arguments
  show (fmt::format ("{:{w}.{p}f}", 3.14159, fmt::arg ("w", 9),
                     fmt::arg ("p", 1)));    // named arguments
  show (fmt::format ("{:.3}", "truncated")); // precision cuts strings

  std::cout << "\n--- 5. Integers: b B c d o x X ---\n";
  show (fmt::format ("{0:d} {0:b} {0:B} {0:o} {0:x} {0:X} {1:c}", 1234, 65));
  show (fmt::format ("{} {} {}", (std::numeric_limits<long long>::min) (),
                     (std::numeric_limits<unsigned long long>::max) (),
                     static_cast<std::uint8_t> (200))); // uint8_t is a number
  show (fmt::format ("{} {}", static_cast<short> (-7),
                     static_cast<signed char> (-5)));

  std::cout << "\n--- 6. Floating point: shortest, e E f F g G a A ---\n";
  show (fmt::format ("{} {} {} {}", 0.1, 1e-5, 1e21, 123456789.0f));
  show (
      fmt::format ("{0:e} {0:E} {0:f} {0:F} {0:g} {0:G} {0:a} {0:A}", 1234.5));
  show (fmt::format ("{:.2e} {:.0f} {:#.0f} {:.3g} {:#g}", 1234.5, 2.5, 2.0,
                     0.000123456, 1.0));
  show (fmt::format ("{} {} {:+} {:F}",
                     std::numeric_limits<double>::infinity (),
                     std::numeric_limits<double>::quiet_NaN (), 0.0,
                     -std::numeric_limits<double>::infinity ()));
  show (fmt::format ("{} {:.3}", 3.0l, 1.0f / 3));

  std::cout << "\n--- 7. Text: strings, characters, bool, pointers ---\n";
  std::string const text = "std::string";
  show (fmt::format ("{} {} {:s}", "C string", text, text));
#if __cplusplus >= 201703L
  show (fmt::format ("{}", std::string_view ("std::string_view")));
#endif
  show (fmt::format ("{}", lumex::core::string_view::view::LumexStringView (
                               "LumexStringView")));
  show (fmt::format ("{:?} {:?}", "tab\there \"quoted\"", '\n')); // debug
  show (fmt::format ("{} {:c} {:d} {:#x}", 'x', 'y', 'z', 'z'));
  show (fmt::format ("{} {:s} {:d} {:>6}", true, false, true, false));
  int const value = 0;
  show (fmt::format ("{} {:p} {:P} {:018}", static_cast<void const *> (&value),
                     nullptr, static_cast<void const *> (&value),
                     static_cast<void const *> (&value)));

  std::cout
      << "\n--- 8. Width counts display columns (grapheme clusters) ---\n";
  show (fmt::format ("{:*^8}", "\xE4\xBD\xA0\xE5\xA5\xBD")); // 2 wide chars
  show (fmt::format ("{:*^6}", "e\xCC\x81")); // e + combining accent: 1 column

  std::cout << "\n--- 9. L: locale-specific form ---\n";
  std::locale const grouped (std::locale::classic (), new GroupedNumbers);
  show (fmt::format ("{:L}", 1234567)); // global locale
  show (fmt::format (grouped, "{:L} {:L} {:L}", 1234567, 1234567.5, true));
  show (fmt::format (grouped, "{:Lx} {:.2Lf}", 0x1234567, 9876.543));
  show (fmt::format (grouped, "{} {}", 1234567, 1.5)); // no L: locale unused

  std::cout << "\n=== example_format finished ===\n";
  return 0;
}
