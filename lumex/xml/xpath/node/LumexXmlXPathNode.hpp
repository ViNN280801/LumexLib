#ifndef LUMEX_XML_XPATH_NODE_HPP
#define LUMEX_XML_XPATH_NODE_HPP

#include "lumex/LumexExport.hpp"

#include "lumex/core/utility/LumexAttributes.hpp"

#include "lumex/xml/attribute/LumexXmlAttribute.hpp"
#include "lumex/xml/node/LumexXmlNode.hpp"

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
        class LUMEX_API LumexXmlXPathNode
        {
        public:
          using unspecified_bool_type = void (*)(LumexXmlXPathNode ***);

          // Default constructor; constructs empty XPath node
          LumexXmlXPathNode() = default;

          // Construct XPath node from XML node/attribute
          LumexXmlXPathNode(Lumex::Xml::Node::LumexXmlNode const &node);
          LumexXmlXPathNode(Lumex::Xml::Attribute::LumexXmlAttribute const &attribute,
                            Lumex::Xml::Node::LumexXmlNode const &parent);

          // Get node/attribute, if any
          LUMEX_ATTRIBUTE_NODISCARD("The returned XML node from the XPath evaluation should be used; discarding it "
                                    "negates the purpose of the getter.")
          Lumex::Xml::Node::LumexXmlNode node() const;

          LUMEX_ATTRIBUTE_NODISCARD(
            "The returned XML attribute from the XPath evaluation should be used; discarding it "
            "negates the purpose of the getter.")
          Lumex::Xml::Attribute::LumexXmlAttribute attribute() const;

          // Get parent of contained node/attribute
          LUMEX_ATTRIBUTE_NODISCARD(
            "The returned parent XML node should be used; discarding it negates the purpose of the getter.")
          Lumex::Xml::Node::LumexXmlNode parent() const;

          // Safe bool conversion operator
          operator unspecified_bool_type() const;

          bool operator!() const;

          // Comparison operators
          bool operator==(LumexXmlXPathNode const &n) const;
          bool operator!=(LumexXmlXPathNode const &n) const;

        private:
          Lumex::Xml::Node::LumexXmlNode m_node;
          Lumex::Xml::Attribute::LumexXmlAttribute m_attribute;
        };
      } // namespace Node
    } // namespace XPath
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_XPATH_NODE_HPP
