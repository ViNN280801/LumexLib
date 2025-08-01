#ifndef LUMEX_XML_XPATH_STRING_HPP
#define LUMEX_XML_XPATH_STRING_HPP

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
        class XPathString
        {
        public:
          static XPathString from_const(char_t const *str);

          static XPathString from_heap_preallocated(char_t const *begin, char_t const *end);

          static XPathString from_heap(char_t const *begin, char_t const *end, XPathAllocator *alloc);

          XPathString();

          void append(XPathString const &other, XPathAllocator *alloc);

          LUMEX_ATTRIBUTE_NODISCARD(
            "Discarding the returned C-style string pointer (c_str()) means losing access to the string's content. The "
            "string's value is essential for subsequent operations or inspection.")
          char_t const *c_str() const;

          LUMEX_ATTRIBUTE_NODISCARD(
            "Discarding the returned string length (length()) means losing crucial information about the string's "
            "size, which is often needed for iteration, buffer allocation, or validation.")
          size_t length() const;

          char_t *data(XPathAllocator *alloc);

          LUMEX_ATTRIBUTE_NODISCARD(
            "Discarding the boolean result of empty() means ignoring whether the string contains any characters, which "
            "is critical for control flow and preventing operations on empty data.")
          bool empty() const;

          bool operator==(XPathString const &other) const;

          bool operator!=(XPathString const &other) const;

          LUMEX_ATTRIBUTE_NODISCARD(
            "Discarding the boolean result of uses_heap() means ignoring crucial information about the string's memory "
            "allocation strategy, which might be important for memory management or performance optimizations.")
          bool uses_heap() const;

        private:
          char_t const *m_buffer;
          bool m_uses_heap;
          size_t m_length_heap;

          static char_t *duplicate_string(char_t const *string, size_t length, XPathAllocator *alloc);

          XPathString(char_t const *buffer, bool uses_heap_, size_t length_heap);
        };

        XPathString string_value(Node::XPathNode const &node, XPathAllocator *alloc);

        XPathString convert_number_to_string(double value, XPathAllocator *alloc);
      } // namespace String
    } // namespace XPath
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_XPATH_STRING_HPP
