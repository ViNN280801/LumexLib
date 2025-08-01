#ifndef LUMEX_XML_DOCUMENT_HPP
#define LUMEX_XML_DOCUMENT_HPP

#include "lumex/core/utility/LumexMacros.hpp"

#include "lumex/xml/memory/XmlAllocator.hpp"
#include "lumex/xml/memory/XmlMemoryPage.hpp"
#include "lumex/xml/node/XmlNodeBase.hpp"

using namespace Lumex::Xml::Node;
using namespace Lumex::Xml::Memory;

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace Document
    {
      struct XmlDocumentBase : public XmlNodeBase, public XmlAllocator {
        XmlDocumentBase(XmlMemoryPage *page);

        char_t const *buffer{}; // NOLINT(misc-non-private-member-variables-in-classes)

        xml_extra_buffer *extra_buffers{}; // NOLINT(misc-non-private-member-variables-in-classes)
      };

      template <typename Object>
      inline XmlDocumentBase &
      get_document(Object const *object)
      {
        LUMEX_ASSERT(object);

        return *static_cast<XmlDocumentBase *>(  // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
          LUMEX_XML_GETPAGE(object)->allocator); // NOLINT(cppcoreguidelines-pro-type-const-cast)
      }
    } // namespace Document
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_DOCUMENT_HPP
