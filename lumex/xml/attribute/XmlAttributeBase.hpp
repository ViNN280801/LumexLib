#ifndef LUMEX_XML_ATTRIBUTE_BASE_HPP
#define LUMEX_XML_ATTRIBUTE_BASE_HPP

#include "lumex/LumexExport.hpp"

#include "lumex/xml/memory/XmlAllocator.hpp"
#include "lumex/xml/memory/XmlMemoryPage.hpp"

using namespace Lumex::Xml::Memory;
using namespace Lumex::Xml::Types;

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace Node
    {
      struct XmlNodeBase;
    }
    namespace Attribute
    {
      /**
       * @brief Base structure for representing an XML attribute.
       * @details This structure holds the raw pointers to the attribute's name and value strings,
       *          along with header information used for memory management and linking in a doubly-linked list.
       *          It is an internal representation primarily used by `XmlAttribute` and memory allocation routines.
       * @note This structure manages raw pointers; its lifecycle and memory management are handled by `XmlAllocator`.
       * @see XmlAttribute
       * @see XmlAllocator
       */
      struct LUMEX_API XmlAttributeBase {
        /**
         * @brief Constructs an `XmlAttributeBase` and initializes its header.
         * @param[in] page A pointer to the `XmlMemoryPage` from which this attribute's memory was allocated.
         * @details The constructor sets up the `header` field which encodes metadata about the attribute, including its
         * associated memory page. Name and value pointers are zero-initialized.
         */
        XmlAttributeBase(XmlMemoryPage *page)
        {
          header = LUMEX_XML_GETHEADER_IMPL(this, page, 0); // NOLINT(cppcoreguidelines-prefer-member-initializer)
        }

        /**
         * @brief Header containing memory page information and allocation flags.
         * @details This `uintptr_t` stores metadata critical for the allocator, such as whether name/value strings are
         * in-situ or separately allocated.
         * @note This is a public member for internal library access and direct manipulation should be avoided unless
         * deeply understanding the memory model.
         */
        uintptr_t header; // NOLINT(misc-non-private-member-variables-in-classes)

        /**
         * @brief Pointer to the null-terminated C-style string representing the attribute's name.
         * @details This pointer can point to memory within the `XmlMemoryPage` or to separately allocated memory if the
         * name is too long to fit in-situ.
         * @note This pointer is not owned by this structure; its lifetime is managed by the associated `XmlAllocator`.
         */
        char_t *name{}; // NOLINT(misc-non-private-member-variables-in-classes)
        /**
         * @brief Pointer to the null-terminated C-style string representing the attribute's value.
         * @details Similar to `name`, this pointer can point to in-situ or separately allocated memory.
         * @note This pointer is not owned by this structure; its lifetime is managed by the associated `XmlAllocator`.
         */
        char_t *value{}; // NOLINT(misc-non-private-member-variables-in-classes)

        /**
         * @brief Pointer to the previous attribute in the doubly-linked list of attributes belonging to a parent node.
         * @details This forms part of a circular linked list for efficient traversal of attributes.
         * @note This pointer is managed by the XML node's attribute list operations.
         */
        XmlAttributeBase *prev_attribute_c{}; // NOLINT(misc-non-private-member-variables-in-classes)
        /**
         * @brief Pointer to the next attribute in the doubly-linked list of attributes belonging to a parent node.
         * @details This pointer points to the subsequent attribute, or `nullptr` if it is the last attribute in the
         * list.
         * @note This pointer is managed by the XML node's attribute list operations.
         */
        XmlAttributeBase *next_attribute{}; // NOLINT(misc-non-private-member-variables-in-classes)
      };

      /**
       * @brief Allocates a new `XmlAttributeBase` object using the provided allocator.
       * @details This function allocates memory for an `XmlAttributeBase` structure from the given `XmlAllocator`
       *          and constructs it in-place. It's the primary way to create new attribute objects within the XML memory
       * model.
       * @param[in,out] alloc The `XmlAllocator` to use for memory allocation.
       * @return A pointer to the newly allocated `XmlAttributeBase` object, or `nullptr` if memory allocation fails.
       * @warning The returned pointer is owned by the `XmlAllocator` and must not be `delete`d directly.
       * @throws std::bad_alloc If the allocator runs out of memory (though the current implementation returns `nullptr`
       * on failure).
       * @see destroy_attribute
       */
      LUMEX_API
      XmlAttributeBase *allocate_attribute(XmlAllocator &alloc);

      /**
       * @brief Destroys and deallocates an `XmlAttributeBase` object.
       * @details This function deallocates the attribute's name and value strings if they were separately allocated,
       *          and then deallocates the `XmlAttributeBase` structure itself using the provided allocator.
       *          It is crucial for proper memory cleanup within the XML memory model.
       * @param[in] attr A pointer to the `XmlAttributeBase` object to destroy. This pointer must have been allocated
       * via `allocate_attribute`.
       * @param[in,out] alloc The `XmlAllocator` that was used to allocate `attr` and its associated strings.
       * @note This function handles checking the `header` flags to determine if name/value strings need separate
       * deallocation.
       * @warning Calling this on an `attr` not allocated by `alloc` or already deallocated leads to undefined behavior.
       * @see allocate_attribute
       */
      LUMEX_API
      void destroy_attribute(XmlAttributeBase *attr, XmlAllocator &alloc);

      /**
       * @brief Prepends an `XmlAttributeBase` to the beginning of a node's attribute list.
       * @details This function inserts `attr` as the new first attribute of `node`. It correctly updates
       *          the `first_attribute` pointer of the node and maintains the doubly-linked list structure.
       *          If the node previously had attributes, `attr` becomes the new head, and the old head's
       * `prev_attribute_c` is updated to point to `attr`. If the node had no attributes, `attr` becomes both the first
       * and the "previous" (circularly linked) attribute to itself.
       * @param[in,out] attr The `XmlAttributeBase` to prepend. Must not be `nullptr`.
       * @param[in,out] node The `XmlNodeBase` to which the attribute will be added. Must not be `nullptr`.
       * @note This function assumes `attr` is already allocated and not currently part of any other attribute list.
       */
      LUMEX_API
      void prepend_attribute(XmlAttributeBase *attr, Node::XmlNodeBase *node);

      /**
       * @brief Inserts an `XmlAttributeBase` after a specified existing attribute.
       * @details This function inserts `attr` immediately after `place` in the attribute list of `node`.
       *          It updates the `next_attribute` and `prev_attribute_c` pointers to maintain the doubly-linked list
       * integrity. If `place` is the last attribute, `attr` becomes the new last attribute, updating the circular link.
       * @param[in,out] attr The `XmlAttributeBase` to insert. Must not be `nullptr`.
       * @param[in] place The existing `XmlAttributeBase` after which `attr` will be inserted. Must not be `nullptr`.
       * @param[in,out] node The `XmlNodeBase` whose attribute list is being modified. Must not be `nullptr`.
       * @note It is assumed that `attr` is not currently linked in any list and that `place` belongs to `node`.
       */
      LUMEX_API
      void insert_attribute_after(XmlAttributeBase *attr, XmlAttributeBase *place, Node::XmlNodeBase *node);

      /**
       * @brief Inserts an `XmlAttributeBase` before a specified existing attribute.
       * @details This function inserts `attr` immediately before `place` in the attribute list of `node`.
       *          It updates the `next_attribute` and `prev_attribute_c` pointers to maintain the doubly-linked list
       * integrity. If `place` is the first attribute, `attr` becomes the new first attribute of the node.
       * @param[in,out] attr The `XmlAttributeBase` to insert. Must not be `nullptr`.
       * @param[in] place The existing `XmlAttributeBase` before which `attr` will be inserted. Must not be `nullptr`.
       * @param[in,out] node The `XmlNodeBase` whose attribute list is being modified. Must not be `nullptr`.
       * @note It is assumed that `attr` is not currently linked in any list and that `place` belongs to `node`.
       */
      LUMEX_API
      void insert_attribute_before(XmlAttributeBase *attr, XmlAttributeBase *place, Node::XmlNodeBase *node);

      /**
       * @brief Removes an `XmlAttributeBase` from its parent node's attribute list.
       * @details This function unlinks `attr` from the doubly-linked list of attributes belonging to `node`.
       *          It correctly updates the `next_attribute` and `prev_attribute_c` pointers of its neighbors,
       *          as well as the `first_attribute` and circular `prev_attribute_c` pointers of the node if `attr` was
       * the first or last. After removal, `attr`'s linking pointers are set to `nullptr`.
       * @param[in,out] attr The `XmlAttributeBase` to remove. Must not be `nullptr` and must be part of `node`'s
       * attribute list.
       * @param[in,out] node The `XmlNodeBase` from which the attribute will be removed. Must not be `nullptr`.
       * @warning The memory for `attr` itself is NOT deallocated by this function. It must be explicitly destroyed via
       * `destroy_attribute`.
       */
      LUMEX_API
      void remove_attribute(XmlAttributeBase *attr, Node::XmlNodeBase *node);

      /**
       * @brief Copies the name and value strings from a source `XmlAttributeBase` to a destination `XmlAttributeBase`.
       * @details This utility function handles the string copying, taking into account whether the strings
       *          are stored in-situ or require separate memory allocation. It also optimizes by detecting
       *          if source and destination attributes share the same allocator to avoid unnecessary reallocations.
       * @param[in,out] da_ The destination `XmlAttributeBase` whose name and value will be set. Must not be `nullptr`.
       * @param[in] sa_ The source `XmlAttributeBase` from which name and value will be copied. Must not be `nullptr`.
       * @note This function relies on internal utility functions (`node_copy_string`) for actual string manipulation.
       * @warning This function only copies the string content; it does not handle linking `da_` into an attribute list.
       */
      LUMEX_API
      void node_copy_attribute(XmlAttributeBase *da_, XmlAttributeBase *sa_);
    }
  }
}

#endif // !LUMEX_XML_ATTRIBUTE_BASE_HPP
