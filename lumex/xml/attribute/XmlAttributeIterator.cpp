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

#include "lumex/core/utility/assert/LumexAssert.hpp"
#include "lumex/xml/attribute/XmlAttribute.hpp"
#include "lumex/xml/node/XmlNode.hpp"

using namespace lumex::xml::node;
using namespace lumex::xml::attribute;

LUMEX_PUBLIC_API
XmlAttributeIterator::XmlAttributeIterator (XmlAttribute const &attr,
                                            XmlNode const &parent)
    : m_wrap (attr), m_parent (parent)
{
}

LUMEX_PUBLIC_API
XmlAttributeIterator::XmlAttributeIterator (XmlAttributeBase *ref,
                                            XmlNodeBase *parent)
    : m_wrap (ref), m_parent (parent)
{
}

LUMEX_PUBLIC_API
bool
XmlAttributeIterator::operator== (XmlAttributeIterator const &rhs) const
{
  return m_wrap.m_attr == rhs.m_wrap.m_attr
         && m_parent.root () == rhs.m_parent.root ();
}

LUMEX_PUBLIC_API
bool
XmlAttributeIterator::operator!= (XmlAttributeIterator const &rhs) const
{
  return m_wrap.m_attr != rhs.m_wrap.m_attr
         || m_parent.root () != rhs.m_parent.root ();
}

LUMEX_PUBLIC_API
XmlAttribute &
XmlAttributeIterator::operator* () const
{
  LUMEX_ASSERT (m_wrap.m_attr);
  return m_wrap;
}

LUMEX_PUBLIC_API
XmlAttribute *
XmlAttributeIterator::operator->() const
{
  LUMEX_ASSERT (m_wrap.m_attr);
  return &m_wrap;
}

LUMEX_PUBLIC_API
XmlAttributeIterator &
XmlAttributeIterator::operator++ ()
{
  LUMEX_ASSERT (m_wrap.m_attr);
  m_wrap.m_attr = m_wrap.m_attr->next_attribute;
  return *this;
}

LUMEX_PUBLIC_API
XmlAttributeIterator
XmlAttributeIterator::operator++ (int)
{
  XmlAttributeIterator temp = *this;
  ++*this;
  return temp;
}

LUMEX_PUBLIC_API
XmlAttributeIterator &
XmlAttributeIterator::operator-- ()
{
  m_wrap = (m_wrap.m_attr != nullptr) ? m_wrap.previous_attribute ()
                                      : m_parent.last_attribute ();
  return *this;
}

LUMEX_PUBLIC_API
XmlAttributeIterator
XmlAttributeIterator::operator-- (int)
{
  XmlAttributeIterator temp = *this;
  --*this;
  return temp;
}
