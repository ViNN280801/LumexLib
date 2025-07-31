#define LUMEX_IMPLEMENTATION
#include "LumexXmlNode.hpp"

#include "LumexXmlNodeIterator.hpp"

using namespace Lumex::Xml::Node;

LUMEX_PUBLIC_API
inline Lumex::Xml::Node::LumexXmlNode::LumexXmlNode() : m_root(nullptr) {}

LUMEX_PUBLIC_API
inline Lumex::Xml::Node::LumexXmlNode::LumexXmlNode(xml_node_t *p) : m_root(p) {}

inline static void
unspecified_bool_xml_node(Lumex::Xml::Node::LumexXmlNode ***)
{}

LUMEX_PUBLIC_API
inline Lumex::Xml::Node::LumexXmlNode::
operator Lumex::Xml::Node::LumexXmlNode::unspecified_bool_type() const
{
  return m_root ? unspecified_bool_xml_node : nullptr;
}

inline bool
Lumex::Xml::Node::LumexXmlNode::operator!() const
{
  return !m_root;
}

inline Lumex::Xml::Node::LumexXmlNode::iterator
Lumex::Xml::Node::LumexXmlNode::begin() const
{
  return Lumex::Xml::Node::LumexXmlNode::iterator(m_root ? m_root->first_child + 0 : nullptr, m_root);
}

inline Lumex::Xml::Node::LumexXmlNode::iterator
Lumex::Xml::Node::LumexXmlNode::end() const
{
  return iterator(nullptr, m_root);
}

inline Lumex::Xml::Node::LumexXmlNode::attribute_iterator
Lumex::Xml::Node::LumexXmlNode::attributes_begin() const
{
  return attribute_iterator(m_root ? m_root->first_attribute + 0 : nullptr, m_root);
}

inline Lumex::Xml::Node::LumexXmlNode::attribute_iterator
Lumex::Xml::Node::LumexXmlNode::attributes_end() const
{
  return attribute_iterator(nullptr, m_root);
}

inline xml_object_range<xml_node_iterator>
LumexXmlNode::children() const
{
  return xml_object_range<xml_node_iterator>(begin(), end());
}

inline xml_object_range<xml_named_node_iterator>
LumexXmlNode::children(char_t const *name_) const
{
  return xml_object_range<xml_named_node_iterator>(xml_named_node_iterator(child(name_).m_root, m_root, name_),
                                                   xml_named_node_iterator(nullptr, m_root, name_));
}

inline xml_object_range<xml_attribute_iterator>
LumexXmlNode::attributes() const
{
  return xml_object_range<xml_attribute_iterator>(attributes_begin(), attributes_end());
}

inline bool
LumexXmlNode::operator==(LumexXmlNode const &other) const
{
  return (m_root == r.m_root);
}

inline bool
LumexXmlNode::operator!=(LumexXmlNode const &other) const
{
  return (m_root != r.m_root);
}

inline bool
LumexXmlNode::operator<(LumexXmlNode const &other) const
{
  return (m_root < r.m_root);
}

inline bool
LumexXmlNode::operator>(LumexXmlNode const &other) const
{
  return (m_root > r.m_root);
}

inline bool
LumexXmlNode::operator<=(LumexXmlNode const &other) const
{
  return (m_root <= r.m_root);
}

inline bool
LumexXmlNode::operator>=(LumexXmlNode const &other) const
{
  return (m_root >= r.m_root);
}

inline bool
LumexXmlNode::empty() const
{
  return !m_root;
}

inline char_t const *
LumexXmlNode::name() const
{
  if(!m_root) return LUMEX_XML_TEXT("");
  char_t const *name = m_root->name;
  return name ? name : LUMEX_XML_TEXT("");
}

inline xml_node_type
LumexXmlNode::type() const
{
  return m_root ? PUGI_IMPL_NODETYPE(m_root) : node_null;
}

inline char_t const *
LumexXmlNode::value() const
{
  if(!m_root) return LUMEX_XML_TEXT("");
  char_t const *value = m_root->value;
  return value ? value : LUMEX_XML_TEXT("");
}

inline LumexXmlNode
LumexXmlNode::child(char_t const *name_) const
{
  if(!m_root) return LumexXmlNode();

  for(xml_node_t *i = m_root->first_child; i; i = i->next_sibling)
  {
    char_t const *iname = i->name;
    if(iname && impl::strequal(name_, iname)) return LumexXmlNode(i);
  }

  return LumexXmlNode();
}

inline xml_attribute
LumexXmlNode::attribute(char_t const *name_) const
{
  if(!m_root) return xml_attribute();

  for(xml_attribute_struct *i = m_root->first_attribute; i; i = i->next_attribute)
  {
    char_t const *iname = i->name;
    if(iname && impl::strequal(name_, iname)) return xml_attribute(i);
  }

  return xml_attribute();
}

inline LumexXmlNode
LumexXmlNode::next_sibling(char_t const *name_) const
{
  if(!m_root) return LumexXmlNode();

  for(xml_node_t *i = m_root->next_sibling; i; i = i->next_sibling)
  {
    char_t const *iname = i->name;
    if(iname && impl::strequal(name_, iname)) return LumexXmlNode(i);
  }

  return LumexXmlNode();
}

inline LumexXmlNode
LumexXmlNode::next_sibling() const
{
  return m_root ? LumexXmlNode(m_root->next_sibling) : LumexXmlNode();
}

