/**
 * Copyright (c) 2026 Vladislav Semykin <vladislav.semykin@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/**
 * @file LumexFormatChrono.hpp
 * @brief `std::chrono::duration` and `system_clock` time points for
 * LumexFormat, with the chrono specification of C++20 `std::format`.
 *
 * @details
 * - `{}` of a duration prints its count and unit: `42s`, `1500ms`, `7us`,
 *   `3min`, `2h`, `5d`, `5[1/3]s`. A floating-point count prints shortest,
 *   or fixed with a precision (`{:.2}` -> `1.56s`).
 * - `{}` of a `system_clock` time point prints `%F %T` in UTC with as many
 *   fractional second digits as its duration has (`2026-09-25 13:04:05.007`);
 *   a whole-day time point prints the date only.
 * - Specification: `[[fill]align][width][.precision][L][chrono-specs]`, where
 *   chrono-specs start with a conversion (`%H:%M`) and may contain literal
 *   text. Durations: `%H %M %S %T %R %I %p %r %j %Q %q %n %t %%`. Time points
 *   additionally: `%Y %C %y %m %d %e %F %D %j %a %A %b %B %h %u %w %U %W %V
 *   %G %g %c %x %X %Z %z` and the `E` / `O` modifiers the standard allows.
 * - `%S` shows as many fractional digits as the duration's period needs
 *   (3 for milliseconds, 7 for 100 ns ticks, 6 when no decimal is exact).
 * - A negative duration prints one `-` before everything else (`-01:02:05`).
 * - Without `L`, names (`%a %b %p`) and `%c %x %X` are those of the C locale
 *   (`%c` = `%a %b %e %T %Y`); with `L` they come from the locale's
 *   `std::time_put`.
 * - The microsecond suffix is the ASCII `us` (the standard allows it; MSVC
 *   prints it too), so the output never depends on the execution charset.
 */
#ifndef LUMEX_CORE_FMT_FORMAT_CHRONO_HPP
#define LUMEX_CORE_FMT_FORMAT_CHRONO_HPP

#include <chrono>
#include <cmath>
#include <cstddef>
#include <ctime>
#include <iterator>
#include <locale>
#include <ratio>
#include <sstream>
#include <string>
#include <type_traits>

