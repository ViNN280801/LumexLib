// Built with -fno-exceptions (see CMakeLists.txt): value () of an empty
// optional must terminate. The optional<int> members are the same entities as
// those of main.cpp, which is built with exceptions.
#include <utility>

#include "lumex/core/optional/opt/LumexOptional.hpp"

#include "lib.hpp"

int
lib_value_of_empty (int overload)
{
  lumex::core::optional::opt::optional<int> empty;
  lumex::core::optional::opt::optional<int> const const_empty;
  switch (overload)
    {
    case 0:
      return empty.value ();
    case 1:
      return std::move (empty).value ();
    case 2:
      return const_empty.value ();
    default:
      return std::move (const_empty).value ();
    }
}
