// Built with -fno-exceptions (see CMakeLists.txt): value () of an expected
// that holds an error must terminate.
#include <utility>

#include "lumex/core/expected/Expected"

#include "lib.hpp"

namespace lx = lumex::core::expected::result;

int
lib_value_of_error (int overload)
{
  lx::expected<int, int> error (lx::unexpect, 1);
  lx::expected<int, int> const const_error (lx::unexpect, 1);
  lx::expected<void, int> void_error (lx::unexpect, 1);
  switch (overload)
    {
    case 0:
      return error.value ();
    case 1:
      return std::move (error).value ();
    case 2:
      return const_error.value ();
    case 3:
      return std::move (const_error).value ();
    case 4:
      static_cast<lx::expected<void, int> const &> (void_error).value ();
      return 0;
    default:
      std::move (void_error).value ();
      return 0;
    }
}
