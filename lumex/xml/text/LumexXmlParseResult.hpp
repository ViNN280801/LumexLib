#ifndef LUMEX_XML_PARSE_RESULT_HPP
#define LUMEX_XML_PARSE_RESULT_HPP

#include "lumex/LumexExport.hpp"

#include "lumex/core/utility/LumexAttributes.hpp"

#include "lumex/xml/types/LumexXmlTypes.hpp"

using namespace Lumex::Xml;

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace Text
    {
      struct LUMEX_API LumexXmlParseResult {
        // Parsing status (see xml_parse_status)
        Types::xml_parse_status status; // NOLINT(misc-non-private-member-variables-in-classes)

        // Last parsed offset (in char_t units from start of input data)
        ptrdiff_t offset; // NOLINT(misc-non-private-member-variables-in-classes)

        // Source document encoding
        Types::xml_encoding encoding; // NOLINT(misc-non-private-member-variables-in-classes)

        // Default constructor, initializes object to failed state
        LumexXmlParseResult();

        // Cast to bool operator
        operator bool() const;

        // Get error description
        LUMEX_ATTRIBUTE_NODISCARD("The returned value is the same as the one returned by the description() method; "
                                  "discarding it negates the purpose of the getter.")
        char const *description() const;
      };
    } // namespace Text
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_PARSE_RESULT_HPP
