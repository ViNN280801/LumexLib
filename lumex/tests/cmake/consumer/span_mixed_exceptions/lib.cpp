// Built with -fno-exceptions (see CMakeLists.txt): at () with an index out of
// range must terminate. The span<int> members are the same entities as those
// of main.cpp, which is built with exceptions.
#include <array>

#include "lumex/core/span/LumexSpan"

#include "lib.hpp"

namespace sp = lumex::core::span::view;

int
lib_at_out_of_range (int overload)
{
  std::array<int, 3> array = { { 1, 2, 3 } };
  sp::span<int> const dynamic (array.data (), array.size ());
  sp::span<int const> const const_elements (array.data (), array.size ());
  sp::span<int, 3> const fixed (array);
  switch (overload)
    {
    case 0:
      return dynamic.at (3);
    case 1:
      return dynamic.at (99);
    case 2:
      return const_elements.at (3);
    default:
      return fixed.at (3);
    }
}
