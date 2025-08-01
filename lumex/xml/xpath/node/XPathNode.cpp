#define LUMEX_IMPLEMENTATION

#include "XPathNode.hpp"

using namespace Lumex::Xml::XPath::Node;

inline XPathNode::XPathNode(Lumex::Xml::Node::XmlNode const &node_) : m_node(node_) {}

inline XPathNode::XPathNode(Lumex::Xml::Attribute::XmlAttribute const &attribute_,
                            Lumex::Xml::Node::XmlNode const &parent_)
    : m_node((attribute_ != nullptr) ? parent_ : Lumex::Xml::Node::XmlNode()), m_attribute(attribute_)
{}

inline Lumex::Xml::Node::XmlNode
XPathNode::node() const
{
  return (m_attribute != nullptr) ? Lumex::Xml::Node::XmlNode() : m_node;
}

inline Lumex::Xml::Attribute::XmlAttribute
XPathNode::attribute() const
{
  return m_attribute;
}

inline Lumex::Xml::Node::XmlNode
XPathNode::parent() const
{
  return (m_attribute != nullptr) ? m_node : m_node.parent();
}

inline static void
unspecified_bool_xpath_node(XPathNode *** /*unused*/) // NOLINT(misc-use-anonymous-namespace)
{}

inline XPathNode::
operator XPathNode::unspecified_bool_type() const
{
  return ((m_node != nullptr) || (m_attribute != nullptr)) ? unspecified_bool_xpath_node : nullptr;
}

inline bool
XPathNode::operator!() const
{
  return (m_node == nullptr) && (m_attribute == nullptr);
}

inline bool
XPathNode::operator==(XPathNode const &n) const
{
  return m_node == n.m_node && m_attribute == n.m_attribute;
}

inline bool
XPathNode::operator!=(XPathNode const &n) const
{
  return m_node != n.m_node || m_attribute != n.m_attribute;
}

inline bool
operator&&(XPathNode const &lhs, bool rhs) // NOLINT(misc-use-internal-linkage)
{
  return (bool)lhs && rhs;
}

inline bool
operator||(XPathNode const &lhs, bool rhs) // NOLINT(misc-use-internal-linkage)
{
  return (bool)lhs || rhs;
}
