#ifndef LUMEX_XML_XPATH_PARSE_RESULT_HPP
#define LUMEX_XML_XPATH_PARSE_RESULT_HPP

#include <cstddef>

#include "lumex/core/utility/LumexAttributes.hpp"

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace XPath
    {
      namespace Parser
      {
        struct XPathParseResult {
          // Error message (0 if no error)
          char const *error; // NOLINT(misc-non-private-member-variables-in-classes)

          // Last parsed offset (in char_t units from string start)
          ptrdiff_t offset; // NOLINT(misc-non-private-member-variables-in-classes)

          // Default constructor, initializes object to failed state
          XPathParseResult();

          // Cast to bool operator
          operator bool() const;

          // Get error description
          LUMEX_ATTRIBUTE_NODISCARD("Error description should not be discarded.")
          char const *description() const;
        };
      } // namespace Parser
    } // namespace XPath
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_XPATH_PARSE_RESULT_HPP
