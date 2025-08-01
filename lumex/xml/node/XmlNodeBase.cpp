#include "lumex/core/utility/LumexMacros.hpp"
#include "lumex/xml/utility/XmlUtils.hpp"

#include "lumex/xml/constants/XmlConstants.hpp"

#include "XmlNodeBase.hpp"

using namespace Lumex::Xml::Attribute;
using namespace Lumex::Xml::Constants;
using namespace Lumex::Xml::Node;
using namespace Lumex::Xml::Memory;
using namespace Lumex::Xml::Types;
using namespace Lumex::Xml::Utility;

inline XmlNodeBase *
allocate_node(XmlAllocator &alloc, xml_node_type type) // NOLINT(misc-use-internal-linkage)
{
  XmlMemoryPage *page{};
  void *memory = alloc.allocate_object(sizeof(XmlNodeBase), page);
  if(memory == nullptr) return nullptr;

  return new(memory) XmlNodeBase(page, type); // NOLINT(cppcoreguidelines-owning-memory)
}

inline void
destroy_node(XmlNodeBase *n, XmlAllocator &alloc) // NOLINT(misc-use-internal-linkage)
{
  if((n->header & kxml_memory_page_name_allocated_mask) != 0) alloc.deallocate_string(n->name);

  if((n->header & kxml_memory_page_value_allocated_mask) != 0) alloc.deallocate_string(n->value);

  for(XmlAttributeBase *attr = n->first_attribute; attr != nullptr;)
  {
    XmlAttributeBase *next = attr->next_attribute;

    destroy_attribute(attr, alloc);

    attr = next;
  }

  for(XmlNodeBase *child = n->first_child; child != nullptr;)
  {
    XmlNodeBase *next = child->next_sibling;

    Lumex::Xml::Node::destroy_node(child, alloc);

    child = next;
  }

  alloc.deallocate_memory(n, sizeof(XmlNodeBase),
                          LUMEX_XML_GETPAGE(n)); // NOLINT(cppcoreguidelines-pro-type-const-cast)
}

inline void
append_node(XmlNodeBase *child, XmlNodeBase *node) // NOLINT(misc-use-internal-linkage)
{
  child->parent     = node;

  XmlNodeBase *head = node->first_child;

  if(head != nullptr)
  {
    XmlNodeBase *tail     = head->prev_sibling_c;

    tail->next_sibling    = child;
    child->prev_sibling_c = tail;
    head->prev_sibling_c  = child;
  }
  else
  {
    node->first_child     = child;
    child->prev_sibling_c = child;
  }
}

inline void
prepend_node(XmlNodeBase *child, XmlNodeBase *node) // NOLINT(misc-use-internal-linkage)
{
  child->parent     = node;

  XmlNodeBase *head = node->first_child;

  if(head != nullptr)
  {
    child->prev_sibling_c = head->prev_sibling_c;
    head->prev_sibling_c  = child;
  }
  else
    child->prev_sibling_c = child;

  child->next_sibling = head;
  node->first_child   = child;
}

inline void
insert_node_after(XmlNodeBase *child, XmlNodeBase *node) // NOLINT(misc-use-internal-linkage)
{
  XmlNodeBase *parent = node->parent;

  child->parent       = parent;

  XmlNodeBase *next   = node->next_sibling;

  if(next != nullptr)
    next->prev_sibling_c = child;
  else
    parent->first_child->prev_sibling_c = child;

  child->next_sibling   = next;
  child->prev_sibling_c = node;

  node->next_sibling    = child;
}

inline void
insert_node_before(XmlNodeBase *child, XmlNodeBase *node) // NOLINT(misc-use-internal-linkage)
{
  XmlNodeBase *parent = node->parent;

  child->parent       = parent;

  XmlNodeBase *prev   = node->prev_sibling_c;

  if(prev->next_sibling != nullptr)
    prev->next_sibling = child;
  else
    parent->first_child = child;

  child->prev_sibling_c = prev;
  child->next_sibling   = node;

  node->prev_sibling_c  = child;
}

