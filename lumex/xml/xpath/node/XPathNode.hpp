#ifndef LUMEX_XML_XPATH_NODE_HPP
#define LUMEX_XML_XPATH_NODE_HPP

#include "lumex/core/utility/LumexAttributes.hpp"

#include "lumex/xml/attribute/XmlAttribute.hpp"
#include "lumex/xml/node/XmlNode.hpp"

using namespace Lumex::Xml::Node;
using namespace Lumex::Xml::Attribute;

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace XPath
    {
      namespace Node
      {
        class XPathNode
        {
        public:
          using unspecified_bool_type = void (*)(XPathNode ***);

          // Default constructor; constructs empty XPath node
          XPathNode() = default;

          // Construct XPath node from XML node/attribute
          XPathNode(Lumex::Xml::Node::XmlNode const &node);
          XPathNode(Lumex::Xml::Attribute::XmlAttribute const &attribute, Lumex::Xml::Node::XmlNode const &parent);

          // Get node/attribute, if any
          LUMEX_ATTRIBUTE_NODISCARD("The returned XML node from the XPath evaluation should be used; discarding it "
                                    "negates the purpose of the getter.")
          Lumex::Xml::Node::XmlNode node() const;

          LUMEX_ATTRIBUTE_NODISCARD(
            "The returned XML attribute from the XPath evaluation should be used; discarding it "
            "negates the purpose of the getter.")
          Lumex::Xml::Attribute::XmlAttribute attribute() const;

          // Get parent of contained node/attribute
          LUMEX_ATTRIBUTE_NODISCARD(
            "The returned parent XML node should be used; discarding it negates the purpose of the getter.")
          Lumex::Xml::Node::XmlNode parent() const;

          // Safe bool conversion operator
          operator unspecified_bool_type() const;

          bool operator!() const;

          // Comparison operators
          bool operator==(XPathNode const &n) const;
          bool operator!=(XPathNode const &n) const;

        private:
          Lumex::Xml::Node::XmlNode m_node;
          Lumex::Xml::Attribute::XmlAttribute m_attribute;
        };

        bool operator&&(XPathNode const &lhs, bool rhs);

        bool operator||(XPathNode const &lhs, bool rhs);
      } // namespace Node
    } // namespace XPath
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_XPATH_NODE_HPP
