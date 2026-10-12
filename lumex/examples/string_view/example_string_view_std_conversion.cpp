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
  // The hash of a view is the same in every standard (it is not the hash of
  // std::string_view, which depends on the standard library).
  std::size_t const hash = std::hash<lumex_string_view> () (own);
  std::string const copy = own.to_string (); // an owning copy is explicit
  std::cout << same << ' '
            << (hash
                == std::hash<lumex_string_view> () (lumex_string_view (back)))
            << ' ' << copy.size () << '\n';
}
