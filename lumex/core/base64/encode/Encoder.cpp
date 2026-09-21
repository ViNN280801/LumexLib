#define LUMEX_IMPLEMENTATION
#include "Encoder.hpp"

using namespace lumex::core::base64::encode;
using namespace lumex::core::base64::codec;
using namespace lumex::core::base64::codec::Types;
using namespace lumex::core::base64::codec::Constants;
using namespace lumex::core::base64::codec::detail;

LUMEX_PUBLIC_API
std::string
Encoder::encode (void const *data, std::size_t size)
{
  if (data == nullptr || size == 0)
    return {};

#if __cplusplus >= 202002L
  return detail::_encode_impl (std::span<const byte_type> (
      static_cast<const byte_type *> (data), size));
#elif __cplusplus >= 201703L
  // For generic binary data, string_view is less ideal as it implies text,
  // but it's the closest non-owning view in C++17.
  // Cast to char* is safe for read-only operations.
  return detail::_encode_impl (
      std::string_view (static_cast<char const *> (data), size));
#else
  // Convert the raw data into a std::vector for safer, bounds-checked access,
  // thereby avoiding direct pointer arithmetic on 'bytes'.
  std::vector<byte_type> bytes_vec (
      static_cast<byte_type const *> (data),
      static_cast<byte_type const *> (data)
          + size); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  return detail::_encode_impl (bytes_vec);
#endif // __cplusplus >= 201703L
}

#if __cplusplus >= 201703L
LUMEX_PUBLIC_API
std::string
Encoder::encode (std::string_view data)
{
  // Reinterpret_cast is safe here for read-only access to underlying bytes
  // as _encode_impl expects byte_type access.
  return detail::_encode_impl (data);
}
#endif

#if __cplusplus >= 202002L
LUMEX_PUBLIC_API
std::string
Encoder::encode (std::span<byte_type const> data)
{
  return detail::_encode_impl (data);
}
#endif

LUMEX_PUBLIC_API
std::string
Encoder::encode (std::vector<byte_type> const &data)
{
  return detail::_encode_impl (data);
}
