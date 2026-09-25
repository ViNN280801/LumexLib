// LumexFormatChrono.tests.cpp
// LumexFormatChrono.hpp: std::chrono::duration and system_clock time points
// with the C++20 chrono specification. The expected texts were checked
// against MSVC std::format (C++20); the two deliberate differences are
// pinned below (precision of floating-point durations, `%c` as the C
// standard defines it).
#include <chrono>
#include <locale>
#include <ratio>
#include <string>

#include <gtest/gtest.h>

#include "lumex/core/fmt/LumexFormatChrono.hpp"
#include "lumex/tests/core/fmt/LumexFormatTestHelpers.hpp"

namespace fmt = lumex::core::fmt;

using format_test_helpers::format_error;

namespace
{
typedef std::chrono::time_point<std::chrono::system_clock,
                                std::chrono::seconds>
    sys_seconds_t;
typedef std::chrono::duration<long long, std::ratio<86400>> days_t;
typedef std::chrono::time_point<std::chrono::system_clock, days_t> sys_days_t;

/** 2026-09-25 13:04:05 UTC (a Friday). */
sys_seconds_t
sample_time ()
{
  return sys_seconds_t (std::chrono::seconds (1790341445LL));
}

sys_days_t
day_at (long long seconds_since_epoch)
{
  return sys_days_t (days_t (seconds_since_epoch / 86400));
}
} // namespace

// ---------------------------------------------------------------------------
// Durations
// ---------------------------------------------------------------------------

TEST (LumexFormatChronoTest, GivenDurations_WhenDefault_ThenCountAndUnit)
{
  EXPECT_EQ (fmt::format ("{}", std::chrono::seconds (42)), "42s");
  EXPECT_EQ (fmt::format ("{}", std::chrono::milliseconds (1500)), "1500ms");
  EXPECT_EQ (fmt::format ("{}", std::chrono::microseconds (7)), "7us");
  EXPECT_EQ (fmt::format ("{}", std::chrono::nanoseconds (-3)), "-3ns");
  EXPECT_EQ (fmt::format ("{}", std::chrono::minutes (3)), "3min");
  EXPECT_EQ (fmt::format ("{}", std::chrono::hours (2)), "2h");
  EXPECT_EQ (fmt::format ("{}", days_t (5)), "5d");
}

TEST (LumexFormatChronoTest, GivenOtherPeriods_WhenDefault_ThenRatioSuffix)
{
  EXPECT_EQ (fmt::format ("{}", std::chrono::duration<int, std::ratio<3>> (5)),
             "5[3]s");
  EXPECT_EQ (
      fmt::format ("{}", std::chrono::duration<int, std::ratio<1, 3>> (5)),
      "5[1/3]s");
  EXPECT_EQ (fmt::format ("{}", std::chrono::duration<int, std::centi> (5)),
             "5cs");
  EXPECT_EQ (fmt::format ("{}", std::chrono::duration<int, std::deci> (5)),
             "5ds");
  EXPECT_EQ (fmt::format ("{}", std::chrono::duration<int, std::kilo> (5)),
             "5ks");
  EXPECT_EQ (fmt::format ("{}", std::chrono::duration<int, std::pico> (5)),
             "5ps");
}

TEST (LumexFormatChronoTest, GivenFloatingDuration_WhenDefault_ThenShortest)
{
  EXPECT_EQ (fmt::format ("{}", std::chrono::duration<double> (1.5)), "1.5s");
  EXPECT_EQ (
      fmt::format ("{}", std::chrono::duration<double, std::milli> (2.25)),
      "2.25ms");
}

TEST (LumexFormatChronoTest, GivenFloatingDuration_WhenPrecision_ThenFixed)
{
  // MSVC's std::format ignores the precision here; the standard (and fmt)
  // apply it to the count.
  EXPECT_EQ (fmt::format ("{:.2}", std::chrono::duration<double> (1.555)),
             "1.55s");
  EXPECT_EQ (fmt::format ("{:.{}%Q}", std::chrono::duration<double> (2.0), 3),
             "2.000");
}

