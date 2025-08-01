#include "lumex/core/utility/LumexMacros.hpp"

#include "XmlAttributeIterator.hpp"

using namespace Lumex::Xml::Attribute;

inline XmlAttributeIterator::XmlAttributeIterator(XmlAttribute const &attr, XmlNode const &parent)
    : m_wrap(attr), m_parent(parent)
{}

inline XmlAttributeIterator::XmlAttributeIterator(XmlAttributeBase *ref, XmlNodeBase *parent)
    : m_wrap(ref), m_parent(parent)
{}

inline bool
XmlAttributeIterator::operator==(XmlAttributeIterator const &rhs) const
{
  return m_wrap.m_attr == rhs.m_wrap.m_attr && m_parent.root() == rhs.m_parent.root();
}

inline bool
XmlAttributeIterator::operator!=(XmlAttributeIterator const &rhs) const
{
  return m_wrap.m_attr != rhs.m_wrap.m_attr || m_parent.root() != rhs.m_parent.root();
}

inline XmlAttribute &
XmlAttributeIterator::operator*() const
{
  LUMEX_ASSERT(m_wrap.m_attr);
  return m_wrap;
}

inline XmlAttribute *
XmlAttributeIterator::operator->() const
{
  LUMEX_ASSERT(m_wrap.m_attr);
  return &m_wrap;
}

inline XmlAttributeIterator &
XmlAttributeIterator::operator++()
{
  LUMEX_ASSERT(m_wrap.m_attr);
  m_wrap.m_attr = m_wrap.m_attr->next_attribute;
  return *this;
}

inline XmlAttributeIterator
XmlAttributeIterator::operator++(int)
{
  XmlAttributeIterator temp = *this;
  ++*this;
  return temp;
}

inline XmlAttributeIterator &
XmlAttributeIterator::operator--()
{
  m_wrap = (m_wrap.m_attr != nullptr) ? m_wrap.previous_attribute() : m_parent.last_attribute();
  return *this;
}

inline XmlAttributeIterator
XmlAttributeIterator::operator--(int)
{
  XmlAttributeIterator temp = *this;
  --*this;
  return temp;
}