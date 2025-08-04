#define LUMEX_IMPLEMENTATION

#include "lumex/core/utility/LumexAssert.hpp"

#include "XmlNodeIterator.hpp"

using namespace Lumex::Xml::Node;

LUMEX_PUBLIC_API
inline XmlNodeIterator::XmlNodeIterator(XmlNode const &node) : m_wrap(node), m_parent(node.parent()) {}

LUMEX_PUBLIC_API
inline XmlNodeIterator::XmlNodeIterator(XmlNodeBase *ref, // NOLINT(bugprone-easily-swappable-parameters)
                                        XmlNodeBase *parent)
    : m_wrap(ref), m_parent(parent)
{}

LUMEX_PUBLIC_API
inline bool
XmlNodeIterator::operator==(XmlNodeIterator const &rhs) const
{
  return m_wrap.m_root == rhs.m_wrap.m_root && m_parent.m_root == rhs.m_parent.m_root;
}

LUMEX_PUBLIC_API
inline bool
XmlNodeIterator::operator!=(XmlNodeIterator const &rhs) const
{
  return m_wrap.m_root != rhs.m_wrap.m_root || m_parent.m_root != rhs.m_parent.m_root;
}

LUMEX_PUBLIC_API
inline XmlNode &
XmlNodeIterator::operator*() const
{
  LUMEX_ASSERT(m_wrap.m_root);
  return m_wrap;
}

LUMEX_PUBLIC_API
inline XmlNode *
XmlNodeIterator::operator->() const
{
  LUMEX_ASSERT(m_wrap.m_root);
  return &m_wrap;
}

LUMEX_PUBLIC_API
inline XmlNodeIterator &
XmlNodeIterator::operator++()
{
  LUMEX_ASSERT(m_wrap.m_root);
  m_wrap.m_root = m_wrap.m_root->next_sibling;
  return *this;
}

LUMEX_PUBLIC_API
inline XmlNodeIterator
XmlNodeIterator::operator++(int)
{
  XmlNodeIterator temp = *this;
  ++*this;
  return temp;
}

LUMEX_PUBLIC_API
inline XmlNodeIterator &
XmlNodeIterator::operator--()
{
  m_wrap = (m_wrap.m_root != nullptr) ? m_wrap.previous_sibling() : m_parent.last_child();
  return *this;
}

LUMEX_PUBLIC_API
inline XmlNodeIterator
XmlNodeIterator::operator--(int)
{
  XmlNodeIterator temp = *this;
  --*this;
  return temp;
}