inline LumexXmlNode
LumexXmlNode::previous_sibling(char_t const *name_) const
{
  if(!m_root) return LumexXmlNode();

  for(xml_node_t *i = m_root->prev_sibling_c; i->next_sibling; i = i->prev_sibling_c)
  {
    char_t const *iname = i->name;
    if(iname && impl::strequal(name_, iname)) return LumexXmlNode(i);
  }

  return LumexXmlNode();
}

#if __cplusplus >= 201703L
inline LumexXmlNode
LumexXmlNode::child(string_view_t name_) const
{
  if(!m_root) return LumexXmlNode();

  for(xml_node_t *i = m_root->first_child; i; i = i->next_sibling)
  {
    char_t const *iname = i->name;
    if(iname && impl::stringview_equal(name_, iname)) return LumexXmlNode(i);
  }

  return LumexXmlNode();
}

inline xml_attribute
LumexXmlNode::attribute(string_view_t name_) const
{
  if(!m_root) return xml_attribute();

  for(xml_attribute_struct *i = m_root->first_attribute; i; i = i->next_attribute)
  {
    char_t const *iname = i->name;
    if(iname && impl::stringview_equal(name_, iname)) return xml_attribute(i);
  }

  return xml_attribute();
}

inline LumexXmlNode
LumexXmlNode::next_sibling(string_view_t name_) const
{
  if(!m_root) return LumexXmlNode();

  for(xml_node_t *i = m_root->next_sibling; i; i = i->next_sibling)
  {
    char_t const *iname = i->name;
    if(iname && impl::stringview_equal(name_, iname)) return LumexXmlNode(i);
  }

  return LumexXmlNode();
}

inline LumexXmlNode
LumexXmlNode::previous_sibling(string_view_t name_) const
{
  if(!m_root) return LumexXmlNode();

  for(xml_node_t *i = m_root->prev_sibling_c; i->next_sibling; i = i->prev_sibling_c)
  {
    char_t const *iname = i->name;
    if(iname && impl::stringview_equal(name_, iname)) return LumexXmlNode(i);
  }

  return LumexXmlNode();
}
#endif

inline xml_attribute
LumexXmlNode::attribute(const char_t *name_, xml_attribute &hint_) const
{
  xml_attribute_struct *hint = hint_._attr;

  // if hint is not an attribute of node, behavior is not defined
  LUMEX_ASSERT(!hint || (m_root && impl::is_attribute_of(hint, m_root)));

  if(!m_root) return xml_attribute();

  // optimistically search from hint up until the end
  for(xml_attribute_struct *i = hint; i; i = i->next_attribute)
  {
    char_t const *iname = i->name;
    if(iname && impl::strequal(name_, iname))
    {
      // update hint to maximize efficiency of searching for consecutive attributes
      hint_._attr = i->next_attribute;

      return xml_attribute(i);
    }
  }

  // wrap around and search from the first attribute until the hint
  // 'j' null pointer check is technically redundant, but it prevents a crash in case the LUMEX_ASSERTion above fails
  for(xml_attribute_struct *j = m_root->first_attribute; j && j != hint; j = j->next_attribute)
  {
    char_t const *jname = j->name;
    if(jname && impl::strequal(name_, jname))
    {
      // update hint to maximize efficiency of searching for consecutive attributes
      hint_._attr = j->next_attribute;

      return xml_attribute(j);
    }
  }

  return xml_attribute();
}

#if __cplusplus >= 201703L
inline xml_attribute
LumexXmlNode::attribute(string_view_t name_, xml_attribute &hint_) const
{
  xml_attribute_struct *hint = hint_._attr;

  // if hint is not an attribute of node, behavior is not defined
  LUMEX_ASSERT(!hint || (m_root && impl::is_attribute_of(hint, m_root)));

  if(!m_root) return xml_attribute();

  // optimistically search from hint up until the end
  for(xml_attribute_struct *i = hint; i; i = i->next_attribute)
  {
    char_t const *iname = i->name;
    if(iname && impl::stringview_equal(name_, iname))
    {
      // update hint to maximize efficiency of searching for consecutive attributes
      hint_._attr = i->next_attribute;

      return xml_attribute(i);
    }
  }

  // wrap around and search from the first attribute until the hint
  // 'j' null pointer check is technically redundant, but it prevents a crash in case the LUMEX_ASSERTion above fails
  for(xml_attribute_struct *j = m_root->first_attribute; j && j != hint; j = j->next_attribute)
  {
    char_t const *jname = j->name;
    if(jname && impl::stringview_equal(name_, jname))
    {
      // update hint to maximize efficiency of searching for consecutive attributes
      hint_._attr = j->next_attribute;

      return xml_attribute(j);
    }
  }

  return xml_attribute();
}
#endif

inline LumexXmlNode
LumexXmlNode::previous_sibling() const
{
  if(!m_root) return LumexXmlNode();
  xml_node_t *prev = m_root->prev_sibling_c;
  return prev->next_sibling ? LumexXmlNode(prev) : LumexXmlNode();
}

inline LumexXmlNode
LumexXmlNode::parent() const
{
  return m_root ? LumexXmlNode(m_root->parent) : LumexXmlNode();
}

inline LumexXmlNode
LumexXmlNode::root() const
{
  return m_root ? LumexXmlNode(&impl::get_document(m_root)) : LumexXmlNode();
}

inline xml_text
LumexXmlNode::text() const
{
  return xml_text(m_root);
}

