/*
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

#define LUMEX_IMPLEMENTATION

#include "XPathNode.hpp"

using namespace lumex::xml::xpath::node;

LUMEX_PUBLIC_API
XPathNode::XPathNode (lumex::xml::node::XmlNode const &node_) : m_node (node_)
{
}

LUMEX_PUBLIC_API
XPathNode::XPathNode (lumex::xml::attribute::XmlAttribute const &attribute_,
                      lumex::xml::node::XmlNode const &parent_)
    : m_node ((attribute_ != nullptr) ? parent_
                                      : lumex::xml::node::XmlNode ()),
      m_attribute (attribute_)
{
}

LUMEX_PUBLIC_API
lumex::xml::node::XmlNode
XPathNode::node () const
{
  return (m_attribute != nullptr) ? lumex::xml::node::XmlNode () : m_node;
}

LUMEX_PUBLIC_API
lumex::xml::attribute::XmlAttribute
XPathNode::attribute () const
{
  return m_attribute;
}

LUMEX_PUBLIC_API
lumex::xml::node::XmlNode
XPathNode::parent () const
{
  return (m_attribute != nullptr) ? m_node : m_node.parent ();
}

inline static void
unspecified_bool_xpath_node (
    XPathNode *** /*unused*/) // NOLINT(misc-use-anonymous-namespace)
{
}

LUMEX_PUBLIC_API
XPathNode::
operator XPathNode::unspecified_bool_type () const
{
  return ((m_node != nullptr) || (m_attribute != nullptr))
             ? unspecified_bool_xpath_node
             : nullptr;
}

LUMEX_PUBLIC_API
bool
XPathNode::operator!() const
{
  return (m_node == nullptr) && (m_attribute == nullptr);
}

LUMEX_PUBLIC_API
bool
XPathNode::operator== (XPathNode const &n) const
{
  return m_node == n.m_node && m_attribute == n.m_attribute;
}

LUMEX_PUBLIC_API
bool
XPathNode::operator!= (XPathNode const &n) const
{
  return m_node != n.m_node || m_attribute != n.m_attribute;
}

LUMEX_PUBLIC_API
bool
lumex::xml::xpath::node::operator&& (
    XPathNode const &lhs, bool rhs) // NOLINT(misc-use-internal-linkage)
{
  return static_cast<bool> (lhs) && rhs;
}

LUMEX_PUBLIC_API
bool
lumex::xml::xpath::node::operator|| (
    XPathNode const &lhs, bool rhs) // NOLINT(misc-use-internal-linkage)
{
  return static_cast<bool> (lhs) || rhs;
}
