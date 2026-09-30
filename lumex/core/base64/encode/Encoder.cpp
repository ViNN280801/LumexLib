#define LUMEX_IMPLEMENTATION
#include <cstddef>
#include <string>
#include <vector>

#include "Encoder.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

using namespace lumex::core::base64::encode;
using namespace lumex::core::base64::codec;
using namespace lumex::core::base64::codec::Types;
using namespace lumex::core::base64::codec::Constants;
using namespace lumex::core::base64::codec::detail;

namespace
{
/// @brief Read-only view of the caller's bytes for `detail::_encode_impl`.
/// The same in every standard, so the exported function does not depend on
/// the standard the library is built with.
struct byte_range_t
{
  byte_type const *first;
  std::size_t count;

  std::size_t
  size () const LUMEX_NOEXCEPT
  {
    return count;
  }

  bool
  empty () const LUMEX_NOEXCEPT
  {
    return count == 0;
  }

  byte_type
  operator[] (std::size_t index) const LUMEX_NOEXCEPT
  {
    return first[index];
  }
};
} // namespace

LUMEX_PUBLIC_API
std::string
Encoder::encode (void const *data, std::size_t size)
{
  if (data == nullptr || size == 0)
    return {};

  byte_range_t const bytes = { static_cast<byte_type const *> (data), size };
  return detail::_encode_impl (bytes);
}

LUMEX_PUBLIC_API
std::string
Encoder::encode (std::vector<byte_type> const &data)
{
  return detail::_encode_impl (data);
}
