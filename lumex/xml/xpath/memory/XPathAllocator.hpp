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
        /**
         * @brief Allocator for XPath-related data structures.
         * @details This allocator manages memory in blocks, optimized for frequent small
         *          allocations typical in XPath evaluation. It maintains a linked list
         *          of `XPathMemoryBlock`s and allocates sequentially within the current block.
         *          It provides functions for allocation, reallocation, and reverting to a
         *          previous allocation state, crucial for backtracking in parsing or evaluation.
         *
         * @note This allocator is designed for performance and assumes a single-threaded
         *       usage context. It does not perform thread synchronization internally.
         * @warning Memory allocated by `XPathAllocator` is managed internally; direct `free`
         *          or `delete` calls on pointers obtained from this allocator should be avoided.
         *          Memory is released when `release()` is called or when `revert()` is used.
         */
        struct LUMEX_API XPathAllocator {
          /// @brief Pointer to the current (root) memory block.
          XPathMemoryBlock *m_root{}; // NOLINT(misc-non-private-member-variables-in-classes)
          /// @brief Current allocation offset within the root memory block.
          size_t m_root_size{}; // NOLINT(misc-non-private-member-variables-in-classes)
          /// @brief Optional pointer to a boolean flag set to `true` on allocation failure.
          bool *m_error{}; // NOLINT(misc-non-private-member-variables-in-classes)

          /**
           * @brief Constructs an XPathAllocator.
           * @details Initializes the allocator with a root memory block and an optional
           *          error flag. All subsequent allocations will occur within this block
           *          or newly allocated blocks linked from it.
           * @param root A pointer to the initial `XPathMemoryBlock` to use for allocations.
           *             Ownership is retained by the caller.
           * @param error An optional pointer to a boolean flag. If provided, this flag
           *              will be set to `true` if an allocation failure occurs.
           */
          XPathAllocator(XPathMemoryBlock *root, bool *error = nullptr);

          /**
           * @brief Allocates a block of memory of the specified size.
           * @details Attempts to allocate `size` bytes. If the current memory block
           *          does not have enough space, a new `XPathMemoryBlock` is allocated
           *          and prepended to the block list.
           * @param size The number of bytes to allocate.
           * @return A `void*` pointer to the newly allocated memory, or `nullptr` if
           *         allocation fails (and `m_error` is set if provided).
           * @note The allocated memory is aligned to `kxpath_memory_block_alignment`.
           * @throws Does not throw exceptions; allocation failures are indicated by returning `nullptr`.
           */
          void *allocate(size_t size);

          /**
           * @brief Reallocates a previously allocated block of memory.
           * @details Attempts to resize a memory block. This function is optimized to
           *          resize only the most recently allocated object if it is located at
           *          the end of the current memory block. If the reallocation cannot be
           *          performed in-place, a new block is allocated, and data is copied.
           * @param ptr A pointer to the memory block to reallocate. Must be `nullptr` for
           *            the first allocation, or a pointer previously returned by `allocate`
           *            or `reallocate`.
           * @param old_size The previous size of the memory block in bytes.
           * @param new_size The new desired size of the memory block in bytes.
           * @return A `void*` pointer to the reallocated memory block, or `nullptr` if
           *         reallocation fails. The returned pointer might be different from `ptr`
           *         if a new block was allocated.
           * @note This function primarily supports growing the last allocated object.
           * @warning Assertions (`LUMEX_ASSERT`) are used to enforce that `ptr` corresponds
           *          to the last allocated object.
           */
          void *reallocate(void *ptr, size_t old_size, size_t new_size);

          /**
           * @brief Reverts the allocator's state to a previously captured state.
           * @details This function is used to undo allocations by freeing any memory blocks
           *          that were allocated after the `state` was captured. It restores the
           *          `m_root` and `m_root_size` to the values from the `state` object.
           * @param state A constant reference to an `XPathAllocator` object representing
           *              the desired state to revert to.
           * @note This is useful for transactional parsing or evaluation where allocations
           *       need to be rolled back on error.
           */
          void revert(XPathAllocator const &state);

          /**
           * @brief Releases all dynamically allocated memory blocks managed by this allocator.
           * @details Iterates through the linked list of `XPathMemoryBlock`s (excluding the
           *          initial root block, which is assumed to be caller-managed if `m_root->next` is `nullptr`)
           *          and frees them. This should be called when the allocator is no longer needed.
           * @note This function is marked `noexcept` and does not throw exceptions.
           * @warning Assumes that the very first `m_root` block is potentially static or stack-allocated
           *          and only frees subsequent blocks if `m_root->next` is not `nullptr`.
           */
          void release() const noexcept;
        };

        /**
         * @brief A RAII helper to capture and restore XPathAllocator state.
         * @details This struct uses the Resource Acquisition Is Initialization (RAII) pattern
         *          to automatically revert an `XPathAllocator` to its state at the time
         *          of `XPathAllocatorCapture` construction when the `XPathAllocatorCapture`
         *          object goes out of scope. This is useful for scoped memory management
         *          and automatic cleanup in functions.
         */
        struct LUMEX_API XPathAllocatorCapture { // NOLINT(cppcoreguidelines-special-member-functions)
                                                 /**
                                                  * @brief Constructs an XPathAllocatorCapture.
                                                  * @details Captures the current state of the provided `XPathAllocator` (`alloc`).
                                                  *          When this `XPathAllocatorCapture` object is destroyed, the `alloc`
                                                  *          will be reverted to this captured state.
                                                  * @param alloc A pointer to the `XPathAllocator` whose state is to be captured.
                                                  *              Ownership of `alloc` is not transferred.
                                                  */
          XPathAllocatorCapture(XPathAllocator *alloc);
          /**
           * @brief Destroys the XPathAllocatorCapture object.
           * @details When this object is destroyed, it automatically calls `revert()` on
           *          the target `XPathAllocator` (`_target`) to restore its state to
           *          what it was when this `XPathAllocatorCapture` object was constructed.
           */
          ~XPathAllocatorCapture();

          /// @brief Pointer to the XPathAllocator whose state is being managed.
          XPathAllocator *_target; // NOLINT(misc-non-private-member-variables-in-classes)
          /// @brief The captured state of the XPathAllocator.
          XPathAllocator _state; // NOLINT(misc-non-private-member-variables-in-classes)
        };
      } // namespace Memory
    } // namespace XPath
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_XPATH_ALLOCATOR_HPP
