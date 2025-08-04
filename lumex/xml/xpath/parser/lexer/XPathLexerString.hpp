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
          struct LUMEX_API XPathLexerString {
            char_t const *begin{}; // NOLINT(misc-non-private-member-variables-in-classes)
            char_t const *end{};   // NOLINT(misc-non-private-member-variables-in-classes)

            XPathLexerString() = default;

            bool operator==(char_t const *other) const;
          };
        }
      }
    }
  }
}

#endif // !LUMEX_XML_XPATH_LEXER_STRING_HPP
