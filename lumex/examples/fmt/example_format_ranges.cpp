// example_format_ranges.cpp
// LumexFormat, part 3: ranges, sets, maps, std::pair and std::tuple
// (LumexFormatRanges.hpp) with the C++23 range specification
// [[fill]align][width][n][m | s | ?s][:element-specification].
#include <array>
#include <deque>
#include <iostream>
#include <list>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "lumex/core/fmt/LumexFormat"

namespace fmt = lumex::core::fmt;

namespace
{
/** A list type that prints as `<a | b | c>`. */
struct pipe_list_t
{
  std::vector<int> values;
};

/** Always prints its elements as zero-padded hex. */
struct register_dump_t
{
  std::vector<int> values;
};

void
show (std::string const &text)
{
  std::cout << "  " << text << '\n';
}
} // namespace

namespace lumex
{
namespace core
{
namespace fmt
{
/**
 * Reuses the range formatter of std::vector<int> and changes its brackets
 * and separator; the element specification still works (`{::#x}`).
 */
template <> class Formatter<pipe_list_t>
{
public:
  Formatter ()
  {
    _range.set_brackets ("<", ">");
    _range.set_separator (" | ");
  }

  char const *
  parse (FormatParseContext &ctx)
  {
    return _range.parse (ctx);
  }

  BasicAppender<char>
  format (pipe_list_t const &list, FormatContext &ctx) const
  {
    return _range.format (list.values, ctx);
  }

private:
  Formatter<std::vector<int>> _range;
};
/**
 * Keeps the range options of the field (brackets, width, `n`) and gives the
 * element formatter its own fixed specification through underlying ().
 */
template <> class Formatter<register_dump_t>
{
public:
  char const *
  parse (FormatParseContext &ctx)
  {
    char const *const end = _range.parse (ctx);
    FormatParseContext element_ctx ("#06x}");
    _range.underlying ().parse (element_ctx);
    return end;
  }

  BasicAppender<char>
  format (register_dump_t const &dump, FormatContext &ctx) const
  {
    return _range.format (dump.values, ctx);
  }

private:
  Formatter<std::vector<int>> _range;
};
} // namespace fmt
} // namespace core
} // namespace lumex

int
main ()
{
  std::cout << "=== LumexFormat: ranges and tuples ===\n";

  std::vector<int> const numbers = { 1, 2, 3 };
  std::cout << "\n--- 1. Sequences ---\n";
  show (fmt::format ("{}", numbers));
  show (fmt::format ("{}", std::list<double>{ 0.5, 1e20 }));
  show (fmt::format ("{}", std::array<int, 2>{ { 4, 5 } }));
  show (fmt::format ("{}", std::deque<bool>{ true, false }));
  show (fmt::format ("{}", std::vector<bool>{ true, false })); // proxy refs
  show (fmt::format ("{}", std::vector<int> ()));

  std::cout << "\n--- 2. Range specification ---\n";
  show (fmt::format ("{:n}", numbers));       // no brackets
  show (fmt::format ("{::#x}", numbers));     // element specification
  show (fmt::format ("{:n:02}", numbers));    // both
  show (fmt::format ("{:*^15}", numbers));    // fill/align/width of all
  show (fmt::format ("{:>{}}", numbers, 12)); // dynamic width
  show (fmt::format ("{::{}}", numbers, 3));  // dynamic element width

  std::cout << "\n--- 3. Strings and characters inside ranges ---\n";
  std::vector<std::string> const words = { "one", "tab\there" };
  show (fmt::format ("{}", words));   // debug (quoted) by default
  show (fmt::format ("{::}", words)); // plain with an element spec
  show (fmt::format ("{::>6}", words));
  std::vector<char> const chars = { 'o', 'k', '\n' };
  show (fmt::format ("{}", chars));
  show (fmt::format ("{:s}", std::vector<char>{ 'o', 'k' })); // as a string
  show (fmt::format ("{:?s}", chars));                        // debug string

  std::cout << "\n--- 4. Sets and maps ---\n";
  show (fmt::format ("{}", std::set<int>{ 3, 1, 2 }));
  std::map<std::string, int> const counts = { { "a", 1 }, { "b", 2 } };
  show (fmt::format ("{}", counts));
  show (fmt::format ("{:n}", counts));
  std::vector<std::pair<int, int>> const pairs = { { 1, 2 }, { 3, 4 } };
  show (fmt::format ("{}", pairs));
  show (fmt::format ("{:m}", pairs)); // pairs as a map

  std::cout << "\n--- 5. Nested ranges ---\n";
  std::vector<std::vector<int>> const matrix = { { 1, 2 }, { 3 } };
  show (fmt::format ("{}", matrix));
  show (fmt::format ("{::n}", matrix));
  show (fmt::format ("{:::#x}", matrix));
  std::map<int, std::vector<std::string>> const index = { { 1, { "x" } } };
  show (fmt::format ("{}", index));

  std::cout << "\n--- 6. pair and tuple ---\n";
  std::pair<int, std::string> const pair (1, "x");
  show (fmt::format ("{} {:n} {:m} {:>10}", pair, pair, pair, pair));
  std::tuple<int, char, double> const tuple (1, 'c', 2.5);
  show (fmt::format ("{} {:n}", tuple, tuple));
  int counter = 3;
  std::string label = "ref";
  show (fmt::format ("{}", std::tie (counter, label))); // references
  show (fmt::format ("{}", std::tuple<> ()));

  std::cout << "\n--- 7. Wide output ---\n";
  std::wcout << L"  " << fmt::format (L"{}", std::vector<std::wstring>{ L"w" })
             << L'\n';

  std::cout << "\n--- 8. Customized range formatter ---\n";
  pipe_list_t const list = { { 10, 11, 12 } };
  show (fmt::format ("{}", list));
  show (fmt::format ("{::#x}", list));
  register_dump_t const dump = { { 1, 255, 4096 } };
  show (fmt::format ("{}", dump));
  show (fmt::format ("{:n}", dump));

  std::cout << "\n=== example_format_ranges finished ===\n";
  return 0;
}