inline char_t const *
LumexXmlNode::child_value() const
{
  if(!m_root) return LUMEX_XML_TEXT("");

  // element nodes can have value if parse_embed_pcdata was used
  if(PUGI_IMPL_NODETYPE(m_root) == node_element && m_root->value) return m_root->value;

  for(xml_node_t *i = m_root->first_child; i; i = i->next_sibling)
  {
    char_t const *ivalue = i->value;
    if(impl::is_text_node(i) && ivalue) return ivalue;
  }

  return LUMEX_XML_TEXT("");
}

inline char_t const *
LumexXmlNode::child_value(char_t const *name_) const
{
  return child(name_).child_value();
}

inline xml_attribute
LumexXmlNode::first_attribute() const
{
  if(!m_root) return xml_attribute();
  return xml_attribute(m_root->first_attribute);
}

inline xml_attribute
LumexXmlNode::last_attribute() const
{
  if(!m_root) return xml_attribute();
  xml_attribute_struct *first = m_root->first_attribute;
  return first ? xml_attribute(first->prev_attribute_c) : xml_attribute();
}

inline LumexXmlNode
LumexXmlNode::first_child() const
{
  if(!m_root) return LumexXmlNode();
  return LumexXmlNode(m_root->first_child);
}

inline LumexXmlNode
LumexXmlNode::last_child() const
{
  if(!m_root) return LumexXmlNode();
  xml_node_t *first = m_root->first_child;
  return first ? LumexXmlNode(first->prev_sibling_c) : LumexXmlNode();
}

inline bool
LumexXmlNode::set_name(char_t const *rhs)
{
  xml_node_type type_ = m_root ? PUGI_IMPL_NODETYPE(m_root) : node_null;

  if(type_ != node_element && type_ != node_pi && type_ != node_declaration) return false;

  return impl::strcpy_insitu(m_root->name, m_root->header, impl::xml_memory_page_name_allocated_mask, rhs,
                             impl::strlength(rhs));
}

inline bool
LumexXmlNode::set_name(char_t const *rhs, size_t size)
{
  xml_node_type type_ = m_root ? PUGI_IMPL_NODETYPE(m_root) : node_null;

  if(type_ != node_element && type_ != node_pi && type_ != node_declaration) return false;

  return impl::strcpy_insitu(m_root->name, m_root->header, impl::xml_memory_page_name_allocated_mask, rhs, size);
}

#if __cplusplus >= 201703L
inline bool
LumexXmlNode::set_name(string_view_t rhs)
{
  xml_node_type type_ = m_root ? PUGI_IMPL_NODETYPE(m_root) : node_null;

  if(type_ != node_element && type_ != node_pi && type_ != node_declaration) return false;

  return impl::strcpy_insitu(m_root->name, m_root->header, impl::xml_memory_page_name_allocated_mask, rhs.data(),
                             rhs.size());
}
#endif

inline bool
LumexXmlNode::set_value(const char_t *rhs)
{
  xml_node_type type_ = m_root ? PUGI_IMPL_NODETYPE(m_root) : node_null;

  if(type_ != node_pcdata && type_ != node_cdata && type_ != node_comment && type_ != node_pi && type_ != node_doctype)
    return false;

  return impl::strcpy_insitu(m_root->value, m_root->header, impl::xml_memory_page_value_allocated_mask, rhs,
                             impl::strlength(rhs));
}

inline bool
LumexXmlNode::set_value(char_t const *rhs, size_t size)
{
  xml_node_type type_ = m_root ? PUGI_IMPL_NODETYPE(m_root) : node_null;

  if(type_ != node_pcdata && type_ != node_cdata && type_ != node_comment && type_ != node_pi && type_ != node_doctype)
    return false;

  return impl::strcpy_insitu(m_root->value, m_root->header, impl::xml_memory_page_value_allocated_mask, rhs, size);
}

#if __cplusplus >= 201703L
inline bool
LumexXmlNode::set_value(string_view_t rhs)
{
  xml_node_type type_ = m_root ? PUGI_IMPL_NODETYPE(m_root) : node_null;

  if(type_ != node_pcdata && type_ != node_cdata && type_ != node_comment && type_ != node_pi && type_ != node_doctype)
    return false;

  return impl::strcpy_insitu(m_root->value, m_root->header, impl::xml_memory_page_value_allocated_mask, rhs.data(),
                             rhs.size());
}
#endif

inline xml_attribute
LumexXmlNode::append_attribute(const char_t *name_)
{
  if(!impl::allow_insert_attribute(type())) return xml_attribute();

  impl::xml_allocator &alloc = impl::get_allocator(m_root);
  if(!alloc.reserve()) return xml_attribute();

  xml_attribute a(impl::allocate_attribute(alloc));
  if(!a) return xml_attribute();

  impl::append_attribute(a._attr, m_root);

  a.set_name(name_);

  return a;
}

inline xml_attribute
LumexXmlNode::prepend_attribute(char_t const *name_)
{
  if(!impl::allow_insert_attribute(type())) return xml_attribute();

  impl::xml_allocator &alloc = impl::get_allocator(m_root);
  if(!alloc.reserve()) return xml_attribute();

  xml_attribute a(impl::allocate_attribute(alloc));
  if(!a) return xml_attribute();

  impl::prepend_attribute(a._attr, m_root);

  a.set_name(name_);

  return a;
}

inline xml_attribute
LumexXmlNode::insert_attribute_after(char_t const *name_, xml_attribute const &attr)
{
  if(!impl::allow_insert_attribute(type())) return xml_attribute();
  if(!attr || !impl::is_attribute_of(attr._attr, m_root)) return xml_attribute();

  impl::xml_allocator &alloc = impl::get_allocator(m_root);
  if(!alloc.reserve()) return xml_attribute();

  xml_attribute a(impl::allocate_attribute(alloc));
  if(!a) return xml_attribute();

  impl::insert_attribute_after(a._attr, attr._attr, m_root);

  a.set_name(name_);

  return a;
}

