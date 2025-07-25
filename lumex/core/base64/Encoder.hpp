#ifndef LUMREPORTGEN_BASE64_ENCODER_HPP
#define LUMREPORTGEN_BASE64_ENCODER_HPP

#include <vector>

#include "Base64.hpp"
#include "lumex/LumexExport.hpp"

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
     * @brief Provides Base64 encoding functionalities.
     *
     * This namespace encapsulates functions for converting binary data to Base64
     * strings.
     */
    namespace Base64
    {
      class LUMEX_API Encoder final
      {
      public:
        /**
         * @brief Encodes raw binary data into a Base64 string.
         *
         * This function takes a pointer to raw binary data and its size, then converts it
         * into a Base64-encoded string. The resulting string can be safely used for transmission
         * or storage in text-based formats.
         *
         * @param[in] data Pointer to the binary data to be encoded.
         * @param[in] size The size of the binary data in bytes.
         * @return A `std::string` containing the Base64-encoded representation of the input data.
         *         Returns an empty string if `data` is `nullptr` or `size` is `0`.
         */
        static std::string encode(void const *data, size_t size);

        /**
         * @brief Encodes binary data from a `std::vector<byte_type>` into a Base64 string.
         *
         * This is an overloaded function that provides a convenient way to encode binary data
         * stored in a `std::vector<byte_type>`. It internally calls the `encode` function
         * that takes a pointer and size.
         *
         * @param[in] data A `std::vector<byte_type>` containing the binary data to be encoded.
         * @return A `std::string` containing the Base64-encoded representation of the input vector's data.
         *         Returns an empty string if the input vector is empty.
         */
        static std::string encode(std::vector<byte_type> const &data);

#if __cplusplus >= 201703L
        /**
         * @brief Encodes binary data from a `std::string_view` into a Base64 string.
         *
         * This overload provides an efficient way to encode string data without copying,
         * leveraging `std::string_view` for read-only access to character sequences.
         *
         * @param[in] data A `std::string_view` containing the binary data to be encoded.
         * @return A `std::string` containing the Base64-encoded representation of the input data.
         *         Returns an empty string if the input `string_view` is empty.
         */
        static std::string encode(std::string_view data);
#endif

#if __cplusplus >= 202002L
        /**
         * @brief Encodes binary data from a `std::span<const byte_type>` into a Base64 string.
         *
         * This overload offers the most generic and efficient way to encode contiguous sequences of bytes
         * without ownership, suitable for C++20 and later.
         *
         * @param[in] data A `std::span<const byte_type>` containing the binary data to be encoded.
         * @return A `std::string` containing the Base64-encoded representation of the input data.
         *         Returns an empty string if the input `span` is empty.
         */
        static std::string encode(std::span<byte_type const> data);
#endif
      };
    } // namespace Base64
  } // namespace Core
} // namespace Lumex

#endif // !LUMREPORTGEN_BASE64_ENCODER_HPP
