#ifndef LUMEX_XML_NODE_BASE_HPP
#define LUMEX_XML_NODE_BASE_HPP

#include "lumex/xml/attribute/XmlAttributeBase.hpp"
#include "lumex/xml/types/XmlTypes.hpp"

using namespace Lumex::Xml::Types;
using namespace Lumex::Xml::Attribute;

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace Node
    {
      struct XmlNodeBase {
        XmlNodeBase(XmlMemoryPage *page, Types::xml_node_type type) : header(LUMEX_XML_GETHEADER_IMPL(this, page, type))
        {}

        uintptr_t header; // NOLINT(misc-non-private-member-variables-in-classes)

        char_t *name{};  // NOLINT(misc-non-private-member-variables-in-classes)
        char_t *value{}; // NOLINT(misc-non-private-member-variables-in-classes)

        XmlNodeBase *parent{}; // NOLINT(misc-non-private-member-variables-in-classes)

        XmlNodeBase *first_child{}; // NOLINT(misc-non-private-member-variables-in-classes)

        XmlNodeBase *prev_sibling_c{}; // NOLINT(misc-non-private-member-variables-in-classes)
        XmlNodeBase *next_sibling{};   // NOLINT(misc-non-private-member-variables-in-classes)

        XmlAttributeBase *first_attribute{}; // NOLINT(misc-non-private-member-variables-in-classes)
      };
    } // namespace Node
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_NODE_BASE_HPP
