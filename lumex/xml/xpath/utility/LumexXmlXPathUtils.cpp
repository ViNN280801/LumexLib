#include <algorithm>

#include "lumex/xml/xpath/LumexXmlXPathDocumentOrderComparator.hpp"

#include "LumexXmlXPathUtils.hpp"

using namespace Lumex::Xml::XPath;
using namespace Lumex::Xml::XPath::Node;
using namespace Lumex::Xml::XPath::Utility;

inline LumexXmlXPathNodeSet::type_t
xpath_get_order(LumexXmlXPathNode const *begin, LumexXmlXPathNode const *end)
{
  if(end - begin < 2) return LumexXmlXPathNodeSet::type_sorted;

  document_order_comparator cmp;

  bool first = cmp(begin[0], begin[1]);

  for(LumexXmlXPathNode const *it = begin + 1; it + 1 < end; ++it)
    if(cmp(it[0], it[1]) != first) return LumexXmlXPathNodeSet::type_unsorted;

  return first ? LumexXmlXPathNodeSet::type_sorted
               : LumexXmlXPathNodeSet::type_sorted_reverse;
}

inline LumexXmlXPathNodeSet::type_t
xpath_sort(LumexXmlXPathNode *begin, LumexXmlXPathNode *end, LumexXmlXPathNodeSet::type_t type,
           bool rev)
{
  LumexXmlXPathNodeSet::type_t order
    = rev ? LumexXmlXPathNodeSet::type_sorted_reverse
          : LumexXmlXPathNodeSet::type_sorted;

  if(type == LumexXmlXPathNodeSet::type_unsorted)
  {
    LumexXmlXPathNodeSet::type_t sorted = xpath_get_order(begin, end);

    if(sorted == LumexXmlXPathNodeSet::type_unsorted)
    {
      std::sort(begin, end, document_order_comparator());

      type = LumexXmlXPathNodeSet::type_sorted;
    }
    else
      type = sorted;
  }

  if(type != order) std::reverse(begin, end);

  return order;
}

inline LumexXmlXPathNode
xpath_first(LumexXmlXPathNode const *begin, LumexXmlXPathNode const *end,
            LumexXmlXPathNodeSet::type_t type)
{
  if(begin == end) return {};

  switch(type)
  {
  case LumexXmlXPathNodeSet::type_sorted: return *begin;

  case LumexXmlXPathNodeSet::type_sorted_reverse: return *(end - 1);

  case LumexXmlXPathNodeSet::type_unsorted:
    return *std::min_element(begin, end, document_order_comparator());

  default:
    LUMEX_ASSERT(false && "Invalid node set type"); // unreachable
    return {};
  }
}

inline bool
copy_xpath_variable(xpath_variable *lhs, xpath_variable const *rhs)
{
  switch(rhs->type())
  {
  case xpath_type_node_set: return lhs->set(static_cast<xpath_variable_node_set const *>(rhs)->value);

  case xpath_type_number: return lhs->set(static_cast<xpath_variable_number const *>(rhs)->value);

  case xpath_type_string: return lhs->set(static_cast<xpath_variable_string const *>(rhs)->value);

  case xpath_type_boolean: return lhs->set(static_cast<xpath_variable_boolean const *>(rhs)->value);

  default:
    LUMEX_ASSERT(false && "Invalid variable type"); // unreachable
    return false;
  }
}
