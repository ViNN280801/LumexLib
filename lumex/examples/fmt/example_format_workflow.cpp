#include <chrono>
#include <iostream>
#include <string>
#include <vector>

#include "lumex/core/fmt/LumexFormat"

namespace fmt = lumex::core::fmt;

namespace
{
struct peak_t
{
  std::string name;
  double retention_minutes;
  double area;
};

/** A user type: reuse the double specification for the area. */
struct area_t
{
  double value;
};
} // namespace

namespace lumex
{
namespace core
{
namespace fmt
{
template <> class Formatter<area_t> : public Formatter<double>
{
public:
  BasicAppender<char>
  format (area_t const &area, FormatContext &ctx) const
  {
    return Formatter<double>::format (area.value, ctx);
  }
};
} // namespace fmt
} // namespace core
} // namespace lumex

int
main ()
{
  std::cout << "=== Workflow: a measurement report and safe log lines ===\n\n";

  std::vector<peak_t> const peaks = { { "caffeine", 3.215, 15234.5 },
                                      { "theobromine", 4.87, 812.25 },
                                      { "theophylline", 5.1, 96.0 } };

  // A table with aligned columns; the header and the rows share widths.
  std::cout << fmt::format ("{:<14}|{:>10}|{:>12}\n", "peak", "RT, min",
                            "area");
  std::cout << fmt::format ("{:-<14}+{:-<10}+{:-<12}\n", "", "", "");
  for (peak_t const &peak : peaks)
    std::cout << fmt::format ("{:<14}|{:>10.3f}|{:>12.1f}\n", peak.name,
                              peak.retention_minutes, area_t{ peak.area });

  // Named fields keep long templates readable.
  std::cout << '\n'
            << fmt::format ("run {run} took {elapsed} ({elapsed:%M:%S})",
                            fmt::arg ("run", 17),
                            fmt::arg ("elapsed", std::chrono::seconds (754)))
            << '\n';

  // A format string that comes from configuration is checked at run time.
  std::string const from_config = "sample {} at {:>6.2f} bar";
  std::cout << fmt::format (fmt::runtime (from_config), "S-01", 101.5) << '\n';

  // try_format never throws: a logger can use it with untrusted templates.
  fmt::try_format_result_t<char> const bad
      = fmt::try_format (fmt::runtime ("{} {} {}"), "only one");
  std::cout << "try_format: success=" << bad.success << " error=\""
            << bad.error << "\"\n";

  std::cout << "\n=== workflow finished ===\n";
  return 0;
}
