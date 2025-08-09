#ifndef LUMEX_XML_ATTRIBUTE_BASE_HPP
#define LUMEX_XML_ATTRIBUTE_BASE_HPP

#include "lumex/LumexExport.hpp"

#include "lumex/xml/memory/XmlAllocator.hpp"
#include "lumex/xml/memory/XmlMemoryPage.hpp"

using namespace Lumex::Xml::Memory;
using namespace Lumex::Xml::Types;

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace Node
    {
      struct XmlNodeBase;
    }
    namespace Attribute
    {
      struct LUMEX_API XmlAttributeBase {
        XmlAttributeBase(XmlMemoryPage *page)
        {
          header = LUMEX_XML_GETHEADER_IMPL(this, page, 0); // NOLINT(cppcoreguidelines-prefer-member-initializer)
        }

        uintptr_t header; // NOLINT(misc-non-private-member-variables-in-classes)

        char_t *name{};  // NOLINT(misc-non-private-member-variables-in-classes)
        char_t *value{}; // NOLINT(misc-non-private-member-variables-in-classes)

        XmlAttributeBase *prev_attribute_c{}; // NOLINT(misc-non-private-member-variables-in-classes)
        XmlAttributeBase *next_attribute{};   // NOLINT(misc-non-private-member-variables-in-classes)
      };

      LUMEX_API
      XmlAttributeBase *allocate_attribute(XmlAllocator &alloc);

      LUMEX_API
      void destroy_attribute(XmlAttributeBase *attr, XmlAllocator &alloc);

      LUMEX_API
      void prepend_attribute(XmlAttributeBase *attr, Node::XmlNodeBase *node);

      LUMEX_API
      void insert_attribute_after(XmlAttributeBase *attr, XmlAttributeBase *place, Node::XmlNodeBase *node);

      LUMEX_API
      void insert_attribute_before(XmlAttributeBase *attr, XmlAttributeBase *place, Node::XmlNodeBase *node);

      LUMEX_API
      void remove_attribute(XmlAttributeBase *attr, Node::XmlNodeBase *node);

      LUMEX_API
      void node_copy_attribute(XmlAttributeBase *da_, XmlAttributeBase *sa_);
    }
  }
}

#endif // !LUMEX_XML_ATTRIBUTE_BASE_HPP
