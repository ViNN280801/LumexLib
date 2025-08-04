#ifndef LUMEX_XML_NODE_BASE_HPP
#define LUMEX_XML_NODE_BASE_HPP

#include "lumex/LumexExport.hpp"

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
      struct LUMEX_API XmlNodeBase {
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

      struct LUMEX_API name_null_sentry { // NOLINT(cppcoreguidelines-special-member-functions)
        XmlNodeBase *node{};              // NOLINT(misc-non-private-member-variables-in-classes)
        char_t *name{};                   // NOLINT(misc-non-private-member-variables-in-classes)

        name_null_sentry(XmlNodeBase *node_) : node(node_), name(node_->name) { node->name = nullptr; }

        ~name_null_sentry() { node->name = name; }
      };

      LUMEX_API
      XmlNodeBase *allocate_node(XmlAllocator &alloc, xml_node_type type);

      LUMEX_API void destroy_node(XmlNodeBase *n, XmlAllocator &alloc);

      LUMEX_API
      void append_node(XmlNodeBase *child, XmlNodeBase *node);

      LUMEX_API
      void prepend_node(XmlNodeBase *child, XmlNodeBase *node);

      LUMEX_API
      void insert_node_after(XmlNodeBase *child, XmlNodeBase *node);

      LUMEX_API
      void insert_node_before(XmlNodeBase *child, XmlNodeBase *node);

      LUMEX_API
      void remove_node(XmlNodeBase *node);

      LUMEX_API
      XmlNodeBase *append_new_node(XmlNodeBase *node, XmlAllocator &alloc, xml_node_type type = node_element);

      LUMEX_PUBLIC_API
      void append_attribute(XmlAttributeBase *attr, XmlNodeBase *node);

      LUMEX_API
      XmlAttributeBase *append_new_attribute(XmlNodeBase *node, XmlAllocator &alloc);

      LUMEX_API
      void node_copy_contents(XmlNodeBase *dn_, XmlNodeBase *sn_, // NOLINT(misc-use-internal-linkage)
                              XmlAllocator *shared_alloc);

      LUMEX_API
      void node_copy_tree(XmlNodeBase *dn_, XmlNodeBase *sn_);

      LUMEX_API
      bool node_is_ancestor(XmlNodeBase *parent, XmlNodeBase *node);
    } // namespace Node
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_NODE_BASE_HPP
