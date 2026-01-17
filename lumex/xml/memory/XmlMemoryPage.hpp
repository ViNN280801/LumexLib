#ifndef LUMEX_XML_MEMORY_PAGE_HPP
#define LUMEX_XML_MEMORY_PAGE_HPP

#include <cstddef>

#include "lumex/LumexExport.hpp"

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace Memory
    {
      // Forward declarations (do not touch this fwd decl !!!)
      struct XmlAllocator;

      /**
       * @brief Represents a single fixed-size memory page used by the `XmlAllocator`.
       * @details Each `XmlMemoryPage` holds a block of raw memory from which smaller objects (nodes, attributes,
       * strings) are allocated. Pages are linked together to form a chain, managed by an `XmlAllocator`. It contains
       * pointers to the owning allocator, previous/next pages in the chain, and tracks the amount of memory currently
       * in use (`busy_size`) and freed (`freed_size`) within itself.
       * @note This is an internal structure fundamental to the XML library's memory management.
       * @see XmlAllocator
       */
      struct LUMEX_API XmlMemoryPage {
        /**
         * @brief Constructs an `XmlMemoryPage` object in a pre-allocated memory block.
         * @param[in] memory A pointer to the raw memory block where the `XmlMemoryPage` will be constructed.
         * @return A pointer to the newly constructed `XmlMemoryPage` object.
         * @details This static method initializes the fields of `XmlMemoryPage` within the provided `memory` address.
         *          All pointers (`allocator`, `prev`, `next`) are set to `nullptr`, and sizes (`busy_size`,
         * `freed_size`) are set to `0`. This is essentially a placement-new like operation without actually calling
         * `new`.
         * @note The `memory` block must be large enough to contain an `XmlMemoryPage` structure.
         */
        static XmlMemoryPage *construct(void *memory);

        /**
         * @brief Pointer to the `XmlAllocator` that owns and manages this memory page.
         * @details This is crucial for retrieving the correct allocator when an object's memory needs to be
         * deallocated.
         * @note This is a public member for internal library operations.
         */
        XmlAllocator *allocator;

        /**
         * @brief Pointer to the previous `XmlMemoryPage` in the allocator's linked list of pages.
         * @details Forms part of the doubly-linked list that the `XmlAllocator` uses to manage its pages.
         * @note This is a public member for internal library operations.
         */
        XmlMemoryPage *prev;
        /**
         * @brief Pointer to the next `XmlMemoryPage` in the allocator's linked list of pages.
         * @details Forms part of the doubly-linked list that the `XmlAllocator` uses to manage its pages.
         * @note This is a public member for internal library operations.
         */
        XmlMemoryPage *next;

        /**
         * @brief The amount of memory currently in use within this page (allocated to objects).
         * @details This value represents the high-water mark of allocations from the beginning of the page's data area.
         * @note This is a public member for internal library operations.
         */
        size_t busy_size;
        /**
         * @brief The amount of memory that has been deallocated within this page.
         * @details This tracks the total size of freed blocks. When `freed_size` equals `busy_size`, the page is
         * considered empty and can be returned to the system (unless it's the root page).
         * @note This is a public member for internal library operations.
         */
        size_t freed_size;
      };

      /**
       * @brief Defines the default size of a memory page for XML allocation.
       * @details This constant specifies the size of the data region within an `XmlMemoryPage`, excluding the page's
       *          own metadata size. It is set to 32KB minus the size of `XmlMemoryPage` structure itself, to optimize
       *          for common system page sizes or cache lines.
       * @note This value can impact performance and memory footprint.
       */
      static size_t const kdefault_xml_memory_page_size = 32768 - sizeof(XmlMemoryPage); ///< Size of the memory page
    } // namespace Memory
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_MEMORY_PAGE_HPP
