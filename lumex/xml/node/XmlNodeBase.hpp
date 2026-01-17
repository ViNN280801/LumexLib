#ifndef LUMEX_XML_NODE_BASE_HPP
#define LUMEX_XML_NODE_BASE_HPP

#include "lumex/LumexExport.hpp"

#include "lumex/xml/attribute/XmlAttributeBase.hpp"
#include "lumex/xml/memory/XmlAllocator.hpp"
#include "lumex/xml/types/XmlTypes.hpp"

using namespace Lumex::Xml::Memory;
using namespace Lumex::Xml::Types;
using namespace Lumex::Xml::Attribute;

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace Node
    {
      /**
       * @brief Base structure representing an XML node in the document tree.
       * @details `XmlNodeBase` holds the fundamental data for any XML node, including its type, name, value,
       *          and pointers for navigating the tree (parent, first child, siblings) and attributes.
       *          It is an internal representation primarily used by `XmlNode` and memory allocation routines.
       * @note This structure manages raw pointers; its lifecycle and memory management are handled by `XmlAllocator`.
       *       The `header` field encapsulates metadata such as the node type and memory page information.
       * @see XmlNode
       * @see XmlAllocator
       */
      struct LUMEX_API XmlNodeBase {
        /**
         * @brief Constructs an `XmlNodeBase` object.
         * @param[in] page A pointer to the `XmlMemoryPage` where this node resides.
         * @param[in] type The `xml_node_type` of this node (e.g., `node_element`, `node_pcdata`).
         * @details Initializes the node's header with the provided memory page and node type.
         */
        XmlNodeBase(XmlMemoryPage *page, Types::xml_node_type type) : header(LUMEX_XML_GETHEADER_IMPL(this, page, type))
        {}

        /// @brief Internal header containing node type, memory page pointer, and allocation flags.
        uintptr_t header; // NOLINT(misc-non-private-member-variables-in-classes)

        /// @brief Pointer to the null-terminated name string of the node. May be `nullptr`.
        char_t *name{}; // NOLINT(misc-non-private-member-variables-in-classes)
        /// @brief Pointer to the null-terminated value string of the node. May be `nullptr`.
        char_t *value{}; // NOLINT(misc-non-private-member-variables-in-classes)

        /// @brief Pointer to the parent node. `nullptr` for the document node.
        XmlNodeBase *parent{}; // NOLINT(misc-non-private-member-variables-in-classes)

        /// @brief Pointer to the first child node. `nullptr` if no children.
        XmlNodeBase *first_child{}; // NOLINT(misc-non-private-member-variables-in-classes)

        /// @brief Pointer to the previous sibling node in the circular doubly-linked list of children.
        ///        For the first child, this points to the last child (circular link).
        XmlNodeBase *prev_sibling_c{}; // NOLINT(misc-non-private-member-variables-in-classes)
        /// @brief Pointer to the next sibling node. `nullptr` for the last child.
        XmlNodeBase *next_sibling{}; // NOLINT(misc-non-private-member-variables-in-classes)

        /// @brief Pointer to the first attribute of this node. `nullptr` if no attributes.
        XmlAttributeBase *first_attribute{}; // NOLINT(misc-non-private-member-variables-in-classes)
      };

      /**
       * @brief A RAII helper to temporarily set a node's name to `nullptr` and restore it upon destruction.
       * @details Used internally during node manipulation to manage string lifetimes and prevent double-frees
       *          or incorrect string references during copy operations.
       */
      struct LUMEX_API name_null_sentry { // NOLINT(cppcoreguidelines-special-member-functions)
        /// @brief Pointer to the `XmlNodeBase` whose name is being managed.
        XmlNodeBase *node{}; // NOLINT(misc-non-private-member-variables-in-classes)
        /// @brief Temporary storage for the original name pointer.
        char_t *name{}; // NOLINT(misc-non-private-member-variables-in-classes)

        /**
         * @brief Constructs a `name_null_sentry` and nullifies the node's name.
         * @param[in,out] node_ The `XmlNodeBase` object whose name will be temporarily set to `nullptr`.
         * @details Stores the original name pointer and sets `node_->name` to `nullptr`.
         */
        name_null_sentry(XmlNodeBase *node_) : node(node_), name(node_->name) { node->name = nullptr; }

        /**
         * @brief Destructor that restores the node's original name.
         * @details Resets `node->name` to its value before the sentry was constructed.
         */
        ~name_null_sentry() { node->name = name; }
      };

      /**
       * @brief Allocates a new `XmlNodeBase` object using the provided allocator.
       * @details This function allocates memory for an `XmlNodeBase` structure from the given `XmlAllocator`'s memory
       * pages. It then constructs the `XmlNodeBase` object in the allocated memory, initializing its header to link
       * back to its memory page.
       * @param[in,out] alloc The `XmlAllocator` instance from which to allocate memory.
       * @param[in] type The `xml_node_type` of the node to allocate.
       * @return A pointer to the newly allocated and constructed `XmlNodeBase` object. Returns `nullptr` if memory
       * allocation fails.
       */
      LUMEX_API
      XmlNodeBase *allocate_node(XmlAllocator &alloc, xml_node_type type);

      /**
       * @brief Destroys an `XmlNodeBase` object and recursively deallocates its children and attributes.
       * @details This function is responsible for releasing all memory associated with the given node,
       *          including its name and value strings, all its attributes, and all its child nodes (recursively).
       *          The memory for the node itself is returned to the `XmlAllocator`.
       * @param[in,out] n A pointer to the `XmlNodeBase` object to destroy. Must not be `nullptr`.
       * @param[in,out] alloc The `XmlAllocator` instance used for deallocation.
       * @note This function is typically called internally by `XmlDocument` during cleanup or node removal.
       */
      LUMEX_API void destroy_node(XmlNodeBase *n, XmlAllocator &alloc);

      /**
       * @brief Appends a child node to the end of a node's child list.
       * @param[in,out] child The `XmlNodeBase` object to append. Its parent will be set to `node`.
       * @param[in,out] node The `XmlNodeBase` to which `child` will be appended.
       * @details This function manages the doubly-linked list pointers to correctly insert `child` as the last child.
       * @note This only links the node; memory management and content copying are handled elsewhere.
       */
      LUMEX_API
      void append_node(XmlNodeBase *child, XmlNodeBase *node);

      /**
       * @brief Prepends a child node to the beginning of a node's child list.
       * @param[in,out] child The `XmlNodeBase` object to prepend. Its parent will be set to `node`.
       * @param[in,out] node The `XmlNodeBase` to which `child` will be prepended.
       * @details This function manages the doubly-linked list pointers to correctly insert `child` as the first child.
       * @note This only links the node; memory management and content copying are handled elsewhere.
       */
      LUMEX_API
      void prepend_node(XmlNodeBase *child, XmlNodeBase *node);

      /**
       * @brief Inserts a child node after an existing sibling node.
       * @param[in,out] child The `XmlNodeBase` object to insert. Its parent will be set to `node`'s parent.
       * @param[in,out] node The existing `XmlNodeBase` after which `child` will be inserted.
       * @details This function adjusts the linked list pointers to insert `child` between `node` and `node`'s next
       * sibling.
       * @note Both `child` and `node` must have a parent for this operation to be valid.
       */
      LUMEX_API
      void insert_node_after(XmlNodeBase *child, XmlNodeBase *node);

      /**
       * @brief Inserts a child node before an existing sibling node.
       * @param[in,out] child The `XmlNodeBase` object to insert. Its parent will be set to `node`'s parent.
       * @param[in,out] node The existing `XmlNodeBase` before which `child` will be inserted.
       * @details This function adjusts the linked list pointers to insert `child` between `node`'s previous sibling and
       * `node`.
       * @note Both `child` and `node` must have a parent for this operation to be valid.
       */
      LUMEX_API
      void insert_node_before(XmlNodeBase *child, XmlNodeBase *node);

      /**
       * @brief Removes a node from its parent's child list.
       * @param[in,out] node The `XmlNodeBase` object to remove. Its parent and sibling pointers will be nullified.
       * @details This function unlinks `node` from its parent's child list by adjusting the `prev_sibling_c` and
       * `next_sibling` pointers of its neighbors. It does NOT deallocate the node's memory or its children; it only
       * removes it from the tree structure.
       * @note The caller is responsible for deallocating `node` after removal if it is no longer needed.
       */
      LUMEX_API
      void remove_node(XmlNodeBase *node);

      /**
       * @brief Appends a newly allocated child node of a specified type to the current node.
       * @param[in,out] node The parent `XmlNodeBase` to which the new child will be appended.
       * @param[in,out] alloc The `XmlAllocator` instance to use for allocating the new child node.
       * @param[in] type The `xml_node_type` of the new child node. Defaults to `node_element`.
       * @return A pointer to the newly created and appended `XmlNodeBase` child, or `nullptr` if allocation fails.
       * @details This function combines allocation and appending into a single operation.
       */
      LUMEX_API
      XmlNodeBase *append_new_node(XmlNodeBase *node, XmlAllocator &alloc, xml_node_type type = node_element);

      /**
       * @brief Appends an attribute to the end of a node's attribute list.
       * @param[in,out] attr The `XmlAttributeBase` object to append.
       * @param[in,out] node The `XmlNodeBase` to which `attr` will be appended.
       * @details This function manages the doubly-linked list pointers to correctly insert `attr` as the last
       * attribute.
       * @note This only links the attribute; memory management and content copying are handled elsewhere.
       */
      LUMEX_PUBLIC_API
      void append_attribute(XmlAttributeBase *attr, XmlNodeBase *node);

      /**
       * @brief Appends a newly allocated attribute to the current node.
       * @param[in,out] node The parent `XmlNodeBase` to which the new attribute will be appended.
       * @param[in,out] alloc The `XmlAllocator` instance to use for allocating the new attribute.
       * @return A pointer to the newly created and appended `XmlAttributeBase` attribute, or `nullptr` if allocation
       * fails.
       * @details This function combines allocation and appending into a single operation.
       */
      LUMEX_API
      XmlAttributeBase *append_new_attribute(XmlNodeBase *node, XmlAllocator &alloc);

      /**
       * @brief Copies the name and value strings and all attributes from a source `XmlNodeBase` to a destination
       * `XmlNodeBase`.
       * @details This utility function handles the string copying for the node's name and value,
       *          and then iterates through all attributes of the source node, copying each one to the destination node.
       *          It takes into account whether strings are stored in-situ or require separate memory allocation.
       * @param[in,out] dn_ The destination `XmlNodeBase` whose name, value, and attributes will be set. Must not be
       * `nullptr`.
       * @param[in] sn_ The source `XmlNodeBase` from which name, value, and attributes will be copied. Must not be
       * `nullptr`.
       * @param[in,out] shared_alloc An optional pointer to an `XmlAllocator` to optimize copying if source and
       * destination share the same allocator. If `nullptr`, strings are always copied.
       * @note This function only copies the direct contents and attributes; it does not copy child nodes.
       */
      LUMEX_API
      void node_copy_contents(XmlNodeBase *dn_, XmlNodeBase *sn_, // NOLINT(misc-use-internal-linkage)
                              XmlAllocator *shared_alloc);

      /**
       * @brief Performs a deep copy of an XML node's entire subtree (including itself, children, and attributes).
       * @param[in,out] dn_ The destination `XmlNodeBase` where the copied tree will be rooted. Its contents will be
       * overwritten.
       * @param[in] sn_ The source `XmlNodeBase` whose entire subtree will be copied.
       * @details This function recursively copies the `sn_` node and all its descendants (children and their
       * attributes) to `dn_`, allocating new memory for all copied elements using `dn_`'s allocator.
       * @note This function is crucial for operations like `XmlNode::append_copy` or `XmlDocument::load` where
       *       an entire tree needs to be duplicated or imported.
       * @warning Circular references in the source tree will lead to infinite loops.
       */
      LUMEX_API
      void node_copy_tree(XmlNodeBase *dn_, XmlNodeBase *sn_);

      /**
       * @brief Checks if a given `parent` node is an ancestor of a `node`.
       * @param[in] parent The potential ancestor node.
       * @param[in] node The node whose ancestry is being checked.
       * @return `true` if `parent` is an ancestor of `node` (including `parent` itself), `false` otherwise.
       * @details This function traverses up the parent chain from `node` until it finds `parent` or reaches the root.
       */
      LUMEX_API
      bool node_is_ancestor(XmlNodeBase *parent, XmlNodeBase *node);
    } // namespace Node
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_NODE_BASE_HPP
