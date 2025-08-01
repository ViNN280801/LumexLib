#ifndef LUMEX_XML_ALLOCATOR_HPP
#define LUMEX_XML_ALLOCATOR_HPP

#include "lumex/core/utility/LumexAssert.hpp"

#include "lumex/xml/types/XmlTypes.hpp"

#include "XmlMemoryPage.hpp"

using Lumex::Xml::Types::char_t;

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace Memory
    {
      struct XmlAllocator {
        XmlAllocator(XmlMemoryPage *root);

        XmlMemoryPage *allocate_page(size_t data_size);

        static void deallocate_page(XmlMemoryPage *page);

        void *allocate_memory_oob(size_t size, XmlMemoryPage *&out_page);

        void *allocate_memory(size_t size, XmlMemoryPage *&out_page);

        void *allocate_object(size_t size, XmlMemoryPage *&out_page);

        void deallocate_memory(void *ptr, size_t size, XmlMemoryPage *page);

        char_t *allocate_string(size_t length);

        void deallocate_string(char_t *string);

        XmlMemoryPage *m_root; // NOLINT(misc-non-private-member-variables-in-classes)
        size_t m_busy_size;    // NOLINT(misc-non-private-member-variables-in-classes)
      };

      template <typename Object>
      inline XmlAllocator &
      get_allocator(Object const *object)
      {
        LUMEX_ASSERT(object);

        return *LUMEX_XML_GETPAGE(object)->allocator; // NOLINT(cppcoreguidelines-pro-type-const-cast)
      }
    } // namespace Memory
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_ALLOCATOR_HPP
