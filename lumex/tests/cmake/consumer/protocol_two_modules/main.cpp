// One protocol category for the whole process: the error code the library
// makes equals the enumerator in the executable, and both modules see the same
// category object. Exit codes: 0 fine, 1..3 the numbered check failed.

#include <cstdio>
#include <system_error>

#include "Module.hpp"

namespace error = lumex::core::protocol::error;

int
main ()
{
  if (fixture::category_in_module () != &error::protocol_category ())
    return 1;

  std::error_code const code = fixture::invalid_value_error ();
  if (!(code == error::protocol_errc::invalid_value))
    return 2;
  if (code.message ()
      != std::error_code (error::protocol_errc::invalid_value).message ())
    return 3;

  std::printf ("protocol_two_modules: one category for all\n");
  return 0;
}
