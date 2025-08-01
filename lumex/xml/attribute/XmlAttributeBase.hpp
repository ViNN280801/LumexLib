#ifndef LUMEX_XML_ATTRIBUTE_BASE_HPP
#define LUMEX_XML_ATTRIBUTE_BASE_HPP

#include "lumex/xml/memory/XmlMemoryPage.hpp"
#include "lumex/xml/types/XmlTypes.hpp"

using namespace Lumex::Xml::Memory;
using namespace Lumex::Xml::Types;

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace Attribute
    {
      struct XmlAttributeBase {
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
    }
  }
}

#endif // !LUMEX_XML_ATTRIBUTE_BASE_HPP
