#ifndef LUMEX_XML_XPATH_DOCUMENT_ORDER_COMPARATOR_HPP
#define LUMEX_XML_XPATH_DOCUMENT_ORDER_COMPARATOR_HPP

#include "lumex/xml/node/XmlNode.hpp"
#include "lumex/xml/xpath/node/XPathNode.hpp"

using namespace Lumex::Xml::XPath::Node;
using namespace Lumex::Xml::Node;

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace XPath
    {
      namespace Document
      {
        bool node_is_before_sibling(XmlNode *ln_node, XmlNode *rn_node);

        bool node_is_before(XmlNode *ln_node, XmlNode *rn_node);

        void const *document_buffer_order(XPathNode const &xnode);

        struct document_order_comparator {
          bool operator()(XPathNode const &lhs, XPathNode const &rhs) const;
        };
      } // namespace Document
    } // namespace XPath
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_XPATH_DOCUMENT_ORDER_COMPARATOR_HPP
