#ifndef LUMEX_XML_DOCUMENT_HPP
#define LUMEX_XML_DOCUMENT_HPP

#include "lumex/xml/memory/LumexXmlAllocator.hpp"
#include "lumex/xml/memory/LumexXmlMemoryPage.hpp"
#include "lumex/xml/node/LumexXmlNode.hpp"

using namespace Lumex::Xml::Node;
using namespace Lumex::Xml::Memory;

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace Document
    {
      struct xml_document_t : public xml_node_t, public xml_allocator_t {
        xml_document_t(xml_mem_page_t *page) : xml_node_t(page, node_document), xml_allocator_t(page) {}

        char_t const *buffer{}; // NOLINT(misc-non-private-member-variables-in-classes)

        xml_extra_buffer *extra_buffers{}; // NOLINT(misc-non-private-member-variables-in-classes)
      };
    } // namespace Document
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_DOCUMENT_HPP
