#define LUMEX_IMPLEMENTATION
#include "lumex/core/utility/LumexAssert.hpp"

#include "XmlAttributeIterator.hpp"

using namespace Lumex::Xml::Attribute;

LUMEX_PUBLIC_API
inline XmlAttributeIterator::XmlAttributeIterator(XmlAttribute const &attr, XmlNode const &parent)
    : m_wrap(attr), m_parent(parent)
{}

LUMEX_PUBLIC_API
inline XmlAttributeIterator::XmlAttributeIterator(XmlAttributeBase *ref, XmlNodeBase *parent)
    : m_wrap(ref), m_parent(parent)
{}

LUMEX_PUBLIC_API
inline bool
XmlAttributeIterator::operator==(XmlAttributeIterator const &rhs) const
{
  return m_wrap.m_attr == rhs.m_wrap.m_attr && m_parent.root() == rhs.m_parent.root();
}

LUMEX_PUBLIC_API
inline bool
XmlAttributeIterator::operator!=(XmlAttributeIterator const &rhs) const
{
  return m_wrap.m_attr != rhs.m_wrap.m_attr || m_parent.root() != rhs.m_parent.root();
}

LUMEX_PUBLIC_API
inline XmlAttribute &
XmlAttributeIterator::operator*() const
{
  LUMEX_ASSERT(m_wrap.m_attr);
  return m_wrap;
}

LUMEX_PUBLIC_API
inline XmlAttribute *
XmlAttributeIterator::operator->() const
{
  LUMEX_ASSERT(m_wrap.m_attr);
  return &m_wrap;
}

LUMEX_PUBLIC_API
inline XmlAttributeIterator &
XmlAttributeIterator::operator++()
{
  LUMEX_ASSERT(m_wrap.m_attr);
  m_wrap.m_attr = m_wrap.m_attr->next_attribute;
  return *this;
}

LUMEX_PUBLIC_API
inline XmlAttributeIterator
XmlAttributeIterator::operator++(int)
{
  XmlAttributeIterator temp = *this;
  ++*this;
  return temp;
}

LUMEX_PUBLIC_API
inline XmlAttributeIterator &
XmlAttributeIterator::operator--()
{
  m_wrap = (m_wrap.m_attr != nullptr) ? m_wrap.previous_attribute() : m_parent.last_attribute();
  return *this;
}

LUMEX_PUBLIC_API
inline XmlAttributeIterator
XmlAttributeIterator::operator--(int)
{
  XmlAttributeIterator temp = *this;
  --*this;
  return temp;
}
