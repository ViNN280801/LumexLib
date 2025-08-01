#include "XmlNodeIterator.hpp"

#include "lumex/core/utility/LumexMacros.hpp"

using namespace Lumex::Xml::Node;

inline XmlNodeIterator::XmlNodeIterator(XmlNode const &node) : m_wrap(node), m_parent(node.parent()) {}

inline XmlNodeIterator::XmlNodeIterator(XmlNodeBase *ref, // NOLINT(bugprone-easily-swappable-parameters)
                                        XmlNodeBase *parent)
    : m_wrap(ref), m_parent(parent)
{}

inline bool
XmlNodeIterator::operator==(XmlNodeIterator const &rhs) const
{
  return m_wrap.m_root == rhs.m_wrap.m_root && m_parent.m_root == rhs.m_parent.m_root;
}

inline bool
XmlNodeIterator::operator!=(XmlNodeIterator const &rhs) const
{
  return m_wrap.m_root != rhs.m_wrap.m_root || m_parent.m_root != rhs.m_parent.m_root;
}

inline XmlNode &
XmlNodeIterator::operator*() const
{
  LUMEX_ASSERT(m_wrap.m_root);
  return m_wrap;
}

inline XmlNode *
XmlNodeIterator::operator->() const
{
  LUMEX_ASSERT(m_wrap.m_root);
  return &m_wrap;
}

inline XmlNodeIterator &
XmlNodeIterator::operator++()
{
  LUMEX_ASSERT(m_wrap.m_root);
  m_wrap.m_root = m_wrap.m_root->next_sibling;
  return *this;
}

inline XmlNodeIterator
XmlNodeIterator::operator++(int)
{
  XmlNodeIterator temp = *this;
  ++*this;
  return temp;
}

inline XmlNodeIterator &
XmlNodeIterator::operator--()
{
  m_wrap = (m_wrap.m_root != nullptr) ? m_wrap.previous_sibling() : m_parent.last_child();
  return *this;
}

inline XmlNodeIterator
XmlNodeIterator::operator--(int)
{
  XmlNodeIterator temp = *this;
  --*this;
  return temp;
}
