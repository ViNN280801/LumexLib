#ifndef LUMEX_XML_ALLOCATOR_HPP
#define LUMEX_XML_ALLOCATOR_HPP

#include "lumex/LumexExport.hpp"

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
      /**
       * @brief Manages memory allocation and deallocation for XML nodes, attributes, and strings within a document.
       * @details `XmlAllocator` operates using a page-based memory management system. It allocates fixed-size memory
       * pages and then doles out smaller blocks from these pages for various XML data structures. When a page is full,
       * a new one is allocated and linked. It also handles out-of-band allocations for very large data blocks (e.g.,
       * long strings or large nodes) that do not fit into standard pages.
       * @note This allocator is designed for performance and efficiency, minimizing overhead by avoiding frequent
       *       system memory calls and managing memory in contiguous blocks.
       * @see XmlMemoryPage
       * @see XmlDocumentBase
       */
      struct LUMEX_API XmlAllocator {
        /**
         * @brief Constructs an `XmlAllocator` associated with a root memory page.
         * @param[in] root A pointer to the initial `XmlMemoryPage` that this allocator will manage. This page is
         * typically embedded within the `XmlDocument` object itself.
         * @details The allocator starts managing memory from the `root` page. `m_busy_size` is initialized to the
         *          `busy_size` of the root page, indicating how much of it is already in use (e.g., by the document
         * base object).
         */
        XmlAllocator(XmlMemoryPage *root);

        /**
         * @brief Allocates a new `XmlMemoryPage` of a specified data size.
         * @param[in] data_size The desired size of the data region within the new page, in bytes. The total allocated
         *                    size will include space for `XmlMemoryPage` metadata.
         * @return A pointer to the newly allocated and constructed `XmlMemoryPage`, or `nullptr` if memory allocation
         * fails.
         * @note The allocated page is raw memory; it is not yet linked into the allocator's page chain.
         * @warning The returned page must be explicitly deallocated using `deallocate_page` when no longer needed.
         */
        XmlMemoryPage *allocate_page(size_t data_size);

        /**
         * @brief Deallocates a memory page previously allocated by this allocator.
         * @param[in] page A pointer to the `XmlMemoryPage` to deallocate.
         * @details This function frees the underlying memory block associated with the `page`. It is a static method
         *          because a page's deallocation is independent of a specific allocator instance once it's removed
         *          from the chain.
         * @warning Calling this on an already freed page or a page not allocated by `malloc` (or equivalent) leads to
         * undefined behavior.
         * @note This only deallocates the page's memory; it does not unlink the page from the allocator's chain (that
         * is handled by `deallocate_memory`).
         */
        static void deallocate_page(XmlMemoryPage *page);

        /**
         * @brief Allocates memory for large objects or strings that do not fit in the current memory page
         * (out-of-band).
         * @param[in] size The number of bytes to allocate.
         * @param[out] out_page A reference to an `XmlMemoryPage` pointer. On success, this will point to the newly
         * allocated page containing the allocated memory block.
         * @return A pointer to the allocated memory block, or `nullptr` if allocation fails.
         * @details This function is called internally by `allocate_memory` when the requested `size` exceeds the
         * remaining space in the current `m_root` page. It intelligently allocates either a default-sized page or a
         * custom-sized page based on `size` and links it into the allocator's internal page chain.
         * @note Marked `LUMEX_ATTRIBUTE_NOINLINE` to prevent inlining for better code size and potentially faster debug
         * builds.
         */
        LUMEX_ATTRIBUTE_NOINLINE void *allocate_memory_oob(size_t size, XmlMemoryPage *&out_page);

        /**
         * @brief Allocates a block of memory from the allocator's managed pages.
         * @param[in] size The number of bytes to allocate.
         * @param[out] out_page A reference to an `XmlMemoryPage` pointer. On success, this will point to the
         * `XmlMemoryPage` from which the memory was allocated.
         * @return A pointer to the allocated memory block, or `nullptr` if allocation fails.
         * @details This is the primary function for allocating memory for nodes and attributes. It attempts to allocate
         *          from the current `m_root` page first. If there isn't enough space, it defers to
         * `allocate_memory_oob` to get a new page.
         * @note Memory is allocated from contiguous blocks within pages. This function is not thread-safe without
         * external synchronization.
         */
        void *allocate_memory(size_t size, XmlMemoryPage *&out_page);

        /**
         * @brief Allocates memory for an object from the allocator's managed pages.
         * @param[in] size The size of the object to allocate in bytes.
         * @param[out] out_page A reference to an `XmlMemoryPage` pointer. On success, this will point to the
         * `XmlMemoryPage` from which the object's memory was allocated.
         * @return A pointer to the allocated memory for the object, or `nullptr` if allocation fails.
         * @details This function is a wrapper around `allocate_memory`, primarily used to allocate memory for
         * `XmlNodeBase` and `XmlAttributeBase` objects.
         */
        void *allocate_object(size_t size, XmlMemoryPage *&out_page);

        /**
         * @brief Deallocates a block of memory previously allocated by this allocator.
         * @param[in] ptr A pointer to the memory block to deallocate.
         * @param[in] size The size of the memory block in bytes. This size is typically the same size that was
         * requested during allocation.
         * @param[in] page A pointer to the `XmlMemoryPage` from which `ptr` was originally allocated.
         * @details This function marks the `size` bytes within the `page` as freed. If a page becomes entirely free
         *          (i.e., `freed_size == busy_size`), and it's not the root page, the page is deallocated and unlinked
         *          from the page chain. The root page is reset instead of deallocated.
         * @note This allocator does not coalesce free blocks; it only tracks total freed size per page.
         * @warning Incorrect `size` or `page` parameters can lead to memory corruption. Calling this on memory not
         * owned by this allocator or already freed is undefined behavior.
         */
        void deallocate_memory(void *ptr, size_t size, XmlMemoryPage *page);

        /**
         * @brief Allocates memory for a null-terminated string.
         * @param[in] length The number of characters (not bytes) required for the string, excluding the null
         * terminator.
         * @return A pointer to the allocated `char_t` buffer where the string content can be written. Returns `nullptr`
         * on failure.
         * @details This function allocates space for the string plus a small header (`xml_mem_str_header_t`). The total
         *          allocation size is rounded up to `kxml_memory_block_alignment`. The header stores information about
         *          the string's location within its memory page and its full allocated size, which is used for
         * deallocation.
         * @note The string memory is managed by this allocator. The caller is responsible for writing the null
         * terminator.
         * @warning The returned `char_t*` points to `header + 1`. The `header` itself is part of the allocated block.
         */
        char_t *allocate_string(size_t length);

        /**
         * @brief Deallocates a string previously allocated by `allocate_string`.
         * @param[in] string A pointer to the `char_t` buffer returned by `allocate_string`.
         * @details This function retrieves the hidden `xml_mem_str_header_t` preceding the `string` pointer,
         *          uses its metadata (page offset and full size) to determine the original allocation block
         *          and its owning `XmlMemoryPage`, and then calls `deallocate_memory` to free the block.
         * @warning Calling this on a `string` not allocated by `allocate_string` or already freed leads to undefined
         * behavior.
         */
        void deallocate_string(char_t *string);

        /**
         * @brief Pointer to the root (current active) memory page of this allocator.
         * @details This pointer always points to the page from which new memory allocations are currently being served.
         *          It advances when the current page is full and a new page is allocated.
         * @note This is a public member for internal library operations. Direct modification is discouraged.
         */
        XmlMemoryPage *m_root; // NOLINT(misc-non-private-member-variables-in-classes)
        /**
         * @brief The amount of memory currently used within the current `m_root` memory page.
         * @details This tracks the "high-water mark" of allocated memory within `m_root`. New allocations
         *          are appended after `m_busy_size`. When `m_busy_size` reaches `kdefault_xml_memory_page_size`,
         *          a new page is typically allocated.
         * @note This is a public member for internal library operations. Direct modification is discouraged.
         */
        size_t m_busy_size; // NOLINT(misc-non-private-member-variables-in-classes)
      };

      template <typename Object>
      /**
       * @brief Retrieves the `XmlAllocator` instance that manages the memory for a given object.
       * @tparam Object The type of the object (e.g., `XmlNodeBase`, `XmlAttributeBase`, `xml_mem_str_header_t`).
       * @param[in] object A pointer to the object for which to retrieve the allocator. Must not be `nullptr`.
       * @return A reference to the `XmlAllocator` that owns `object`'s memory.
       * @details This function determines the `XmlMemoryPage` that contains `object` and then accesses the
       *          `allocator` pointer stored within that page. Since each page points back to its managing allocator
       *          (which is typically the `XmlDocumentBase`), this provides a way to get the correct allocator for any
       *          object within an XML document.
       * @throws `LUMEX_ASSERT` if `object` is `nullptr` (debug builds only).
       * @note This is an internal utility function.
       */
      LUMEX_API inline XmlAllocator &
      get_allocator(Object const *object)
      {
        LUMEX_ASSERT(object);

        return *LUMEX_XML_GETPAGE(object)->allocator; // NOLINT(cppcoreguidelines-pro-type-const-cast)
      }
    } // namespace Memory
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_ALLOCATOR_HPP
