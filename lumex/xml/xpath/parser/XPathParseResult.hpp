#ifndef LUMEX_XML_XPATH_PARSE_RESULT_HPP
#define LUMEX_XML_XPATH_PARSE_RESULT_HPP

#include "lumex/LumexExport.hpp"

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
        /**
         * @brief Represents the result of an XPath parsing operation.
         * @details This structure encapsulates whether an XPath expression was parsed
         *          successfully, and if not, provides details about the error, including
         *          an error message and the offset in the query string where the error occurred.
         */
        struct LUMEX_API XPathParseResult {
          /// @brief A null-terminated C-string containing the error message if parsing failed, or `nullptr` if
          /// successful.
          char const *error; // NOLINT(misc-non-private-member-variables-in-classes)

          /// @brief The offset (in `char_t` units) from the beginning of the XPath query string where the error
          /// occurred.
          ptrdiff_t offset; // NOLINT(misc-non-private-member-variables-in-classes)

          /**
           * @brief Default constructor. Initializes the object to a failed state.
           * @details By default, a newly constructed `XPathParseResult` indicates an
           *          "Internal error" at offset 0, implying an unsuccessful parsing attempt
           *          unless explicitly updated.
           */
          XPathParseResult();

          /**
           * @brief Conversion operator to `bool`.
           * @details Allows `XPathParseResult` objects to be used in boolean contexts.
           *          It evaluates to `true` if `error` is `nullptr` (no error), and `false` otherwise.
           * @return `true` if parsing was successful, `false` if an error occurred.
           */
          operator bool() const;

          /**
           * @brief Gets the error description.
           * @details Returns the error message if parsing failed, or "No error" if successful.
           * @return A null-terminated C-string describing the parsing result.
           */
          // Error description should not be discarded.
          LUMEX_ATTRIBUTE_NODISCARD("Error description should not be discarded.")
          char const *description() const;
        };
      } // namespace Parser
    } // namespace XPath
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_XPATH_PARSE_RESULT_HPP
