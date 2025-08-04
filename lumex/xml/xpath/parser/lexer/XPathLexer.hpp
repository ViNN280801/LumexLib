#ifndef LUMEX_XML_XPATH_PARSER_XPATHLEXER_HPP
#define LUMEX_XML_XPATH_PARSER_XPATHLEXER_HPP

#include "lumex/LumexExport.hpp"

#include "lumex/core/utility/LumexAttributes.hpp"

#include "lumex/xml/xpath/parser/lexer/XPathLexerString.hpp"

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
          class LUMEX_API XPathLexer
          {
            char_t const *_cur{};
            char_t const *_cur_lexeme_pos{};
            XPathLexerString _cur_lexeme_contents{};

            lexeme_t _cur_lexeme{};

          public:
            explicit XPathLexer(char_t const *query);

            LUMEX_ATTRIBUTE_NODISCARD(
              "Discarding the lexer's state means losing the current parsing position, which is essential for error "
              "reporting, debugging, or resuming parsing from a specific point.")
            char_t const *state() const;

            void next();

            LUMEX_ATTRIBUTE_NODISCARD(
              "Discarding the current lexeme means ignoring the token type recognized by the lexer, which is crucial "
              "for guiding the parser's logic and determining the next parsing action.")
            lexeme_t current() const;

            LUMEX_ATTRIBUTE_NODISCARD(
              "Discarding the current lexer position means losing the exact character pointer where the current lexeme "
              "begins. This position is vital for error reporting, context tracking, and precise string manipulation.")
            char_t const *current_pos() const;

            LUMEX_ATTRIBUTE_NODISCARD(
              "Discarding the lexeme's content means ignoring the actual string value of the token, which is often "
              "needed for semantic analysis, variable extraction, or direct processing of string literals.")
            XPathLexerString const &contents() const;
          };
        } // namespace Lexer
      } // namespace Parser
    } // namespace XPath
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_XPATH_PARSER_XPATHLEXER_HPP
