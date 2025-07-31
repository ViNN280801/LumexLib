#define LUMEX_IMPLEMENTATION

#include "LumexXmlNodeIterator.hpp"

inline LumexXmlNodeIterator::LumexXmlNodeIterator(LumexXmlNode const &node) : m_wrap(node), m_parent(node.parent()) {}

inline LumexXmlNodeIterator::LumexXmlNodeIterator(xml_node_t *ref, // NOLINT(bugprone-easily-swappable-parameters)
                                                  xml_node_t *parent)
    : m_wrap(ref), m_parent(parent)
{}

inline bool
LumexXmlNodeIterator::operator==(LumexXmlNodeIterator const &rhs) const
{
  return m_wrap.m_root == rhs.m_wrap.m_root && m_parent.m_root == rhs.m_parent.m_root;
}

inline bool
LumexXmlNodeIterator::operator!=(LumexXmlNodeIterator const &rhs) const
{
  return m_wrap.m_root != rhs.m_wrap.m_root || m_parent.m_root != rhs.m_parent.m_root;
}

inline Lumex::Xml::Node::LumexXmlNode &
LumexXmlNodeIterator::operator*() const
{
  LUMEX_ASSERT(m_wrap.m_root);
  return m_wrap;
}

inline Lumex::Xml::Node::LumexXmlNode *
LumexXmlNodeIterator::operator->() const
{
  LUMEX_ASSERT(m_wrap.m_root);
  return &m_wrap;
}

inline LumexXmlNodeIterator &
LumexXmlNodeIterator::operator++()
{
  LUMEX_ASSERT(m_wrap.m_root);
  m_wrap.m_root = m_wrap.m_root->next_sibling;
  return *this;
}

inline LumexXmlNodeIterator
LumexXmlNodeIterator::operator++(int)
{
  LumexXmlNodeIterator temp = *this;
  ++*this;
  return temp;
}

inline LumexXmlNodeIterator &
LumexXmlNodeIterator::operator--()
{
  m_wrap = (m_wrap.m_root != nullptr) ? m_wrap.previous_sibling() : m_parent.last_child();
  return *this;
}

inline LumexXmlNodeIterator
LumexXmlNodeIterator::operator--(int)
{
  LumexXmlNodeIterator temp = *this;
  --*this;
  return temp;
}
