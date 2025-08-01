#ifndef LUMEX_XML_PARSE_RESULT_HPP
#define LUMEX_XML_PARSE_RESULT_HPP

#include "lumex/core/utility/LumexAttributes.hpp"

#include "lumex/xml/types/XmlTypes.hpp"

using namespace Lumex::Xml;

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace Document
    {
      class XmlDocumentBase;
    }

    namespace Node
    {
      class XmlNodeBase;
    }

    namespace Text
    {
      struct XmlParseResult {
        // Parsing status (see xml_parse_status)
        Types::xml_parse_status status; // NOLINT(misc-non-private-member-variables-in-classes)

        // Last parsed offset (in char_t units from start of input data)
        ptrdiff_t offset; // NOLINT(misc-non-private-member-variables-in-classes)

        // Source document encoding
        Types::xml_encoding encoding; // NOLINT(misc-non-private-member-variables-in-classes)

        // Default constructor, initializes object to failed state
        XmlParseResult();

        XmlParseResult(Types::xml_parse_status status);

        // Cast to bool operator
        operator bool() const;

        // Get error description
        LUMEX_ATTRIBUTE_NODISCARD("The returned value is the same as the one returned by the description() method; "
                                  "discarding it negates the purpose of the getter.")
        char const *description() const;
      };

      inline XmlParseResult
      make_parse_result(Types::xml_parse_status status, ptrdiff_t offset = 0)
      {
        XmlParseResult result;
        result.status = status;
        result.offset = offset;

        return result;
      }

      XmlParseResult
      load_buffer_impl(Document::XmlDocumentBase *doc, Node::XmlNodeBase *root, void *contents,
                       size_t size, // NOLINT(bugprone-easily-swappable-parameters)
                       unsigned int options, Types::xml_encoding encoding, bool is_mutable, bool own,
                       Types::char_t **out_buffer);
    } // namespace Text
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_PARSE_RESULT_HPP