TEST (LumexFormatChronoTest, GivenWidth_WhenDuration_ThenPaddedLeftByDefault)
{
  EXPECT_EQ (fmt::format ("{:>10}", std::chrono::seconds (42)), "       42s");
  EXPECT_EQ (fmt::format ("{:*^9}", std::chrono::seconds (42)), "***42s***");
  EXPECT_EQ (fmt::format ("{:12}", std::chrono::seconds (42)), "42s         ");
  EXPECT_EQ (fmt::format ("{:>12%T}", std::chrono::seconds (61)),
             "    00:01:01");
  EXPECT_EQ (fmt::format ("{::>6%M}", std::chrono::minutes (5)), "::::05");
}

TEST (LumexFormatChronoTest, GivenTimeOfDaySpecs_WhenDuration_ThenFields)
{
  EXPECT_EQ (fmt::format ("{:%H:%M:%S}", std::chrono::seconds (3725)),
             "01:02:05");
  EXPECT_EQ (fmt::format ("{:%R}", std::chrono::minutes (125)), "02:05");
  EXPECT_EQ (fmt::format ("{:%I:%M %p}", std::chrono::hours (13)), "01:00 PM");
  EXPECT_EQ (fmt::format ("{:%r}", std::chrono::seconds (3725)),
             "01:02:05 AM");
  EXPECT_EQ (fmt::format ("{:%p}", std::chrono::seconds (0)), "AM");
  EXPECT_EQ (fmt::format ("{:%I}", std::chrono::hours (12)), "12");
  EXPECT_EQ (fmt::format ("{:%OH}", std::chrono::seconds (3600)), "01");
}

TEST (LumexFormatChronoTest, GivenSubsecondPeriods_WhenSeconds_ThenFraction)
{
  EXPECT_EQ (fmt::format ("{:%T}", std::chrono::milliseconds (3725123)),
             "01:02:05.123");
  EXPECT_EQ (fmt::format ("{:%S}", std::chrono::milliseconds (1500)),
             "01.500");
  EXPECT_EQ (fmt::format ("{:%S}", std::chrono::microseconds (1500)),
             "00.001500");
  EXPECT_EQ (fmt::format ("{:%S}", std::chrono::nanoseconds (1500)),
             "00.000001500");
  EXPECT_EQ (
      fmt::format ("{:%S}",
                   std::chrono::duration<long long, std::ratio<1, 10000000>> (
                       15000000)),
      "01.5000000");
  EXPECT_EQ (
      fmt::format ("{:%S}", std::chrono::duration<int, std::ratio<1, 3>> (4)),
      "01.333333");
  EXPECT_EQ (
      fmt::format ("{:%H:%M:%S}", std::chrono::nanoseconds (1234567891011LL)),
      "00:20:34.567891011");
}

TEST (LumexFormatChronoTest,
      GivenFloatingDuration_WhenSeconds_ThenPeriodDigits)
{
  EXPECT_EQ (fmt::format ("{:%S}", std::chrono::duration<double> (1.5)), "01");
  EXPECT_EQ (fmt::format ("{:%S}",
                          std::chrono::duration<double, std::milli> (1500.25)),
             "01.500");
  EXPECT_EQ (fmt::format ("{:%Q}", std::chrono::duration<double> (1.5)),
             "1.5");
}

TEST (LumexFormatChronoTest, GivenLongDurations_WhenHours_ThenNotWrapped)
{
  EXPECT_EQ (fmt::format ("{:%H}", std::chrono::hours (30)), "30");
  EXPECT_EQ (fmt::format ("{:%T}", std::chrono::hours (30)), "30:00:00");
  EXPECT_EQ (fmt::format ("{:%j}", std::chrono::hours (30)), "1");
}

TEST (LumexFormatChronoTest, GivenNegativeDuration_WhenSpecs_ThenOneMinus)
{
  EXPECT_EQ (fmt::format ("{:%T}", std::chrono::seconds (-3725)), "-01:02:05");
  EXPECT_EQ (fmt::format ("{:%H}", std::chrono::seconds (-1)), "-00");
  EXPECT_EQ (fmt::format ("{:%j}", std::chrono::hours (-50)), "-2");
}

TEST (LumexFormatChronoTest, GivenCountAndLiterals_WhenSpecs_ThenCopied)
{
  EXPECT_EQ (fmt::format ("{:%Q %q}", std::chrono::milliseconds (15)),
             "15 ms");
  EXPECT_EQ (fmt::format ("{:%n|%t|%%}", std::chrono::seconds (1)), "\n|\t|%");
  EXPECT_EQ (fmt::format ("{:%H h %M min}", std::chrono::minutes (61)),
             "01 h 01 min");
}

