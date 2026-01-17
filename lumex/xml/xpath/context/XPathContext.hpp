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
        /**
         * @brief Represents the XPath evaluation context for an expression.
         * @details This structure holds the essential components of the XPath context,
         *          including the current node, its position within the current node list,
         *          and the total size of that node list. This context is critical for
         *          evaluating XPath expressions that depend on positional information
         *          (e.g., `position()`, `last()`).
         *
         * @note This struct is designed to be lightweight and is typically passed by const reference
         *       during XPath expression evaluation. It is not thread-safe and assumes single-threaded
         *       access within an evaluation context.
         */
        struct LUMEX_API XPathContext {
          /// @brief The current context node.
          Xml::XPath::Node::XPathNode node; // NOLINT(misc-non-private-member-variables-in-classes)
          /// @brief The position of the context node within the context node list (1-indexed).
          size_t position{}; // NOLINT(misc-non-private-member-variables-in-classes)
          /// @brief The total number of nodes in the context node list.
          size_t size{}; // NOLINT(misc-non-private-member-variables-in-classes)

          /**
           * @brief Constructs an XPathContext with the specified node, position, and size.
           * @details Initializes a new XPath context, providing all necessary information
           *          for evaluating XPath expressions relative to a specific node within a set.
           * @param node_ The `XPathNode` representing the current context node.
           * @param position_ The 1-indexed position of `node_` within its containing node list.
           * @param size_ The total number of nodes in the containing node list.
           */
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
