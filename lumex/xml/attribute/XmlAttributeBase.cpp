#include "lumex/xml/constants/XmlConstants.hpp"
#include "lumex/xml/node/XmlNodeBase.hpp"
#include "lumex/xml/utility/XmlUtils.hpp"

#include "XmlAttributeBase.hpp"

using namespace Lumex::Xml::Constants;
using namespace Lumex::Xml::Node;
using namespace Lumex::Xml::Utility;

inline XmlAttributeBase *
allocate_attribute(XmlAllocator &alloc) // NOLINT(misc-use-internal-linkage)
{
  XmlMemoryPage *page{};
  void *memory = alloc.allocate_object(sizeof(XmlAttributeBase), page);
  if(memory == nullptr) return nullptr;

  return new(memory) XmlAttributeBase(page); // NOLINT(cppcoreguidelines-owning-memory)
}

inline void
destroy_attribute(XmlAttributeBase *attr, XmlAllocator &alloc) // NOLINT(misc-use-internal-linkage)
{
  if((attr->header & kxml_memory_page_name_allocated_mask) != 0) alloc.deallocate_string(attr->name);

  if((attr->header & kxml_memory_page_value_allocated_mask) != 0) alloc.deallocate_string(attr->value);

  alloc.deallocate_memory(attr, sizeof(XmlAttributeBase),
                          LUMEX_XML_GETPAGE(attr)); // NOLINT(cppcoreguidelines-pro-type-const-cast)
}

inline void
append_attribute(XmlAttributeBase *attr, XmlNodeBase *node) // NOLINT(misc-use-internal-linkage)
{
  XmlAttributeBase *head = node->first_attribute;

  if(head != nullptr)
  {
    XmlAttributeBase *tail = head->prev_attribute_c;

    tail->next_attribute   = attr;
    attr->prev_attribute_c = tail;
    head->prev_attribute_c = attr;
  }
  else
  {
    node->first_attribute  = attr;
    attr->prev_attribute_c = attr;
  }
}

inline void
prepend_attribute(XmlAttributeBase *attr, XmlNodeBase *node) // NOLINT(misc-use-internal-linkage)
{
  XmlAttributeBase *head = node->first_attribute;

  if(head != nullptr)
  {
    attr->prev_attribute_c = head->prev_attribute_c;
    head->prev_attribute_c = attr;
  }
  else
    attr->prev_attribute_c = attr;

  attr->next_attribute  = head;
  node->first_attribute = attr;
}

inline void
insert_attribute_after(XmlAttributeBase *attr, XmlAttributeBase *place, // NOLINT(misc-use-internal-linkage)
                       XmlNodeBase *node)
{
  XmlAttributeBase *next = place->next_attribute;

  if(next != nullptr)
    next->prev_attribute_c = attr;
  else
    node->first_attribute->prev_attribute_c = attr;

  attr->next_attribute   = next;
  attr->prev_attribute_c = place;
  place->next_attribute  = attr;
}

inline void
insert_attribute_before(XmlAttributeBase *attr, XmlAttributeBase *place, // NOLINT(misc-use-internal-linkage)
                        XmlNodeBase *node)
{
  XmlAttributeBase *prev = place->prev_attribute_c;

  if(prev->next_attribute != nullptr)
    prev->next_attribute = attr;
  else
    node->first_attribute = attr;

  attr->prev_attribute_c  = prev;
  attr->next_attribute    = place;
  place->prev_attribute_c = attr;
}

inline void
remove_attribute(XmlAttributeBase *attr, XmlNodeBase *node) // NOLINT(misc-use-internal-linkage)
{
  XmlAttributeBase *next = attr->next_attribute;
  XmlAttributeBase *prev = attr->prev_attribute_c;

  if(next != nullptr)
    next->prev_attribute_c = prev;
  else
    node->first_attribute->prev_attribute_c = prev;

  if(prev->next_attribute != nullptr)
    prev->next_attribute = next;
  else
    node->first_attribute = next;

  attr->prev_attribute_c = nullptr;
  attr->next_attribute   = nullptr;
}

LUMEX_ATTRIBUTE_NOINLINE XmlAttributeBase *
append_new_attribute(XmlNodeBase *node, XmlAllocator &alloc) // NOLINT(misc-use-internal-linkage)
{
  XmlAttributeBase *attr = Lumex::Xml::Attribute::allocate_attribute(alloc);
  if(attr == nullptr) return nullptr;

  Lumex::Xml::Attribute::append_attribute(attr, node);

  return attr;
}

inline void
node_copy_attribute(XmlAttributeBase *da_, XmlAttributeBase *sa_) // NOLINT(misc-use-internal-linkage)
{
  XmlAllocator &alloc        = get_allocator(da_);
  XmlAllocator *shared_alloc = (&alloc == &get_allocator(sa_)) ? &alloc : nullptr;

  Lumex::Xml::Utility::node_copy_string(da_->name, da_->header, kxml_memory_page_name_allocated_mask, sa_->name,
                                        sa_->header, shared_alloc);
  Lumex::Xml::Utility::node_copy_string(da_->value, da_->header, kxml_memory_page_value_allocated_mask, sa_->value,
                                        sa_->header, shared_alloc);
}
