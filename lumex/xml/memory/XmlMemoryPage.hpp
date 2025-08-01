#ifndef LUMEX_XML_MEMORY_PAGE_HPP
#define LUMEX_XML_MEMORY_PAGE_HPP

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace Memory
    {
      // Forward declarations (do not touch this fwd decl !!!)
      struct XmlAllocator;

      struct XmlMemoryPage {
        static XmlMemoryPage *construct(void *memory);

        XmlAllocator *allocator;

        XmlMemoryPage *prev;
        XmlMemoryPage *next;

        size_t busy_size;
        size_t freed_size;
      };

      static size_t const kdefault_xml_memory_page_size = 32768 - sizeof(XmlMemoryPage); ///< Size of the memory page

    } // namespace Memory
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_MEMORY_PAGE_HPP
