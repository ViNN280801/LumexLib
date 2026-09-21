/**
 * Copyright (c) 2026 Vladislav Semykin <vladislav.semykin@gmail.com>
 *
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of
 * charge, to any person obtaining a copy
 * of this software and associated
 * documentation files (the "Software"), to deal
 * in the Software without
 * restriction, including without limitation the rights
 * to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the
 * Software, and to permit persons to whom the Software is
 * furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice
 * and this permission notice shall be included in
 * all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT
 * WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO
 * THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH
 * THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef LUMEX_XML_XPATH_DOCUMENT_HPP
#define LUMEX_XML_XPATH_DOCUMENT_HPP

#include "lumex/LumexExport.hpp"

#include "lumex/xml/node/XmlNodeBase.hpp"
#include "lumex/xml/xpath/node/XPathNode.hpp"

using namespace lumex::xml::xpath::node;
using namespace lumex::xml::node;

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace xml
{
namespace xpath
{
namespace document
{
/**
 * @brief Determines if `ln_node` comes before `rn_node` among siblings in
 * document order.
 * @details This function compares two sibling `XmlNodeBase` pointers to
 * establish their relative order within the same parent's child list. It
 * assumes both nodes share the same parent or are both root nodes of different
 * documents.
 * @param ln_node Pointer to the left-hand side `XmlNodeBase`.
 * @param rn_node Pointer to the right-hand side `XmlNodeBase`.
 * @return `true` if `ln_node` appears before `rn_node` in document order,
 * `false` otherwise.
 * @note Asserts that `ln_node` and `rn_node` have the same parent.
 * @note This function is primarily for internal use by `node_is_before`.
 */
LUMEX_API
bool node_is_before_sibling (XmlNodeBase *ln_node, XmlNodeBase *rn_node);

/**
 * @brief Determines if `ln_node` comes before `rn_node` in overall document
 * order.
 * @details This function implements a robust comparison of two `XmlNodeBase`
 * pointers based on their full document order, even if they are not siblings
 * or are at different depths within the XML tree. It finds their common
 * ancestor and then determines the order based on sibling relationships or
 * ancestor-descendant relationships.
 * @param ln_node Pointer to the left-hand side `XmlNodeBase`.
 * @param rn_node Pointer to the right-hand side `XmlNodeBase`.
 * @return `true` if `ln_node` appears before `rn_node` in document order,
 * `false` otherwise.
 */
LUMEX_API
bool node_is_before (XmlNodeBase *ln_node, XmlNodeBase *rn_node);

/**
 * @brief Retrieves a pointer to a buffer location for document order
 * comparison optimization.
 * @details This function attempts to return a pointer to an internal memory
 * buffer associated with the `XPathNode` (either its name or value) if that
 * memory is not shared and can be used for direct pointer comparison to infer
 * document order. This provides an optimized path for document order
 * comparison when nodes reside in contiguous memory blocks.
 * @param xnode The `XPathNode` for which to retrieve the buffer order pointer.
 * @return A `void const*` pointer to a memory location that can be used for
 * ordering, or `nullptr` if no such optimization is possible (e.g., for shared
 * memory or non-contiguous data).
 */
LUMEX_API
void const *document_buffer_order (XPathNode const &xnode);

/**
 * @brief Functor for comparing two XPathNode objects based on their document
 * order.
 * @details This comparator provides the logic for sorting or comparing
 * `XPathNode` objects according to the XML Document Order. It first attempts
 * an optimized comparison using `document_buffer_order` and falls back to a
 * more robust, recursive comparison using `node_is_before` if the optimization
 * is not applicable. It correctly handles comparisons between element nodes,
 * attribute nodes, and combinations thereof.
 */
struct LUMEX_API document_order_comparator
{
  /**
   * @brief Compares two XPathNode objects for their document order.
   * @details This operator overload defines the comparison logic for
   * `document_order_comparator`. It prioritizes an optimized buffer order
   * comparison if available, otherwise it performs a full document order
   * traversal. Special handling is included for attribute nodes which always
   * appear after their parent element but before any of their parent's
   * children.
   * @param lhs The left-hand side `XPathNode` to compare.
   * @param rhs The right-hand side `XPathNode` to compare.
   * @return `true` if `lhs` comes before `rhs` in document order, `false`
   * otherwise.
   */
  bool operator() (XPathNode const &lhs, XPathNode const &rhs) const;
};
} // namespace document
} // namespace xpath
} // namespace xml
} // namespace lumex

#endif // !LUMEX_XML_XPATH_DOCUMENT_HPP
