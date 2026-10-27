#include "Module.hpp"

namespace fixture
{
std::error_code
invalid_value_error ()
{
  return lumex::core::protocol::error::protocol_errc::invalid_value;
}

std::error_category const *
category_in_module ()
{
  return &lumex::core::protocol::error::protocol_category ();
}
} // namespace fixture