TEST (LumexFormatChronoTest, GivenBadDurationSpecs_WhenFormat_ThenFormatError)
{
  std::chrono::seconds const one (1);
  EXPECT_EQ (format_error ("{:%Y}", one), "invalid format specifier");
  EXPECT_EQ (format_error ("{:%F}", one), "invalid format specifier");
  EXPECT_EQ (format_error ("{:%M%}", one), "invalid format string");
  EXPECT_EQ (format_error ("{:%}", one), "invalid format string");
  EXPECT_EQ (format_error ("{:%E}", one), "invalid format string");
  EXPECT_EQ (format_error ("{:%EH}", one), "invalid format specifier");
  EXPECT_EQ (format_error ("{:abc}", one), "invalid format specifier");
  EXPECT_EQ (format_error ("{:.2}", one), "invalid format specifier");
  EXPECT_EQ (format_error ("{:.2f}", std::chrono::duration<double> (1.0)),
             "invalid format specifier");
  EXPECT_EQ (format_error ("{:%H{}", one), "invalid format string");
}

// ---------------------------------------------------------------------------
// Time points
// ---------------------------------------------------------------------------

TEST (LumexFormatChronoTest, GivenTimePoint_WhenDefault_ThenDateAndTime)
{
  EXPECT_EQ (fmt::format ("{}", sample_time ()), "2026-09-25 13:04:05");
  typedef std::chrono::time_point<std::chrono::system_clock,
                                  std::chrono::milliseconds>
      sys_milliseconds_t;
  sys_milliseconds_t const with_ms
      = std::chrono::time_point_cast<std::chrono::milliseconds> (
            sample_time ())
        + std::chrono::milliseconds (7);
  EXPECT_EQ (fmt::format ("{}", with_ms), "2026-09-25 13:04:05.007");
  EXPECT_EQ (fmt::format ("{:%S}", with_ms), "05.007");
  EXPECT_EQ (fmt::format ("{}", day_at (1790341445LL)), "2026-09-25");
  EXPECT_EQ (fmt::format ("{:>25}", sample_time ()),
             "      2026-09-25 13:04:05");
  EXPECT_EQ (fmt::format ("{:30}", sample_time ()),
             "2026-09-25 13:04:05           ");
}

TEST (LumexFormatChronoTest, GivenSystemClockNow_WhenDefault_ThenClockDigits)
{
  // The fraction has as many digits as system_clock::duration needs
  // (7 on MSVC, 9 on libstdc++ / libc++).
  std::chrono::system_clock::time_point const now
      = std::chrono::time_point_cast<std::chrono::system_clock::duration> (
          sample_time ());
  std::string const text = fmt::format ("{}", now);
  EXPECT_EQ (text.substr (0, 20), "2026-09-25 13:04:05.");
  EXPECT_EQ (text.size (), 20u
                               + static_cast<std::size_t> (
                                   fmt::Detail::fractional_digits<
                                       std::chrono::system_clock::period> ()));
}

TEST (LumexFormatChronoTest, GivenDateSpecs_WhenTimePoint_ThenCalendarFields)
{
  sys_seconds_t const time = sample_time ();
  EXPECT_EQ (fmt::format ("{:%F %T}", time), "2026-09-25 13:04:05");
  EXPECT_EQ (fmt::format ("{:%Y-%m-%d}", time), "2026-09-25");
  EXPECT_EQ (fmt::format ("{:%D|%y|%C|%e|%j}", time), "09/25/26|26|20|25|268");
  EXPECT_EQ (fmt::format ("{:%a %A %b %B %h}", time),
             "Fri Friday Sep September Sep");
  EXPECT_EQ (fmt::format ("{:%x}", time), "09/25/26");
  EXPECT_EQ (fmt::format ("{:%X}", time), "13:04:05");
  EXPECT_EQ (fmt::format ("{:%p %I %r}", time), "PM 01 01:04:05 PM");
  EXPECT_EQ (fmt::format ("{:%u %w}", time), "5 5");
  EXPECT_EQ (fmt::format ("{:%Z %z %Ez %Oz}", time),
             "UTC +0000 +00:00 +00:00");
  EXPECT_EQ (fmt::format ("{:%Ex|%EX|%Od|%OH}", time),
             "09/25/26|13:04:05|25|13");
}

