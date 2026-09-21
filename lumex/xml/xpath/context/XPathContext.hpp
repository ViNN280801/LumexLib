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

#ifndef LUMEX_XML_XPATH_CONTEXT_HPP
#define LUMEX_XML_XPATH_CONTEXT_HPP

#include "lumex/LumexExport.hpp"

#include "lumex/xml/xpath/node/XPathNode.hpp"

using namespace lumex::xml::xpath::node;

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace xml
{
namespace xpath
{
namespace context
{
/**
 * @brief Represents the XPath evaluation context for an expression.
 * @details This structure holds the essential components of the XPath context,
 *          including the current node, its position within the current node
 * list, and the total size of that node list. This context is critical for
 *          evaluating XPath expressions that depend on positional information
 *          (e.g., `position()`, `last()`).
 *
 * @note This struct is designed to be lightweight and is typically passed by
 * const reference during XPath expression evaluation. It is not thread-safe
 * and assumes single-threaded access within an evaluation context.
 */
struct LUMEX_API XPathContext
{
  /// @brief The current context node.
  xml::xpath::node::XPathNode
      node; // NOLINT(misc-non-private-member-variables-in-classes)
  /// @brief The position of the context node within the context node list
  /// (1-indexed).
  std::size_t
      position{}; // NOLINT(misc-non-private-member-variables-in-classes)
  /// @brief The total number of nodes in the context node list.
  std::size_t size{}; // NOLINT(misc-non-private-member-variables-in-classes)

  /**
   * @brief Constructs an XPathContext with the specified node, position, and
   * size.
   * @details Initializes a new XPath context, providing all necessary
   * information for evaluating XPath expressions relative to a specific node
   * within a set.
   * @param node_ The `XPathNode` representing the current context node.
   * @param position_ The 1-indexed position of `node_` within its containing
   * node list.
   * @param size_ The total number of nodes in the containing node list.
   */
  XPathContext (
      xml::xpath::node::XPathNode const &node_,
      std::size_t position_, // NOLINT(bugprone-easily-swappable-parameters)
      std::size_t size_)
      : node (node_), position (position_), size (size_)
  {
  }
};
} // namespace context
} // namespace xpath
} // namespace xml
} // namespace lumex

#endif // !LUMEX_XML_XPATH_CONTEXT_HPP
