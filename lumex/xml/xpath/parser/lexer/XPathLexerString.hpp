#ifndef LUMEX_XML_XPATH_LEXER_STRING_HPP
#define LUMEX_XML_XPATH_LEXER_STRING_HPP

#include "lumex/LumexExport.hpp"

#include "lumex/xml/types/XmlTypes.hpp"

using namespace Lumex::Xml::Types;

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace XPath
    {
      namespace Parser
      {
        namespace Lexer
        {
          /**
           * @brief Represents a string view used by the XPath lexer.
           * @details This structure holds pointers to the beginning and end of a character
           *          sequence, effectively providing a non-owning view of a string. It is
           *          used by the XPath lexer to represent tokens found in the XPath expression.
           *
           * @note This is a lightweight view and does not manage the lifetime of the underlying
           *       character data. The character data must remain valid for the lifetime of
           *       `XPathLexerString`.
           */
          struct LUMEX_API XPathLexerString {
            /// @brief Pointer to the beginning of the string data.
            char_t const *begin{}; // NOLINT(misc-non-private-member-variables-in-classes)
            /// @brief Pointer to one past the end of the string data.
            char_t const *end{}; // NOLINT(misc-non-private-member-variables-in-classes)

            /**
             * @brief Default constructor. Constructs an empty string view.
             * @details Initializes `begin` and `end` to `nullptr`, representing an empty string.
             */
            XPathLexerString() = default;

            /**
             * @brief Equality comparison operator.
             * @details Compares the string view with a null-terminated C-style string (`other`).
             *          The comparison is performed based on the character sequence and length.
             * @param other A null-terminated C-style string to compare against.
             * @return `true` if the string view's content is equal to `other`, `false` otherwise.
             */
            bool operator==(char_t const *other) const;
          };
        }
      }
    }
  }
}

#endif // !LUMEX_XML_XPATH_LEXER_STRING_HPP
