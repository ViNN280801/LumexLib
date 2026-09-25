// example_format_custom.cpp
// LumexFormat, part 5: user types. Formatter specializations that reuse a
// built-in specification, parse their own, write through BasicAppender,
// support wchar_t; types formatted through operator<< (OstreamFormatter,
// streamed); reflected enums.
#include <iostream>
#include <ostream>
#include <string>
#include <vector>

#include "lumex/core/fmt/LumexFormat"
#include "lumex/core/reflection/reflected_enum/LumexReflectedEnum.hpp"

namespace fmt = lumex::core::fmt;

LUMEX_DEFINE_REFLECTED_ENUM (PumpState, int, (idle, 0), (running, 10),
                             (fault, -1))

namespace
{
/** Reuses the double specification. */
struct pressure_t
{
  double bar;
};

/** Parses its own specification: `{}` or `{:iso}`. */
struct date_t
{
  int year;
  int month;
  int day;
};

/** Has operator<<; formatted through OstreamFormatter. */
struct point_t
{
  int x;
  int y;
};

std::ostream &
operator<< (std::ostream &stream, point_t const &point)
{
  return stream << '(' << point.x << ", " << point.y << ')';
}

/** Has operator<< and no Formatter: formatted through streamed (). */
struct legacy_t
{
  std::string name;
};

std::ostream &
operator<< (std::ostream &stream, legacy_t const &legacy)
{
  return stream << "legacy<" << legacy.name << '>';
}

/** Writes character by character through the output iterator. */
struct stars_t
{
  int count;
};
} // namespace

namespace lumex
{
namespace core
{
namespace fmt
{
template <> class Formatter<pressure_t> : public Formatter<double>
{
public:
  BasicAppender<char>
  format (pressure_t const &value, FormatContext &ctx) const
  {
    BasicAppender<char> out = Formatter<double>::format (value.bar, ctx);
    return fmt::format_to (out, " bar");
  }
};

/** The same type for wide format strings. */
template <>
class Formatter<pressure_t, wchar_t> : public Formatter<double, wchar_t>
{
public:
  wchar_t const *
  parse (WFormatParseContext &ctx)
  {
    return Formatter<double, wchar_t>::parse (ctx);
  }

  BasicAppender<wchar_t>
  format (pressure_t const &value, WFormatContext &ctx) const
  {
    BasicAppender<wchar_t> out
        = Formatter<double, wchar_t>::format (value.bar, ctx);
    return fmt::format_to (out, L" bar");
  }
};

template <> class Formatter<date_t>
{
public:
  char const *
  parse (FormatParseContext &ctx)
  {
    char const *it = ctx.begin ();
    if (ctx.end () - it >= 3 && it[0] == 'i' && it[1] == 's' && it[2] == 'o')
      {
        _iso = true;
        it += 3;
      }
    if (it != ctx.end () && *it != '}')
      throw FormatError ("date_t: expected {} or {:iso}");
    return it;
  }

  BasicAppender<char>
  format (date_t const &date, FormatContext &ctx) const
  {
    if (_iso)
      return fmt::format_to (ctx.out (), "{:04}-{:02}-{:02}", date.year,
                             date.month, date.day);
    return fmt::format_to (ctx.out (), "{}/{}/{}", date.day, date.month,
                           date.year);
  }

private:
  bool _iso = false;
};

template <> class Formatter<point_t> : public OstreamFormatter<char>
{
};

/**
 * One template for every character type: BasicFormatParseContext<Char> and
 * BasicFormatContext<Char> are what FormatParseContext / FormatContext
 * (char) and WFormatParseContext / WFormatContext (wchar_t) name.
 */
template <typename Char> class Formatter<stars_t, Char>
{
public:
  Char const *
  parse (BasicFormatParseContext<Char> &ctx)
  {
    return ctx.begin (); // no options
  }

  BasicAppender<Char>
  format (stars_t const &stars, BasicFormatContext<Char> &ctx) const
  {
    BasicAppender<Char> out = ctx.out ();
    for (int i = 0; i < stars.count; ++i)
      *out++ = static_cast<Char> ('*');
    return out;
  }
};
} // namespace fmt
} // namespace core
} // namespace lumex

int
main ()
{
  std::cout << "=== LumexFormat: user types ===\n";

  std::cout << "\n--- 1. Reusing a built-in specification ---\n";
  pressure_t const pressure = { 101.325 };
  std::cout << fmt::format ("[{}] [{:.1f}] [{:>10.2f}]", pressure, pressure,
                            pressure)
            << '\n';
  std::wcout << fmt::format (L"[{:.0f}]", pressure) << L'\n';

  std::cout << "\n--- 2. Own specification ---\n";
  date_t const date = { 2026, 9, 25 };
  std::cout << fmt::format ("{} / {:iso}", date, date) << '\n';
  try
    {
      (void)fmt::format (fmt::runtime ("{:us}"), date);
    }
  catch (fmt::FormatError const &error)
    {
      std::cout << "rejected: " << error.what () << '\n';
    }

  std::cout << "\n--- 3. Through operator<< ---\n";
  point_t const point = { 3, 4 };
  std::cout << fmt::format ("[{}] [{:>10}] [{:.3}]", point, point, point)
            << '\n';
  legacy_t const legacy = { "old" };
  std::cout << fmt::format ("[{}] [{:*^16}]", fmt::streamed (legacy),
                            fmt::streamed (legacy))
            << '\n';

  std::cout << "\n--- 4. Writing through BasicAppender ---\n";
  std::cout << fmt::format ("rating: {}", stars_t{ 5 }) << '\n';
  std::wcout << fmt::format (L"wide rating: {}", stars_t{ 3 }) << L'\n';

  std::cout << "\n--- 5. Reflected enums ---\n";
  std::cout << fmt::format ("{} {:>9} {:d} {:#x} {:+d}", PumpState::running,
                            PumpState::idle, PumpState::running,
                            PumpState::running, PumpState::fault)
            << '\n';
  std::cout << fmt::format ("{}", static_cast<PumpState> (3)) << '\n';

  std::cout << "\n--- 6. Custom types inside ranges and named fields ---\n";
  std::cout << fmt::format ("{} {when:iso}", std::vector<point_t>{ { 1, 2 } },
                            fmt::arg ("when", date))
            << '\n';

  std::cout << "\n=== example_format_custom finished ===\n";
  return 0;
}
