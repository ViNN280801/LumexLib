#ifndef LUMEX_XML_XPATH_CONTEXT_HPP
#define LUMEX_XML_XPATH_CONTEXT_HPP

#include "lumex/LumexExport.hpp"

#include "lumex/xml/xpath/node/XPathNode.hpp"

using namespace Lumex::Xml::XPath::Node;

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace XPath
    {
      namespace Context
      {
        struct LUMEX_API XPathContext {
          Xml::XPath::Node::XPathNode node; // NOLINT(misc-non-private-member-variables-in-classes)
          size_t position{};                // NOLINT(misc-non-private-member-variables-in-classes)
          size_t size{};                    // NOLINT(misc-non-private-member-variables-in-classes)

          XPathContext(Xml::XPath::Node::XPathNode const &node_,
                       size_t position_, // NOLINT(bugprone-easily-swappable-parameters)
                       size_t size_)
              : node(node_), position(position_), size(size_)
          {}
        };
      } // namespace Context
    } // namespace XPath
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_XPATH_CONTEXT_HPP
