// example_format_chrono.cpp
// LumexFormat, part 4: std::chrono durations and system_clock time points
// (LumexFormatChrono.hpp) with the chrono specification
// [[fill]align][width][.precision][L][chrono-specs].
#include <chrono>
#include <iostream>
#include <locale>
#include <ratio>
#include <string>

#include "lumex/core/fmt/LumexFormat"

namespace fmt = lumex::core::fmt;

namespace
{
typedef std::chrono::time_point<std::chrono::system_clock,
                                std::chrono::seconds>
    sys_seconds_t;
typedef std::chrono::duration<long long, std::ratio<86400>> days_t;
typedef std::chrono::time_point<std::chrono::system_clock, days_t> sys_days_t;

void
show (std::string const &text)
{
  std::cout << "  [" << text << "]\n";
}
} // namespace

int
main ()
{
  std::cout << "=== LumexFormat: chrono ===\n";

  std::cout << "\n--- 1. Durations: count and unit ---\n";
  show (fmt::format ("{} {} {} {}", std::chrono::nanoseconds (5),
                     std::chrono::microseconds (7),
                     std::chrono::milliseconds (1500),
                     std::chrono::seconds (42)));
  show (fmt::format ("{} {} {}", std::chrono::minutes (3),
                     std::chrono::hours (2), days_t (5)));
  show (fmt::format ("{} {}", std::chrono::duration<int, std::ratio<1, 3>> (5),
                     std::chrono::duration<int, std::kilo> (5)));
  show (fmt::format ("{} {:.2}", std::chrono::duration<double> (1.5),
                     std::chrono::duration<double> (1.555))); // precision
  show (fmt::format ("{:>10} {:*^9}", std::chrono::seconds (42),
                     std::chrono::seconds (42)));

  std::cout << "\n--- 2. Durations: chrono-specs ---\n";
  std::chrono::milliseconds const elapsed (3725123);
  show (fmt::format ("{:%H:%M:%S}", elapsed)); // fraction by period
  show (fmt::format ("{:%T} {:%R}", elapsed, elapsed));
  show (fmt::format ("{:%I:%M %p} {:%r}", std::chrono::hours (13), elapsed));
  show (
      fmt::format ("{:%j} days, {:%Q %q}", std::chrono::hours (50), elapsed));
  show (fmt::format ("{:%T}", std::chrono::seconds (-3725))); // one minus
  show (fmt::format ("{:%H h %M min%n%t%%}", std::chrono::minutes (61)));
  show (fmt::format ("{:%OH}", std::chrono::hours (7)));
  show (fmt::format ("{:>12%T}", std::chrono::seconds (61)));

  std::cout << "\n--- 3. Time points (UTC) ---\n";
  sys_seconds_t const time (std::chrono::seconds (1790341445LL));
  show (fmt::format ("{}", time)); // %F %T
  show (fmt::format (
      "{}", std::chrono::time_point_cast<std::chrono::milliseconds> (time)));
  show (fmt::format ("{}", sys_days_t (days_t (20721)))); // date only
  show (fmt::format ("{:%Y-%m-%d %H:%M:%S}", time));
  show (fmt::format ("{:%F|%D|%T|%R}", time));
  show (fmt::format ("{:%a %A %b %B %h}", time));
  show (fmt::format ("{:%y %C %e %j %u %w}", time));
  show (fmt::format ("{:%U %W %V %G %g}", time));
  show (fmt::format ("{:%I:%M %p|%r}", time));
  show (fmt::format ("{:%c|%x|%X}", time));
  show (fmt::format ("{:%Ec|%Ex|%EX|%EY|%Od|%OH|%Ez}", time));
  show (fmt::format ("{:%Z %z}", time));
  show (fmt::format ("{:>30}", time));

  std::cout << "\n--- 4. Locale-specific names (L) ---\n";
  show (fmt::format (std::locale::classic (), "{:L%A %d %B %Y}", time));

  std::cout << "\n--- 5. Wide output ---\n";
  std::wcout << L"  " << fmt::format (L"{:%F %T}", time) << L'\n';

  std::cout << "\n=== example_format_chrono finished ===\n";
  return 0;
}
