#include <cstddef>
#include <iostream>
#include <string>

#include "lumex/core/string_view/LumexStringView"

namespace
{
// A text parameter: by value, one overload for every standard.
std::size_t
count_slashes (lumex_string_view path)
{
  std::size_t count = 0;
  for (char const c : path)
    {
      if (c == '/')
        ++count;
    }
  return count;
}
}

int
main ()
{
  std::string const owned = "methods/gradient/fast.ini";
  std::cout << count_slashes ("methods/a.ini") << ' ' // a literal
            << count_slashes (owned) << ' '           // a std::string
            << count_slashes (lumex_string_view (owned.data (), 7)) << '\n';
}
