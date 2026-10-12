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

#include "XmlNode.hpp"

#include "lumex/core/utility/assert/LumexAssert.hpp"

using namespace lumex::xml::node;

LUMEX_PUBLIC_API
XmlNodeIterator::XmlNodeIterator (XmlNode const &node)
    : m_wrap (node), m_parent (node.parent ())
{
}

LUMEX_PUBLIC_API
XmlNodeIterator::XmlNodeIterator (
    XmlNodeBase *ref, // NOLINT(bugprone-easily-swappable-parameters)
    XmlNodeBase *parent)
    : m_wrap (ref), m_parent (parent)
{
}

LUMEX_PUBLIC_API
bool
XmlNodeIterator::operator== (XmlNodeIterator const &rhs) const
{
  return m_wrap.m_root == rhs.m_wrap.m_root
         && m_parent.m_root == rhs.m_parent.m_root;
}

LUMEX_PUBLIC_API
bool
XmlNodeIterator::operator!= (XmlNodeIterator const &rhs) const
{
  return m_wrap.m_root != rhs.m_wrap.m_root
         || m_parent.m_root != rhs.m_parent.m_root;
}

LUMEX_PUBLIC_API
XmlNode &
XmlNodeIterator::operator* () const
{
  LUMEX_ASSERT (m_wrap.m_root);
  return m_wrap;
}

LUMEX_PUBLIC_API
XmlNode *
XmlNodeIterator::operator->() const
{
  LUMEX_ASSERT (m_wrap.m_root);
  return &m_wrap;
}

LUMEX_PUBLIC_API
XmlNodeIterator &
XmlNodeIterator::operator++ ()
{
  LUMEX_ASSERT (m_wrap.m_root);
  m_wrap.m_root = m_wrap.m_root->next_sibling;
  return *this;
}

LUMEX_PUBLIC_API
XmlNodeIterator
XmlNodeIterator::operator++ (int)
{
  XmlNodeIterator temp = *this;
  ++*this;
  return temp;
}

LUMEX_PUBLIC_API
XmlNodeIterator &
XmlNodeIterator::operator-- ()
{
  m_wrap = (m_wrap.m_root != nullptr) ? m_wrap.previous_sibling ()
                                      : m_parent.last_child ();
  return *this;
}

LUMEX_PUBLIC_API
XmlNodeIterator
XmlNodeIterator::operator-- (int)
{
  XmlNodeIterator temp = *this;
  --*this;
  return temp;
}
