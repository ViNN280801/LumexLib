#include <cstddef>
#include <functional>
#include <iostream>
#include <string>
#include <string_view>

#include "lumex/core/string_view/LumexStringView"

int
main ()
{
  std::string_view standard = "methods/isocratic.ini";
  lumex_string_view own = standard;   // implicit, no copy of the text
  std::string_view back = own;        // implicit
  bool const same = (own == standard) // compares like two views
                    && !(own != back) && (own < lumex_string_view ("z"));
  // The view has no std::hash: hash the standard view it converts to.
  std::size_t const hash = std::hash<std::string_view> () (own);
  std::string const copy = own.to_string (); // an owning copy is explicit
  std::cout << same << ' ' << (hash == std::hash<std::string_view> () (back))
            << ' ' << copy.size () << '\n';
}