inline xml_attribute
LumexXmlNode::insert_attribute_before(char_t const *name_, xml_attribute const &attr)
{
  if(!impl::allow_insert_attribute(type())) return xml_attribute();
  if(!attr || !impl::is_attribute_of(attr._attr, m_root)) return xml_attribute();

  impl::xml_allocator &alloc = impl::get_allocator(m_root);
  if(!alloc.reserve()) return xml_attribute();

  xml_attribute a(impl::allocate_attribute(alloc));
  if(!a) return xml_attribute();

  impl::insert_attribute_before(a._attr, attr._attr, m_root);

  a.set_name(name_);

  return a;
}

#if __cplusplus >= 201703L
inline xml_attribute
LumexXmlNode::append_attribute(string_view_t name_)
{
  if(!impl::allow_insert_attribute(type())) return xml_attribute();

  impl::xml_allocator &alloc = impl::get_allocator(m_root);
  if(!alloc.reserve()) return xml_attribute();

  xml_attribute a(impl::allocate_attribute(alloc));
  if(!a) return xml_attribute();

  impl::append_attribute(a._attr, m_root);

  a.set_name(name_);

  return a;
}

inline xml_attribute
LumexXmlNode::prepend_attribute(string_view_t name_)
{
  if(!impl::allow_insert_attribute(type())) return xml_attribute();

  impl::xml_allocator &alloc = impl::get_allocator(m_root);
  if(!alloc.reserve()) return xml_attribute();

  xml_attribute a(impl::allocate_attribute(alloc));
  if(!a) return xml_attribute();

  impl::prepend_attribute(a._attr, m_root);

  a.set_name(name_);

  return a;
}

inline xml_attribute
LumexXmlNode::insert_attribute_after(string_view_t name_, xml_attribute const &attr)
{
  if(!impl::allow_insert_attribute(type())) return xml_attribute();
  if(!attr || !impl::is_attribute_of(attr._attr, m_root)) return xml_attribute();

  impl::xml_allocator &alloc = impl::get_allocator(m_root);
  if(!alloc.reserve()) return xml_attribute();

  xml_attribute a(impl::allocate_attribute(alloc));
  if(!a) return xml_attribute();

  impl::insert_attribute_after(a._attr, attr._attr, m_root);

  a.set_name(name_);

  return a;
}

inline xml_attribute
LumexXmlNode::insert_attribute_before(string_view_t name_, xml_attribute const &attr)
{
  if(!impl::allow_insert_attribute(type())) return xml_attribute();
  if(!attr || !impl::is_attribute_of(attr._attr, m_root)) return xml_attribute();

  impl::xml_allocator &alloc = impl::get_allocator(m_root);
  if(!alloc.reserve()) return xml_attribute();

  xml_attribute a(impl::allocate_attribute(alloc));
  if(!a) return xml_attribute();

  impl::insert_attribute_before(a._attr, attr._attr, m_root);

  a.set_name(name_);

  return a;
}
#endif

inline xml_attribute
LumexXmlNode::append_copy(const xml_attribute &proto)
{
  if(!proto) return xml_attribute();
  if(!impl::allow_insert_attribute(type())) return xml_attribute();

  impl::xml_allocator &alloc = impl::get_allocator(m_root);
  if(!alloc.reserve()) return xml_attribute();

  xml_attribute a(impl::allocate_attribute(alloc));
  if(!a) return xml_attribute();

  impl::append_attribute(a._attr, m_root);
  impl::node_copy_attribute(a._attr, proto._attr);

  return a;
}

inline xml_attribute
LumexXmlNode::prepend_copy(xml_attribute const &proto)
{
  if(!proto) return xml_attribute();
  if(!impl::allow_insert_attribute(type())) return xml_attribute();

  impl::xml_allocator &alloc = impl::get_allocator(m_root);
  if(!alloc.reserve()) return xml_attribute();

  xml_attribute a(impl::allocate_attribute(alloc));
  if(!a) return xml_attribute();

  impl::prepend_attribute(a._attr, m_root);
  impl::node_copy_attribute(a._attr, proto._attr);

  return a;
}

inline xml_attribute
LumexXmlNode::insert_copy_after(xml_attribute const &proto, xml_attribute const &attr)
{
  if(!proto) return xml_attribute();
  if(!impl::allow_insert_attribute(type())) return xml_attribute();
  if(!attr || !impl::is_attribute_of(attr._attr, m_root)) return xml_attribute();

  impl::xml_allocator &alloc = impl::get_allocator(m_root);
  if(!alloc.reserve()) return xml_attribute();

  xml_attribute a(impl::allocate_attribute(alloc));
  if(!a) return xml_attribute();

  impl::insert_attribute_after(a._attr, attr._attr, m_root);
  impl::node_copy_attribute(a._attr, proto._attr);

  return a;
}

inline xml_attribute
LumexXmlNode::insert_copy_before(xml_attribute const &proto, xml_attribute const &attr)
{
  if(!proto) return xml_attribute();
  if(!impl::allow_insert_attribute(type())) return xml_attribute();
  if(!attr || !impl::is_attribute_of(attr._attr, m_root)) return xml_attribute();

  impl::xml_allocator &alloc = impl::get_allocator(m_root);
  if(!alloc.reserve()) return xml_attribute();

  xml_attribute a(impl::allocate_attribute(alloc));
  if(!a) return xml_attribute();

  impl::insert_attribute_before(a._attr, attr._attr, m_root);
  impl::node_copy_attribute(a._attr, proto._attr);

  return a;
}

