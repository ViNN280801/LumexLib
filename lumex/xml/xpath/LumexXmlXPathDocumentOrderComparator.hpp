#ifndef LUMEX_XML_XPATH_DOCUMENT_ORDER_COMPARATOR_HPP
#define LUMEX_XML_XPATH_DOCUMENT_ORDER_COMPARATOR_HPP

#include "lumex/xml/document/LumexXmlDocument.hpp"

#include "lumex/xml/xpath/node/LumexXmlXPathNode.hpp"

using namespace Lumex::Xml::Document;
using namespace Lumex::Xml::XPath::Node;
using namespace Lumex::Xml::Node;
using namespace Lumex::Xml::Constants;

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace XPath
    {
      bool node_is_before_sibling(xml_node_t *ln_node, xml_node_t *rn_node);

      bool node_is_before(xml_node_t *ln_node, xml_node_t *rn_node);

      template <typename Object>
      inline xml_document_t &
      get_document(Object const *object)
      {
        LUMEX_ASSERT(object);

        return *static_cast<xml_document_t *>(   // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
          LUMEX_XML_GETPAGE(object)->allocator); // NOLINT(cppcoreguidelines-pro-type-const-cast)
      }

      void const *document_buffer_order(LumexXmlXPathNode const &xnode);

      struct document_order_comparator {
        bool operator()(LumexXmlXPathNode const &lhs, LumexXmlXPathNode const &rhs) const;
      };
    } // namespace XPath
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_XPATH_DOCUMENT_ORDER_COMPARATOR_HPP
