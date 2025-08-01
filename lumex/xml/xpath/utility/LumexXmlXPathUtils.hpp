#ifndef LUMEX_XML_XPATH_UTILS_HPP
#define LUMEX_XML_XPATH_UTILS_HPP

#include "lumex/xml/xpath/node/LumexXmlXPathNode.hpp"
#include "lumex/xml/xpath/node/LumexXmlXPathNodeSet.hpp"

using namespace Lumex::Xml::XPath::Node;

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace XPath
    {
      namespace Variable
      {
        class LumexXmlXPathVariable;
      }

      namespace Utility
      {
        using namespace Variable;

        inline LumexXmlXPathNodeSet::type_t
        xpath_get_order(LumexXmlXPathNode const *begin, LumexXmlXPathNode const *end);

        inline LumexXmlXPathNodeSet::type_t
        xpath_sort(LumexXmlXPathNode *begin, LumexXmlXPathNode *end, LumexXmlXPathNodeSet::type_t type, bool rev);

        inline LumexXmlXPathNode
        xpath_first(LumexXmlXPathNode const *begin, LumexXmlXPathNode const *end, LumexXmlXPathNodeSet::type_t type);

        inline bool copy_xpath_variable(LumexXmlXPathVariable *lhs, LumexXmlXPathVariable const *rhs);
      } // namespace Utility
    } // namespace XPath
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_XPATH_UTILS_HPP
