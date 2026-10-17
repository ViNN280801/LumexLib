// Built with -fno-exceptions (see CMakeLists.txt): uses at () and operator<<.
#include <sstream>

#include "lumex/core/string_view/LumexStringView"

#include "lib.hpp"

char
lib_first (lumex::core::string_view::view::lumex_string_view view)
{
  return view.empty () ? '\0' : view.at (0);
}

void
lib_print (std::ostream &out,
           lumex::core::string_view::view::lumex_string_view view)
{
  out << view;
}
