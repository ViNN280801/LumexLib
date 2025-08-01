#ifndef LUMEX_XML_NODE_BASE_HPP
#define LUMEX_XML_NODE_BASE_HPP

#include "lumex/xml/attribute/XmlAttributeBase.hpp"
#include "lumex/xml/memory/XmlAllocator.hpp"
#include "lumex/xml/types/XmlTypes.hpp"

using namespace Lumex::Xml::Memory;
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

      struct name_null_sentry { // NOLINT(cppcoreguidelines-special-member-functions)
        XmlNodeBase *node{};    // NOLINT(misc-non-private-member-variables-in-classes)
        char_t *name{};         // NOLINT(misc-non-private-member-variables-in-classes)

        name_null_sentry(XmlNodeBase *node_) : node(node_), name(node_->name) { node->name = nullptr; }

        ~name_null_sentry() { node->name = name; }
      };

      XmlNodeBase *allocate_node(XmlAllocator &alloc, xml_node_type type);

      void destroy_node(XmlNodeBase *n, XmlAllocator &alloc);

      void append_node(XmlNodeBase *child, XmlNodeBase *node);

      void prepend_node(XmlNodeBase *child, XmlNodeBase *node);

      void insert_node_after(XmlNodeBase *child, XmlNodeBase *node);

      void insert_node_before(XmlNodeBase *child, XmlNodeBase *node);

      void remove_node(XmlNodeBase *node);

      LUMEX_ATTRIBUTE_NOINLINE XmlNodeBase *
      append_new_node(XmlNodeBase *node, XmlAllocator &alloc, xml_node_type type = node_element);

      void node_copy_contents(XmlNodeBase *dn_, XmlNodeBase *sn_, XmlAllocator *shared_alloc);

      void node_copy_tree(XmlNodeBase *dn_, XmlNodeBase *sn_);
    } // namespace Node
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_NODE_BASE_HPP
