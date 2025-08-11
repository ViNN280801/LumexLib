/**
 * @file  Base64.hpp
 * @brief Cross-platform Base64 encoding and decoding utilities.
 *
 * @link  https://en.wikipedia.org/wiki/Base64
 * @link  https://renenyffenegger.ch/notes/development/Base64/Encoding-and-decoding-base-64-with-cpp/
 *
 * This header provides a set of functions for converting binary data to its Base64 string
 * representation and vice versa. The implementation adheres to standard Base64 principles,
 * suitable for various data serialization and transmission needs. It supports encoding
 * from raw pointers or `std::vector<byte>` and decoding into `std::vector<byte>`.
 *
 * IMPORTANT: If the user wants to encode/decode text in a specific encoding, the task of converting this encoding
 * (e.g., from UTF-8 to UTF-16 or vice versa, or from std::wstring to UTF-8 bytes) must be solved before
 * calling the Base64 encoding functions and after calling the Base64 decoding functions. This is a separate
 * layer of logic that should not be part of the Base64 library itself. Base64 is a low-level utility for bytes.
 */

#ifndef LUMEX_BASE64_HPP
#define LUMEX_BASE64_HPP

#include <array>
#include <string>
#include <type_traits>

#if __cplusplus >= 201703L
  #include <string_view>
