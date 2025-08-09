#ifndef LUMEX_XML_XPATH_ALLOCATOR_HPP
#define LUMEX_XML_XPATH_ALLOCATOR_HPP

#include <cstdlib>
#include <cstring>

#include "lumex/LumexExport.hpp"

#include "XPathMemoryBlock.hpp"

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace XPath
    {
      namespace Memory
      {
        struct LUMEX_API XPathAllocator {
          XPathMemoryBlock *m_root{}; // NOLINT(misc-non-private-member-variables-in-classes)
          size_t m_root_size{};       // NOLINT(misc-non-private-member-variables-in-classes)
          bool *m_error{};            // NOLINT(misc-non-private-member-variables-in-classes)

          XPathAllocator(XPathMemoryBlock *root, bool *error = nullptr);

          void *allocate(size_t size);

          void *reallocate(void *ptr, size_t old_size, size_t new_size);

          void revert(XPathAllocator const &state);

          void release() const noexcept;
        };

        struct LUMEX_API XPathAllocatorCapture { // NOLINT(cppcoreguidelines-special-member-functions)
          XPathAllocatorCapture(XPathAllocator *alloc);
          ~XPathAllocatorCapture();

          XPathAllocator *_target; // NOLINT(misc-non-private-member-variables-in-classes)
          XPathAllocator _state;   // NOLINT(misc-non-private-member-variables-in-classes)
        };
      } // namespace Memory
    } // namespace XPath
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_XPATH_ALLOCATOR_HPP
