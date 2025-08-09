#ifndef LUMEX_XML_ATTRIBUTE_HPP
#define LUMEX_XML_ATTRIBUTE_HPP

#if __cplusplus >= 201703L
  #include <string_view>
#endif

#include "lumex/LumexExport.hpp"

#include "lumex/core/utility/LumexAttributes.hpp"

#include "lumex/xml/utility/XmlMacros.hpp"

#include "XmlAttributeBase.hpp"

using namespace Lumex::Xml::Types;

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace Attribute
    {
      /**
       * @brief Represents an XML attribute, providing a safe and convenient interface for accessing and modifying its
       * name and value.
       * @details This class acts as a lightweight wrapper around `XmlAttributeBase` pointers, offering value-like
       * semantics and various type conversion methods for attribute values. It supports implicit boolean conversion to
       * check for emptiness and provides operators for comparison and assignment. The class is designed to be efficient,
       * avoiding heavy memory allocations by operating on internal pointers managed by the XML document's allocator.
       * @note This class does not own the underlying `XmlAttributeBase` pointer; its lifetime is managed by the XML
       * document.
       * @see XmlAttributeBase
       * @see XmlNode
       */
      class LUMEX_API XmlAttribute
      {
        friend class XmlAttributeIterator;
        friend class LumexXmlNode;

      public:
        using unspecified_bool_type = void (*)(XmlAttribute ***);

        /**
         * @brief Default constructor. Constructs an empty (null) attribute.
         * @details An empty attribute does not point to any valid XML attribute and its methods will generally return
         * default values or false.
         * @note This constructor initializes the internal pointer to `nullptr`.
         */
        XmlAttribute();

        /**
         * @brief Constructs an `XmlAttribute` from an internal `XmlAttributeBase` pointer.
         * @param[in] attr A pointer to the underlying `XmlAttributeBase` object. This pointer is not owned by the
         * `XmlAttribute` instance.
         * @note This constructor is explicit to prevent unintended conversions from raw pointers.
         */
        explicit XmlAttribute(XmlAttributeBase *attr);

        /**
         * @brief Provides safe boolean conversion for `XmlAttribute` objects.
         * @details Allows an `XmlAttribute` object to be used in boolean contexts (e.g., `if (attribute)`).
         *          It evaluates to `true` if the attribute points to a valid `XmlAttributeBase` object (i.e., not
         * null), and `false` otherwise.
         * @return A pointer to a dummy function if the internal attribute pointer is not null, otherwise `nullptr`.
         * @note This conversion prevents problematic implicit conversions to arithmetic types.
         */
        operator unspecified_bool_type() const;

        /**
         * @brief Overloads the logical NOT operator.
         * @details Provides a convenient way to check if an `XmlAttribute` object is empty.
         * @return `true` if the attribute is empty (internal pointer is null), `false` otherwise.
         * @see empty()
         */
        bool operator!() const;

        /**
         * @brief Compares two `XmlAttribute` objects for equality.
         * @details Two `XmlAttribute` objects are considered equal if they wrap the same underlying `XmlAttributeBase`
         * pointer.
         * @param[in] other The `XmlAttribute` object to compare with.
         * @return `true` if both attributes point to the same internal `XmlAttributeBase`, `false` otherwise.
         * @note This performs a pointer comparison, not a value comparison of the attribute's name or value.
         */
        bool operator==(XmlAttribute const &other) const;

        /**
         * @brief Compares two `XmlAttribute` objects for inequality.
         * @details Two `XmlAttribute` objects are considered unequal if they wrap different underlying
         * `XmlAttributeBase` pointers.
         * @param[in] other The `XmlAttribute` object to compare with.
         * @return `true` if the attributes point to different internal `XmlAttributeBase` objects, `false` otherwise.
         * @note This performs a pointer comparison, not a value comparison of the attribute's name or value.
         */
        bool operator!=(XmlAttribute const &other) const;

        /**
         * @brief Compares two `XmlAttribute` objects using the less-than operator.
         * @details The comparison is based on the memory addresses of the wrapped `XmlAttributeBase` pointers.
         * @param[in] other The `XmlAttribute` object to compare with.
         * @return `true` if the internal pointer of this attribute is less than that of the `other` attribute, `false`
         * otherwise.
         * @note This operator is provided for completeness and might be useful in contexts requiring ordering based on
         * pointer addresses, such as STL containers.
         */
        bool operator<(XmlAttribute const &other) const;

        /**
         * @brief Compares two `XmlAttribute` objects using the greater-than operator.
         * @details The comparison is based on the memory addresses of the wrapped `XmlAttributeBase` pointers.
         * @param[in] other The `XmlAttribute` object to compare with.
         * @return `true` if the internal pointer of this attribute is greater than that of the `other` attribute,
         * `false` otherwise.
         * @note This operator is provided for completeness and might be useful in contexts requiring ordering based on
         * pointer addresses, such as STL containers.
         */
        bool operator>(XmlAttribute const &other) const;

        /**
         * @brief Compares two `XmlAttribute` objects using the less-than-or-equal-to operator.
         * @details The comparison is based on the memory addresses of the wrapped `XmlAttributeBase` pointers.
         * @param[in] other The `XmlAttribute` object to compare with.
         * @return `true` if the internal pointer of this attribute is less than or equal to that of the `other`
         * attribute, `false` otherwise.
         * @note This operator is provided for completeness and might be useful in contexts requiring ordering based on
         * pointer addresses, such as STL containers.
         */
        bool operator<=(XmlAttribute const &other) const;

        /**
         * @brief Compares two `XmlAttribute` objects using the greater-than-or-equal-to operator.
         * @details The comparison is based on the memory addresses of the wrapped `XmlAttributeBase` pointers.
         * @param[in] other The `XmlAttribute` object to compare with.
         * @return `true` if the internal pointer of this attribute is greater than or equal to that of the `other`
         * attribute, `false` otherwise.
         * @note This operator is provided for completeness and might be useful in contexts requiring ordering based on
         * pointer addresses, such as STL containers.
         */
        bool operator>=(XmlAttribute const &other) const;

        LUMEX_ATTRIBUTE_NODISCARD("The returned boolean indicates whether the attribute is empty; discarding it "
                                  "negates the purpose of the getter")
        /**
         * @brief Checks if the `XmlAttribute` object is empty.
         * @details An attribute is considered empty if its internal `XmlAttributeBase` pointer is `nullptr`.
         * @return `true` if the attribute is empty, `false` otherwise.
         * @note This method is equivalent to `!operator bool()`.
         */
        bool empty() const;

        LUMEX_ATTRIBUTE_NODISCARD("The returned attribute name (C-style string) should be used; discarding it "
                                  "negates the purpose of the getter")
        /**
         * @brief Retrieves the name of the XML attribute.
         * @details If the attribute is valid, this method returns a C-style string pointing to the attribute's name.
         *          If the attribute is empty (null), an empty string literal `LUMEX_XML_TEXT("")` is returned.
         * @return A null-terminated C-style string representing the attribute's name.
         * @note The returned pointer points to internal memory and should not be deallocated or modified. Its lifetime
         * is tied to the XML document.
         */
        char_t const *name() const;

        LUMEX_ATTRIBUTE_NODISCARD("The returned attribute value (C-style string) should be used; discarding it "
                                  "negates the purpose of the getter")
        /**
         * @brief Retrieves the value of the XML attribute as a C-style string.
         * @details If the attribute is valid, this method returns a C-style string pointing to the attribute's value.
         *          If the attribute is empty (null), an empty string literal `LUMEX_XML_TEXT("")` is returned.
         * @return A null-terminated C-style string representing the attribute's value.
         * @note The returned pointer points to internal memory and should not be deallocated or modified. Its lifetime
         * is tied to the XML document.
         */
        char_t const *value() const;

        /**
         * @brief Retrieves the attribute's value as a C-style string, or a default value if the attribute is empty.
         * @param[in] def The default C-style string to return if the attribute is empty or its value is null. Defaults
         * to `LUMEX_XML_TEXT("")`.
         * @return A null-terminated C-style string representing the attribute's value, or `def` if the attribute is
         * empty or its value is null.
         * @note The returned pointer points to internal memory (if not `def`) and should not be deallocated or
         * modified.
         */
        char_t const *as_string(char_t const *def = LUMEX_XML_TEXT("")) const;

        // Get attribute value as a number, or the default value if conversion did not succeed or attribute is empty
        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned value is the default value; discarding it negates the purpose of the getter")
        /**
         * @brief Converts and retrieves the attribute's value as an `int`.
         * @details Attempts to parse the attribute's string value into an `int`.
         *          If the attribute is empty, its value is null, or the conversion fails, the `def` value is returned.
         * @param[in] def The default `int` value to return if conversion fails or the attribute is empty. Defaults to
         * `0`.
         * @return The attribute's value as an `int`, or `def` if parsing fails or the attribute is invalid.
         */
        int as_int(int def = 0) const;

        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned value is the default value; discarding it negates the purpose of the getter")
        /**
         * @brief Converts and retrieves the attribute's value as an `unsigned int`.
         * @details Attempts to parse the attribute's string value into an `unsigned int`.
         *          If the attribute is empty, its value is null, or the conversion fails, the `def` value is returned.
         * @param[in] def The default `unsigned int` value to return if conversion fails or the attribute is empty.
         * Defaults to `0`.
         * @return The attribute's value as an `unsigned int`, or `def` if parsing fails or the attribute is invalid.
         */
        unsigned int as_uint(unsigned int def = 0) const;

        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned value is the default value; discarding it negates the purpose of the getter")
        /**
         * @brief Converts and retrieves the attribute's value as a `double`.
         * @details Attempts to parse the attribute's string value into a `double`.
         *          If the attribute is empty, its value is null, or the conversion fails, the `def` value is returned.
         * @param[in] def The default `double` value to return if conversion fails or the attribute is empty. Defaults
         * to `0.0`.
         * @return The attribute's value as a `double`, or `def` if parsing fails or the attribute is invalid.
         */
        double as_double(double def = 0) const;

        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned value is the default value; discarding it negates the purpose of the getter")
        /**
         * @brief Converts and retrieves the attribute's value as a `float`.
         * @details Attempts to parse the attribute's string value into a `float`.
         *          If the attribute is empty, its value is null, or the conversion fails, the `def` value is returned.
         * @param[in] def The default `float` value to return if conversion fails or the attribute is empty. Defaults to
         * `0.0f`.
         * @return The attribute's value as a `float`, or `def` if parsing fails or the attribute is invalid.
         */
        float as_float(float def = 0) const;

        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned value is the default value; discarding it negates the purpose of the getter")
        /**
         * @brief Converts and retrieves the attribute's value as a `long long`.
         * @details Attempts to parse the attribute's string value into a `long long`.
         *          If the attribute is empty, its value is null, or the conversion fails, the `def` value is returned.
         * @param[in] def The default `long long` value to return if conversion fails or the attribute is empty.
         * Defaults to `0LL`.
         * @return The attribute's value as a `long long`, or `def` if parsing fails or the attribute is invalid.
         */
        long long as_llong(long long def = 0) const;

        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned value is the default value; discarding it negates the purpose of the getter")
        /**
         * @brief Converts and retrieves the attribute's value as an `unsigned long long`.
         * @details Attempts to parse the attribute's string value into an `unsigned long long`.
         *          If the attribute is empty, its value is null, or the conversion fails, the `def` value is returned.
         * @param[in] def The default `unsigned long long` value to return if conversion fails or the attribute is
         * empty. Defaults to `0ULL`.
         * @return The attribute's value as an `unsigned long long`, or `def` if parsing fails or the attribute is
         * invalid.
         */
        unsigned long long as_ullong(unsigned long long def = 0) const;

        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned value is the default value; discarding it negates the purpose of the getter")
        /**
         * @brief Converts and retrieves the attribute's value as a `bool`.
         * @details The conversion logic considers values starting with '1', 't', 'T', 'y', 'Y' (case-insensitive) as
         * `true`. If the attribute is empty, its value is null, or the conversion logic does not yield `true`, the
         * `def` value is returned.
         * @param[in] def The default `bool` value to return if conversion fails or the attribute is empty. Defaults to
         * `false`.
         * @return The attribute's value as a `bool`, or `def` if parsing fails or the attribute is invalid.
         */
        bool as_bool(bool def = false) const;

        // Set attribute name/value (returns false if attribute is empty or there is not enough memory)
        /**
         * @brief Sets the name of the XML attribute from a null-terminated C-style string.
         * @details This function attempts to copy the provided string into the attribute's name buffer.
         *          It returns `false` if the attribute is empty or if there isn't enough memory to allocate the new
         * name.
         * @param[in] rhs The new name for the attribute. Must be null-terminated.
         * @return `true` if the name was successfully set, `false` otherwise.
         * @note The underlying memory for the name might be reallocated if the new name is larger than the current
         * buffer.
         */
        bool set_name(char_t const *rhs);

        /**
         * @brief Sets the name of the XML attribute from a C-style string with a specified size.
         * @details This function attempts to copy the provided string (up to `size` characters) into the attribute's
         * name buffer. It returns `false` if the attribute is empty or if there isn't enough memory to allocate the new
         * name.
         * @param[in] rhs The new name for the attribute.
         * @param[in] size The number of characters to copy from `rhs`.
         * @return `true` if the name was successfully set, `false` otherwise.
         * @note The underlying memory for the name might be reallocated if the new name is larger than the current
         * buffer.
         */
        bool set_name(char_t const *rhs, size_t size);

#if __cplusplus >= 201703L
        /**
         * @brief Sets the name of the XML attribute from a `std::string_view`.
         * @details This function attempts to copy the provided string view's content into the attribute's name buffer.
         *          It returns `false` if the attribute is empty or if there isn't enough memory to allocate the new
         * name.
         * @param[in] rhs The new name for the attribute as a `std::string_view`.
         * @return `true` if the name was successfully set, `false` otherwise.
         * @note The underlying memory for the name might be reallocated if the new name is larger than the current
         * buffer.
         * @note Available only when compiled with C++17 or later.
         */
        bool set_name(std::string_view rhs);
#endif
        /**
         * @brief Sets the value of the XML attribute from a null-terminated C-style string.
         * @details This function attempts to copy the provided string into the attribute's value buffer.
         *          It returns `false` if the attribute is empty or if there isn't enough memory to allocate the new
         * value.
         * @param[in] rhs The new value for the attribute. Must be null-terminated.
         * @return `true` if the value was successfully set, `false` otherwise.
         * @note The underlying memory for the value might be reallocated if the new value is larger than the current
         * buffer.
         */
        bool set_value(char_t const *rhs);

        /**
         * @brief Sets the value of the XML attribute from a C-style string with a specified size.
         * @details This function attempts to copy the provided string (up to `size` characters) into the attribute's
         * value buffer. It returns `false` if the attribute is empty or if there isn't enough memory to allocate the
         * new value.
         * @param[in] rhs The new value for the attribute.
         * @param[in] size The number of characters to copy from `rhs`.
         * @return `true` if the value was successfully set, `false` otherwise.
         * @note The underlying memory for the value might be reallocated if the new value is larger than the current
         * buffer.
         */
        bool set_value(char_t const *rhs, size_t size);

#if __cplusplus >= 201703L
        /**
         * @brief Sets the value of the XML attribute from a `std::string_view`.
         * @details This function attempts to copy the provided string view's content into the attribute's value buffer.
         *          It returns `false` if the attribute is empty or if there isn't enough memory to allocate the new
         * value.
         * @param[in] rhs The new value for the attribute as a `std::string_view`.
         * @return `true` if the value was successfully set, `false` otherwise.
         * @note The underlying memory for the value might be reallocated if the new value is larger than the current
         * buffer.
         * @note Available only when compiled with C++17 or later.
         */
        bool set_value(std::string_view rhs);
#endif

        // Set attribute value with type conversion (numbers are converted to strings, boolean is converted to
        // "true"/"false")
        /**
         * @brief Sets the attribute's value after converting an `int` to its string representation.
         * @details The integer value `rhs` is converted to a string and then stored as the attribute's value.
         * @param[in] rhs The `int` value to set.
         * @return `true` if the value was successfully set, `false` otherwise (e.g., if the attribute is empty or
         * memory allocation fails).
         * @note The underlying memory for the value might be reallocated.
         */
        bool set_value(int rhs);

        /**
         * @brief Sets the attribute's value after converting an `unsigned int` to its string representation.
         * @details The unsigned integer value `rhs` is converted to a string and then stored as the attribute's value.
         * @param[in] rhs The `unsigned int` value to set.
         * @return `true` if the value was successfully set, `false` otherwise (e.g., if the attribute is empty or
         * memory allocation fails).
         * @note The underlying memory for the value might be reallocated.
         */
        bool set_value(unsigned int rhs);

        /**
         * @brief Sets the attribute's value after converting a `long` to its string representation.
         * @details The long integer value `rhs` is converted to a string and then stored as the attribute's value.
         * @param[in] rhs The `long` value to set.
         * @return `true` if the value was successfully set, `false` otherwise (e.g., if the attribute is empty or
         * memory allocation fails).
         * @note The underlying memory for the value might be reallocated.
         */
        bool set_value(long rhs);

        /**
         * @brief Sets the attribute's value after converting an `unsigned long` to its string representation.
         * @details The unsigned long integer value `rhs` is converted to a string and then stored as the attribute's
         * value.
         * @param[in] rhs The `unsigned long` value to set.
         * @return `true` if the value was successfully set, `false` otherwise (e.g., if the attribute is empty or
         * memory allocation fails).
         * @note The underlying memory for the value might be reallocated.
         */
        bool set_value(unsigned long rhs);

        /**
         * @brief Sets the attribute's value after converting a `double` to its string representation using default
         * precision.
         * @details The double-precision floating-point value `rhs` is converted to a string and then stored as the
         * attribute's value. The conversion uses a default precision setting.
         * @param[in] rhs The `double` value to set.
         * @return `true` if the value was successfully set, `false` otherwise (e.g., if the attribute is empty or
         * memory allocation fails).
         * @note The underlying memory for the value might be reallocated.
         * @see set_value(double, int)
         */
        bool set_value(double rhs);

        /**
         * @brief Sets the attribute's value after converting a `double` to its string representation with specified
         * precision.
         * @details The double-precision floating-point value `rhs` is converted to a string with the given `precision`
         * and then stored as the attribute's value.
         * @param[in] rhs The `double` value to set.
         * @param[in] precision The number of digits after the decimal point to use for floating-point conversion.
         * @return `true` if the value was successfully set, `false` otherwise (e.g., if the attribute is empty or
         * memory allocation fails).
         * @note The underlying memory for the value might be reallocated.
         */
        bool set_value(double rhs, int precision);

        /**
         * @brief Sets the attribute's value after converting a `float` to its string representation using default
         * precision.
         * @details The single-precision floating-point value `rhs` is converted to a string and then stored as the
         * attribute's value. The conversion uses a default precision setting.
         * @param[in] rhs The `float` value to set.
         * @return `true` if the value was successfully set, `false` otherwise (e.g., if the attribute is empty or
         * memory allocation fails).
         * @note The underlying memory for the value might be reallocated.
         * @see set_value(float, int)
         */
        bool set_value(float rhs);

        /**
         * @brief Sets the attribute's value after converting a `float` to its string representation with specified
         * precision.
         * @details The single-precision floating-point value `rhs` is converted to a string with the given `precision`
         * and then stored as the attribute's value.
         * @param[in] rhs The `float` value to set.
         * @param[in] precision The number of digits after the decimal point to use for floating-point conversion.
         * @return `true` if the value was successfully set, `false` otherwise (e.g., if the attribute is empty or
         * memory allocation fails).
         * @note The underlying memory for the value might be reallocated.
         */
        bool set_value(float rhs, int precision);

        /**
         * @brief Sets the attribute's value after converting a `bool` to its string representation.
         * @details The boolean value `rhs` is converted to `LUMEX_XML_TEXT("true")` or `LUMEX_XML_TEXT("false")` and
         * then stored as the attribute's value.
         * @param[in] rhs The `bool` value to set.
         * @return `true` if the value was successfully set, `false` otherwise (e.g., if the attribute is empty or
         * memory allocation fails).
         * @note The underlying memory for the value might be reallocated.
         */
        bool set_value(bool rhs);

        /**
         * @brief Sets the attribute's value after converting a `long long` to its string representation.
         * @details The long long integer value `rhs` is converted to a string and then stored as the attribute's value.
         * @param[in] rhs The `long long` value to set.
         * @return `true` if the value was successfully set, `false` otherwise (e.g., if the attribute is empty or
         * memory allocation fails).
         * @note The underlying memory for the value might be reallocated.
         */
        bool set_value(long long rhs);

        /**
         * @brief Sets the attribute's value after converting an `unsigned long long` to its string representation.
         * @details The unsigned long long integer value `rhs` is converted to a string and then stored as the
         * attribute's value.
         * @param[in] rhs The `unsigned long long` value to set.
         * @return `true` if the value was successfully set, `false` otherwise (e.g., if the attribute is empty or
         * memory allocation fails).
         * @note The underlying memory for the value might be reallocated.
         */
        bool set_value(unsigned long long rhs);

        // Set attribute value (equivalent to set_value without error checking)
        /**
         * @brief Assigns a null-terminated C-style string as the attribute's value.
         * @details This operator is equivalent to `set_value(rhs)` but without explicit error checking.
         *          It assumes the underlying `XmlAttributeBase` is valid and memory operations will succeed.
         * @param[in] rhs The new value for the attribute.
         * @return A reference to the current `XmlAttribute` object.
         * @note Prefer `set_value` for robust error handling.
         */
        XmlAttribute &operator=(char_t const *rhs);

        /**
         * @brief Assigns an `int` as the attribute's value, converting it to a string.
         * @details This operator is equivalent to `set_value(rhs)` but without explicit error checking.
         *          It assumes the underlying `XmlAttributeBase` is valid and memory operations will succeed.
         * @param[in] rhs The new `int` value for the attribute.
         * @return A reference to the current `XmlAttribute` object.
         * @note Prefer `set_value` for robust error handling.
         */
        XmlAttribute &operator=(int rhs);

        /**
         * @brief Assigns an `unsigned int` as the attribute's value, converting it to a string.
         * @details This operator is equivalent to `set_value(rhs)` but without explicit error checking.
         *          It assumes the underlying `XmlAttributeBase` is valid and memory operations will succeed.
         * @param[in] rhs The new `unsigned int` value for the attribute.
         * @return A reference to the current `XmlAttribute` object.
         * @note Prefer `set_value` for robust error handling.
         */
        XmlAttribute &operator=(unsigned int rhs);

        /**
         * @brief Assigns a `long` as the attribute's value, converting it to a string.
         * @details This operator is equivalent to `set_value(rhs)` but without explicit error checking.
         *          It assumes the underlying `XmlAttributeBase` is valid and memory operations will succeed.
         * @param[in] rhs The new `long` value for the attribute.
         * @return A reference to the current `XmlAttribute` object.
         * @note Prefer `set_value` for robust error handling.
         */
        XmlAttribute &operator=(long rhs);

        /**
         * @brief Assigns an `unsigned long` as the attribute's value, converting it to a string.
         * @details This operator is equivalent to `set_value(rhs)` but without explicit error checking.
         *          It assumes the underlying `XmlAttributeBase` is valid and memory operations will succeed.
         * @param[in] rhs The new `unsigned long` value for the attribute.
         * @return A reference to the current `XmlAttribute` object.
         * @note Prefer `set_value` for robust error handling.
         */
        XmlAttribute &operator=(unsigned long rhs);

        /**
         * @brief Assigns a `double` as the attribute's value, converting it to a string with default precision.
         * @details This operator is equivalent to `set_value(rhs)` but without explicit error checking.
         *          It assumes the underlying `XmlAttributeBase` is valid and memory operations will succeed.
         * @param[in] rhs The new `double` value for the attribute.
         * @return A reference to the current `XmlAttribute` object.
         * @note Prefer `set_value` for robust error handling.
         */
        XmlAttribute &operator=(double rhs);

        /**
         * @brief Assigns a `float` as the attribute's value, converting it to a string with default precision.
         * @details This operator is equivalent to `set_value(rhs)` but without explicit error checking.
         *          It assumes the underlying `XmlAttributeBase` is valid and memory operations will succeed.
         * @param[in] rhs The new `float` value for the attribute.
         * @return A reference to the current `XmlAttribute` object.
         * @note Prefer `set_value` for robust error handling.
         */
        XmlAttribute &operator=(float rhs);

        /**
         * @brief Assigns a `bool` as the attribute's value, converting it to "true" or "false".
         * @details This operator is equivalent to `set_value(rhs)` but without explicit error checking.
         *          It assumes the underlying `XmlAttributeBase` is valid and memory operations will succeed.
         * @param[in] rhs The new `bool` value for the attribute.
         * @return A reference to the current `XmlAttribute` object.
         * @note Prefer `set_value` for robust error handling.
         */
        XmlAttribute &operator=(bool rhs);

#if __cplusplus >= 201703L
        /**
         * @brief Assigns a `std::string_view` as the attribute's value.
         * @details This operator is equivalent to `set_value(rhs)` but without explicit error checking.
         *          It assumes the underlying `XmlAttributeBase` is valid and memory operations will succeed.
         * @param[in] rhs The new `std::string_view` value for the attribute.
         * @return A reference to the current `XmlAttribute` object.
         * @note Prefer `set_value` for robust error handling.
         * @note Available only when compiled with C++17 or later.
         */
        XmlAttribute &operator=(std::string_view rhs);
#endif

        /**
         * @brief Assigns a `long long` as the attribute's value, converting it to a string.
         * @details This operator is equivalent to `set_value(rhs)` but without explicit error checking.
         *          It assumes the underlying `XmlAttributeBase` is valid and memory operations will succeed.
         * @param[in] rhs The new `long long` value for the attribute.
         * @return A reference to the current `XmlAttribute` object.
         * @note Prefer `set_value` for robust error handling.
         */
        XmlAttribute &operator=(long long rhs);

        /**
         * @brief Assigns an `unsigned long long` as the attribute's value, converting it to a string.
         * @details This operator is equivalent to `set_value(rhs)` but without explicit error checking.
         *          It assumes the underlying `XmlAttributeBase` is valid and memory operations will succeed.
         * @param[in] rhs The new `unsigned long long` value for the attribute.
         * @return A reference to the current `XmlAttribute` object.
         * @note Prefer `set_value` for robust error handling.
         */
        XmlAttribute &operator=(unsigned long long rhs);

        // Get next/previous attribute in the attribute list of the parent node
        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned attribute should be used; discarding it negates the purpose of the getter")
        /**
         * @brief Retrieves the next attribute in the list of attributes belonging to the same parent node.
         * @details If the current attribute is the last one, or if the `XmlAttribute` object is empty, an empty
         * `XmlAttribute` is returned.
         * @return An `XmlAttribute` object representing the next attribute, or an empty attribute if no next attribute
         * exists.
         */
        XmlAttribute next_attribute() const;

        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned attribute should be used; discarding it negates the purpose of the getter")
        /**
         * @brief Retrieves the previous attribute in the list of attributes belonging to the same parent node.
         * @details This method handles the circular nature of the attribute list. If the current attribute is the
         * first, it will correctly find the previous element. If the `XmlAttribute` object is empty, an empty
         * `XmlAttribute` is returned.
         * @return An `XmlAttribute` object representing the previous attribute, or an empty attribute if no previous
         * attribute exists (e.g., if the current attribute is the head but has no other attributes).
         */
        XmlAttribute previous_attribute() const;

        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned hash value should be used; discarding it negates the purpose of the getter")
        /**
         * @brief Calculates a hash value for the `XmlAttribute` object.
         * @details The hash value is derived from the memory address of the internal `XmlAttributeBase` pointer.
         *          This provides a unique hash for each distinct attribute object, making it suitable for use in
         * hash-based containers.
         * @return A `size_t` value representing the hash of the attribute's internal pointer. Returns `0` if the
         * attribute is empty.
         * @note The specific hash calculation (`reinterpret_cast<uintptr_t>(m_attr) / sizeof(XmlAttributeBase)`) aims
         * to provide a distribution for attribute handles.
         */
        size_t hash_value() const;

        // Get internal pointer
        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned pointer should be used; discarding it negates the purpose of the getter")
        /**
         * @brief Retrieves the raw internal `XmlAttributeBase` pointer wrapped by this `XmlAttribute` object.
         * @warning Direct manipulation of the returned pointer can lead to undefined behavior if not done carefully,
         *          as it bypasses the safe interface of `XmlAttribute`.
         * @return A pointer to the internal `XmlAttributeBase` structure. Can be `nullptr` if the `XmlAttribute` is
         * empty.
         */
        XmlAttributeBase *get() const;

        /**
         * @brief Sets the internal `XmlAttributeBase` pointer for this `XmlAttribute` object.
         * @details This method allows direct assignment of an underlying attribute pointer.
         * @param[in] attr The `XmlAttributeBase` pointer to set. A `nullptr` value will make the `XmlAttribute` empty.
         */
        void set(XmlAttributeBase *attr);

      private:
        XmlAttributeBase *m_attr;
      };

      LUMEX_API
      bool operator&&(XmlAttribute const &lhs, bool rhs);

      LUMEX_API
      bool operator||(XmlAttribute const &lhs, bool rhs);
    } // namespace Attribute
    namespace Utility
    {
      LUMEX_API
      bool is_attribute_of(Attribute::XmlAttributeBase *attr, Node::XmlNodeBase *node);
    } // namespace Utility
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_ATTRIBUTE_HPP
