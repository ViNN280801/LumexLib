#ifndef LUMEX_BASE64_DECODER_HPP
#define LUMEX_BASE64_DECODER_HPP

#include <vector>

#include "lumex/LumexExport.hpp"
#include "lumex/core/base64/Base64.hpp"

using namespace Lumex::Core::Base64::Types;

#if __cplusplus >= 201703L
  #include <string_view>
#endif

#if __cplusplus >= 202002L
  #include <span>
#endif

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Core
  {
    /**
     * @brief Provides Base64 decoding functionalities.
     *
     * This namespace encapsulates functions for converting Base64 strings to binary data.
     */
    namespace Base64
    {
      class LUMEX_API Decoder final
      {
      public:
        /**
         * @brief Decodes a Base64 string into binary data and stores it in an output vector.
         *
         * This function attempts to decode the given Base64 string. The decoded binary data
         * is appended to the provided `out` vector. Before appending, the `out` vector is
         * cleared to ensure it only contains the result of the current decoding operation.
         *
         * @param[in] encoded The Base64-encoded input string.
         * @param[out] out A `std::vector<byte_type>` that will store the decoded binary data.
         *                 It is cleared at the beginning of the function.
         * @return `true` if the decoding was successful and the input string was a valid Base64 format,
         *         `false` otherwise (e.g., invalid length, contains non-Base64 characters, or incorrect padding).
         */
#if __cplusplus >= 201703L
        static bool decode(std::string_view encoded, std::vector<byte_type> &out);
#else
        static bool decode(std::string const &encoded, std::vector<byte_type> &out);
#endif

        /**
         * @brief Decodes a Base64 string into binary data and returns it as a `std::vector<byte_type>`.
         *
         * This is an overloaded function that provides a convenient way to decode a Base64 string
         * and receive the result directly as a returned `std::vector<byte_type>`.
         *
         * @param[in] encoded The Base64-encoded input string.
         * @return A `std::vector<byte_type>` containing the decoded binary data.
         *         Returns an empty vector if the decoding fails or the input string is invalid.
         */
#if __cplusplus >= 201703L
        static std::vector<byte_type> decode(std::string_view encoded);
#else
        static std::vector<byte_type> decode(std::string const &encoded);
#endif
      };
    } // namespace Base64
  } // namespace Core
} // namespace Lumex

#endif // !LUMEX_BASE64_DECODER_HPP
