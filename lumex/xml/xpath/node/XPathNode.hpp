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

#ifndef LUMEX_XML_XPATH_NODE_XPATH_NODE_HPP
#define LUMEX_XML_XPATH_NODE_XPATH_NODE_HPP

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-libc-call"
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wswitch-enum"
#pragma clang diagnostic ignored "-Wnrvo"
#pragma clang diagnostic ignored "-Wheader-hygiene"
#pragma clang diagnostic ignored "-Wused-but-marked-unused"
#pragma clang diagnostic ignored "-Wundefined-var-template"
#pragma clang diagnostic ignored "-Wdeprecated-redundant-constexpr-static-def"
#pragma clang diagnostic ignored "-Wvariadic-macro-arguments-omitted"
#pragma clang diagnostic ignored "-Wunused-result"
#pragma clang diagnostic ignored "-Wextra-semi-stmt"
#pragma clang diagnostic ignored "-Wexpansion-to-defined"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wundefined-func-template"
#pragma clang diagnostic ignored "-Wfloat-equal"
#endif

#include "lumex/LumexExport.hpp"

#include "lumex/core/utility/attr/LumexAttributes.hpp"

#include "lumex/xml/attribute/XmlAttribute.hpp"
#include "lumex/xml/node/XmlNode.hpp"