inline void
remove_node(XmlNodeBase *node) // NOLINT(misc-use-internal-linkage)
{
  XmlNodeBase *parent = node->parent;

  XmlNodeBase *next   = node->next_sibling;
  XmlNodeBase *prev   = node->prev_sibling_c;

  if(next != nullptr)
    next->prev_sibling_c = prev;
  else
    parent->first_child->prev_sibling_c = prev;

  if(prev->next_sibling != nullptr)
    prev->next_sibling = next;
  else
    parent->first_child = next;

  node->parent         = nullptr;
  node->prev_sibling_c = nullptr;
  node->next_sibling   = nullptr;
}

LUMEX_ATTRIBUTE_NOINLINE XmlNodeBase *
append_new_node(XmlNodeBase *node, XmlAllocator &alloc, xml_node_type type) // NOLINT(misc-use-internal-linkage)
{
  XmlNodeBase *child = Lumex::Xml::Node::allocate_node(alloc, type);
  if(child == nullptr) return nullptr;

  Lumex::Xml::Node::append_node(child, node);

  return child;
}

inline void
node_copy_contents(XmlNodeBase *dn_, XmlNodeBase *sn_, // NOLINT(misc-use-internal-linkage)
                   XmlAllocator *shared_alloc)
{
  Lumex::Xml::Utility::node_copy_string(dn_->name, dn_->header, kxml_memory_page_name_allocated_mask, sn_->name,
                                        sn_->header, shared_alloc);
  Lumex::Xml::Utility::node_copy_string(dn_->value, dn_->header, kxml_memory_page_value_allocated_mask, sn_->value,
                                        sn_->header, shared_alloc);

  for(XmlAttributeBase *sa = sn_->first_attribute; sa != nullptr; sa = sa->next_attribute)
  {
    XmlAttributeBase *da_ = append_new_attribute(dn_, get_allocator(dn_));

    if(da_ != nullptr)
    {
      Lumex::Xml::Utility::node_copy_string(da_->name, da_->header, kxml_memory_page_name_allocated_mask, sa->name,
                                            sa->header, shared_alloc);
      Lumex::Xml::Utility::node_copy_string(da_->value, da_->header, kxml_memory_page_value_allocated_mask, sa->value,
                                            sa->header, shared_alloc);
    }
  }
}

inline void
node_copy_tree(XmlNodeBase *dn_, XmlNodeBase *sn_) // NOLINT(misc-use-internal-linkage)
{
  XmlAllocator &alloc        = get_allocator(dn_);
  XmlAllocator *shared_alloc = (&alloc == &get_allocator(sn_)) ? &alloc : nullptr;

  Lumex::Xml::Node::node_copy_contents(dn_, sn_, shared_alloc);

  XmlNodeBase *dit = dn_;
  XmlNodeBase *sit = sn_->first_child;

  while((sit != nullptr) && sit != sn_)
  {
    // loop invariant: dit is inside the subtree rooted at dn
    LUMEX_ASSERT(dit);

    // when a tree is copied into one of the descendants, we need to skip that subtree to avoid an infinite loop
    if(sit != dn_)
    {
      XmlNodeBase *copy = Lumex::Xml::Node::append_new_node(dit, alloc, LUMEX_XML_NODETYPE(sit));

      if(copy != nullptr)
      {
        Lumex::Xml::Node::node_copy_contents(copy, sit, shared_alloc);

        if(sit->first_child != nullptr)
        {
          dit = copy;
          sit = sit->first_child;
          continue;
        }
      }
    }

    // continue to the next node
    do { // NOLINT(cppcoreguidelines-avoid-do-while)
      if(sit->next_sibling != nullptr)
      {
        sit = sit->next_sibling;
        break;
      }

      sit = sit->parent;
      dit = dit->parent;

      // loop invariant: dit is inside the subtree rooted at dn while sit is inside sn
      LUMEX_ASSERT((sit == sn_) || (dit != nullptr));
    } while(sit != sn_);
  }

  LUMEX_ASSERT((sit == nullptr) || (dit == dn_->parent));
}
