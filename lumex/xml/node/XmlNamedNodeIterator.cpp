#include "lumex/core/utility/LumexAssert.hpp"

#include "lumex/xml/utility/XmlUtils.hpp"

#include "XmlNamedNodeIterator.hpp"

using namespace Lumex::Xml::Node;
using namespace Lumex::Xml::Utility;

inline XmlNamedNodeIterator::XmlNamedNodeIterator() : m_name(nullptr) {}

inline XmlNamedNodeIterator::XmlNamedNodeIterator(XmlNode const &node, char_t const *name)
    : m_wrap(node), m_parent(node.parent()), m_name(name)
{}

inline XmlNamedNodeIterator::XmlNamedNodeIterator(XmlNodeBase *ref, // NOLINT(bugprone-easily-swappable-parameters)
                                                  XmlNodeBase *parent, char_t const *name)
    : m_wrap(ref), m_parent(parent), m_name(name)
{}

inline bool
XmlNamedNodeIterator::operator==(XmlNamedNodeIterator const &rhs) const
{
  return m_wrap.root() == rhs.m_wrap.root() && m_parent.root() == rhs.m_parent.root();
}

inline bool
XmlNamedNodeIterator::operator!=(XmlNamedNodeIterator const &rhs) const
{
  return m_wrap.root() != rhs.m_wrap.root() || m_parent.root() != rhs.m_parent.root();
}

inline XmlNode &
XmlNamedNodeIterator::operator*() const
{
  LUMEX_ASSERT(m_wrap.root());
  return m_wrap;
}

inline XmlNode *
XmlNamedNodeIterator::operator->() const
{
  LUMEX_ASSERT(m_wrap.root());
  return &m_wrap;
}

inline XmlNamedNodeIterator &
XmlNamedNodeIterator::operator++()
{
  LUMEX_ASSERT(m_wrap.root());
  m_wrap = m_wrap.next_sibling(m_name);
  return *this;
}

inline XmlNamedNodeIterator
XmlNamedNodeIterator::operator++(int)
{
  XmlNamedNodeIterator temp = *this;
  ++*this;
  return temp;
}

inline XmlNamedNodeIterator &
XmlNamedNodeIterator::operator--()
{
  if(m_wrap.root() != nullptr)
    m_wrap = m_wrap.previous_sibling(m_name);
  else
  {
    m_wrap = m_parent.last_child();

    if(!Utility::strequal(m_wrap.name(), m_name)) m_wrap = m_wrap.previous_sibling(m_name);
  }

  return *this;
}

inline XmlNamedNodeIterator
XmlNamedNodeIterator::operator--(int)
{
  XmlNamedNodeIterator temp = *this;
  --*this;
  return temp;
}
