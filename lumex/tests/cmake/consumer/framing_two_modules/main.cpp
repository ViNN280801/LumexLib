// One framing category for the whole process: the error code the library
// makes equals the enumerator in the executable, and both modules see the same
// category object. Exit codes: 0 fine, 1..3 the numbered check failed.

#include <cstdio>
#include <system_error>

#include "Module.hpp"

namespace codec = lumex::core::framing::codec;

int
main ()
{
  if (fixture::category_in_module () != &codec::framing_category ())
    return 1;

  std::error_code const error = fixture::checksum_error ();
  if (!(error == codec::framing_errc::bad_checksum))
    return 2;
  if (error.message ()
      != std::error_code (codec::framing_errc::bad_checksum).message ())
    return 3;

  std::printf ("framing_two_modules: one category for all\n");
  return 0;
}