#endif

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Core
  {
    namespace Base64
    {
      namespace Types
      {
        using byte_type = unsigned char;

#if __cplusplus >= 201703L
        using string_type_t = std::string_view;
#else
        using string_type_t = std::string const &;
#endif
      } // namespace Types

      namespace Constants
      {
        constexpr Types::byte_type kBase64DecodeInvalidChar = 0xFF; // Represents 0b11111111 for invalid characters
        constexpr Types::byte_type kBase64MaskSixBits       = 0x3F; // Represents 0b00111111 for masking 6 bits
        constexpr Types::byte_type kBase64MaskFourBits      = 0x0F; // Represents 0b00001111 for masking 4 bits
        constexpr Types::byte_type kBase64MaskTwoBits       = 0x03; // Represents 0b00000011 for masking 2 bits
        constexpr Types::byte_type kBase64RightShiftSixBits = 0x06; // Represents 0b00000110 for right shifting 6 bits
      } // namespace Constants

      namespace detail
      {
        /// @brief Base64 alphabet.
        std::array<char, 64> const _base64_chars = {
          'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V',
          'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r',
          's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'};

        /// @brief Decode table - maps ASCII chars to base64 values.
        std::array<Types::byte_type, 256> const _decode_table
          = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
             0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
             0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x3E, 0xFF, 0xFF, 0xFF, 0x3F, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
             0x3A, 0x3B, 0x3C, 0x3D, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
             0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
             0x19, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24,
             0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0xFF, 0xFF, 0xFF,
             0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
             0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
             0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
             0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
             0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
             0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
             0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
             0xFF, 0xFF, 0xFF, 0xFF};

        /**
         * @brief Internal helper to check if a character is a valid Base64 *alphabet* character.
         *        This excludes the padding '=' character.
         *
         * @param chr The character to check.
         * @return true if the character is a valid Base64 alphabet character, false otherwise.
         */
        inline bool
        _is_base64_char_impl(Types::byte_type chr) noexcept
        {
          // A character is a valid Base64 alphabet character if its decoded value is not kBase64DecodeInvalidChar.
          // This function specifically excludes '=' as a valid alphabet character, as '=' is padding.
          return _decode_table.at(chr) != Constants::kBase64DecodeInvalidChar;
        }

        // C++11 compatible void_t replacement
        template <typename...> struct void_t_impl {
          using type = void;
        };
        template <typename... Ts> using void_t = typename void_t_impl<Ts...>::type;

        // C++11 compatible enable_if_t replacement
        template <bool B, typename T = void> using enable_if_t = typename std::enable_if<B, T>::type;

        /**
         * @brief Primary template for `has_convertible_size` trait.
         *
         * This template serves as the default, base case for the `has_convertible_size`
         * type trait. By default, it inherits from `std::false_type`, indicating that
         * a given type `T` is assumed *not* to possess a callable `.size()` method
         * whose return type is convertible to `size_t`. This setup is fundamental
         * to the SFINAE (Substitution Failure Is Not An Error) pattern, where the
         * compiler attempts to match more specialized templates first. If no more
         * specialized template can be successfully instantiated (due to substitution
         * failures in their template parameters), this general template is selected,
         * resulting in a compile-time `false` value.
         *
         * @tparam T The type to be inspected for the presence and convertibility of its `.size()` method.
         * @tparam = void An unused parameter, typically used in SFINAE to enable or disable
         *         template specializations based on the validity of expressions within `void_t`.
         *         In this primary template, it simply completes the template signature.
         */
        template <typename T, typename = void> struct has_convertible_size : std::false_type {};

        /**
         * @brief Specialization of `has_convertible_size` for types with a suitable `.size()` method.
         *
         * This template specialization is enabled via SFINAE when `T` provides a `.size()`
         * member function whose return type is convertible to `size_t`.
         *
         * - `void_t<decltype(std::declval<T>().size())>`: Checks if `T::size()` is a valid
         *   expression. `std::declval<T>()` provides a `T` object without requiring a default
         *   constructor, enabling compile-time checks on its member functions.
         * - `enable_if_t<std::is_convertible<decltype(std::declval<T>().size()), size_t>::value>>`:
         *   Further constrains the specialization, ensuring that the return type of `T::size()`
         *   can be implicitly converted to `size_t`. If either of these conditions fails,
         *   this specialization is discarded from the overload set, and the primary template
         *   (inheriting from `std::false_type`) is chosen instead.
         *
         * If this specialization is chosen, it inherits from `std::true_type`, indicating
         * that the type `T` meets the specified criteria.
         *
         * @tparam T The type being checked.
         */
        template <typename T>
        struct has_convertible_size<
          T, void_t<decltype(std::declval<T>().size()),
                    enable_if_t<std::is_convertible<decltype(std::declval<T>().size()), size_t>::value>>>
            : std::true_type {};

        /**
         * @brief Primary template for `has_convertible_indexed_access` trait.
         *
         * This is the default, base case for the `has_convertible_indexed_access` type trait.
         * By default, it inherits from `std::false_type`, signifying that a type `T` is
         * assumed *not* to have a callable `operator[]` that accepts `size_t` as an index
         * and whose return type is convertible to `byte_type`. This is part of the SFINAE
         * mechanism, where more specialized templates are preferred if their template
         * arguments can be successfully substituted. If substitution fails for all
         * specializations, this general template is used, resulting in a compile-time `false`.
         *
         * @tparam T The type to be checked for `operator[]` and its return type convertibility.
         * @tparam = void An unused SFINAE-enabling parameter that completes the template signature.
         */
        template <typename T, typename = void> struct has_convertible_indexed_access : std::false_type {};

        /**
         * @brief Specialization of `has_convertible_indexed_access` for types with suitable `operator[]`.
         *
         * This template specialization is actively selected by the compiler if the type `T`         * satisfies the
         * following conditions, verified through SFINAE:
         *
         * - `void_t<decltype(std::declval<T>()[std::declval<size_t>()])>`: Checks if `T::operator[]`
         *   is a valid expression when invoked with a `size_t` argument. `std::declval<size_t>()`
         *   provides a `size_t` value for compile-time expression evaluation.
         * - `enable_if_t<std::is_convertible<decltype(std::declval<T>()[std::declval<size_t>()]),
         *   Types::byte_type>::value>>>`: Ensures that the return type of `T::operator[](size_t)`
         *   is implicitly convertible to `Lumex::Utility::Base64::Types::byte_type`. This is critical
         *   for ensuring that the indexed access yields a byte-compatible value.
         *
         * If both conditions are met, this specialization is chosen, and it inherits from
         * `std::true_type`, confirming that `T` supports convertible byte access via `operator[]`.
         * Otherwise, the primary `has_convertible_indexed_access` template is used.
         *
         * @tparam T The type being checked.
         */
        template <typename T>
        struct has_convertible_indexed_access<
          T, void_t<decltype(std::declval<T>()[std::declval<size_t>()]), // Check with size_t as index
                    enable_if_t<std::is_convertible<decltype(std::declval<T>()[std::declval<size_t>()]),
                                                    Types::byte_type>::value>>> : std::true_type {};

        /**
         * @brief Internal helper to encode binary data into a Base64 string from a view-like type.
         * @tparam T A type that provides `operator[]` for byte access and `size()` for length.
         * @param data_view The view-like object containing binary data.
         * @return A `std::string` containing the Base64-encoded representation.
         */
        template <typename T>
        std::string
        _encode_impl(T const &data_view)
        {
          static_assert(
            has_convertible_size<T>::value,
            "Encoding data requires a type with a .size() method whose return type is convertible to size_t.");
          static_assert(has_convertible_indexed_access<T>::value,
                        "Encoding data requires a type with an operator[] that accepts size_t as an index and returns "
                        "a type convertible to byte_type.");

          if(data_view.empty()) return {};

          // Calculate output length and reserve space
          std::string result;
          size_t out_length{4 * ((data_view.size() + 2) / 3)};
          result.reserve(out_length);

          // 1. Process 3-byte chunks
          size_t pos{};
          while(pos + 2 < data_view.size())
          {
            // 1.1. Get the first 6 bits of the first byte
            result += detail::_base64_chars.at((data_view[pos] >> 2) & Constants::kBase64MaskSixBits);

            // 1.2. Get the last 2 bits of the first byte and the first 4 bits of the second byte
            result += detail::_base64_chars.at(((data_view[pos] & Constants::kBase64MaskTwoBits) << 4)
                                               | ((data_view[pos + 1] >> 4) & Constants::kBase64MaskFourBits));

            // 1.3. Get the last 4 bits of the second byte and the first 2 bits of the third byte
            result += detail::_base64_chars.at(
              ((data_view[pos + 1] & Constants::kBase64MaskFourBits) << 2)
              | ((data_view[pos + 2] >> Constants::kBase64RightShiftSixBits) & Constants::kBase64MaskTwoBits));

            // 1.4. Get the last 6 bits of the third byte
            result += detail::_base64_chars.at(data_view[pos + 2] & Constants::kBase64MaskSixBits);

            // 1.5. Move to the next 3 bytes to process
            pos += 3;
          }

          // 2. Handle remaining bytes
          if(pos < data_view.size())
          {
            // 2.1. Get the first 6 bits of the first byte
            result += detail::_base64_chars.at((data_view[pos] >> 2) & Constants::kBase64MaskSixBits);

            if(pos + 1 < data_view.size())
            {
              // 2.1.1. Get the last 2 bits of the first byte and the first 4 bits of the second byte
              result += detail::_base64_chars.at(((data_view[pos] & Constants::kBase64MaskTwoBits) << 4)
                                                 | ((data_view[pos + 1] >> 4) & Constants::kBase64MaskFourBits));

              // 2.1.2. Get the last 4 bits of the second byte
              result += detail::_base64_chars.at((data_view[pos + 1] & Constants::kBase64MaskFourBits) << 2);

              // 2.1.3. Add padding
              result += '=';
            }
            else
            {
              // 2.2.1. Get the last 2 bits of the first byte and the first 4 bits of the second byte
              result += detail::_base64_chars.at((data_view[pos] & Constants::kBase64MaskTwoBits) << 4);

              // 2.2.2. Add double padding, because there is only one byte left
              result += "==";
            }
          }

          // 3. Return the result
          return result;
        }
      } // namespace detail
    } // namespace Base64
  } // namespace Core
} // namespace Lumex

#endif // !LUMEX_BASE64_HPP
