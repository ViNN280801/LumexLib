// compile_time.cpp
// The translation unit compile_time.py compiles with -fsyntax-only to time
// what it costs to include and use one way of formatting text. BENCH_FMT_IMPL
// is 1 (LumexFormat), 2 (std::format), 3 (std::ostringstream), 4
// (std::snprintf), or 0 for the same unit with <string> and <iterator> only,
// which is the cost of the headers every variant shares. BENCH_COMPILE_MODE is
// 0 to include the header and format one int, 1 to format what the scenarios
// of bench_format.cpp format (an int, an int as padded hex, a double with a
// precision, a padded string, a line of three fields, and a format into a
// std::string through a back inserter).
#include <iterator>
#include <string>

#ifndef BENCH_COMPILE_MODE
#define BENCH_COMPILE_MODE 1
#endif

#if BENCH_FMT_IMPL == 1
#include "lumex/core/fmt/LumexFormat.hpp"
#elif BENCH_FMT_IMPL == 2
#include <format>
#elif BENCH_FMT_IMPL == 3
#include <iomanip>
#include <sstream>
#elif BENCH_FMT_IMPL == 4
#include <cstdarg>
#include <cstdio>
#endif

#if BENCH_FMT_IMPL == 4
namespace
{
std::string
snprintf_text (char const *pattern, ...)
{
  char buffer[256];
  va_list args;
  va_start (args, pattern);
  int const size = std::vsnprintf (buffer, sizeof (buffer), pattern, args);
  va_end (args);
  return std::string (buffer, static_cast<std::size_t> (size));
}
} // namespace
#endif

int
main ()
{
  std::size_t result = 0;
  int const number = 7919;
  double const value = 1.5;
  std::string const word ("channel");
#if BENCH_COMPILE_MODE == 0
#if BENCH_FMT_IMPL == 0
  result += std::to_string (number).size ();
#elif BENCH_FMT_IMPL == 1
  result += lumex::core::fmt::format ("{}", number).size ();
#elif BENCH_FMT_IMPL == 2
  result += std::format ("{}", number).size ();
#elif BENCH_FMT_IMPL == 3
  std::ostringstream stream;
  stream << number;
  result += stream.str ().size ();
#else
  result += snprintf_text ("%d", number).size ();
#endif
#else
  std::string buffer;
#if BENCH_FMT_IMPL == 0
  result += std::to_string (number).size () + std::to_string (value).size ()
            + word.size ();
  buffer += word;
  buffer.insert (buffer.end (), word.begin (), word.end ());
#elif BENCH_FMT_IMPL == 1
  namespace lfmt = lumex::core::fmt;
  result += lfmt::format ("{}", number).size ();
  result += lfmt::format ("{:#010x}", number).size ();
  result += lfmt::format ("{:.3f}", value).size ();
  result += lfmt::format ("{:>16}", word).size ();
  result += lfmt::format ("{} {} {}", number, value, word).size ();
  lfmt::format_to (std::back_inserter (buffer), "{}:{:.2f};", number, value);
#elif BENCH_FMT_IMPL == 2
  result += std::format ("{}", number).size ();
  result += std::format ("{:#010x}", number).size ();
  result += std::format ("{:.3f}", value).size ();
  result += std::format ("{:>16}", word).size ();
  result += std::format ("{} {} {}", number, value, word).size ();
  std::format_to (std::back_inserter (buffer), "{}:{:.2f};", number, value);
#elif BENCH_FMT_IMPL == 3
  {
    std::ostringstream stream;
    stream << number;
    result += stream.str ().size ();
  }
  {
    std::ostringstream stream;
    stream << "0x" << std::hex << std::setw (8) << std::setfill ('0')
           << number;
    result += stream.str ().size ();
  }
  {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision (3) << value;
    result += stream.str ().size ();
  }
  {
    std::ostringstream stream;
    stream << std::setw (16) << std::right << word;
    result += stream.str ().size ();
  }
  {
    std::ostringstream stream;
    stream << number << ' ' << value << ' ' << word;
    result += stream.str ().size ();
  }
  {
    std::ostringstream stream;
    stream << number << ':' << std::fixed << std::setprecision (2) << value
           << ';';
    buffer += stream.str ();
  }
#else
  result += snprintf_text ("%d", number).size ();
  result += snprintf_text ("%#010x", static_cast<unsigned> (number)).size ();
  result += snprintf_text ("%.3f", value).size ();
  result += snprintf_text ("%16s", word.c_str ()).size ();
  result += snprintf_text ("%d %g %s", number, value, word.c_str ()).size ();
  buffer += snprintf_text ("%d:%.2f;", number, value);
#endif
  result += buffer.size ();
#endif
  return result == 0 ? 1 : 0;
}
