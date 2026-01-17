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
          /**
           * @brief Lexer (tokenizer) for XPath expressions.
           * @details This class is responsible for breaking down an XPath query string into a
           *          sequence of tokens (lexemes). It handles various XPath lexical constructs
           *          including operators, axis specifiers, node tests, numbers, strings,
           *          and variable references. It skips whitespace and identifies multi-character
           *          tokens (e.g., `>=`, `//`).
           *
           * @note The lexer operates on a raw `char_t` string and does not perform any
           *       syntax validation beyond token recognition. It maintains its internal
           *       state, allowing for sequential token retrieval.
           * @warning This class does not own the `query` string passed to its constructor;
           *          the `query` must remain valid throughout the lexer's lifetime.
           */
          class LUMEX_API XPathLexer
          {
            /// @brief Current position in the XPath query string.
            char_t const *_cur{};
            /// @brief Starting position of the currently recognized lexeme.
            char_t const *_cur_lexeme_pos{};
            /// @brief Contents of the currently recognized lexeme as an `XPathLexerString`.
            XPathLexerString _cur_lexeme_contents{};

            /// @brief The type of the currently recognized lexeme.
            lexeme_t _cur_lexeme{};

          public:
            /**
             * @brief Constructs an XPathLexer and initializes it with a query string.
             * @details The lexer immediately processes the first token upon construction.
             * @param query A null-terminated C-style string containing the XPath expression to lex.
             * @warning The `query` string must remain valid and unchanged for the lifetime of this lexer.
             */
            explicit XPathLexer(char_t const *query);

            /**
             * @brief Returns the current parsing position in the query string.
             * @return A `char_t const*` pointing to the character just after the current lexeme.
             */
            // Discarding the lexer's state means losing the current parsing position, which is essential for error
            // reporting, debugging, or resuming parsing from a specific point.
            LUMEX_ATTRIBUTE_NODISCARD(
              "Discarding the lexer's state means losing the current parsing position, which is essential for error "
              "reporting, debugging, or resuming parsing from a specific point.")
            char_t const *state() const;

            /**
             * @brief Advances the lexer to the next token in the XPath query.
             * @details This function parses the input string from the current position,
             *          skips any whitespace, identifies the next lexeme, and updates
             *          the internal state (`_cur`, `_cur_lexeme_pos`, `_cur_lexeme_contents`, `_cur_lexeme`).
             */
            void next();

            /**
             * @brief Returns the type of the current lexeme.
             * @return A `lexeme_t` enumeration value representing the type of the token
             *         at the current lexer position.
             */
            LUMEX_ATTRIBUTE_NODISCARD(
              "Discarding the current lexeme means ignoring the token type recognized by the lexer, which is crucial "
              "for guiding the parser's logic and determining the next parsing action.")
            lexeme_t current() const;

            /**
             * @brief Returns the starting position of the current lexeme in the query string.
             * @return A `char_t const*` pointing to the first character of the current lexeme.
             * @note This position is useful for error reporting or extracting the raw token string.
             */
            LUMEX_ATTRIBUTE_NODISCARD(
              "Discarding the current lexer position means losing the exact character pointer where the current lexeme "
              "begins. This position is vital for error reporting, context tracking, and precise string manipulation.")
            char_t const *current_pos() const;

            /**
             * @brief Returns the contents of the current lexeme as an `XPathLexerString`.
             * @details This provides a view of the actual characters that constitute the
             *          current token (e.g., the name of a variable, the value of a string literal).
             * @return A constant reference to an `XPathLexerString` object. The underlying
             *         character data is valid as long as the original `query` string is valid.
             * @throws `LUMEX_ASSERT` if the current lexeme type does not have associated contents
             *         (e.g., simple operators like `+` or `-`).
             */
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
