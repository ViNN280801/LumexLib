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

#include "lumex/xml/utility/XmlUtils.hpp"

using namespace lumex::xml::node;
using namespace lumex::xml::utility;

LUMEX_PUBLIC_API
XmlNamedNodeIterator::XmlNamedNodeIterator () : m_name (nullptr) {}

LUMEX_PUBLIC_API
XmlNamedNodeIterator::XmlNamedNodeIterator (XmlNode const &node,
                                            char_t const *name)
    : m_wrap (node), m_parent (node.parent ()), m_name (name)
{
}

LUMEX_PUBLIC_API
XmlNamedNodeIterator::XmlNamedNodeIterator (
    XmlNodeBase *ref, // NOLINT(bugprone-easily-swappable-parameters)
    XmlNodeBase *parent, char_t const *name)
    : m_wrap (ref), m_parent (parent), m_name (name)
{
}

LUMEX_PUBLIC_API
bool
XmlNamedNodeIterator::operator== (XmlNamedNodeIterator const &rhs) const
{
  return m_wrap.root () == rhs.m_wrap.root ()
         && m_parent.root () == rhs.m_parent.root ();
}

LUMEX_PUBLIC_API
bool
XmlNamedNodeIterator::operator!= (XmlNamedNodeIterator const &rhs) const
{
  return m_wrap.root () != rhs.m_wrap.root ()
         || m_parent.root () != rhs.m_parent.root ();
}

LUMEX_PUBLIC_API
XmlNode &
XmlNamedNodeIterator::operator* () const
{
  LUMEX_ASSERT (m_wrap.root ());
  return m_wrap;
}

LUMEX_PUBLIC_API
XmlNode *
XmlNamedNodeIterator::operator->() const
{
  LUMEX_ASSERT (m_wrap.root ());
  return &m_wrap;
}

LUMEX_PUBLIC_API
XmlNamedNodeIterator &
XmlNamedNodeIterator::operator++ ()
{
  LUMEX_ASSERT (m_wrap.root ());
  m_wrap = m_wrap.next_sibling (m_name);
  return *this;
}

LUMEX_PUBLIC_API
XmlNamedNodeIterator
XmlNamedNodeIterator::operator++ (int)
{
  XmlNamedNodeIterator temp = *this;
  ++*this;
  return temp;
}

LUMEX_PUBLIC_API
XmlNamedNodeIterator &
XmlNamedNodeIterator::operator-- ()
{
  if (m_wrap.root () != nullptr)
    m_wrap = m_wrap.previous_sibling (m_name);
  else
    {
      m_wrap = m_parent.last_child ();

      if (!utility::strequal (m_wrap.name (), m_name))
        m_wrap = m_wrap.previous_sibling (m_name);
    }

  return *this;
}

LUMEX_PUBLIC_API
XmlNamedNodeIterator
XmlNamedNodeIterator::operator-- (int)
{
  XmlNamedNodeIterator temp = *this;
  --*this;
  return temp;
}
