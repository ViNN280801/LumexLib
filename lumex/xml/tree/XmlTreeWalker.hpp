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

#ifndef LUMEX_XML_TREE_HPP
#define LUMEX_XML_TREE_HPP

#include "lumex/LumexExport.hpp"

#include "lumex/core/utility/attr/LumexAttributes.hpp"

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace xml
{
namespace tree
{
/**
 * @brief Abstract base class for implementing custom XML tree traversal logic.
 * @details `XmlTreeWalker` defines a customizable interface for walking an XML
 * document tree. Users can derive from this class and override `begin`,
 * `for_each`, and `end` methods to implement specific behaviors during
 * traversal, such as filtering nodes, collecting data, or performing
 * modifications.
 * @note This class is intended to be used with `XmlNode::traverse()`.
 * @see XmlNode::traverse()
 */
class LUMEX_API
    XmlTreeWalker // NOLINT(cppcoreguidelines-special-member-functions)
{
  friend class XmlNode;

public:
  /**
   * @brief Default constructor for `XmlTreeWalker`.
   * @details Initializes the walker.
   */
  XmlTreeWalker () = default;
  /**
   * @brief Virtual destructor for `XmlTreeWalker`.
   * @details Ensures proper cleanup of derived classes.
   */
  virtual ~XmlTreeWalker () = default;

  /**
   * @brief Callback function invoked when the tree traversal begins for a
   * node.
   * @param[in,out] node The `XmlNode` at which the traversal is beginning.
   * @return `true` to continue traversal, `false` to stop.
   * @details This method can be overridden to perform actions or checks before
   * visiting a node's children.
   */
  virtual bool
  begin (XmlNode & /* unused */)
  {
    return true;
  }

  /**
   * @brief Pure virtual callback function invoked for each node traversed.
   * @param[in,out] node The `XmlNode` currently being visited.
   * @return `true` to continue traversal, `false` to stop.
   * @details This method must be implemented by derived classes to define the
   * core logic of the tree walk.
   */
  virtual bool for_each (XmlNode &node) = 0;

  /**
   * @brief Callback function invoked when the tree traversal ends for a node.
   * @param[in,out] node The `XmlNode` for which traversal is ending (i.e., all
   * its children and their subtrees have been visited).
   * @return `true` to continue traversal, `false` to stop.
   * @details This method can be overridden to perform actions or cleanup after
   * visiting a node's children.
   */
  virtual bool
  end (XmlNode & /* unused */)
  {
    return true;
  }

  /**
   * @brief Retrieves the current traversal depth.
   * @return An integer representing the current depth in the XML tree (root is
   * depth 0).
   */
  LUMEX_ATTRIBUTE_NODISCARD ("The returned integer indicates the current "
                             "traversal depth; discarding it negates the "
                             "purpose of the getter")
  int
  depth () const
  {
    return m_depth;
  }

  /**
   * @brief Sets the current traversal depth.
   * @param[in] depth The new depth value.
   */
  void
  set_depth (int depth)
  {
    m_depth = depth;
  }

  /**
   * @brief Increments the current traversal depth.
   * @details Used internally during tree descent.
   */
  void
  increment_depth ()
  {
    ++m_depth;
  }

  /**
   * @brief Decrements the current traversal depth.
   * @details Used internally during tree ascent.
   */
  void
  decrement_depth ()
  {
    --m_depth;
  }

private:
  /// @brief Internal member to store the current traversal depth.
  int m_depth{};
};
} // namespace tree
} // namespace xml
} // namespace lumex

#endif // !LUMEX_XML_TREE_HPP
