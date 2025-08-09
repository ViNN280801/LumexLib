#ifndef LUMEX_XML_XPATH_STRING_HPP
#define LUMEX_XML_XPATH_STRING_HPP

#include "lumex/LumexExport.hpp"

#include "lumex/core/utility/LumexAttributes.hpp"

#include "lumex/xml/types/XmlTypes.hpp"
#include "lumex/xml/xpath/memory/XPathAllocator.hpp"

using namespace Lumex::Xml::Types;
using namespace Lumex::Xml::XPath::Memory;

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace XPath
    {
      namespace Node
      {
        class XPathNode;
      }
      namespace String
      {
        /**
         * @brief Represents an XPath string value, optimized for both constant and heap-allocated strings.
         * @details This class provides a flexible string type used within XPath evaluation.
         *          It can hold references to constant C-style strings (avoiding copies)
         *          or manage its own heap-allocated buffer for dynamic string operations
         *          like concatenation. It integrates with `XPathAllocator` for efficient memory management.
         *
         * @note This class implements a "small string optimization" concept where short strings
         *       might directly use a small internal buffer, though this specific implementation
         *       uses a `bool m_uses_heap` to distinguish.
         * @warning When `m_uses_heap` is `true`, the `m_buffer` points to memory managed
         *          by an `XPathAllocator`. This memory should not be freed manually outside
         *          the allocator's control.
         */
        class LUMEX_API XPathString
        {
        public:
          /**
           * @brief Creates an `XPathString` from a constant C-style string.
           * @details This factory method creates an `XPathString` that refers directly
           *          to the provided `str` without making a copy. The `str` must remain
           *          valid and unchanged for the lifetime of the `XPathString`.
           * @param str A null-terminated C-style string.
           * @return An `XPathString` instance.
           */
          static XPathString from_const(char_t const *str);

          /**
           * @brief Creates an `XPathString` from a pre-allocated heap buffer.
           * @details This factory method is used when the string data is already on the heap
           *          and is guaranteed to be null-terminated at `end`. The `XPathString`
           *          takes ownership of this buffer and will manage its lifetime through
           *          the `XPathAllocator`.
           * @param begin Pointer to the beginning of the string data.
           * @param end Pointer to the null terminator of the string data.
           * @return An `XPathString` instance.
           * @throws `LUMEX_ASSERT` if `begin` is greater than `end` or if `*end` is not null.
           */
          static XPathString from_heap_preallocated(char_t const *begin, char_t const *end);

          /**
           * @brief Creates an `XPathString` by copying data to a new heap buffer.
           * @details This factory method allocates new memory from `alloc`, copies the
           *          characters from the range `[begin, end)` into it, and creates an
           *          `XPathString` that owns this new buffer.
           * @param begin Pointer to the beginning of the string data to copy.
           * @param end Pointer to one past the end of the string data to copy.
           * @param alloc A pointer to the `XPathAllocator` to use for memory allocation.
           * @return An `XPathString` instance, or an empty `XPathString` if allocation fails.
           * @throws `LUMEX_ASSERT` if `begin` is greater than `end`.
           */
          static XPathString from_heap(char_t const *begin, char_t const *end, XPathAllocator *alloc);

          /**
           * @brief Default constructor. Constructs an empty `XPathString`.
           * @details Initializes an `XPathString` that represents an empty string ("").
           */
          XPathString();

          /**
           * @brief Appends another `XPathString` to this string.
           * @details Concatenates the `other` string to the end of this `XPathString`.
           *          If this string is a constant string, it might be converted to a
           *          heap-allocated string to accommodate the appended data.
           * @param other The `XPathString` to append.
           * @param alloc The `XPathAllocator` to use for potential reallocations if this
           *              string needs to grow its internal buffer.
           * @note Modifies this `XPathString` in-place.
           * @warning If memory reallocation fails during append, the string might remain unchanged
           *          or be in an indeterminate state.
           */
          void append(XPathString const &other, XPathAllocator *alloc);

          /**
           * @brief Returns a pointer to the null-terminated C-style string.
           * @return A `char_t const*` pointer to the internal character buffer.
           * @note The returned pointer is only valid for the lifetime of the `XPathString` object.
           *       Do not modify the returned string if `uses_heap()` is `false`.
           */
          // Discarding the returned C-style string pointer (c_str()) means losing access to the string's content. The
          // string's value is essential for subsequent operations or inspection.
          LUMEX_ATTRIBUTE_NODISCARD(
            "Discarding the returned C-style string pointer (c_str()) means losing access to the string's content. The "
            "string's value is essential for subsequent operations or inspection.")
          char_t const *c_str() const;

          /**
           * @brief Returns the length of the string (number of characters).
           * @return The `size_t` length of the string, excluding the null terminator.
           * @note For constant strings, this involves a `strlength` call. For heap-allocated
           *       strings, it's a direct member access.
           */
          // Discarding the returned string length (length()) means losing crucial information about the string's
          // size, which is often needed for iteration, buffer allocation, or validation.
          LUMEX_ATTRIBUTE_NODISCARD(
            "Discarding the returned string length (length()) means losing crucial information about the string's "
            "size, which is often needed for iteration, buffer allocation, or validation.")
          size_t length() const;

          /**
           * @brief Provides a mutable pointer to the string data, ensuring it's on the heap.
           * @details If the string is currently a constant string, this function will allocate
           *          a new heap buffer using `alloc`, copy the string content, and update
           *          internal pointers. If it's already on the heap, it returns a mutable pointer
           *          to the existing buffer.
           * @param alloc The `XPathAllocator` to use if a new heap buffer needs to be allocated.
           * @return A `char_t*` pointer to the mutable string data, or `nullptr` if allocation fails.
           * @note This function might change the internal representation of the string.
           */
          char_t *data(XPathAllocator *alloc);

          /**
           * @brief Checks if the string is empty.
           * @return `true` if the string has a length of 0, `false` otherwise.
           */
          // Discarding the boolean result of empty() means ignoring whether the string contains any characters, which
          // is critical for control flow and preventing operations on empty data.
          LUMEX_ATTRIBUTE_NODISCARD(
            "Discarding the boolean result of empty() means ignoring whether the string contains any characters, which "
            "is critical for control flow and preventing operations on empty data.")
          bool empty() const;

          /**
           * @brief Equality comparison operator for `XPathString`.
           * @param other The `XPathString` to compare with.
           * @return `true` if the contents of the two strings are equal, `false` otherwise.
           */
          bool operator==(XPathString const &other) const;

          /**
           * @brief Inequality comparison operator for `XPathString`.
           * @param other The `XPathString` to compare with.
           * @return `true` if the contents of the two strings are not equal, `false` otherwise.
           */
          bool operator!=(XPathString const &other) const;

          /**
           * @brief Checks if the string's data is stored on the heap.
           * @return `true` if the internal buffer is heap-allocated, `false` if it points
           *         to a constant string literal.
           */
          // Discarding the boolean result of uses_heap() means ignoring crucial information about the string's memory
          // allocation strategy, which might be important for memory management or performance optimizations.
          LUMEX_ATTRIBUTE_NODISCARD(
            "Discarding the boolean result of uses_heap() means ignoring crucial information about the string's memory "
            "allocation strategy, which might be important for memory management or performance optimizations.")
          bool uses_heap() const;

        private:
          /// @brief Pointer to the beginning of the string data.
          char_t const *m_buffer;
          /// @brief Flag indicating if the string data is heap-allocated (`true`) or refers to a constant string
          /// (`false`).
          bool m_uses_heap;
          /// @brief The length of the string when `m_uses_heap` is `true`. Not used when `m_uses_heap` is `false`.
          size_t m_length_heap;

          /**
           * @brief Internal helper to duplicate a string into an `XPathAllocator` managed buffer.
           * @param string The source C-style string to duplicate.
           * @param length The length of the source string.
           * @param alloc The `XPathAllocator` to use for the new buffer.
           * @return A `char_t*` pointer to the newly allocated and copied string, or `nullptr` on failure.
           *         The returned string is null-terminated.
           */
          static char_t *duplicate_string(char_t const *string, size_t length, XPathAllocator *alloc);

          /**
           * @brief Private constructor for internal use, allowing full control over string state.
           * @param buffer The character buffer for the string.
           * @param uses_heap_ Boolean indicating if `buffer` is heap-managed.
           * @param length_heap The length of the string if `uses_heap_` is `true`.
           */
          XPathString(char_t const *buffer, bool uses_heap_, size_t length_heap);
        };

        /**
         * @brief Retrieves the string value of an XPath node.
         * @details This function extracts the string value of a given `XPathNode` according to
         *          XPath 1.0 rules. For element nodes, it concatenates the text content of
         *          the node and all its descendant text nodes. For attributes, comments,
         *          and processing instructions, it returns their respective values.
         * @param node The `XPathNode` (element, attribute, etc.) from which to get the string value.
         * @param alloc A pointer to the `XPathAllocator` to use for accumulating string data
         *              if concatenation or copying is necessary.
         * @return An `XPathString` representing the string value of the node.
         */
        LUMEX_API
        XPathString string_value(Node::XPathNode const &node, XPathAllocator *alloc);

        /**
         * @brief Converts a double-precision floating-point number to an XPath string representation.
         * @details This function converts a `double` value into its canonical XPath string form.
         *          It handles special values like NaN, positive/negative infinity, and formats
         *          numbers without unnecessary leading/trailing zeroes or decimal points
         *          when representing integers.
         * @param value The `double` value to convert.
         * @param alloc A pointer to the `XPathAllocator` to use for allocating the string buffer.
         * @return An `XPathString` representing the number's string value, or an empty `XPathString` on allocation
         * failure.
         */
        LUMEX_API
        XPathString convert_number_to_string(double value, XPathAllocator *alloc);
      } // namespace String
    } // namespace XPath
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_XPATH_STRING_HPP