inline LumexXmlNode
LumexXmlNode::append_child(xml_node_type type_)
{
  if(!impl::allow_insert_child(type(), type_)) return LumexXmlNode();

  impl::xml_allocator &alloc = impl::get_allocator(m_root);
  if(!alloc.reserve()) return LumexXmlNode();

  LumexXmlNode n(impl::allocate_node(alloc, type_));
  if(!n) return LumexXmlNode();

  impl::append_node(n.m_root, m_root);

  if(type_ == node_declaration) n.set_name(LUMEX_XML_TEXT("xml"));

  return n;
}

inline LumexXmlNode
LumexXmlNode::prepend_child(xml_node_type type_)
{
  if(!impl::allow_insert_child(type(), type_)) return LumexXmlNode();

  impl::xml_allocator &alloc = impl::get_allocator(m_root);
  if(!alloc.reserve()) return LumexXmlNode();

  LumexXmlNode n(impl::allocate_node(alloc, type_));
  if(!n) return LumexXmlNode();

  impl::prepend_node(n.m_root, m_root);

  if(type_ == node_declaration) n.set_name(LUMEX_XML_TEXT("xml"));

  return n;
}

inline LumexXmlNode
LumexXmlNode::insert_child_before(xml_node_type type_, LumexXmlNode const &node)
{
  if(!impl::allow_insert_child(type(), type_)) return LumexXmlNode();
  if(!node.m_root || node.m_root->parent != m_root) return LumexXmlNode();

  impl::xml_allocator &alloc = impl::get_allocator(m_root);
  if(!alloc.reserve()) return LumexXmlNode();

  LumexXmlNode n(impl::allocate_node(alloc, type_));
  if(!n) return LumexXmlNode();

  impl::insert_node_before(n.m_root, node.m_root);

  if(type_ == node_declaration) n.set_name(LUMEX_XML_TEXT("xml"));

  return n;
}

inline LumexXmlNode
LumexXmlNode::insert_child_after(xml_node_type type_, LumexXmlNode const &node)
{
  if(!impl::allow_insert_child(type(), type_)) return LumexXmlNode();
  if(!node.m_root || node.m_root->parent != m_root) return LumexXmlNode();

  impl::xml_allocator &alloc = impl::get_allocator(m_root);
  if(!alloc.reserve()) return LumexXmlNode();

  LumexXmlNode n(impl::allocate_node(alloc, type_));
  if(!n) return LumexXmlNode();

  impl::insert_node_after(n.m_root, node.m_root);

  if(type_ == node_declaration) n.set_name(LUMEX_XML_TEXT("xml"));

  return n;
}

inline LumexXmlNode
LumexXmlNode::append_child(char_t const *name_)
{
  LumexXmlNode result = append_child(node_element);

  result.set_name(name_);

  return result;
}

inline LumexXmlNode
LumexXmlNode::prepend_child(char_t const *name_)
{
  LumexXmlNode result = prepend_child(node_element);

  result.set_name(name_);

  return result;
}

inline LumexXmlNode
LumexXmlNode::insert_child_after(char_t const *name_, LumexXmlNode const &node)
{
  LumexXmlNode result = insert_child_after(node_element, node);

  result.set_name(name_);

  return result;
}

inline LumexXmlNode
LumexXmlNode::insert_child_before(char_t const *name_, LumexXmlNode const &node)
{
  LumexXmlNode result = insert_child_before(node_element, node);

  result.set_name(name_);

  return result;
}

#if __cplusplus >= 201703L
inline LumexXmlNode
LumexXmlNode::append_child(string_view_t name_)
{
  LumexXmlNode result = append_child(node_element);

  result.set_name(name_);

  return result;
}

inline LumexXmlNode
LumexXmlNode::prepend_child(string_view_t name_)
{
  LumexXmlNode result = prepend_child(node_element);

  result.set_name(name_);

  return result;
}

inline LumexXmlNode
LumexXmlNode::insert_child_after(string_view_t name_, LumexXmlNode const &node)
{
  LumexXmlNode result = insert_child_after(node_element, node);

  result.set_name(name_);

  return result;
}

inline LumexXmlNode
LumexXmlNode::insert_child_before(string_view_t name_, LumexXmlNode const &node)
{
  LumexXmlNode result = insert_child_before(node_element, node);

  result.set_name(name_);

  return result;
}
#endif

inline LumexXmlNode
LumexXmlNode::append_copy(const LumexXmlNode &proto)
{
  xml_node_type type_ = proto.type();
  if(!impl::allow_insert_child(type(), type_)) return LumexXmlNode();

  impl::xml_allocator &alloc = impl::get_allocator(m_root);
  if(!alloc.reserve()) return LumexXmlNode();

  LumexXmlNode n(impl::allocate_node(alloc, type_));
  if(!n) return LumexXmlNode();

  impl::append_node(n.m_root, m_root);
  impl::node_copy_tree(n.m_root, proto.m_root);

  return n;
}

