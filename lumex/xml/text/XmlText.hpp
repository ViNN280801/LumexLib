#ifndef LUMEX_XML_TEXT_HPP
#define LUMEX_XML_TEXT_HPP

#include "lumex/LumexExport.hpp"

#include "lumex/core/utility/LumexAttributes.hpp"

#include "lumex/xml/types/XmlTypes.hpp"

using namespace Lumex::Xml::Types;

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    // Forward declaration
    namespace Node
    {
      class XmlNode;
      struct XmlNodeBase;
    }
    using namespace Node;

    namespace Text
    {
      /**
       * @brief Represents the text content of an XML node, providing type-safe access and modification.
       * @details `XmlText` acts as a facade over internal `node_pcdata` or `node_cdata` nodes,
       *          or the value of element nodes (if `kparse_embed_pcdata` was used during parsing).
       *          It allows retrieving the text as various C++ types (int, double, bool, string) and setting it
       *          with automatic type conversion.
       * @note This object does not own the memory for the text content; its lifetime is tied to the `XmlDocument`
       *       from which its underlying node originates.
       * @see XmlNode::text()
       */
      class LUMEX_API XmlText
      {
        friend class Node::XmlNode;

      public:
        /// @brief Type definition for safe boolean conversion, preventing problematic implicit conversions.
        using unspecified_bool_type = void (*)(XmlText ***);

        /**
         * @brief Default constructor. Constructs an empty `XmlText` object.
         * @details An empty `XmlText` object does not point to any valid text content.
         */
        XmlText();

        /**
         * @brief Safe boolean conversion operator.
         * @details Allows an `XmlText` object to be used in boolean contexts (e.g., `if (text)`).
         *          It evaluates to `true` if the `XmlText` object points to valid text content, and `false` otherwise.
         * @return A pointer to a dummy function if the internal text data is not null, otherwise `nullptr`.
         */
        operator unspecified_bool_type() const;

        /**
         * @brief Logical NOT operator.
         * @details Returns `true` if the `XmlText` object is empty (does not point to valid text content), `false` otherwise.
         * @return `true` if the text object is empty, `false` otherwise.
         */
        bool operator!() const;

        /**
         * @brief Checks if the text object is empty (null).
         * @return `true` if the text object is empty, `false` otherwise.
         */
        LUMEX_ATTRIBUTE_NODISCARD("The returned boolean indicates whether the text object is empty; discarding it "
                                  "negates the purpose of the getter.")
        bool empty() const;

        /**
         * @brief Retrieves the text content as a C-style string.
         * @return A null-terminated C-style string representing the text content, or `LUMEX_XML_TEXT("")` if the object is empty.
         * @note The returned pointer points to internal memory and should not be deallocated or modified. Its lifetime is tied to the XML document.
         */
        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned C-style string text should be used; discarding it negates the purpose of the getter.")
        char_t const *get() const;

        /**
         * @brief Retrieves the text content as a C-style string, or a default value if empty.
         * @param[in] def The default C-style string to return if the text object is empty. Defaults to `LUMEX_XML_TEXT("")`.
         * @return A null-terminated C-style string representing the text content, or `def` if the object is empty.
         */
        char_t const *as_string(char_t const *def = LUMEX_XML_TEXT("")) const;

        /**
         * @brief Converts the text content to an integer.
         * @param[in] def The default integer value to return if conversion fails or the object is empty. Defaults to `0`.
         * @return The text content as an `int`, or `def` if conversion is not possible or the object is empty.
         */
        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned integer value should be used; discarding it negates the purpose of the getter.")
        int as_int(int def = 0) const;

        /**
         * @brief Converts the text content to an unsigned integer.
         * @param[in] def The default unsigned integer value to return if conversion fails or the object is empty. Defaults to `0`.
         * @return The text content as an `unsigned int`, or `def` if conversion is not possible or the object is empty.
         */
        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned unsigned integer value should be used; discarding it negates the purpose of the getter.")
        unsigned int as_uint(unsigned int def = 0) const;

        /**
         * @brief Converts the text content to a double-precision floating-point number.
         * @param[in] def The default double value to return if conversion fails or the object is empty. Defaults to `0.0`.
         * @return The text content as a `double`, or `def` if conversion is not possible or the object is empty.
         */
        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned double value should be used; discarding it negates the purpose of the getter.")
        double as_double(double def = 0) const;

        /**
         * @brief Converts the text content to a single-precision floating-point number.
         * @param[in] def The default float value to return if conversion fails or the object is empty. Defaults to `0.0f`.
         * @return The text content as a `float`, or `def` if conversion is not possible or the object is empty.
         */
        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned float value should be used; discarding it negates the purpose of the getter.")
        float as_float(float def = 0) const;

        /**
         * @brief Converts the text content to a long long integer.
         * @param[in] def The default long long value to return if conversion fails or the object is empty. Defaults to `0LL`.
         * @return The text content as a `long long`, or `def` if conversion is not possible or the object is empty.
         */
        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned long long value should be used; discarding it negates the purpose of the getter.")
        long long as_llong(long long def = 0) const;

        /**
         * @brief Converts the text content to an unsigned long long integer.
         * @param[in] def The default unsigned long long value to return if conversion fails or the object is empty. Defaults to `0ULL`.
         * @return The text content as an `unsigned long long`, or `def` if conversion is not possible or the object is empty.
         */
        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned unsigned long long value should be used; discarding it negates the purpose of the getter.")
        unsigned long long as_ullong(unsigned long long def = 0) const;

        /**
         * @brief Converts the text content to a boolean.
         * @details Returns `true` if the first character of the text is '1', 't', 'T', 'y', or 'Y' (case-insensitive).
         * @param[in] def The default boolean value to return if the object is empty. Defaults to `false`.
         * @return The text content as a `bool`, or `def` if the object is empty.
         */
        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned boolean value should be used; discarding it negates the purpose of the getter.")
        bool as_bool(bool def = false) const;

        /**
         * @brief Sets the text content from a null-terminated C-style string.
         * @param[in] rhs The new text content.
         * @return `true` if the text was successfully set, `false` otherwise (e.g., if the object is empty or there is insufficient memory).
         * @details This function handles memory allocation/reallocation for the text string.
         */
        bool set(char_t const *rhs);
        /**
         * @brief Sets the text content from a C-style string with a specified size.
         * @param[in] rhs The new text content.
         * @param[in] size The number of characters in `rhs` (excluding null terminator).
         * @return `true` if the text was successfully set, `false` otherwise.
         * @details This function handles memory allocation/reallocation for the text string.
         */
        bool set(char_t const *rhs, size_t size);
#if __cplusplus >= 201703L
        /**
         * @brief Sets the text content from a `std::string_view`.
         * @param[in] rhs The new text content as a `std::string_view`.
         * @return `true` if the text was successfully set, `false` otherwise.
         * @note Available only when compiled with C++17 or later.
         */
        bool set(string_view_t rhs);
#endif

        /**
         * @brief Sets the text content from an integer, converting it to a string.
         * @param[in] rhs The integer value.
         * @return `true` if the text was successfully set, `false` otherwise.
         */
        bool set(int rhs);
        /**
         * @brief Sets the text content from an unsigned integer, converting it to a string.
         * @param[in] rhs The unsigned integer value.
         * @return `true` if the text was successfully set, `false` otherwise.
         */
        bool set(unsigned int rhs);
        /**
         * @brief Sets the text content from a long integer, converting it to a string.
         * @param[in] rhs The long integer value.
         * @return `true` if the text was successfully set, `false` otherwise.
         */
        bool set(long rhs);
        /**
         * @brief Sets the text content from an unsigned long integer, converting it to a string.
         * @param[in] rhs The unsigned long integer value.
         * @return `true` if the text was successfully set, `false` otherwise.
         */
        bool set(unsigned long rhs);
        /**
         * @brief Sets the text content from a double-precision floating-point number, converting it to a string.
         * @param[in] rhs The double value.
         * @return `true` if the text was successfully set, `false` otherwise.
         */
        bool set(double rhs);
        /**
         * @brief Sets the text content from a double-precision floating-point number with a specified precision.
         * @param[in] rhs The double value.
         * @param[in] precision The number of digits after the decimal point.
         * @return `true` if the text was successfully set, `false` otherwise.
         */
        bool set(double rhs, int precision);
        /**
         * @brief Sets the text content from a single-precision floating-point number, converting it to a string.
         * @param[in] rhs The float value.
         * @return `true` if the text was successfully set, `false` otherwise.
         */
        bool set(float rhs);
        /**
         * @brief Sets the text content from a single-precision floating-point number with a specified precision.
         * @param[in] rhs The float value.
         * @param[in] precision The number of digits after the decimal point.
         * @return `true` if the text was successfully set, `false` otherwise.
         */
        bool set(float rhs, int precision);
        /**
         * @brief Sets the text content from a boolean, converting it to "true" or "false".
         * @param[in] rhs The boolean value.
         * @return `true` if the text was successfully set, `false` otherwise.
         */
        bool set(bool rhs);

        /**
         * @brief Sets the text content from a long long integer, converting it to a string.
         * @param[in] rhs The long long integer value.
         * @return `true` if the text was successfully set, `false` otherwise.
         */
        bool set(long long rhs);
        /**
         * @brief Sets the text content from an unsigned long long integer, converting it to a string.
         * @param[in] rhs The unsigned long long integer value.
         * @return `true` if the text was successfully set, `false` otherwise.
         */
        bool set(unsigned long long rhs);

        /**
         * @brief Assignment operator for a C-style string.
         * @param[in] rhs The null-terminated C-style string to assign.
         * @return A reference to the modified `XmlText` object.
         * @details Equivalent to `set(rhs)` but without error checking in the return value.
         */
        XmlText &operator=(char_t const *rhs);
        /**
         * @brief Assignment operator for an integer.
         * @param[in] rhs The integer to assign.
         * @return A reference to the modified `XmlText` object.
         * @details Converts the integer to a string and assigns it.
         */
        XmlText &operator=(int rhs);
        /**
         * @brief Assignment operator for an unsigned integer.
         * @param[in] rhs The unsigned integer to assign.
         * @return A reference to the modified `XmlText` object.
         * @details Converts the unsigned integer to a string and assigns it.
         */
        XmlText &operator=(unsigned int rhs);
        /**
         * @brief Assignment operator for a long integer.
         * @param[in] rhs The long integer to assign.
         * @return A reference to the modified `XmlText` object.
         * @details Converts the long integer to a string and assigns it.
         */
        XmlText &operator=(long rhs);
        /**
         * @brief Assignment operator for an unsigned long integer.
         * @param[in] rhs The unsigned long integer to assign.
         * @return A reference to the modified `XmlText` object.
         * @details Converts the unsigned long integer to a string and assigns it.
         */
        XmlText &operator=(unsigned long rhs);
        /**
         * @brief Assignment operator for a double-precision floating-point number.
         * @param[in] rhs The double value to assign.
         * @return A reference to the modified `XmlText` object.
         * @details Converts the double to a string and assigns it.
         */
        XmlText &operator=(double rhs);
        /**
         * @brief Assignment operator for a single-precision floating-point number.
         * @param[in] rhs The float value to assign.
         * @return A reference to the modified `XmlText` object.
         * @details Converts the float to a string and assigns it.
         */
        XmlText &operator=(float rhs);
        /**
         * @brief Assignment operator for a boolean.
         * @param[in] rhs The boolean value to assign.
         * @return A reference to the modified `XmlText` object.
         * @details Converts the boolean to "true" or "false" and assigns it.
         */
        XmlText &operator=(bool rhs);

#if __cplusplus >= 201703L
        /**
         * @brief Assignment operator for a `std::string_view`.
         * @param[in] rhs The `std::string_view` to assign.
         * @return A reference to the modified `XmlText` object.
         * @note Available only when compiled with C++17 or later.
         */
        XmlText &operator=(string_view_t rhs);
#endif

        /**
         * @brief Assignment operator for a long long integer.
         * @param[in] rhs The long long integer to assign.
         * @return A reference to the modified `XmlText` object.
         * @details Converts the long long integer to a string and assigns it.
         */
        XmlText &operator=(long long rhs);
        /**
         * @brief Assignment operator for an unsigned long long integer.
         * @param[in] rhs The unsigned long long integer to assign.
         * @return A reference to the modified `XmlText` object.
         * @details Converts the unsigned long long integer to a string and assigns it.
         */
        XmlText &operator=(unsigned long long rhs);

        /**
         * @brief Retrieves the underlying data node (`node_pcdata` or `node_cdata`) for this text object.
         * @return An `XmlNode` object representing the data node, or an empty `XmlNode` if no such node exists.
         * @details This allows direct interaction with the node that actually holds the text content.
         */
        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned data node should be used; discarding it negates the purpose of the getter.")
        XmlNode data() const;

      private:
        /**
         * @brief Private constructor. Used internally to create an `XmlText` object from an `XmlNodeBase` pointer.
         * @param[in] root The `XmlNodeBase` pointer that this `XmlText` object will wrap.
         */
        explicit XmlText(XmlNodeBase *root);

        /// @brief Pointer to the underlying `XmlNodeBase` that contains or points to the text data.
        XmlNodeBase *m_root;

        /**
         * @brief Internal utility to get or create a data node for setting text.
         * @details If a text node already exists, it returns it. Otherwise, it appends a new `node_pcdata` child
         *          to `m_root` and returns it.
         * @return A pointer to the `XmlNodeBase` that can hold the text data.
         */
        XmlNodeBase *_data_new();

        /**
         * @brief Internal utility to retrieve the raw `XmlNodeBase` pointer containing the text data.
         * @return A pointer to the internal `XmlNodeBase` that stores the text, or `nullptr` if no text node is found.
         */
        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned internal pointer should be used; discarding it negates the purpose of the getter.")
        XmlNodeBase *_data() const;
      };

      /**
       * @brief Overload for logical AND operator with `XmlText` on the left-hand side.
       * @param[in] lhs The `XmlText` object.
       * @param[in] rhs The boolean value.
       * @return `true` if `lhs` is valid and `rhs` is `true`, `false` otherwise.
       * @details This enables expressions like `if (xml_text && some_boolean_condition)`.
       */
      LUMEX_API
      bool operator&&(XmlText const &lhs, bool rhs);

      /**
       * @brief Overload for logical OR operator with `XmlText` on the left-hand side.
       * @param[in] lhs The `XmlText` object.
       * @param[in] rhs The boolean value.
       * @return `true` if `lhs` is valid or `rhs` is `true`, `false` otherwise.
       * @details This enables expressions like `if (xml_text || some_boolean_condition)`.
       */
      LUMEX_API
      bool operator||(XmlText const &lhs, bool rhs);
    } // namespace Text
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_TEXT_HPP
