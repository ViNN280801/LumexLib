#ifndef LUMEX_XML_XPATH_DOCUMENT_ORDER_COMPARATOR_HPP
#define LUMEX_XML_XPATH_DOCUMENT_ORDER_COMPARATOR_HPP

#include "lumex/LumexExport.hpp"

#include "lumex/xml/node/XmlNodeBase.hpp"
#include "lumex/xml/xpath/node/XPathNode.hpp"

using namespace Lumex::Xml::XPath::Node;
using namespace Lumex::Xml::Node;

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace XPath
    {
      namespace Document
      {
        /**
         * @brief Determines if `ln_node` comes before `rn_node` among siblings in document order.
         * @details This function compares two sibling `XmlNodeBase` pointers to establish
         *          their relative order within the same parent's child list. It assumes
         *          both nodes share the same parent or are both root nodes of different documents.
         * @param ln_node Pointer to the left-hand side `XmlNodeBase`.
         * @param rn_node Pointer to the right-hand side `XmlNodeBase`.
         * @return `true` if `ln_node` appears before `rn_node` in document order, `false` otherwise.
         * @note Asserts that `ln_node` and `rn_node` have the same parent.
         * @note This function is primarily for internal use by `node_is_before`.
         */
        LUMEX_API
        bool node_is_before_sibling(XmlNodeBase *ln_node, XmlNodeBase *rn_node);

        /**
         * @brief Determines if `ln_node` comes before `rn_node` in overall document order.
         * @details This function implements a robust comparison of two `XmlNodeBase` pointers
         *          based on their full document order, even if they are not siblings or are
         *          at different depths within the XML tree. It finds their common ancestor
         *          and then determines the order based on sibling relationships or ancestor-descendant relationships.
         * @param ln_node Pointer to the left-hand side `XmlNodeBase`.
         * @param rn_node Pointer to the right-hand side `XmlNodeBase`.
         * @return `true` if `ln_node` appears before `rn_node` in document order, `false` otherwise.
         */
        LUMEX_API
        bool node_is_before(XmlNodeBase *ln_node, XmlNodeBase *rn_node);

        /**
         * @brief Retrieves a pointer to a buffer location for document order comparison optimization.
         * @details This function attempts to return a pointer to an internal memory buffer
         *          associated with the `XPathNode` (either its name or value) if that memory
         *          is not shared and can be used for direct pointer comparison to infer document order.
         *          This provides an optimized path for document order comparison when nodes
         *          reside in contiguous memory blocks.
         * @param xnode The `XPathNode` for which to retrieve the buffer order pointer.
         * @return A `void const*` pointer to a memory location that can be used for ordering,
         *         or `nullptr` if no such optimization is possible (e.g., for shared memory
         *         or non-contiguous data).
         */
        LUMEX_API
        void const *document_buffer_order(XPathNode const &xnode);

        /**
         * @brief Functor for comparing two XPathNode objects based on their document order.
         * @details This comparator provides the logic for sorting or comparing `XPathNode` objects
         *          according to the XML Document Order. It first attempts an optimized comparison
         *          using `document_buffer_order` and falls back to a more robust, recursive
         *          comparison using `node_is_before` if the optimization is not applicable.
         *          It correctly handles comparisons between element nodes, attribute nodes,
         *          and combinations thereof.
         */
        struct LUMEX_API document_order_comparator {
          /**
           * @brief Compares two XPathNode objects for their document order.
           * @details This operator overload defines the comparison logic for `document_order_comparator`.
           *          It prioritizes an optimized buffer order comparison if available, otherwise
           *          it performs a full document order traversal. Special handling is included
           *          for attribute nodes which always appear after their parent element but
           *          before any of their parent's children.
           * @param lhs The left-hand side `XPathNode` to compare.
           * @param rhs The right-hand side `XPathNode` to compare.
           * @return `true` if `lhs` comes before `rhs` in document order, `false` otherwise.
           */
          bool operator()(XPathNode const &lhs, XPathNode const &rhs) const;
        };
      } // namespace Document
    } // namespace XPath
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_XPATH_DOCUMENT_ORDER_COMPARATOR_HPP