inline LumexXmlNode
LumexXmlNode::prepend_copy(LumexXmlNode const &proto)
{
  xml_node_type type_ = proto.type();
  if(!impl::allow_insert_child(type(), type_)) return LumexXmlNode();

  impl::xml_allocator &alloc = impl::get_allocator(m_root);
  if(!alloc.reserve()) return LumexXmlNode();

  LumexXmlNode n(impl::allocate_node(alloc, type_));
  if(!n) return LumexXmlNode();

  impl::prepend_node(n.m_root, m_root);
  impl::node_copy_tree(n.m_root, proto.m_root);

  return n;
}

inline LumexXmlNode
LumexXmlNode::insert_copy_after(LumexXmlNode const &proto, LumexXmlNode const &node)
{
  xml_node_type type_ = proto.type();
  if(!impl::allow_insert_child(type(), type_)) return LumexXmlNode();
  if(!node.m_root || node.m_root->parent != m_root) return LumexXmlNode();

  impl::xml_allocator &alloc = impl::get_allocator(m_root);
  if(!alloc.reserve()) return LumexXmlNode();

  LumexXmlNode n(impl::allocate_node(alloc, type_));
  if(!n) return LumexXmlNode();

  impl::insert_node_after(n.m_root, node.m_root);
  impl::node_copy_tree(n.m_root, proto.m_root);

  return n;
}

inline LumexXmlNode
LumexXmlNode::insert_copy_before(LumexXmlNode const &proto, LumexXmlNode const &node)
{
  xml_node_type type_ = proto.type();
  if(!impl::allow_insert_child(type(), type_)) return LumexXmlNode();
  if(!node.m_root || node.m_root->parent != m_root) return LumexXmlNode();

  impl::xml_allocator &alloc = impl::get_allocator(m_root);
  if(!alloc.reserve()) return LumexXmlNode();

  LumexXmlNode n(impl::allocate_node(alloc, type_));
  if(!n) return LumexXmlNode();

  impl::insert_node_before(n.m_root, node.m_root);
  impl::node_copy_tree(n.m_root, proto.m_root);

  return n;
}

inline LumexXmlNode
LumexXmlNode::append_move(LumexXmlNode const &moved)
{
  if(!impl::allow_move(*this, moved)) return LumexXmlNode();

  impl::xml_allocator &alloc = impl::get_allocator(m_root);
  if(!alloc.reserve()) return LumexXmlNode();

  // disable document_buffer_order optimization since moving nodes around changes document order without changing
  // buffer pointers
  impl::get_document(m_root).header |= impl::xml_memory_page_contents_shared_mask;

  impl::remove_node(moved.m_root);
  impl::append_node(moved.m_root, m_root);

  return moved;
}

inline LumexXmlNode
LumexXmlNode::prepend_move(LumexXmlNode const &moved)
{
  if(!impl::allow_move(*this, moved)) return LumexXmlNode();

  impl::xml_allocator &alloc = impl::get_allocator(m_root);
  if(!alloc.reserve()) return LumexXmlNode();

  // disable document_buffer_order optimization since moving nodes around changes document order without changing
  // buffer pointers
  impl::get_document(m_root).header |= impl::xml_memory_page_contents_shared_mask;

  impl::remove_node(moved.m_root);
  impl::prepend_node(moved.m_root, m_root);

  return moved;
}

inline LumexXmlNode
LumexXmlNode::insert_move_after(LumexXmlNode const &moved, LumexXmlNode const &node)
{
  if(!impl::allow_move(*this, moved)) return LumexXmlNode();
  if(!node.m_root || node.m_root->parent != m_root) return LumexXmlNode();
  if(moved.m_root == node.m_root) return LumexXmlNode();

  impl::xml_allocator &alloc = impl::get_allocator(m_root);
  if(!alloc.reserve()) return LumexXmlNode();

  // disable document_buffer_order optimization since moving nodes around changes document order without changing
  // buffer pointers
  impl::get_document(m_root).header |= impl::xml_memory_page_contents_shared_mask;

  impl::remove_node(moved.m_root);
  impl::insert_node_after(moved.m_root, node.m_root);

  return moved;
}

inline LumexXmlNode
LumexXmlNode::insert_move_before(LumexXmlNode const &moved, LumexXmlNode const &node)
{
  if(!impl::allow_move(*this, moved)) return LumexXmlNode();
  if(!node.m_root || node.m_root->parent != m_root) return LumexXmlNode();
  if(moved.m_root == node.m_root) return LumexXmlNode();

  impl::xml_allocator &alloc = impl::get_allocator(m_root);
  if(!alloc.reserve()) return LumexXmlNode();

  // disable document_buffer_order optimization since moving nodes around changes document order without changing
  // buffer pointers
  impl::get_document(m_root).header |= impl::xml_memory_page_contents_shared_mask;

  impl::remove_node(moved.m_root);
  impl::insert_node_before(moved.m_root, node.m_root);

  return moved;
}

inline bool
LumexXmlNode::remove_attribute(char_t const *name_)
{
  return remove_attribute(attribute(name_));
}

#if __cplusplus >= 201703L
inline bool
LumexXmlNode::remove_attribute(string_view_t name_)
{
  return remove_attribute(attribute(name_));
}
#endif

inline bool
LumexXmlNode::remove_attribute(const xml_attribute &attr)
{
  if(!m_root || !a._attr) return false;
  if(!impl::is_attribute_of(a._attr, m_root)) return false;

  impl::xml_allocator &alloc = impl::get_allocator(m_root);
  if(!alloc.reserve()) return false;

  impl::remove_attribute(a._attr, m_root);
  impl::destroy_attribute(a._attr, alloc);

  return true;
}