TEST (LumexFormatChronoTest, GivenCSpec_WhenTimePoint_ThenCStandardForm)
{
  // The C standard's "C" locale %c; MSVC's std::format prints
  // "09/25/26 13:04:05" instead.
  EXPECT_EQ (fmt::format ("{:%c}", sample_time ()),
             "Fri Sep 25 13:04:05 2026");
  EXPECT_EQ (fmt::format ("{:%c}", day_at (951782400LL)),
             "Tue Feb 29 00:00:00 2000");
}

TEST (LumexFormatChronoTest, GivenWeekNumbers_WhenTimePoint_ThenAllSystems)
{
  EXPECT_EQ (fmt::format ("{:%U %W %V %G %g}", sample_time ()),
             "38 38 39 2026 26");
  EXPECT_EQ (fmt::format ("{:%U %W %V %G %g}", day_at (1798761600LL)),
             "00 00 53 2026 26");
  EXPECT_EQ (fmt::format ("{:%U %W %V %G %g}", day_at (1735516800LL)),
             "52 53 01 2025 25");
  EXPECT_EQ (fmt::format ("{:%U %W %V %G %g}", day_at (1609632000LL)),
             "01 00 53 2020 20");
  EXPECT_EQ (fmt::format ("{:%j}", day_at (1735603200LL)), "366");
  EXPECT_EQ (fmt::format ("{:%a %d %b %Y}", day_at (951782400LL)),
             "Tue 29 Feb 2000");
}

TEST (LumexFormatChronoTest, GivenTimesBefore1970_WhenFormat_ThenFloored)
{
  EXPECT_EQ (fmt::format ("{:%F}", day_at (-86400LL)), "1969-12-31");
  EXPECT_EQ (
      fmt::format ("{:%F %T}", sys_seconds_t (std::chrono::seconds (-1))),
      "1969-12-31 23:59:59");
  typedef std::chrono::time_point<std::chrono::system_clock,
                                  std::chrono::nanoseconds>
      sys_nanoseconds_t;
  EXPECT_EQ (
      fmt::format ("{:%T}", sys_nanoseconds_t (std::chrono::nanoseconds (-1))),
      "23:59:59.999999999");
}

TEST (LumexFormatChronoTest, GivenExtremeYears_WhenFormat_ThenSignedPadded)
{
  sys_days_t const minus_one (days_t (-719893));
  EXPECT_EQ (fmt::format ("{:%F}", minus_one), "-0001-01-01");
  EXPECT_EQ (fmt::format ("{:%Y|%C|%y}", minus_one), "-0001|-01|99");
  EXPECT_EQ (fmt::format ("{:%F}", day_at (253402214400LL)), "9999-12-31");
  EXPECT_EQ (fmt::format ("{:%F}", day_at (253402214400LL) + days_t (1)),
             "10000-01-01");
}

TEST (LumexFormatChronoTest, GivenBadTimePointSpecs_WhenFormat_ThenError)
{
  sys_seconds_t const time = sample_time ();
  EXPECT_EQ (format_error ("{:%Q}", time), "invalid format specifier");
  EXPECT_EQ (format_error ("{:%q}", time), "invalid format specifier");
  EXPECT_EQ (format_error ("{:.2}", time), "invalid format specifier");
  EXPECT_EQ (format_error ("{:%Ed}", time), "invalid format specifier");
  EXPECT_EQ (format_error ("{:%Oa}", time), "invalid format specifier");
  EXPECT_EQ (format_error ("{:%K}", time), "invalid format specifier");
}

TEST (LumexFormatChronoTest, GivenLocalizedNames_WhenClassicLocale_ThenCNames)
{
  sys_seconds_t const time = sample_time ();
  EXPECT_EQ (fmt::format (std::locale::classic (), "{:L%a %b}", time),
             "Fri Sep");
  EXPECT_EQ (fmt::format (std::locale::classic (), "{:L%H:%M}", time),
             "13:04");
}

TEST (LumexFormatChronoTest, GivenWideFormat_WhenChrono_ThenWideOutput)
{
  EXPECT_EQ (fmt::format (L"{}", std::chrono::milliseconds (5)), L"5ms");
  EXPECT_EQ (fmt::format (L"{:%F %T}", sample_time ()),
             L"2026-09-25 13:04:05");
  EXPECT_EQ (fmt::format (L"{:>8%R}", std::chrono::minutes (65)), L"   01:05");
}
