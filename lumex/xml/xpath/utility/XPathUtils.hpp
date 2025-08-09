#ifndef LUMEX_XML_XPATH_UTILS_HPP
#define LUMEX_XML_XPATH_UTILS_HPP

#include "lumex/LumexExport.hpp"

#include "lumex/xml/xpath/node/XPathNode.hpp"
#include "lumex/xml/xpath/node/XPathNodeSet.hpp"

using namespace Lumex::Xml::XPath::Node;

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace XPath
    {
      namespace Variable
      {
        class XPathVariable;
      }

      namespace Utility
      {
        using namespace Variable;

        LUMEX_API
        XPathNodeSet::type_t xpath_get_order(XPathNode const *begin, XPathNode const *end);

        LUMEX_API
        XPathNodeSet::type_t xpath_sort(XPathNode *begin, XPathNode *end, XPathNodeSet::type_t type, bool rev);

        LUMEX_API
        XPathNode xpath_first(XPathNode const *begin, XPathNode const *end, XPathNodeSet::type_t type);

        LUMEX_API
        bool copy_xpath_variable(XPathVariable *lhs, XPathVariable const *rhs);
      } // namespace Utility
    } // namespace XPath
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_XPATH_UTILS_HPP
