#ifndef LUMEX_XML_MEMORY_PAGE_HPP
#define LUMEX_XML_MEMORY_PAGE_HPP

#include "lumex/LumexExport.hpp"

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace Memory
    {
      // Forward declarations
      struct xml_allocator_t;

      struct LUMEX_API xml_mem_page_t {
        static xml_mem_page_t *
        construct(void *memory)
        {
          auto *result       = static_cast<xml_mem_page_t *>(memory);

          result->allocator  = nullptr;
          result->prev       = nullptr;
          result->next       = nullptr;
          result->busy_size  = 0;
          result->freed_size = 0;

          return result;
        }

        xml_allocator_t *allocator;

        xml_mem_page_t *prev;
        xml_mem_page_t *next;

        size_t busy_size;
        size_t freed_size;
      };

      static size_t const kdefault_xml_memory_page_size = 32768 - sizeof(xml_mem_page_t); ///< Size of the memory page
    } // namespace detail
  } // namespace Core
} // namespace Lumex

#endif // !LUMEX_XML_MEMORY_PAGE_HPP
