#ifndef LUMEX_XML_XPATH_QUERY_HPP
#define LUMEX_XML_XPATH_QUERY_HPP

#include "lumex/LumexExport.hpp"

#include "lumex/core/utility/LumexAttributes.hpp"

#include "lumex/xml/text/LumexXmlParseResult.hpp"
#include "lumex/xml/types/LumexXmlTypes.hpp"
#include "lumex/xml/xpath/memory/LumexXmlXPathAllocator.hpp"
#include "lumex/xml/xpath/memory/LumexXmlXPathMemoryBlock.hpp"

using namespace Lumex::Xml::Types;
using namespace Lumex::Xml::Text;
using namespace Lumex::Xml::XPath::Memory;

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace XPath
    {
      // Forward declarations
      class LumexXmlXPathNode;
      class LumexXmlXPathNodeSet;
      class LumexXmlXPathVariableSet;
      class LumexXmlXPathParseResult;
      class LumexXmlXPathAstNode;

      namespace Query
      {
        class LUMEX_API LumexXmlXPathQuery
        {
        private:
          void *m_impl;
          LumexXmlParseResult m_result;

          LumexXmlXPathQuery(LumexXmlXPathQuery const &);
          LumexXmlXPathQuery &operator=(LumexXmlXPathQuery const &);

        public:
          using unspecified_bool_type = void (*)(LumexXmlXPathQuery ***);

          explicit LumexXmlXPathQuery(char_t const *query, LumexXmlXPathVariableSet *variables = nullptr);

          LumexXmlXPathQuery();

          ~LumexXmlXPathQuery();

          LumexXmlXPathQuery(LumexXmlXPathQuery &&rhs) noexcept;
          LumexXmlXPathQuery &operator=(LumexXmlXPathQuery &&rhs) noexcept;

          LUMEX_ATTRIBUTE_NODISCARD("Return type of XPath query should not be discarded.")
          xpath_value_type return_type() const;

          LUMEX_ATTRIBUTE_NODISCARD("Boolean evaluation result should not be discarded.")
          bool evaluate_boolean(LumexXmlXPathNode const &n) const;

          LUMEX_ATTRIBUTE_NODISCARD("Numerical evaluation result should not be discarded.")
          double evaluate_number(LumexXmlXPathNode const &n) const;

          LUMEX_ATTRIBUTE_NODISCARD("String evaluation result should not be discarded.")
          string_t evaluate_string(LumexXmlXPathNode const &n) const;

          size_t evaluate_string(char_t *buffer, size_t capacity, LumexXmlXPathNode const &n) const;

          LUMEX_ATTRIBUTE_NODISCARD("Node set evaluation result should not be discarded.")
          LumexXmlXPathNodeSet evaluate_node_set(LumexXmlXPathNode const &n) const;

          LUMEX_ATTRIBUTE_NODISCARD("Single node evaluation result should not be discarded.")
          LumexXmlXPathNode evaluate_node(LumexXmlXPathNode const &n) const;

          LUMEX_ATTRIBUTE_NODISCARD("Query result set should not be discarded.")
          LumexXmlXPathParseResult const &result() const;

          operator unspecified_bool_type() const;

          bool operator!() const;
        };
      } // namespace Query
      struct LumexXmlXPathQueryImpl {
        static LumexXmlXPathQueryImpl *create();

        static void destroy(LumexXmlXPathQueryImpl *impl);

        LumexXmlXPathQueryImpl();

        LumexXmlXPathAstNode *root{};     // NOLINT(misc-non-private-member-variables-in-classes)
        LumexXmlXPathAllocator alloc;     // NOLINT(misc-non-private-member-variables-in-classes)
        LumexXmlXPathMemoryBlock block{}; // NOLINT(misc-non-private-member-variables-in-classes)
        bool oom{};                       // NOLINT(misc-non-private-member-variables-in-classes)
      };
    } // namespace XPath
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_XPATH_QUERY_HPP
