#define LUMEX_IMPLEMENTATION

#include "LumexXmlXPathDocumentOrderComparator.hpp"
#include "lumex/xml/constants/LumexXmlConstants.hpp"

using namespace Lumex::Xml::XPath;

inline bool
node_is_before_sibling(xml_node_t *ln_node, xml_node_t *rn_node) // NOLINT(misc-use-internal-linkage)
{
  LUMEX_ASSERT(ln_node->parent == rn_node->parent);

  // there is no common ancestor (the shared parent is null), nodes are from different documents
  if(ln_node->parent == nullptr) return ln_node < rn_node;

  // determine sibling order
  xml_node_t *ls_node = ln_node;
  xml_node_t *rs_node = rn_node;

  while(ls_node != nullptr && rs_node != nullptr)
  {
    if(ls_node == rn_node) return true;
    if(rs_node == ln_node) return false;

    ls_node = ls_node->next_sibling;
    rs_node = rs_node->next_sibling;
  }

  // if rn sibling chain ended ln must be before rn
  return rs_node == nullptr;
}

inline bool
node_is_before(xml_node_t *ln_node, xml_node_t *rn_node) // NOLINT(misc-use-internal-linkage)
{
  // find common ancestor at the same depth, if any
  xml_node_t *lp_node = ln_node;
  xml_node_t *rp_node = rn_node;

  while(lp_node != nullptr && rp_node != nullptr && lp_node->parent != rp_node->parent)
  {
    lp_node = lp_node->parent;
    rp_node = rp_node->parent;
  }

  // parents are the same!
  if(lp_node != nullptr && rp_node != nullptr) return XPath::node_is_before_sibling(lp_node, rp_node);

  // nodes are at different depths, need to normalize heights
  bool left_higher = lp_node == nullptr;

  while(lp_node != nullptr)
  {
    lp_node = lp_node->parent;
    ln_node = ln_node->parent;
  }

  while(rp_node != nullptr)
  {
    rp_node = rp_node->parent;
    rn_node = rn_node->parent;
  }

  // one node is the ancestor of the other
  if(ln_node == rn_node) return left_higher;

  // find common ancestor... again
  while(ln_node->parent != rn_node->parent)
  {
    ln_node = ln_node->parent;
    rn_node = rn_node->parent;
  }

  return XPath::node_is_before_sibling(ln_node, rn_node);
}

inline void const *
document_buffer_order(LumexXmlXPathNode const &xnode) // NOLINT(misc-use-internal-linkage)
{
  xml_node_t *node = xnode.node().get();

  if(node != nullptr)
  {
    if((get_document(node).header & kxml_memory_page_contents_shared_mask) == 0)
    {
      if((node->name != nullptr) && (node->header & kxml_memory_page_name_allocated_or_shared_mask) == 0)
        return node->name;
      if((node->value != nullptr) && (node->header & kxml_memory_page_value_allocated_or_shared_mask) == 0)
        return node->value;
    }

    return nullptr;
  }

  xml_attr_t *attr = xnode.attribute().get();

  if(attr != nullptr)
  {
    if((get_document(attr).header & kxml_memory_page_contents_shared_mask) == 0)
    {
      if((attr->header & kxml_memory_page_name_allocated_or_shared_mask) == 0) return attr->name;
      if((attr->header & kxml_memory_page_value_allocated_or_shared_mask) == 0) return attr->value;
    }

    return nullptr;
  }

  return nullptr;
}

inline bool
document_order_comparator::operator()(LumexXmlXPathNode const &lhs, LumexXmlXPathNode const &rhs) const
{
  // optimized document order based check
  void const *lo_doc = XPath::document_buffer_order(lhs);
  void const *ro_doc = XPath::document_buffer_order(rhs);

  if(lo_doc != nullptr && ro_doc != nullptr) return lo_doc < ro_doc;

  // slow comparison
  Lumex::Xml::Node::LumexXmlNode ln_node = lhs.node();
  Lumex::Xml::Node::LumexXmlNode rn_node = rhs.node();

  // compare attributes
  if(lhs.attribute() != nullptr && rhs.attribute() != nullptr)
  {
    // shared parent
    if(lhs.parent() == rhs.parent())
    {
      // determine sibling order
      for(Lumex::Xml::Attribute::LumexXmlAttribute attr = lhs.attribute(); attr != nullptr;
          attr                                          = attr.next_attribute())
        if(attr == rhs.attribute()) return true;

      return false;
    }

    // compare attribute parents
    ln_node = lhs.parent();
    rn_node = rhs.parent();
  }
  else if(lhs.attribute() != nullptr)
  {
    // attributes go after the parent element
    if(lhs.parent() == rhs.node()) return false;

    ln_node = lhs.parent();
  }
  else if(rhs.attribute() != nullptr)
  {
    // attributes go after the parent element
    if(rhs.parent() == lhs.node()) return true;

    rn_node = rhs.parent();
  }

  if(ln_node == rn_node) return false;

  if(!ln_node || !rn_node) return ln_node < rn_node;

  return XPath::node_is_before(ln_node.get(), rn_node.get());
}
