#include <algorithm>

#include "lumex/core/utility/LumexMacros.hpp"

#include "lumex/xml/xpath/document/XPathDocumentOrderComparator.hpp"
#include "lumex/xml/xpath/variable/XPathVariable.hpp"

#include "XPathUtils.hpp"

using namespace Lumex::Xml::XPath;
using namespace Lumex::Xml::XPath::Variable;
using namespace Lumex::Xml::XPath::Document;
using namespace Lumex::Xml::XPath::Node;
using namespace Lumex::Xml::XPath::Utility;

inline XPathNodeSet::type_t
xpath_get_order(XPathNode const *begin, XPathNode const *end) // NOLINT(misc-use-internal-linkage)
{
  if(end - begin < 2) return XPathNodeSet::type_sorted;

  document_order_comparator cmp;

  bool first = cmp(begin[0], begin[1]);

  for(XPathNode const *it = begin + 1; it + 1 < end; ++it)
    if(cmp(it[0], it[1]) != first) return XPathNodeSet::type_unsorted;

  return first ? XPathNodeSet::type_sorted : XPathNodeSet::type_sorted_reverse;
}

inline XPathNodeSet::type_t
xpath_sort(XPathNode *begin, XPathNode *end, XPathNodeSet::type_t type, bool rev) // NOLINT(misc-use-internal-linkage)
{
  XPathNodeSet::type_t order = rev ? XPathNodeSet::type_sorted_reverse : XPathNodeSet::type_sorted;

  if(type == XPathNodeSet::type_unsorted)
  {
    XPathNodeSet::type_t sorted = Lumex::Xml::XPath::Utility::xpath_get_order(begin, end);

    if(sorted == XPathNodeSet::type_unsorted)
    {
      std::sort(begin, end, document_order_comparator());

      type = XPathNodeSet::type_sorted;
    }
    else
      type = sorted;
  }

  if(type != order) std::reverse(begin, end);

  return order;
}

inline XPathNode
xpath_first(XPathNode const *begin, XPathNode const *end, // NOLINT(misc-use-internal-linkage)
            XPathNodeSet::type_t type)
{
  if(begin == end) return {};

  switch(type)
  {
  case XPathNodeSet::type_sorted: return *begin;

  case XPathNodeSet::type_sorted_reverse: return *(end - 1);

  case XPathNodeSet::type_unsorted: return *std::min_element(begin, end, document_order_comparator());

  default:
    LUMEX_ASSERT(false && "Invalid node set type"); // unreachable
    return {};
  }
}

inline bool
copy_xpath_variable(XPathVariable *lhs, XPathVariable const *rhs) // NOLINT(misc-use-internal-linkage)
{
  switch(rhs->type())
  {
  case xpath_type_node_set:      // NOLINT(bugprone-branch-clone)
    return lhs->set(static_cast< // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast
                      xpath_variable_node_set const *>(rhs)
                      ->value);
  case xpath_type_number:
    return lhs->set(static_cast< // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
                      xpath_variable_number const *>(rhs)
                      ->value);
  case xpath_type_string:
    return lhs->set(static_cast< // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast
                      xpath_variable_string const *>(rhs)
                      ->value);
  case xpath_type_boolean:
    return lhs->set(static_cast< // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast
                      xpath_variable_boolean const *>(rhs)
                      ->value);

  default:
    LUMEX_ASSERT(false && "Invalid variable type"); // unreachable
    return false;
  }
}
