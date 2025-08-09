#ifndef LUMEX_XML_PARSER_HPP
#define LUMEX_XML_PARSER_HPP

#include "lumex/LumexExport.hpp"

#include "lumex/xml/document/XmlDocumentBase.hpp"
#include "lumex/xml/memory/XmlAllocator.hpp"
#include "lumex/xml/node/XmlNodeBase.hpp"
#include "lumex/xml/text/XmlParseResult.hpp"
#include "lumex/xml/types/XmlTypes.hpp"

using namespace Lumex::Xml::Document;
using namespace Lumex::Xml::Text;
using namespace Lumex::Xml::Node;
using namespace Lumex::Xml::Types;
using namespace Lumex::Xml::Memory;

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace Text
    {
      /**
       * @brief Internal XML parser structure responsible for tokenizing and building the XML document tree.
       * @details This structure contains the state for the XML parsing process, including the allocator,
       *          error status, and error offset. It provides core methods for parsing different XML constructs
       *          such as DOCTYPE, comments, CDATA, processing instructions, and the main element tree.
       * @note This is a low-level parsing component; most users will interact with `XmlDocument` for parsing.
       * @see XmlDocument
       */
      struct LUMEX_API XmlParser {
        /// @brief Pointer to the XML allocator used for memory management during parsing.
        XmlAllocator *alloc; // NOLINT(misc-non-private-member-variables-in-classes)
        /// @brief Stores the offset in the input buffer where a parsing error occurred.
        char_t *error_offset{}; // NOLINT(misc-non-private-member-variables-in-classes)
        /// @brief Stores the status of the last parsing operation.
        xml_parse_status error_status{}; // NOLINT(misc-non-private-member-variables-in-classes)

        /**
         * @brief Constructs an `XmlParser` object with a given allocator.
         * @param[in,out] alloc_ The `XmlAllocator` instance to use for memory operations during parsing.
         */
        XmlParser(XmlAllocator *alloc_);

        /**
         * @brief Parses a primitive DOCTYPE section, which can be a quoted string or a PI/comment-like section.
         * @param[in,out] str A pointer to the current position in the XML buffer.
         * @return A pointer to the position in the buffer after the primitive DOCTYPE section, or `nullptr` on error.
         * @note Used internally by `parse_doctype_group`.
         */
        char_t *parse_doctype_primitive(char_t *str);

        /**
         * @brief Parses a DOCTYPE group, which can contain nested sections of different types.
         * @param[in,out] str A pointer to the current position in the XML buffer.
         * @param[in] endch The expected character that marks the end of the DOCTYPE group (typically `>`).
         * @return A pointer to the position in the buffer after the DOCTYPE group, or `nullptr` on error.
         * @details Handles recursive parsing of nested DOCTYPE structures like `<!ELEMENT ...>` or `<!ATTLIST ...>`.
         */
        char_t *parse_doctype_group(char_t *str, char_t endch);

        /**
         * @brief Parses an `<![IGNORE[...]]>` section within a DOCTYPE.
         * @param[in,out] str A pointer to the current position in the XML buffer.
         * @return A pointer to the position in the buffer after the ignore section, or `nullptr` on error.
         * @details This function handles nested ignore sections.
         */
        char_t *parse_doctype_ignore(char_t *str);

        /**
         * @brief Parses XML constructs starting with an exclamation mark (`<!...`).
         * @param[in,out] str A pointer to the current position in the XML buffer.
         * @param[in,out] cursor A reference to the current `XmlNodeBase` being built; updated if a new node is pushed.
         * @param[in] optmsk A bitmask of parsing options.
         * @param[in] endch The character expected at the end of the current section (e.g., `>`).
         * @return A pointer to the position in the buffer after the parsed exclamation construct, or `nullptr` on
         * error.
         * @details This function dispatches parsing for comments (`<!--`), CDATA sections (`<![CDATA[`),
         *          and DOCTYPE declarations (`<!DOCTYPE`).
         */
        char_t *parse_exclamation(char_t *str, XmlNodeBase *cursor, unsigned int optmsk, char_t endch);

        /**
         * @brief Parses XML processing instructions (`<?...?>`) or XML declarations (`<?xml...?>`).
         * @param[in,out] str A pointer to the current position in the XML buffer.
         * @param[in,out] ref_cursor A reference to the current `XmlNodeBase` being built; updated if a new node is
         * pushed.
         * @param[in] optmsk A bitmask of parsing options.
         * @param[in] endch The character expected at the end of the current section (typically `>`).
         * @return A pointer to the position in the buffer after the parsed PI or declaration, or `nullptr` on error.
         * @details Differentiates between general processing instructions and the XML declaration, handling attributes
         * for the latter.
         */
        char_t *parse_question(char_t *str, XmlNodeBase *&ref_cursor, unsigned int optmsk, char_t endch);

        /**
         * @brief The main parsing function that constructs the XML document tree from a buffer.
         * @param[in,out] str A pointer to the beginning of the XML data buffer.
         * @param[in,out] root The root `XmlNodeBase` where the parsed tree will be attached.
         * @param[in] optmsk A bitmask of `xml_parse_option` flags controlling parsing behavior.
         * @param[in] endch The last character of the original buffer, used for boundary checks.
         * @return A pointer to the position in the buffer after the parsed tree, or `nullptr` on error.
         * @details This function iteratively parses elements, attributes, PCDATA, comments, and processing
         * instructions, building the `XmlNodeBase` hierarchy linked to the `root`.
         * @note This is the core recursive parsing logic.
         */
        char_t *parse_tree(char_t *str, XmlNodeBase *root, unsigned int optmsk, char_t endch);

        /**
         * @brief Skips the Byte Order Mark (BOM) from the beginning of the input string.
         * @param[in] str A pointer to the beginning of the XML buffer.
         * @return A pointer to the position immediately after the BOM if present, otherwise the original pointer.
         * @note This function is overloaded for `char_t` (UTF-8) and `wchar_t` (UTF-16) modes.
         */
#ifdef LUMEX_XML_WCHAR_MODE
        static char_t *parse_skip_bom(char_t *str);
#else
        static char_t *parse_skip_bom(char_t *str);
#endif

        /**
         * @brief Checks if any of the given node's siblings (including itself) are element nodes.
         * @param[in] node The starting `XmlNodeBase` to check.
         * @return `true` if an element node is found among siblings, `false` otherwise.
         */
        static bool has_element_node_siblings(XmlNodeBase *node);

        /**
         * @brief Parses an XML buffer and builds the document tree.
         * @param[in,out] buffer A pointer to the mutable XML data buffer.
         * @param[in] length The size of the buffer in `char_t` units.
         * @param[in,out] xmldoc A pointer to the `XmlDocumentBase` associated with the parsing operation.
         * @param[in,out] root The root `XmlNodeBase` where the parsed document structure will be attached.
         * @param[in] optmsk A bitmask of `xml_parse_option` flags.
         * @return An `XmlParseResult` object indicating the parsing status and error details.
         * @details This is the primary entry point for parsing operations within the `XmlParser` component.
         *          It handles BOM skipping, buffer termination, actual tree parsing, and final result verification.
         */
        static XmlParseResult
        parse(char_t *buffer, size_t length, Document::XmlDocumentBase *xmldoc, XmlNodeBase *root, unsigned int optmsk);
      };
    } // namespace Text
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_PARSER_HPP
