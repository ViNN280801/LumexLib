#define LUMEX_IMPLEMENTATION

#include "XPathNode.hpp"

using namespace lumex::xml::xpath::node;

LUMEX_PUBLIC_API
inline XPathNode::XPathNode (lumex::xml::node::XmlNode const &node_)
    : m_node (node_)
{
}

LUMEX_PUBLIC_API
inline XPathNode::XPathNode (
    lumex::xml::attribute::XmlAttribute const &attribute_,
    lumex::xml::node::XmlNode const &parent_)
    : m_node ((attribute_ != nullptr) ? parent_
                                      : lumex::xml::node::XmlNode ()),
      m_attribute (attribute_)
{
}

LUMEX_PUBLIC_API
inline lumex::xml::node::XmlNode
XPathNode::node () const
{
  return (m_attribute != nullptr) ? lumex::xml::node::XmlNode () : m_node;
}

LUMEX_PUBLIC_API
inline lumex::xml::attribute::XmlAttribute
XPathNode::attribute () const
{
  return m_attribute;
}

LUMEX_PUBLIC_API
inline lumex::xml::node::XmlNode
XPathNode::parent () const
{
  return (m_attribute != nullptr) ? m_node : m_node.parent ();
}

inline static void
unspecified_bool_xpath_node (
    XPathNode *** /*unused*/) // NOLINT(misc-use-anonymous-namespace)
{
}

LUMEX_PUBLIC_API
inline XPathNode::
operator XPathNode::unspecified_bool_type () const
{
  return ((m_node != nullptr) || (m_attribute != nullptr))
             ? unspecified_bool_xpath_node
             : nullptr;
}

LUMEX_PUBLIC_API
inline bool
XPathNode::operator!() const
{
  return (m_node == nullptr) && (m_attribute == nullptr);
}

LUMEX_PUBLIC_API
inline bool
XPathNode::operator== (XPathNode const &n) const
{
  return m_node == n.m_node && m_attribute == n.m_attribute;
}

LUMEX_PUBLIC_API
inline bool
XPathNode::operator!= (XPathNode const &n) const
{
  return m_node != n.m_node || m_attribute != n.m_attribute;
}

LUMEX_PUBLIC_API
inline bool
lumex::xml::xpath::node::operator&& (
    XPathNode const &lhs, bool rhs) // NOLINT(misc-use-internal-linkage)
{
  return (bool)lhs && rhs;
}

LUMEX_PUBLIC_API
inline bool
lumex::xml::xpath::node::operator|| (
    XPathNode const &lhs, bool rhs) // NOLINT(misc-use-internal-linkage)
{
  return (bool)lhs || rhs;
}
