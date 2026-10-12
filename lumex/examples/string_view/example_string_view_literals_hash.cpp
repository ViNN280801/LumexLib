#include <iomanip>
#include <iostream>
#include <string>
#include <unordered_map>

#include "lumex/core/string_view/LumexStringView"

using namespace lumex::core::string_view::view::literals;

namespace
{
// Evaluated by the compiler, in C++11 as well.
constexpr lumex_string_view method = "isocratic.ini"_sv;
static_assert (method.find ('.') == 9 && method.ends_with (".ini")
                   && method.contains ("crat"),
               "constant evaluation");
}

int
main ()
{
  // std::hash: a view is a key, and an equal text found elsewhere finds it.
  std::unordered_map<lumex_string_view, int> priority;
  priority["isocratic.ini"_sv] = 1;
  priority["gradient.ini"_sv] = 2;
  std::string const name = "gradient.ini";
  int const found = priority.at (lumex_string_view (name));

  std::cout << found << ' ' << method.contains ("crat") << ' ';
  // operator<< honors the width and the fill, and resets the width.
  std::cout << '[' << std::setw (8) << std::setfill ('*') << "ab"_sv << "]\n";
}
