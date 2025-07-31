#define LUMEX_IMPLEMENTATION

#include "LumexXmlXPathNode.hpp"

using namespace Lumex::Xml::XPath::Node;

inline LumexXmlXPathNode::LumexXmlXPathNode(Lumex::Xml::Node::LumexXmlNode const &node_) : m_node(node_) {}

inline LumexXmlXPathNode::LumexXmlXPathNode(Lumex::Xml::Attribute::LumexXmlAttribute const &attribute_,
                                            Lumex::Xml::Node::LumexXmlNode const &parent_)
    : m_node((attribute_ != nullptr) ? parent_ : Lumex::Xml::Node::LumexXmlNode()), m_attribute(attribute_)
{}

inline Lumex::Xml::Node::LumexXmlNode
LumexXmlXPathNode::node() const
{
  return (m_attribute != nullptr) ? Lumex::Xml::Node::LumexXmlNode() : m_node;
}

inline Lumex::Xml::Attribute::LumexXmlAttribute
LumexXmlXPathNode::attribute() const
{
  return m_attribute;
}

inline Lumex::Xml::Node::LumexXmlNode
LumexXmlXPathNode::parent() const
{
  return (m_attribute != nullptr) ? m_node : m_node.parent();
}

inline static void
unspecified_bool_xpath_node(LumexXmlXPathNode *** /*unused*/) // NOLINT(misc-use-anonymous-namespace)
{}

inline LumexXmlXPathNode::
operator LumexXmlXPathNode::unspecified_bool_type() const
{
  return ((m_node != nullptr) || (m_attribute != nullptr)) ? unspecified_bool_xpath_node : nullptr;
}

inline bool
LumexXmlXPathNode::operator!() const
{
  return (m_node == nullptr) && (m_attribute == nullptr);
}

inline bool
LumexXmlXPathNode::operator==(LumexXmlXPathNode const &n) const
{
  return m_node == n.m_node && m_attribute == n.m_attribute;
}

inline bool
LumexXmlXPathNode::operator!=(LumexXmlXPathNode const &n) const
{
  return m_node != n.m_node || m_attribute != n.m_attribute;
}