using namespace lumex::xml::node;
using namespace lumex::xml::attribute;

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace xml
{
namespace xpath
{
namespace node
{
/**
 * @brief Represents a node in an XPath node-set, which can be either an XML
 * element/document node or an XML attribute.
 * @details This class provides a unified wrapper for
 * `lumex::xml::node::XmlNode` and `lumex::xml::attribute::XmlAttribute`,
 * allowing XPath expressions to operate on both types of nodes seamlessly. It
 * maintains internal pointers to either an `XmlNode` or an `XmlAttribute` (and
 * its parent `XmlNode` for attributes). It supports safe boolean conversion
 * and comparison operations.
 *
 * @note An `XPathNode` is considered "empty" or "null" if it does not contain
 * either a valid `XmlNode` or `XmlAttribute`.
 * @warning This class does not manage the lifetime of the underlying `XmlNode`
 * or `XmlAttribute` objects; it only holds references. These underlying
 * objects must be managed externally.
 */
class LUMEX_API XPathNode
{
public:
  /**
   * @brief Type definition for safe boolean conversion.
   * @details This is a standard C++ idiom for enabling safe boolean
   * conversions, preventing accidental implicit conversions to integral types.
   */
  using unspecified_bool_type = void (*) (XPathNode ***);

  /**
   * @brief Default constructor; constructs an empty XPath node.
   * @details Initializes an `XPathNode` instance that does not refer to any
   *          XML element, document, or attribute.
   */
  XPathNode () = default;

  /**
   * @brief Constructs an XPath node from an XML node.
   * @details Initializes an `XPathNode` that wraps a
   * `lumex::xml::node::XmlNode`.
   * @param node The `lumex::xml::node::XmlNode` to wrap.
   */
  XPathNode (lumex::xml::node::XmlNode const &node);
  /**
   * @brief Constructs an XPath node from an XML attribute and its parent.
   * @details Initializes an `XPathNode` that wraps a
   * `lumex::xml::attribute::XmlAttribute`. The parent `XmlNode` is also
   * provided as attributes do not have direct parent pointers but are
   * associated with an element.
   * @param attribute The `lumex::xml::attribute::XmlAttribute` to wrap.
   * @param parent The `lumex::xml::node::XmlNode` that is the parent of the
   * attribute. This is stored internally to allow `parent()` queries for
   * attributes.
   */
  XPathNode (lumex::xml::attribute::XmlAttribute const &attribute,
             lumex::xml::node::XmlNode const &parent);

  /**
   * @brief Gets the wrapped `XmlNode` if this `XPathNode` represents an
   * element/document node.
   * @details If this `XPathNode` holds an `XmlAttribute`, this function
   * returns an empty `XmlNode`.
   * @return The `lumex::xml::node::XmlNode` if present, otherwise an empty
   * `XmlNode`.
   */
  // The returned XML node from the XPath evaluation should be used; discarding
  // it negates the purpose of the getter.
  LUMEX_ATTRIBUTE_NODISCARD ("The returned XML node from the XPath evaluation "
                             "should be used; discarding it "
                             "negates the purpose of the getter.")
  lumex::xml::node::XmlNode node () const;

  /**
   * @brief Gets the wrapped `XmlAttribute` if this `XPathNode` represents an
   * attribute.
   * @details If this `XPathNode` holds an `XmlNode`, this function returns an
   *          empty `XmlAttribute`.
   * @return The `lumex::xml::attribute::XmlAttribute` if present, otherwise an
   * empty `XmlAttribute`.
   */
  LUMEX_ATTRIBUTE_NODISCARD ("The returned XML attribute from the XPath "
                             "evaluation should be used; discarding it "
                             "negates the purpose of the getter.")
  lumex::xml::attribute::XmlAttribute attribute () const;

  /**
   * @brief Gets the parent `XmlNode` of the wrapped node or attribute.
   * @details If this `XPathNode` wraps an `XmlNode`, it returns that node's
   * parent. If it wraps an `XmlAttribute`, it returns the parent `XmlNode`
   * that was provided during construction.
   * @return The parent `lumex::xml::node::XmlNode`. Returns an empty `XmlNode`
   *         if the current node is a document root or if no parent exists.
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "The returned parent XML node should be used; discarding it negates the "
      "purpose of the getter.")
  lumex::xml::node::XmlNode parent () const;

  /**
   * @brief Safe boolean conversion operator.
   * @details Allows an `XPathNode` object to be used in boolean contexts
   * (e.g., `if (xpathNode)`). It evaluates to `true` if the `XPathNode` wraps
   * a valid `XmlNode` or `XmlAttribute`, and `false` otherwise. This prevents
   * unintended implicit conversions.
   * @return A pointer to a dummy function if the node is valid, `nullptr`
   * otherwise.
   */
  operator unspecified_bool_type () const;

  /**
   * @brief Logical NOT operator.
   * @details Returns `true` if this `XPathNode` is empty (does not wrap any
   * XML node or attribute), and `false` otherwise.
   * @return `true` if the node is empty, `false` otherwise.
   */
  bool operator!() const;

  /**
   * @brief Equality comparison operator.
   * @details Compares two `XPathNode` objects for equality based on their
   *          wrapped `XmlNode` and `XmlAttribute` instances.
   * @param n The `XPathNode` to compare with.
   * @return `true` if both `m_node` and `m_attribute` members are equal,
   * `false` otherwise.
   */
  bool operator== (XPathNode const &n) const;
  /**
   * @brief Inequality comparison operator.
   * @details Compares two `XPathNode` objects for inequality.
   * @param n The `XPathNode` to compare with.
   * @return `true` if either `m_node` or `m_attribute` members are not equal,
   * `false` otherwise.
   */
  bool operator!= (XPathNode const &n) const;

private:
  /// @brief Internal storage for the wrapped XML node. Used when representing
  /// an element or document.
  lumex::xml::node::XmlNode m_node;
  /// @brief Internal storage for the wrapped XML attribute. Used when
  /// representing an attribute.
  lumex::xml::attribute::XmlAttribute m_attribute;
};

/**
 * @brief Logical AND operator for `XPathNode` and boolean.
 * @details Enables the use of `XPathNode` in logical AND expressions (`&&`)
 *          with a boolean right-hand side, treating the `XPathNode` as a
 * boolean.
 * @param lhs The left-hand side `XPathNode`.
 * @param rhs The right-hand side boolean value.
 * @return The result of the logical AND operation.
 */
LUMEX_API
bool operator&& (XPathNode const &lhs, bool rhs);

/**
 * @brief Logical OR operator for `XPathNode` and boolean.
 * @details Enables the use of `XPathNode` in logical OR expressions (`||`)
 *          with a boolean right-hand side, treating the `XPathNode` as a
 * boolean.
 * @param lhs The left-hand side `XPathNode`.
 * @param rhs The right-hand side boolean value.
 * @return The result of the logical OR operation.
 */
LUMEX_API
bool operator|| (XPathNode const &lhs, bool rhs);
} // namespace node
} // namespace xpath
} // namespace xml
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_XML_XPATH_NODE_XPATH_NODE_HPP
