#ifndef LUMEX_XML_ALLOCATOR_HPP
#define LUMEX_XML_ALLOCATOR_HPP

#include "lumex/LumexExport.hpp"

#include "LumexXmlMemoryPage.hpp"

#include "lumex/xml/types/LumexXmlTypes.hpp"

using Lumex::Xml::Types::char_t;

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace Memory
    {
      struct LUMEX_API xml_allocator_t {
        xml_allocator_t(xml_mem_page_t *root);

        xml_mem_page_t *allocate_page(size_t data_size);

        static void deallocate_page(xml_mem_page_t *page);

        void *allocate_memory_oob(size_t size, xml_mem_page_t *&out_page);

        void *allocate_memory(size_t size, xml_mem_page_t *&out_page);

        void *allocate_object(size_t size, xml_mem_page_t *&out_page);

        void deallocate_memory(void *ptr, size_t size, xml_mem_page_t *page);

        char_t *allocate_string(size_t length);

        void deallocate_string(char_t *string);

        xml_mem_page_t *m_root; // NOLINT(misc-non-private-member-variables-in-classes)
        size_t m_busy_size;     // NOLINT(misc-non-private-member-variables-in-classes)
      };
    } // namespace detail
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_ALLOCATOR_HPP
