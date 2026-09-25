// Compile-time format string checks of LumexFormat (C++20 consteval).
//
// Built by try_compile once per case. LUMEX_FORMAT_GOOD_CASE=N selects a
// call that must compile, LUMEX_FORMAT_BAD_CASE=N one the consteval checker
// must reject. Without either macro the file is the consteval probe.
#include <string>

#include "lumex/core/fmt/LumexFormat.hpp"

namespace fmt = lumex::core::fmt;

struct date_t
{
  int year;
};

namespace lumex
{
namespace core
{
namespace fmt
{
template <> class Formatter<date_t>
{
public:
  char const *
  parse (FormatParseContext &ctx)
  {
    char const *it = ctx.begin ();
    while (it != ctx.end () && *it != '}')
      ++it;
    return it;
  }

  BasicAppender<char>
  format (date_t const &date, FormatContext &ctx) const
  {
    return fmt::format_to (ctx.out (), "{}", date.year);
  }
};
} // namespace fmt
} // namespace core
} // namespace lumex

consteval int
consteval_probe ()
{
  return 0;
}

int
main ()
{
  std::string text;
#if defined(LUMEX_FORMAT_GOOD_CASE)
#if LUMEX_FORMAT_GOOD_CASE == 1
  text = fmt::format ("{} {:>8.2f} {:#x}", "a", 1.5, 255);
#elif LUMEX_FORMAT_GOOD_CASE == 2
  text = fmt::format ("{name} {0}", 1, fmt::arg ("name", 2));
#elif LUMEX_FORMAT_GOOD_CASE == 3
  text = fmt::format ("{:{}.{}f}", 3.14, 8, 2);
#elif LUMEX_FORMAT_GOOD_CASE == 4
  // A run-time string is checked when used, never at compile time.
  text = fmt::format (fmt::runtime ("{"), 1);
#elif LUMEX_FORMAT_GOOD_CASE == 5
  // User formatters parse their own specification at run time.
  text = fmt::format ("{:anything}", date_t{ 2026 });
#elif LUMEX_FORMAT_GOOD_CASE == 6
  text = fmt::format ("{{}} {:?} {:c}", std::string ("s"), 65);
#elif LUMEX_FORMAT_GOOD_CASE == 7
  std::wstring const wide = fmt::format (L"{:>4}", 1);
  text.assign (wide.size (), ' ');
#endif
#elif defined(LUMEX_FORMAT_BAD_CASE)
#if LUMEX_FORMAT_BAD_CASE == 1
  text = fmt::format ("{", 1);
#elif LUMEX_FORMAT_BAD_CASE == 2
  text = fmt::format ("}", 1);
#elif LUMEX_FORMAT_BAD_CASE == 3
  text = fmt::format ("{} {}", 1);
#elif LUMEX_FORMAT_BAD_CASE == 4
  text = fmt::format ("{0} {}", 1, 2);
#elif LUMEX_FORMAT_BAD_CASE == 5
  text = fmt::format ("{:d}", "text");
#elif LUMEX_FORMAT_BAD_CASE == 6
  text = fmt::format ("{:.2}", 1);
#elif LUMEX_FORMAT_BAD_CASE == 7
  text = fmt::format ("{:#}", "s");
#elif LUMEX_FORMAT_BAD_CASE == 8
  text = fmt::format ("{:{}}", 1, "w");
#elif LUMEX_FORMAT_BAD_CASE == 9
  text = fmt::format ("{name}", 1);
#elif LUMEX_FORMAT_BAD_CASE == 10
  text = fmt::format ("{:x}", 1.5);
#elif LUMEX_FORMAT_BAD_CASE == 11
  text = fmt::format ("{:05}", "s");
#elif LUMEX_FORMAT_BAD_CASE == 12
  std::wstring const wide = fmt::format (L"{:q}", 1);
  text.assign (wide.size (), ' ');
#endif
#endif
  return consteval_probe () + static_cast<int> (text.size () > 1000);
}
