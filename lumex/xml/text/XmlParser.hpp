#ifndef LUMEX_XML_PARSER_HPP
#define LUMEX_XML_PARSER_HPP

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
      struct XmlParser {
        XmlAllocator *alloc;             // NOLINT(misc-non-private-member-variables-in-classes)
        char_t *error_offset{};          // NOLINT(misc-non-private-member-variables-in-classes)
        xml_parse_status error_status{}; // NOLINT(misc-non-private-member-variables-in-classes)

        XmlParser(XmlAllocator *alloc_);

        // DOCTYPE consists of nested sections of the following possible types:
        // <!-- ... -->, <? ... ?>, "...", '...'
        // <![...]]>
        // <!...>
        // First group can not contain nested groups
        // Second group can contain nested groups of the same type
        // Third group can contain all other groups
        char_t *parse_doctype_primitive(char_t *str);

        char_t *parse_doctype_group(char_t *str, char_t endch);

        char_t *parse_doctype_ignore(char_t *str);

        char_t *parse_exclamation(char_t *str, XmlNodeBase *cursor, unsigned int optmsk, char_t endch);

        char_t *parse_question(char_t *str, XmlNodeBase *&ref_cursor, unsigned int optmsk, char_t endch);

        char_t *parse_tree(char_t *str, XmlNodeBase *root, unsigned int optmsk, char_t endch);

#ifdef LUMEX_XML_WCHAR_MODE
        static char_t *parse_skip_bom(char_t *str);
#else
        static char_t *parse_skip_bom(char_t *str);
#endif

        static bool has_element_node_siblings(XmlNodeBase *node);

        static XmlParseResult
        parse(char_t *buffer, size_t length, Document::XmlDocumentBase *xmldoc, XmlNodeBase *root, unsigned int optmsk);
      };
    } // namespace Text
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_PARSER_HPP