inline bool
LumexXmlNode::remove_attributes()
{
  if(!m_root) return false;

  impl::xml_allocator &alloc = impl::get_allocator(m_root);
  if(!alloc.reserve()) return false;

  for(xml_attribute_struct *attr = m_root->first_attribute; attr;)
  {
    xml_attribute_struct *next = attr->next_attribute;

    impl::destroy_attribute(attr, alloc);

    attr = next;
  }

  m_root->first_attribute = nullptr;

  return true;
}

inline bool
LumexXmlNode::remove_child(char_t const *name_)
{
  return remove_child(child(name_));
}

#if __cplusplus >= 201703L
inline bool
LumexXmlNode::remove_child(string_view_t name_)
{
  return remove_child(child(name_));
}
#endif

inline bool
LumexXmlNode::remove_child(const LumexXmlNode &n)
{
  if(!m_root || !n.m_root || n.m_root->parent != m_root) return false;

  impl::xml_allocator &alloc = impl::get_allocator(m_root);
  if(!alloc.reserve()) return false;

  impl::remove_node(n.m_root);
  impl::destroy_node(n.m_root, alloc);

  return true;
}

inline bool
LumexXmlNode::remove_children()
{
  if(!m_root) return false;

  impl::xml_allocator &alloc = impl::get_allocator(m_root);
  if(!alloc.reserve()) return false;

  for(xml_node_t *cur = m_root->first_child; cur;)
  {
    xml_node_t *next = cur->next_sibling;

    impl::destroy_node(cur, alloc);

    cur = next;
  }

  m_root->first_child = nullptr;

  return true;
}

inline xml_parse_result
LumexXmlNode::append_buffer(void const *contents, size_t size, unsigned int options, xml_encoding encoding)
{
  // append_buffer is only valid for elements/documents
  if(!impl::allow_insert_child(type(), node_element)) return impl::make_parse_result(status_append_invalid_root);

  // append buffer can not merge PCDATA into existing PCDATA nodes
  if((options & parse_merge_pcdata) != 0 && last_child().type() == node_pcdata)
    return impl::make_parse_result(status_append_invalid_root);

  // get document node
  impl::xml_document_struct *doc = &impl::get_document(m_root);

  // disable document_buffer_order optimization since in a document with multiple buffers comparing buffer pointers
  // does not make sense
  doc->header |= impl::xml_memory_page_contents_shared_mask;

  // get extra buffer element (we'll store the document fragment buffer there so that we can deallocate it later)
  impl::xml_memory_page *page   = nullptr;
  impl::xml_extra_buffer *extra = static_cast<impl::xml_extra_buffer *>(
    doc->allocate_memory(sizeof(impl::xml_extra_buffer) + sizeof(void *), page));
  (void)page;

  if(!extra) return impl::make_parse_result(status_out_of_memory);

  // add extra buffer to the list
  extra->buffer      = nullptr;
  extra->next        = doc->extra_buffers;
  doc->extra_buffers = extra;

  // name of the root has to be nullptr before parsing - otherwise closing node mismatches will not be detected at the
  // top level
  impl::name_null_sentry sentry(m_root);

  return impl::load_buffer_impl(doc, m_root, const_cast<void *>(contents), size, options, encoding, false, false,
                                &extra->buffer);
}

inline LumexXmlNode
LumexXmlNode::find_child_by_attribute(char_t const *name_, char_t const *attr_name, char_t const *attr_value) const
{
  if(!m_root) return LumexXmlNode();

  for(xml_node_t *i = m_root->first_child; i; i = i->next_sibling)
  {
    char_t const *iname = i->name;
    if(iname && impl::strequal(name_, iname))
    {
      for(xml_attribute_struct *a = i->first_attribute; a; a = a->next_attribute)
      {
        char_t const *aname = a->name;
        if(aname && impl::strequal(attr_name, aname))
        {
          char_t const *avalue = a->value;
          if(impl::strequal(attr_value, avalue ? avalue : LUMEX_XML_TEXT(""))) return LumexXmlNode(i);
        }
      }
    }
  }

  return LumexXmlNode();
}

inline LumexXmlNode
LumexXmlNode::find_child_by_attribute(char_t const *attr_name, char_t const *attr_value) const
{
  if(!m_root) return LumexXmlNode();

  for(xml_node_t *i = m_root->first_child; i; i = i->next_sibling)
    for(xml_attribute_struct *a = i->first_attribute; a; a = a->next_attribute)
    {
      char_t const *aname = a->name;
      if(aname && impl::strequal(attr_name, aname))
      {
        char_t const *avalue = a->value;
        if(impl::strequal(attr_value, avalue ? avalue : LUMEX_XML_TEXT(""))) return LumexXmlNode(i);
      }
    }

  return LumexXmlNode();
}

inline string_t
LumexXmlNode::path(char_t delimiter) const
{
  if(!m_root) return string_t();

  size_t offset = 0;

  for(xml_node_t *i = m_root; i; i = i->parent)
  {
    char_t const *iname = i->name;
    offset += (i != m_root);
    offset += iname ? impl::strlength(iname) : 0;
  }

  string_t result;
  result.resize(offset);

  for(xml_node_t *j = m_root; j; j = j->parent)
  {
    if(j != m_root) result[--offset] = delimiter;

    char_t const *jname = j->name;
    if(jname)
    {
      size_t length = impl::strlength(jname);

      offset -= length;
      memcpy(&result[offset], jname, length * sizeof(char_t));
    }
  }

  LUMEX_ASSERT(offset == 0);

  return result;
}