#include "lumex/core/fmt/LumexFormat.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace core
{
namespace fmt
{
namespace Detail
{
// ----------------------------------------------------------------------
// Calendar arithmetic (proleptic Gregorian, days since 1970-01-01)
// ----------------------------------------------------------------------

/** @brief `value / divisor` rounded toward negative infinity. */
inline long long
floor_div (long long value, long long divisor) LUMEX_NOEXCEPT
{
  long long quotient = value / divisor;
  if ((value % divisor != 0) && ((value < 0) != (divisor < 0)))
    --quotient;
  return quotient;
}

/** @brief `value mod divisor` in `[0, divisor)` for a positive divisor. */
inline long long
floor_mod (long long value, long long divisor) LUMEX_NOEXCEPT
{
  return value - floor_div (value, divisor) * divisor;
}

/** @brief Days since 1970-01-01 of a civil date. */
inline long long
days_from_civil (long long year_value, int month_value,
                 int day_value) LUMEX_NOEXCEPT
{
  year_value -= month_value <= 2 ? 1 : 0;
  long long const era = floor_div (year_value, 400);
  long long const year_of_era = year_value - era * 400;
  long long const day_of_year
      = (153 * (month_value > 2 ? month_value - 3 : month_value + 9) + 2) / 5
        + day_value - 1;
  long long const day_of_era
      = year_of_era * 365 + year_of_era / 4 - year_of_era / 100 + day_of_year;
  return era * 146097 + day_of_era - 719468;
}

/** @brief Broken-down calendar date and time of day. */
struct civil_time_t
{
  long long year;
  int month;       ///< 1..12
  int day;         ///< 1..31
  int weekday;     ///< 0 = Sunday .. 6
  int year_day;    ///< 0-based day of the year
  long long hours; ///< Hours (not reduced modulo 24 for durations)
  int minutes;     ///< 0..59
  int seconds;     ///< 0..59
  long long days;  ///< Whole days (durations: for `%j`)
  long long ticks; ///< Fraction of the second in `10^-digits` units
  int digits;      ///< Fractional second digits shown by `%S`
  bool negative;   ///< Durations: the value is below zero
  bool has_date;   ///< Time points
};

/** @brief Fills the date part of `time` from days since 1970-01-01. */
inline void
civil_from_days (long long day_count, civil_time_t &time) LUMEX_NOEXCEPT
{
  long long const shifted = day_count + 719468;
  long long const era = floor_div (shifted, 146097);
  long long const day_of_era = shifted - era * 146097;
  long long const year_of_era = (day_of_era - day_of_era / 1460
                                 + day_of_era / 36524 - day_of_era / 146096)
                                / 365;
  long long const day_of_year
      = day_of_era - (365 * year_of_era + year_of_era / 4 - year_of_era / 100);
  long long const month_index = (5 * day_of_year + 2) / 153;
  time.day = static_cast<int> (day_of_year - (153 * month_index + 2) / 5 + 1);
  time.month = static_cast<int> (month_index < 10 ? month_index + 3
                                                  : month_index - 9);
  time.year = year_of_era + era * 400 + (time.month <= 2 ? 1 : 0);
  time.weekday = static_cast<int> (floor_mod (day_count + 4, 7));
  time.year_day
      = static_cast<int> (day_count - days_from_civil (time.year, 1, 1));
}

// ----------------------------------------------------------------------
// Fractional seconds
// ----------------------------------------------------------------------

/** @brief `10^exponent` for `exponent` in 0..18. */
inline long long
power_of_ten (int exponent) LUMEX_NOEXCEPT
{
  long long value = 1;
  for (int i = 0; i < exponent; ++i)
    value *= 10;
  return value;
}

/**
 * @brief Fractional second digits of `%S` for `Period`: the smallest `n`
 * (at most 18) with `Period * 10^n` whole, 6 when there is none.
 */
template <typename Period>
int
fractional_digits () LUMEX_NOEXCEPT
{
  for (int digits = 0; digits <= 18; ++digits)
    if (power_of_ten (digits) % Period::den == 0)
      return digits;
  return 6;
}

/**
 * @brief Splits a non-negative duration into whole seconds and the
 * fraction in `10^-digits` units (truncated).
 */
template <typename Rep, typename Period>
void
split_seconds (std::chrono::duration<Rep, Period> const &value, int digits,
               long long &whole_seconds, long long &ticks)
{
  std::chrono::seconds const whole
      = std::chrono::duration_cast<std::chrono::seconds> (value);
  whole_seconds = static_cast<long long> (whole.count ());
  ticks = 0;
  if (digits == 0)
    return;
  typedef typename std::common_type<std::chrono::duration<Rep, Period>,
                                    std::chrono::seconds>::type common_type;
  common_type const rest = common_type (value) - common_type (whole);
  typedef typename common_type::period period;
  long long const scale = power_of_ten (digits);
  if (!std::is_floating_point<Rep>::value && scale % period::den == 0)
    {
      // Exact in integers: rest < 1 s, so the product stays below 10^18.
      ticks = static_cast<long long> (rest.count ()) * period::num
              * (scale / period::den);
      return;
    }
  // Floating-point counts, or a period without an exact decimal fraction
  // (6 digits, truncated).
  typedef std::chrono::duration<double> double_seconds;
  double const fraction
      = std::chrono::duration_cast<double_seconds> (rest).count ();
  ticks = static_cast<long long> (fraction * static_cast<double> (scale));
  if (ticks >= scale)
    ticks = scale - 1;
  else if (ticks <= -scale)
    ticks = 1 - scale;
}

/** @brief Unit suffix of `%q` / `{}` for `Period`. */
template <typename Period>
std::string
unit_suffix ()
{
  typedef std::ratio<Period::num, Period::den> period;
  if (std::ratio_equal<period, std::atto>::value)
    return "as";
  if (std::ratio_equal<period, std::femto>::value)
    return "fs";
  if (std::ratio_equal<period, std::pico>::value)
    return "ps";
  if (std::ratio_equal<period, std::nano>::value)
    return "ns";
  if (std::ratio_equal<period, std::micro>::value)
    return "us";
  if (std::ratio_equal<period, std::milli>::value)
    return "ms";
  if (std::ratio_equal<period, std::centi>::value)
    return "cs";
  if (std::ratio_equal<period, std::deci>::value)
    return "ds";
  if (std::ratio_equal<period, std::ratio<1>>::value)
    return "s";
  if (std::ratio_equal<period, std::deca>::value)
    return "das";
  if (std::ratio_equal<period, std::hecto>::value)
    return "hs";
  if (std::ratio_equal<period, std::kilo>::value)
    return "ks";
  if (std::ratio_equal<period, std::mega>::value)
    return "Ms";
  if (std::ratio_equal<period, std::giga>::value)
    return "Gs";
  if (std::ratio_equal<period, std::tera>::value)
    return "Ts";
  if (std::ratio_equal<period, std::peta>::value)
    return "Ps";
  if (std::ratio_equal<period, std::exa>::value)
    return "Es";
  if (std::ratio_equal<period, std::ratio<60>>::value)
    return "min";
  if (std::ratio_equal<period, std::ratio<3600>>::value)
    return "h";
  if (std::ratio_equal<period, std::ratio<86400>>::value)
    return "d";
  if (period::den == 1)
    return "[" + std::to_string (static_cast<long long> (period::num)) + "]s";
  return "[" + std::to_string (static_cast<long long> (period::num)) + "/"
         + std::to_string (static_cast<long long> (period::den)) + "]s";
}

// ----------------------------------------------------------------------
// Conversion specifications
// ----------------------------------------------------------------------

/** @brief `%<conversion>` is valid for a duration / a time point. */
template <typename Char>
bool
is_chrono_conversion (Char conversion, Char modifier,
                      bool is_duration) LUMEX_NOEXCEPT
{
  char const value = static_cast<char> (conversion);
  if (static_cast<Char> (value) != conversion || value == '\0')
    return false;
  // Time of day and literals work for both; `%Q %q` only for durations;
  // calendar conversions only for time points.
  static char const common_ok[] = "HMSTRIprjnt%";
  static char const duration_ok[] = "Qq";
  static char const date_ok[] = "YCymdeFDaAbBhuwUWVGgcxXZz";
  std::string const allowed
      = std::string (common_ok) + (is_duration ? duration_ok : date_ok);
  if (allowed.find (value) == std::string::npos)
    return false;
  if (modifier == static_cast<Char> ('E'))
    {
      static char const e_ok[] = "cCxXyYz";
      for (char const *it = e_ok; *it != '\0'; ++it)
        if (*it == value)
          return !is_duration;
      return false;
    }
  if (modifier == static_cast<Char> ('O'))
    {
      static char const o_ok[] = "deHImMSuUVwWyz";
      for (char const *it = o_ok; *it != '\0'; ++it)
        if (*it == value)
          return !is_duration || value == 'H' || value == 'I' || value == 'M'
                 || value == 'S';
      return false;
    }
  return true;
}

/**
 * @brief Checks `[begin, end)` as chrono-specs; they must start with a
 * conversion.
 */
template <typename Char>
void
check_chrono_specs (Char const *begin, Char const *end, bool is_duration)
{
  if (begin != end && *begin != static_cast<Char> ('%'))
    throw FormatError ("invalid format specifier");
  for (Char const *it = begin; it != end; ++it)
    {
      if (*it == static_cast<Char> ('{'))
        throw FormatError ("invalid format string");
      if (*it != static_cast<Char> ('%'))
        continue;
      ++it;
      if (it == end)
        throw FormatError ("invalid format string");
      Char modifier = Char ();
      if (*it == static_cast<Char> ('E') || *it == static_cast<Char> ('O'))
        {
          modifier = *it;
          ++it;
          if (it == end)
            throw FormatError ("invalid format string");
        }
      if (!is_chrono_conversion (*it, modifier, is_duration))
        throw FormatError ("invalid format specifier");
    }
}

template <typename Char>
void
append_ascii (std::basic_string<Char> &out, std::string const &text)
{
  for (std::size_t i = 0; i < text.size (); ++i)
    out.push_back (static_cast<Char> (text[i]));
}

/** @brief `value` with at least `width` digits, zero padded, with sign. */
template <typename Char>
void
append_number (std::basic_string<Char> &out, long long value, int width)
{
  if (value < 0)
    {
      out.push_back (static_cast<Char> ('-'));
      value = -value;
    }
  std::string digits = std::to_string (value);
  if (static_cast<int> (digits.size ()) < width)
    digits.insert (0, static_cast<std::size_t> (width) - digits.size (), '0');
  append_ascii (out, digits);
}

template <typename Char>
void
append_seconds (std::basic_string<Char> &out, civil_time_t const &time)
{
  append_number (out, time.seconds, 2);
  if (time.digits > 0)
    {
      out.push_back (static_cast<Char> ('.'));
      append_number (out, time.ticks, time.digits);
    }
}

/** @brief One conversion through the locale's `std::time_put` (`L`). */
template <typename Char>
void
append_localized (std::basic_string<Char> &out, civil_time_t const &time,
                  char conversion, char modifier, std::locale const &locale)
{
  std::tm tm_value = std::tm ();
  tm_value.tm_year = static_cast<int> (time.year - 1900);
  tm_value.tm_mon = time.month - 1;
  tm_value.tm_mday = time.day;
  tm_value.tm_hour = static_cast<int> (time.hours % 24);
  tm_value.tm_min = time.minutes;
  tm_value.tm_sec = time.seconds;
  tm_value.tm_wday = time.weekday;
  tm_value.tm_yday = time.year_day;
  std::basic_ostringstream<Char> stream;
  stream.imbue (locale);
  std::use_facet<std::time_put<Char>> (locale).put (
      std::ostreambuf_iterator<Char> (stream), stream, stream.fill (),
      &tm_value, conversion, modifier);
  out += stream.str ();
}

inline char const *
weekday_name (int weekday_index, bool full) LUMEX_NOEXCEPT
{
  static char const *const short_names[]
      = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
  static char const *const full_names[]
      = { "Sunday",   "Monday", "Tuesday", "Wednesday",
          "Thursday", "Friday", "Saturday" };
  return full ? full_names[weekday_index] : short_names[weekday_index];
}

inline char const *
month_name (int month_index, bool full) LUMEX_NOEXCEPT
{
  static char const *const short_names[]
      = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
          "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
  static char const *const full_names[] = {
    "January", "February", "March",     "April",   "May",      "June",
    "July",    "August",   "September", "October", "November", "December"
  };
  return full ? full_names[month_index - 1] : short_names[month_index - 1];
}

/** @brief ISO 8601 week-based year and week number. */
inline void
iso_week (civil_time_t const &time, long long &iso_year,
          int &week) LUMEX_NOEXCEPT
{
  long long const day_count
      = days_from_civil (time.year, time.month, time.day);
  int const iso_weekday = time.weekday == 0 ? 7 : time.weekday;
  long long const thursday = day_count + 4 - iso_weekday;
  civil_time_t thursday_time = civil_time_t ();
  civil_from_days (thursday, thursday_time);
  iso_year = thursday_time.year;
  week = thursday_time.year_day / 7 + 1;
}

/** @brief Writes `time` following the checked chrono-specs `[it, end)`. */
template <typename Char>
void
write_chrono (std::basic_string<Char> &out, Char const *it, Char const *end,
              civil_time_t const &time, std::basic_string<Char> const &count,
              std::string const &unit, bool localized,
              std::locale const &locale)
{
  if (time.negative)
    out.push_back (static_cast<Char> ('-'));
  for (; it != end; ++it)
    {
      if (*it != static_cast<Char> ('%'))
        {
          out.push_back (*it);
          continue;
        }
      ++it;
      char modifier = '\0';
      if (*it == static_cast<Char> ('E') || *it == static_cast<Char> ('O'))
        {
          modifier = static_cast<char> (*it);
          ++it;
        }
      char const conversion = static_cast<char> (*it);
      long long const hour12 = time.hours % 12 == 0 ? 12 : time.hours % 12;
      bool const names = conversion == 'a' || conversion == 'A'
                         || conversion == 'b' || conversion == 'B'
                         || conversion == 'h' || conversion == 'p'
                         || conversion == 'c' || conversion == 'x'
                         || conversion == 'X' || conversion == 'r';
      if (localized && names)
        {
          append_localized (out, time, conversion, modifier, locale);
          continue;
        }
      switch (conversion)
        {
        case 'n':
          out.push_back (static_cast<Char> ('\n'));
          break;
        case 't':
          out.push_back (static_cast<Char> ('\t'));
          break;
        case '%':
          out.push_back (static_cast<Char> ('%'));
          break;
        case 'Q':
          out += count;
          break;
        case 'q':
          append_ascii (out, unit);
          break;
        case 'H':
          append_number (out, time.hours, 2);
          break;
        case 'I':
          append_number (out, hour12, 2);
          break;
        case 'M':
          append_number (out, time.minutes, 2);
          break;
        case 'S':
          append_seconds (out, time);
          break;
        case 'p':
          append_ascii (out, time.hours % 24 < 12 ? "AM" : "PM");
          break;
        case 'R':
          append_number (out, time.hours, 2);
          out.push_back (static_cast<Char> (':'));
          append_number (out, time.minutes, 2);
          break;
        case 'T':
          append_number (out, time.hours, 2);
          out.push_back (static_cast<Char> (':'));
          append_number (out, time.minutes, 2);
          out.push_back (static_cast<Char> (':'));
          append_seconds (out, time);
          break;
        case 'r':
          append_number (out, hour12, 2);
          out.push_back (static_cast<Char> (':'));
          append_number (out, time.minutes, 2);
          out.push_back (static_cast<Char> (':'));
          append_number (out, time.seconds, 2);
          append_ascii (out, time.hours % 24 < 12 ? " AM" : " PM");
          break;
        case 'j':
          if (time.has_date)
            append_number (out, time.year_day + 1, 3);
          else
            append_number (out, time.days, 0);
          break;
        case 'Y':
          append_number (out, time.year, 4);
          break;
        case 'C':
          append_number (out, floor_div (time.year, 100), 2);
          break;
        case 'y':
          append_number (out, floor_mod (time.year, 100), 2);
          break;
        case 'm':
          append_number (out, time.month, 2);
          break;
        case 'd':
          append_number (out, time.day, 2);
          break;
        case 'e':
          if (time.day < 10)
            out.push_back (static_cast<Char> (' '));
          append_number (out, time.day, 1);
          break;
        case 'F':
          append_number (out, time.year, 4);
          out.push_back (static_cast<Char> ('-'));
          append_number (out, time.month, 2);
          out.push_back (static_cast<Char> ('-'));
          append_number (out, time.day, 2);
          break;
        case 'D':
        case 'x':
          append_number (out, time.month, 2);
          out.push_back (static_cast<Char> ('/'));
          append_number (out, time.day, 2);
          out.push_back (static_cast<Char> ('/'));
          append_number (out, floor_mod (time.year, 100), 2);
          break;
        case 'X':
          append_number (out, time.hours, 2);
          out.push_back (static_cast<Char> (':'));
          append_number (out, time.minutes, 2);
          out.push_back (static_cast<Char> (':'));
          append_number (out, time.seconds, 2);
          break;
        case 'c':
          append_ascii (out, weekday_name (time.weekday, false));
          out.push_back (static_cast<Char> (' '));
          append_ascii (out, month_name (time.month, false));
          out.push_back (static_cast<Char> (' '));
          if (time.day < 10)
            out.push_back (static_cast<Char> (' '));
          append_number (out, time.day, 1);
          out.push_back (static_cast<Char> (' '));
          append_number (out, time.hours, 2);
          out.push_back (static_cast<Char> (':'));
          append_number (out, time.minutes, 2);
          out.push_back (static_cast<Char> (':'));
          append_number (out, time.seconds, 2);
          out.push_back (static_cast<Char> (' '));
          append_number (out, time.year, 4);
          break;
        case 'a':
        case 'A':
          append_ascii (out, weekday_name (time.weekday, conversion == 'A'));
          break;
        case 'b':
        case 'h':
        case 'B':
          append_ascii (out, month_name (time.month, conversion == 'B'));
          break;
        case 'u':
          append_number (out, time.weekday == 0 ? 7 : time.weekday, 1);
          break;
        case 'w':
          append_number (out, time.weekday, 1);
          break;
        case 'U':
          append_number (out, (time.year_day + 7 - time.weekday) / 7, 2);
          break;
        case 'W':
          append_number (out, (time.year_day + 7 - (time.weekday + 6) % 7) / 7,
                         2);
          break;
        case 'V':
        case 'G':
        case 'g':
          {
            long long iso_year = 0;
            int week = 0;
            iso_week (time, iso_year, week);
            if (conversion == 'V')
              append_number (out, week, 2);
            else if (conversion == 'G')
              append_number (out, iso_year, 4);
            else
              append_number (out, floor_mod (iso_year, 100), 2);
          }
          break;
        case 'Z':
          append_ascii (out, "UTC");
          break;
        case 'z':
          append_ascii (out, modifier != '\0' ? "+00:00" : "+0000");
          break;
        default:
          throw FormatError ("invalid format specifier");
        }
    }
}

/**
 * @class ChronoFormatter
 * @brief Parses `[[fill]align][width][.precision][L][chrono-specs]` and pads
 * the chrono text; base of the duration and time point formatters.
 */
template <typename Char> class ChronoFormatter
{
public:
  ChronoFormatter () : _specs (), _chrono () {}

protected:
  Char const *
  parse_chrono (BasicFormatParseContext<Char> &ctx, bool is_duration,
                bool allows_precision)
  {
    _specs = format_specs_t<Char> (); // a second parse starts from scratch
    _chrono.clear ();
    Char const *it = parse_fill_align_width (ctx, _specs, true);
    Char const *const end = ctx.end ();
    if (it != end && *it == static_cast<Char> ('.'))
      {
        if (!allows_precision)
          throw FormatError ("invalid format specifier");
        ++it;
        if (it != end && is_digit (*it))
          _specs.precision = parse_nonnegative_int (it, end);
        else if (it != end && *it == static_cast<Char> ('{'))
          _specs.precision_ref = parse_dynamic_ref (it, end, ctx);
        else
          throw FormatError ("invalid precision");
      }
    if (it != end && *it == static_cast<Char> ('L'))
      {
        _specs.localized = true;
        ++it;
      }
    Char const *const specs_begin = it;
    while (it != end && *it != static_cast<Char> ('}'))
      ++it;
    check_chrono_specs (specs_begin, it, is_duration);
    _chrono.assign (specs_begin, it);
    return it;
  }

  /** @brief Precision (resolved) or -1. */
  int
  precision (BasicFormatContext<Char> const &ctx) const
  {
    return resolve_dynamic (_specs.precision_ref, _specs.precision, ctx);
  }

  void
  write (BasicFormatContext<Char> &ctx,
         std::basic_string<Char> const &text) const
  {
    format_specs_t<Char> specs = _specs;
    specs.width = resolve_dynamic (specs.width_ref, specs.width, ctx);
    write_padded (ctx.out ().buffer (), specs, Align::left, text);
  }

  bool
  has_chrono_specs () const LUMEX_NOEXCEPT
  {
    return !_chrono.empty ();
  }

  std::basic_string<Char> const &
  chrono_specs () const LUMEX_NOEXCEPT
  {
    return _chrono;
  }

  bool
  localized () const LUMEX_NOEXCEPT
  {
    return _specs.localized;
  }

private:
  format_specs_t<Char> _specs;
  std::basic_string<Char> _chrono;
};

/** @brief Count of a duration as `{}` / `%Q` print it. */
template <typename Char, typename Rep>
std::basic_string<Char>
duration_count (Rep count, int precision)
{
  std::basic_string<Char> text;
  StringBuffer<Char> buffer (text);
  std::basic_string<Char> spec;
  if (std::is_floating_point<Rep>::value && precision >= 0)
    {
      spec.push_back (static_cast<Char> ('.'));
      append_number (spec, precision, 1);
      spec.push_back (static_cast<Char> ('f'));
    }
  spec.push_back (static_cast<Char> ('}'));
  Formatter<Rep, Char> formatter;
  BasicFormatParseContext<Char> parse_ctx (
      BasicStringRef<Char> (spec.data (), spec.size ()));
  formatter.parse (parse_ctx);
  BasicFormatArgs<Char> const args;
  BasicFormatContext<Char> ctx (buffer, args, nullptr);
  formatter.format (count, ctx);
  return text;
}
} // namespace Detail

// ----------------------------------------------------------------------
// Public formatters
// ----------------------------------------------------------------------

/** @brief `std::chrono::duration`: `42ms`, or chrono-specs such as `%T`. */
template <typename Rep, typename Period, typename Char>
class Formatter<
    std::chrono::duration<Rep, Period>, Char,
    typename std::enable_if<Detail::has_formatter<Char, Rep> ()
                            && std::is_arithmetic<Rep>::value>::type>
    : public Detail::ChronoFormatter<Char>
{
public:
  Char const *
  parse (BasicFormatParseContext<Char> &ctx)
  {
    return this->parse_chrono (ctx, true, std::is_floating_point<Rep>::value);
  }

  BasicAppender<Char>
  format (std::chrono::duration<Rep, Period> const &value,
          BasicFormatContext<Char> &ctx) const
  {
    int const precision = this->precision (ctx);
    std::basic_string<Char> text;
    if (!this->has_chrono_specs ())
      {
        text = Detail::duration_count<Char> (value.count (), precision);
        Detail::append_ascii (text, Detail::unit_suffix<Period> ());
      }
    else
      {
        typedef std::chrono::duration<Rep, Period> duration_type;
        bool const negative = value < duration_type::zero ();
        if (std::is_floating_point<Rep>::value)
          {
            // Whole seconds must fit a long long; inf / nan have no fields.
            double const seconds
                = std::chrono::duration_cast<std::chrono::duration<double>> (
                      value)
                      .count ();
            if (!(std::fabs (seconds) < 9.2e18))
              throw FormatError ("duration is out of range for chrono-specs");
          }
        Detail::civil_time_t time = Detail::civil_time_t ();
        time.negative = negative;
        time.digits = Detail::fractional_digits<Period> ();
        // Split the signed value (both parts truncate toward zero), then take
        // the magnitude of the parts: negating the value itself would
        // overflow for duration::min ().
        long long signed_seconds = 0;
        long long signed_ticks = 0;
        Detail::split_seconds (value, time.digits, signed_seconds,
                               signed_ticks);
        unsigned long long const total_seconds
            = signed_seconds < 0
                  ? 0ull - static_cast<unsigned long long> (signed_seconds)
                  : static_cast<unsigned long long> (signed_seconds);
        time.ticks = signed_ticks < 0 ? -signed_ticks : signed_ticks;
        time.hours = static_cast<long long> (total_seconds / 3600);
        time.minutes = static_cast<int> (total_seconds % 3600 / 60);
        time.seconds = static_cast<int> (total_seconds % 60);
        time.days = static_cast<long long> (total_seconds / 86400);
        std::basic_string<Char> count
            = Detail::duration_count<Char> (value.count (), precision);
        if (!count.empty () && count[0] == static_cast<Char> ('-'))
          count.erase (0, 1); // the sign is printed once, in front
        std::basic_string<Char> const &specs = this->chrono_specs ();
        Detail::write_chrono (
            text, specs.data (), specs.data () + specs.size (), time, count,
            Detail::unit_suffix<Period> (), this->localized (), ctx.locale ());
      }
    this->write (ctx, text);
    return ctx.out ();
  }
};

/**
 * @brief `std::chrono::system_clock` time points (UTC): `%F %T` by default,
 * the date only for whole-day time points.
 */
template <typename Duration, typename Char>
class Formatter<std::chrono::time_point<std::chrono::system_clock, Duration>,
                Char,
                typename std::enable_if<!std::chrono::treat_as_floating_point<
                    typename Duration::rep>::value>::type>
    : public Detail::ChronoFormatter<Char>
{
public:
  Char const *
  parse (BasicFormatParseContext<Char> &ctx)
  {
    return this->parse_chrono (ctx, false, false);
  }

  BasicAppender<Char>
  format (std::chrono::time_point<std::chrono::system_clock, Duration> const
              &value,
          BasicFormatContext<Char> &ctx) const
  {
    typedef std::chrono::duration<long long, std::ratio<86400>> days_type;
    typedef typename std::common_type<Duration, days_type,
                                      std::chrono::seconds>::type common_type;
    common_type const since_epoch (value.time_since_epoch ());
    // Whole seconds rounded down (duration_cast truncates toward zero),
    // then whole days rounded down, so times before 1970 stay correct.
    std::chrono::seconds whole
        = std::chrono::duration_cast<std::chrono::seconds> (since_epoch);
    if (common_type (whole) > since_epoch)
      whole -= std::chrono::seconds (1);
    long long const day_count
        = Detail::floor_div (static_cast<long long> (whole.count ()), 86400);
    common_type const time_of_day
        = since_epoch - common_type (days_type (day_count));
    Detail::civil_time_t time = Detail::civil_time_t ();
    time.has_date = true;
    Detail::civil_from_days (day_count, time);
    time.digits = Detail::fractional_digits<typename Duration::period> ();
    long long total_seconds = 0;
    Detail::split_seconds (time_of_day, time.digits, total_seconds,
                           time.ticks);
    time.hours = total_seconds / 3600;
    time.minutes = static_cast<int> (total_seconds % 3600 / 60);
    time.seconds = static_cast<int> (total_seconds % 60);

    static Char const date_time[] = { '%', 'F', ' ', '%', 'T' };
    static Char const date_only[] = { '%', 'F' };
    bool const whole_days = std::ratio_greater_equal<typename Duration::period,
                                                     std::ratio<86400>>::value;
    std::basic_string<Char> const default_specs
        = whole_days ? std::basic_string<Char> (date_only, 2)
                     : std::basic_string<Char> (date_time, 5);
    std::basic_string<Char> const &specs
        = this->has_chrono_specs () ? this->chrono_specs () : default_specs;
    std::basic_string<Char> text;
    Detail::write_chrono (text, specs.data (), specs.data () + specs.size (),
                          time, std::basic_string<Char> (), std::string (),
                          this->localized (), ctx.locale ());
    this->write (ctx, text);
    return ctx.out ();
  }
};
} // namespace fmt
} // namespace core
} // namespace lumex

#endif // !LUMEX_CORE_FMT_FORMAT_CHRONO_HPP
