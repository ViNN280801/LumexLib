#ifndef LUMEX_XML_XPATH_QUERY_HPP
#define LUMEX_XML_XPATH_QUERY_HPP

#include "lumex/core/utility/LumexAttributes.hpp"

#include "lumex/xml/text/XmlParseResult.hpp"
#include "lumex/xml/types/XmlTypes.hpp"
#include "lumex/xml/xpath/memory/XPathAllocator.hpp"
#include "lumex/xml/xpath/memory/XPathMemoryBlock.hpp"

using namespace Lumex::Xml::Types;
using namespace Lumex::Xml::Text;
using namespace Lumex::Xml::XPath::Memory;

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace XPath
    {
      namespace Ast
      {
        class XPathAstNode;
      }
      namespace Node
      {
        class XPathNode;
        class XPathNodeSet;
      }
      namespace Variable
      {
        class XPathVariableSet;
      }
      namespace Parser
      {
        class XPathParseResult;
      }

      namespace Query
      {
        class XPathQuery
        {
        private:
          void *m_impl;
          XmlParseResult m_result;

          XPathQuery(XPathQuery const &);
          XPathQuery &operator=(XPathQuery const &);

        public:
          using unspecified_bool_type = void (*)(XPathQuery ***);

          explicit XPathQuery(char_t const *query, Variable::XPathVariableSet *variables = nullptr);

          XPathQuery();

          ~XPathQuery();

          XPathQuery(XPathQuery &&rhs) noexcept;
          XPathQuery &operator=(XPathQuery &&rhs) noexcept;

          LUMEX_ATTRIBUTE_NODISCARD("Return type of XPath query should not be discarded.")
          xpath_value_type return_type() const;

          LUMEX_ATTRIBUTE_NODISCARD("Boolean evaluation result should not be discarded.")
          bool evaluate_boolean(Node::XPathNode const &n) const;

          LUMEX_ATTRIBUTE_NODISCARD("Numerical evaluation result should not be discarded.")
          double evaluate_number(Node::XPathNode const &n) const;

          LUMEX_ATTRIBUTE_NODISCARD("String evaluation result should not be discarded.")
          string_t evaluate_string(Node::XPathNode const &n) const;

          size_t evaluate_string(char_t *buffer, size_t capacity, Node::XPathNode const &n) const;

          LUMEX_ATTRIBUTE_NODISCARD("Node set evaluation result should not be discarded.")
          Node::XPathNodeSet evaluate_node_set(Node::XPathNode const &n) const;

          LUMEX_ATTRIBUTE_NODISCARD("Single node evaluation result should not be discarded.")
          Node::XPathNode evaluate_node(Node::XPathNode const &n) const;

          LUMEX_ATTRIBUTE_NODISCARD("Query result set should not be discarded.")
          Parser::XPathParseResult const &result() const;

          operator unspecified_bool_type() const;

          bool operator!() const;
        };
      } // namespace Query
      struct XPathQueryImpl {
        static XPathQueryImpl *create();

        static void destroy(XPathQueryImpl *impl);

        XPathQueryImpl();

        Ast::XPathAstNode *root{}; // NOLINT(misc-non-private-member-variables-in-classes)
        XPathAllocator alloc;         // NOLINT(misc-non-private-member-variables-in-classes)
        XPathMemoryBlock block{};     // NOLINT(misc-non-private-member-variables-in-classes)
        bool oom{};                   // NOLINT(misc-non-private-member-variables-in-classes)
      };
    } // namespace XPath
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_XPATH_QUERY_HPP
