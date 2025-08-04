#ifndef LUMEX_XML_XPATH_DOCUMENT_ORDER_COMPARATOR_HPP
#define LUMEX_XML_XPATH_DOCUMENT_ORDER_COMPARATOR_HPP

#include "lumex/LumexExport.hpp"

#include "lumex/xml/node/XmlNodeBase.hpp"
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
        LUMEX_API
        bool node_is_before_sibling(XmlNodeBase *ln_node, XmlNodeBase *rn_node);

        LUMEX_API
        bool node_is_before(XmlNodeBase *ln_node, XmlNodeBase *rn_node);

        LUMEX_API
        void const *document_buffer_order(XPathNode const &xnode);

        struct LUMEX_API document_order_comparator {
          bool operator()(XPathNode const &lhs, XPathNode const &rhs) const;
        };
      } // namespace Document
    } // namespace XPath
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_XPATH_DOCUMENT_ORDER_COMPARATOR_HPP