inline LumexXmlNode
LumexXmlNode::first_element_by_path(char_t const *path_, char_t delimiter) const
{
  LumexXmlNode context = path_[0] == delimiter ? root() : *this;

  if(!context.m_root) return LumexXmlNode();

  char_t const *path_segment = path_;

  while(*path_segment == delimiter) ++path_segment;

  char_t const *path_segment_end = path_segment;

  while(*path_segment_end && *path_segment_end != delimiter) ++path_segment_end;

  if(path_segment == path_segment_end) return context;

  char_t const *next_segment = path_segment_end;

  while(*next_segment == delimiter) ++next_segment;

  if(*path_segment == '.' && path_segment + 1 == path_segment_end)
    return context.first_element_by_path(next_segment, delimiter);
  else if(*path_segment == '.' && *(path_segment + 1) == '.' && path_segment + 2 == path_segment_end)
    return context.parent().first_element_by_path(next_segment, delimiter);
  else
  {
    for(xml_node_t *j = context.m_root->first_child; j; j = j->next_sibling)
    {
      char_t const *jname = j->name;
      if(jname && impl::strequalrange(jname, path_segment, static_cast<size_t>(path_segment_end - path_segment)))
      {
        LumexXmlNode subsearch = LumexXmlNode(j).first_element_by_path(next_segment, delimiter);

        if(subsearch) return subsearch;
      }
    }

    return LumexXmlNode();
  }
}

inline bool
LumexXmlNode::traverse(xml_tree_walker &walker)
{
  walker._depth = -1;

  LumexXmlNode arg_begin(m_root);
  if(!walker.begin(arg_begin)) return false;

  xml_node_t *cur = m_root ? m_root->first_child + 0 : nullptr;

  if(cur)
  {
    ++walker._depth;

    do {
      LumexXmlNode arg_for_each(cur);
      if(!walker.for_each(arg_for_each)) return false;

      if(cur->first_child)
      {
        ++walker._depth;
        cur = cur->first_child;
      }
      else if(cur->next_sibling)
        cur = cur->next_sibling;
      else
      {
        while(!cur->next_sibling && cur != m_root && cur->parent)
        {
          --walker._depth;
          cur = cur->parent;
        }

        if(cur != m_root) cur = cur->next_sibling;
      }
    } while(cur && cur != m_root);
  }

  LUMEX_ASSERT(walker._depth == -1);

  LumexXmlNode arg_end(m_root);
  return walker.end(arg_end);
}

inline size_t
LumexXmlNode::hash_value() const
{
  return reinterpret_cast<uintptr_t>(m_root) / sizeof(xml_node_t);
}

inline xml_node_t *
LumexXmlNode::get() const
{
  return m_root;
}

inline void
LumexXmlNode::print(xml_writer &writer, char_t const *indent, unsigned int flags, xml_encoding encoding,
                    unsigned int depth) const
{
  if(!m_root) return;

  impl::xml_buffered_writer buffered_writer(writer, encoding);

  impl::node_output(buffered_writer, m_root, indent, flags, depth);

  buffered_writer.flush();
}

inline void
LumexXmlNode::print(std::basic_ostream<char> &stream, char_t const *indent, unsigned int flags, xml_encoding encoding,
                    unsigned int depth) const
{
  xml_writer_stream writer(stream);

  print(writer, indent, flags, encoding, depth);
}

inline void
LumexXmlNode::print(std::basic_ostream<wchar_t> &stream, char_t const *indent, unsigned int flags,
                    unsigned int depth) const
{
  xml_writer_stream writer(stream);

  print(writer, indent, flags, encoding_wchar, depth);
}

inline ptrdiff_t
LumexXmlNode::offset_debug() const
{
  if(!m_root) return -1;

  impl::xml_document_struct &doc = impl::get_document(m_root);

  // we can determine the offset reliably only if there is exactly once parse buffer
  if(!doc.buffer || doc.extra_buffers) return -1;

  switch(type())
  {
  case node_document: return 0;

  case node_element:
  case node_declaration:
  case node_pi:
    return m_root->name && (m_root->header & impl::xml_memory_page_name_allocated_or_shared_mask) == 0
             ? m_root->name - doc.buffer
             : -1;

  case node_pcdata:
  case node_cdata:
  case node_comment:
  case node_doctype:
    return m_root->value && (m_root->header & impl::xml_memory_page_value_allocated_or_shared_mask) == 0
             ? m_root->value - doc.buffer
             : -1;

  default:
    LUMEX_ASSERT(false && "Invalid node type"); // unreachable
    return -1;
  }
}

inline xpath_node
LumexXmlNode::select_node(char_t const *query, xpath_variable_set *variables) const
{
  xpath_query q(query, variables);
  return q.evaluate_node(*this);
}

inline xpath_node
LumexXmlNode::select_node(xpath_query const &query) const
{
  return query.evaluate_node(*this);
}

inline xpath_node_set
LumexXmlNode::select_nodes(char_t const *query, xpath_variable_set *variables) const
{
  xpath_query q(query, variables);
  return q.evaluate_node_set(*this);
}

inline xpath_node_set
LumexXmlNode::select_nodes(xpath_query const &query) const
{
  return query.evaluate_node_set(*this);
}

inline xpath_node
LumexXmlNode::select_single_node(char_t const *query, xpath_variable_set *variables) const
{
  xpath_query q(query, variables);
  return q.evaluate_node(*this);
}

inline xpath_node
LumexXmlNode::select_single_node(xpath_query const &query) const
{
  return query.evaluate_node(*this);
}
