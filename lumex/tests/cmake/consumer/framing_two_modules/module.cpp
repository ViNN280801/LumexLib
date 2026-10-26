#include "Module.hpp"

namespace fixture
{
std::error_code
checksum_error ()
{
  return lumex::core::framing::codec::framing_errc::bad_checksum;
}

std::error_category const *
category_in_module ()
{
  return &lumex::core::framing::codec::framing_category ();
}
} // namespace fixture
