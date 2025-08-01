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
      namespace detail
      {
        // Forward declarations
        class LumexXmlXPathNode;
        class LumexXmlXPathNodeSet;
        class LumexXmlXPathVariableSet;
        class LumexXmlXPathParseResult;
        class LumexXmlXPathAstNode;
      }

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

          explicit LumexXmlXPathQuery(char_t const *query, detail::LumexXmlXPathVariableSet *variables = nullptr);

          LumexXmlXPathQuery();

          ~LumexXmlXPathQuery();

          LumexXmlXPathQuery(LumexXmlXPathQuery &&rhs) noexcept;
          LumexXmlXPathQuery &operator=(LumexXmlXPathQuery &&rhs) noexcept;

          LUMEX_ATTRIBUTE_NODISCARD("Return type of XPath query should not be discarded.")
          xpath_value_type return_type() const;

          LUMEX_ATTRIBUTE_NODISCARD("Boolean evaluation result should not be discarded.")
          bool evaluate_boolean(detail::LumexXmlXPathNode const &n) const;

          LUMEX_ATTRIBUTE_NODISCARD("Numerical evaluation result should not be discarded.")
          double evaluate_number(detail::LumexXmlXPathNode const &n) const;

          LUMEX_ATTRIBUTE_NODISCARD("String evaluation result should not be discarded.")
          string_t evaluate_string(detail::LumexXmlXPathNode const &n) const;

          size_t evaluate_string(char_t *buffer, size_t capacity, detail::LumexXmlXPathNode const &n) const;

          LUMEX_ATTRIBUTE_NODISCARD("Node set evaluation result should not be discarded.")
          detail::LumexXmlXPathNodeSet evaluate_node_set(detail::LumexXmlXPathNode const &n) const;

          LUMEX_ATTRIBUTE_NODISCARD("Single node evaluation result should not be discarded.")
          detail::LumexXmlXPathNode evaluate_node(detail::LumexXmlXPathNode const &n) const;

          LUMEX_ATTRIBUTE_NODISCARD("Query result set should not be discarded.")
          detail::LumexXmlXPathParseResult const &result() const;

          operator unspecified_bool_type() const;

          bool operator!() const;
        };
      } // namespace Query
      struct LumexXmlXPathQueryImpl {
        static LumexXmlXPathQueryImpl *create();

        static void destroy(LumexXmlXPathQueryImpl *impl);

        LumexXmlXPathQueryImpl();

        detail::LumexXmlXPathAstNode *root{}; // NOLINT(misc-non-private-member-variables-in-classes)
        LumexXmlXPathAllocator alloc;         // NOLINT(misc-non-private-member-variables-in-classes)
        LumexXmlXPathMemoryBlock block{};     // NOLINT(misc-non-private-member-variables-in-classes)
        bool oom{};                           // NOLINT(misc-non-private-member-variables-in-classes)
      };
    } // namespace XPath
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_XPATH_QUERY_HPP
