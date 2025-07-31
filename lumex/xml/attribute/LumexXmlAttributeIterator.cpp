#define LUMEX_IMPLEMENTATION

#include "lumex/core/utility/LumexMacros.hpp"

#include "LumexXmlAttributeIterator.hpp"

using namespace Lumex::Xml::Attribute;

inline LumexXmlAttributeIterator::LumexXmlAttributeIterator(LumexXmlAttribute const &attr,
                                                            Lumex::Xml::Node::LumexXmlNode const &parent)
    : m_wrap(attr), m_parent(parent)
{}

inline LumexXmlAttributeIterator::LumexXmlAttributeIterator(xml_attr_t *ref, xml_node_t *parent)
    : m_wrap(ref), m_parent(parent)
{}

inline bool
LumexXmlAttributeIterator::operator==(LumexXmlAttributeIterator const &rhs) const
{
  return m_wrap.m_attr == rhs.m_wrap.m_attr && m_parent.m_root == rhs.m_parent.m_root;
}

inline bool
LumexXmlAttributeIterator::operator!=(LumexXmlAttributeIterator const &rhs) const
{
  return m_wrap.m_attr != rhs.m_wrap.m_attr || m_parent.m_root != rhs.m_parent.m_root;
}

inline LumexXmlAttribute &
LumexXmlAttributeIterator::operator*() const
{
  LUMEX_ASSERT(m_wrap.m_attr);
  return m_wrap;
}

inline LumexXmlAttribute *
LumexXmlAttributeIterator::operator->() const
{
  LUMEX_ASSERT(m_wrap.m_attr);
  return &m_wrap;
}

inline LumexXmlAttributeIterator &
LumexXmlAttributeIterator::operator++()
{
  LUMEX_ASSERT(m_wrap.m_attr);
  m_wrap.m_attr = m_wrap.m_attr->next_attribute;
  return *this;
}

inline LumexXmlAttributeIterator
LumexXmlAttributeIterator::operator++(int)
{
  LumexXmlAttributeIterator temp = *this;
  ++*this;
  return temp;
}

inline LumexXmlAttributeIterator &
LumexXmlAttributeIterator::operator--()
{
  m_wrap = (m_wrap.m_attr != nullptr) ? m_wrap.previous_attribute() : m_parent.last_attribute();
  return *this;
}

inline LumexXmlAttributeIterator
LumexXmlAttributeIterator::operator--(int)
{
  LumexXmlAttributeIterator temp = *this;
  --*this;
  return temp;
}