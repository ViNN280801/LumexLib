#include "lumex/xml/attribute/XmlAttributeIterator.hpp"
#include "lumex/xml/utility/XmlUtils.hpp"

#include "XmlNode.hpp"
#include "XmlNodeIterator.hpp"

using namespace Lumex::Xml::Attribute;
using namespace Lumex::Xml::Node;
using namespace Lumex::Xml::Utility;

inline XmlNode::XmlNode() : m_root(nullptr) {}

inline XmlNode::XmlNode(XmlNodeBase *p) : m_root(p) {}

inline static void
unspecified_bool_xml_node(XmlNode ***)
{}

inline XmlNode::
operator XmlNode::unspecified_bool_type() const
{
  return m_root ? unspecified_bool_xml_node : nullptr;
}

inline bool
XmlNode::operator!() const
{
  return !m_root;
}

inline XmlNodeIterator
XmlNode::begin() const
{
  return XmlNodeIterator(m_root ? m_root->first_child + 0 : nullptr, m_root);
}

inline XmlNodeIterator
XmlNode::end() const
{
  return XmlNodeIterator(nullptr, m_root);
}

inline XmlAttributeIterator
XmlNode::attributes_begin() const
{
  return XmlAttributeIterator(m_root != nullptr ? m_root->first_attribute + 0 : nullptr, m_root);
}

inline XmlAttributeIterator
XmlNode::attributes_end() const
{
  return XmlAttributeIterator(nullptr, m_root);
}

// inline XmlObjectRange<xml_node_XmlNodeIterator>
// XmlNode::children() const
// {
//   return XmlObjectRange<xml_node_XmlNodeIterator>(begin(), end());
// }

// inline XmlObjectRange<XmlNamedNodeIterator>
// XmlNode::children(char_t const *name_) const
// {
//   return XmlObjectRange<XmlNamedNodeIterator>(XmlNamedNodeIterator(child(name_).m_root, m_root, name_),
//                                               XmlNamedNodeIterator(nullptr, m_root, name_));
// }

// inline XmlObjectRange<xml_XmlAttributeIterator>
// XmlNode::attributes() const
// {
//   return XmlObjectRange<xml_XmlAttributeIterator>(attributes_begin(), attributes_end());
// }

inline bool
XmlNode::operator==(XmlNode const &other) const
{
  return (m_root == other.m_root);
}

inline bool
XmlNode::operator!=(XmlNode const &other) const
{
  return (m_root != other.m_root);
}

inline bool
XmlNode::operator<(XmlNode const &other) const
{
  return (m_root < other.m_root);
}

inline bool
XmlNode::operator>(XmlNode const &other) const
{
  return (m_root > other.m_root);
}

inline bool
XmlNode::operator<=(XmlNode const &other) const
{
  return (m_root <= other.m_root);
}

inline bool
XmlNode::operator>=(XmlNode const &other) const
{
  return (m_root >= other.m_root);
}

inline bool
XmlNode::empty() const
{
  return !m_root;
}

inline char_t const *
XmlNode::name() const
{
  if(!m_root) return LUMEX_XML_TEXT("");
  char_t const *name = m_root->name;
  return name ? name : LUMEX_XML_TEXT("");
}

inline xml_node_type
XmlNode::type() const
{
  return m_root ? LUMEX_XML_NODETYPE(m_root) : node_null;
}

inline char_t const *
XmlNode::value() const
{
  if(!m_root) return LUMEX_XML_TEXT("");
  char_t const *value = m_root->value;
  return value ? value : LUMEX_XML_TEXT("");
}

inline XmlNode
XmlNode::child(char_t const *name_) const
{
  if(!m_root) return XmlNode();

  for(XmlNodeBase *i = m_root->first_child; i; i = i->next_sibling)
  {
    char_t const *iname = i->name;
    if(iname && Utility::strequal(name_, iname)) return XmlNode(i);
  }

  return XmlNode();
}

inline XmlAttribute
XmlNode::attribute(char_t const *name_) const
{
  if(m_root == nullptr) return {};

  for(XmlAttributeBase *i = m_root->first_attribute; i; i = i->next_attribute)
  {
    char_t const *iname = i->name;
    if(iname && Utility::strequal(name_, iname)) return XmlAttribute(i);
  }

  return {};
}

inline XmlNode
XmlNode::next_sibling(char_t const *name_) const
{
  if(!m_root) return XmlNode();

  for(XmlNodeBase *i = m_root->next_sibling; i; i = i->next_sibling)
  {
    char_t const *iname = i->name;
    if(iname && Utility::strequal(name_, iname)) return XmlNode(i);
  }

  return XmlNode();
}

inline XmlNode
XmlNode::next_sibling() const
{
  return m_root ? XmlNode(m_root->next_sibling) : XmlNode();
}

inline XmlNode
XmlNode::previous_sibling(char_t const *name_) const
{
  if(!m_root) return XmlNode();

  for(XmlNodeBase *i = m_root->prev_sibling_c; i->next_sibling; i = i->prev_sibling_c)
  {
    char_t const *iname = i->name;
    if(iname && Utility::strequal(name_, iname)) return XmlNode(i);
  }

  return XmlNode();
}

#if __cplusplus >= 201703L
inline XmlNode
XmlNode::child(string_view_t name_) const
{
  if(!m_root) return XmlNode();

  for(XmlNodeBase *i = m_root->first_child; i; i = i->next_sibling)
  {
    char_t const *iname = i->name;
    if(iname && Utility::stringview_equal(name_, iname)) return XmlNode(i);
  }

  return XmlNode();
}

inline XmlAttribute
XmlNode::attribute(string_view_t name_) const
{
  if(m_root == nullptr) return {};

  for(XmlAttributeBase *i = m_root->first_attribute; i; i = i->next_attribute)
  {
    char_t const *iname = i->name;
    if(iname && Utility::stringview_equal(name_, iname)) return XmlAttribute(i);
  }

  return {};
}

inline XmlNode
XmlNode::next_sibling(string_view_t name_) const
{
  if(!m_root) return XmlNode();

  for(XmlNodeBase *i = m_root->next_sibling; i; i = i->next_sibling)
  {
    char_t const *iname = i->name;
    if(iname && Utility::stringview_equal(name_, iname)) return XmlNode(i);
  }

  return XmlNode();
}

inline XmlNode
XmlNode::previous_sibling(string_view_t name_) const
{
  if(!m_root) return XmlNode();

  for(XmlNodeBase *i = m_root->prev_sibling_c; i->next_sibling; i = i->prev_sibling_c)
  {
    char_t const *iname = i->name;
    if(iname && Utility::stringview_equal(name_, iname)) return XmlNode(i);
  }

  return XmlNode();
}
#endif

inline XmlAttribute
XmlNode::attribute(const char_t *name_, XmlAttribute &hint_) const
{
  XmlAttributeBase *hint = hint_._attr;

  // if hint is not an attribute of node, behavior is not defined
  LUMEX_ASSERT(!hint || (m_root && impl::is_attribute_of(hint, m_root)));

  if(m_root == nullptr) return {};

  // optimistically search from hint up until the end
  for(XmlAttributeBase *i = hint; i; i = i->next_attribute)
  {
    char_t const *iname = i->name;
    if(iname && Utility::strequal(name_, iname))
    {
      // update hint to maximize efficiency of searching for consecutive attributes
      hint_._attr = i->next_attribute;

      return XmlAttribute(i);
    }
  }

  // wrap around and search from the first attribute until the hint
  // 'j' null pointer check is technically redundant, but it prevents a crash in case the LUMEX_ASSERTion above fails
  for(XmlAttributeBase *j = m_root->first_attribute; j && j != hint; j = j->next_attribute)
  {
    char_t const *jname = j->name;
    if(jname && Utility::strequal(name_, jname))
    {
      // update hint to maximize efficiency of searching for consecutive attributes
      hint_._attr = j->next_attribute;

      return XmlAttribute(j);
    }
  }

  return {};
}

#if __cplusplus >= 201703L
inline XmlAttribute
XmlNode::attribute(string_view_t name_, XmlAttribute &hint_) const
{
  XmlAttributeBase *hint = hint_._attr;

  // if hint is not an attribute of node, behavior is not defined
  LUMEX_ASSERT(!hint || (m_root && impl::is_attribute_of(hint, m_root)));

  if(m_root == nullptr) return {};

  // optimistically search from hint up until the end
  for(XmlAttributeBase *i = hint; i; i = i->next_attribute)
  {
    char_t const *iname = i->name;
    if(iname && impl::stringview_equal(name_, iname))
    {
      // update hint to maximize efficiency of searching for consecutive attributes
      hint_._attr = i->next_attribute;

      return XmlAttribute(i);
    }
  }

  // wrap around and search from the first attribute until the hint
  // 'j' null pointer check is technically redundant, but it prevents a crash in case the LUMEX_ASSERTion above fails
  for(XmlAttributeBase *j = m_root->first_attribute; j && j != hint; j = j->next_attribute)
  {
    char_t const *jname = j->name;
    if(jname && impl::stringview_equal(name_, jname))
    {
      // update hint to maximize efficiency of searching for consecutive attributes
      hint_._attr = j->next_attribute;

      return XmlAttribute(j);
    }
  }

  return {};
}
#endif

inline XmlNode
XmlNode::previous_sibling() const
{
  if(!m_root) return XmlNode();
  XmlNodeBase *prev = m_root->prev_sibling_c;
  return prev->next_sibling ? XmlNode(prev) : XmlNode();
}

inline XmlNode
XmlNode::parent() const
{
  return m_root ? XmlNode(m_root->parent) : XmlNode();
}

inline XmlNode
XmlNode::root() const
{
  return m_root ? XmlNode(&impl::get_document(m_root)) : XmlNode();
}

inline xml_text
XmlNode::text() const
{
  return xml_text(m_root);
}

inline char_t const *
XmlNode::child_value() const
{
  if(!m_root) return LUMEX_XML_TEXT("");

  // element nodes can have value if parse_embed_pcdata was used
  if(PUGI_IMPL_NODETYPE(m_root) == node_element && m_root->value) return m_root->value;

  for(XmlNodeBase *i = m_root->first_child; i; i = i->next_sibling)
  {
    char_t const *ivalue = i->value;
    if(impl::is_text_node(i) && ivalue) return ivalue;
  }

  return LUMEX_XML_TEXT("");
}

inline char_t const *
XmlNode::child_value(char_t const *name_) const
{
  return child(name_).child_value();
}

inline XmlAttribute
XmlNode::first_attribute() const
{
  if(m_root == nullptr) return {};
  return XmlAttribute(m_root->first_attribute);
}

inline XmlAttribute
XmlNode::last_attribute() const
{
  if(m_root == nullptr) return {};
  XmlAttributeBase *first = m_root->first_attribute;
  return first ? XmlAttribute(first->prev_attribute_c) : XmlAttribute();
}

inline XmlNode
XmlNode::first_child() const
{
  if(!m_root) return XmlNode();
  return XmlNode(m_root->first_child);
}

inline XmlNode
XmlNode::last_child() const
{
  if(!m_root) return XmlNode();
  XmlNodeBase *first = m_root->first_child;
  return first ? XmlNode(first->prev_sibling_c) : XmlNode();
}

inline bool
XmlNode::set_name(char_t const *rhs)
{
  xml_node_type type_ = m_root ? PUGI_IMPL_NODETYPE(m_root) : node_null;

  if(type_ != node_element && type_ != node_pi && type_ != node_declaration) return false;

  return impl::strcpy_insitu(m_root->name, m_root->header, impl::xml_memory_page_name_allocated_mask, rhs,
                             impl::strlength(rhs));
}

inline bool
XmlNode::set_name(char_t const *rhs, size_t size)
{
  xml_node_type type_ = m_root ? PUGI_IMPL_NODETYPE(m_root) : node_null;

  if(type_ != node_element && type_ != node_pi && type_ != node_declaration) return false;

  return impl::strcpy_insitu(m_root->name, m_root->header, impl::xml_memory_page_name_allocated_mask, rhs, size);
}

#if __cplusplus >= 201703L
inline bool
XmlNode::set_name(string_view_t rhs)
{
  xml_node_type type_ = m_root ? PUGI_IMPL_NODETYPE(m_root) : node_null;

  if(type_ != node_element && type_ != node_pi && type_ != node_declaration) return false;

  return impl::strcpy_insitu(m_root->name, m_root->header, impl::xml_memory_page_name_allocated_mask, rhs.data(),
                             rhs.size());
}
#endif

inline bool
XmlNode::set_value(const char_t *rhs)
{
  xml_node_type type_ = m_root ? PUGI_IMPL_NODETYPE(m_root) : node_null;

  if(type_ != node_pcdata && type_ != node_cdata && type_ != node_comment && type_ != node_pi && type_ != node_doctype)
    return false;

  return impl::strcpy_insitu(m_root->value, m_root->header, impl::xml_memory_page_value_allocated_mask, rhs,
                             impl::strlength(rhs));
}

inline bool
XmlNode::set_value(char_t const *rhs, size_t size)
{
  xml_node_type type_ = m_root ? PUGI_IMPL_NODETYPE(m_root) : node_null;

  if(type_ != node_pcdata && type_ != node_cdata && type_ != node_comment && type_ != node_pi && type_ != node_doctype)
    return false;

  return impl::strcpy_insitu(m_root->value, m_root->header, impl::xml_memory_page_value_allocated_mask, rhs, size);
}

#if __cplusplus >= 201703L
inline bool
XmlNode::set_value(string_view_t rhs)
{
  xml_node_type type_ = m_root ? PUGI_IMPL_NODETYPE(m_root) : node_null;

  if(type_ != node_pcdata && type_ != node_cdata && type_ != node_comment && type_ != node_pi && type_ != node_doctype)
    return false;

  return impl::strcpy_insitu(m_root->value, m_root->header, impl::xml_memory_page_value_allocated_mask, rhs.data(),
                             rhs.size());
}
#endif

inline XmlAttribute
XmlNode::append_attribute(const char_t *name_)
{
  if(!impl::allow_insert_attribute(type())) return {};

  impl::xml_allocator &alloc = impl::get_allocator(m_root);
  if(!alloc.reserve()) return {};

  XmlAttribute a(impl::allocate_attribute(alloc));
  if(!a) return {};

  impl::append_attribute(a._attr, m_root);

  a.set_name(name_);

  return a;
}

inline XmlAttribute
XmlNode::prepend_attribute(char_t const *name_)
{
  if(!impl::allow_insert_attribute(type())) return {};

  impl::xml_allocator &alloc = impl::get_allocator(m_root);
  if(!alloc.reserve()) return {};

  XmlAttribute a(impl::allocate_attribute(alloc));
  if(!a) return {};

  impl::prepend_attribute(a._attr, m_root);

  a.set_name(name_);

  return a;
}

inline XmlAttribute
XmlNode::insert_attribute_after(char_t const *name_, XmlAttribute const &attr)
{
  if(!impl::allow_insert_attribute(type())) return {};
  if(!attr || !impl::is_attribute_of(attr._attr, m_root)) return {};

  impl::xml_allocator &alloc = impl::get_allocator(m_root);
  if(!alloc.reserve()) return {};

  XmlAttribute a(impl::allocate_attribute(alloc));
  if(!a) return {};

  impl::insert_attribute_after(a._attr, attr._attr, m_root);

  a.set_name(name_);

  return a;
}

inline XmlAttribute
XmlNode::insert_attribute_before(char_t const *name_, XmlAttribute const &attr)
{
  if(!impl::allow_insert_attribute(type())) return {};
  if(!attr || !impl::is_attribute_of(attr._attr, m_root)) return {};

  impl::xml_allocator &alloc = impl::get_allocator(m_root);
  if(!alloc.reserve()) return {};

  XmlAttribute a(impl::allocate_attribute(alloc));
  if(!a) return {};

  impl::insert_attribute_before(a._attr, attr._attr, m_root);

  a.set_name(name_);

  return a;
}

#if __cplusplus >= 201703L
inline XmlAttribute
XmlNode::append_attribute(string_view_t name_)
{
  if(!impl::allow_insert_attribute(type())) return {};

  impl::xml_allocator &alloc = impl::get_allocator(m_root);
  if(!alloc.reserve()) return {};

  XmlAttribute a(impl::allocate_attribute(alloc));
  if(!a) return {};

  impl::append_attribute(a._attr, m_root);

  a.set_name(name_);

  return a;
}

inline XmlAttribute
XmlNode::prepend_attribute(string_view_t name_)
{
  if(!impl::allow_insert_attribute(type())) return {};

  impl::xml_allocator &alloc = impl::get_allocator(m_root);
  if(!alloc.reserve()) return {};

  XmlAttribute a(impl::allocate_attribute(alloc));
  if(!a) return {};

  impl::prepend_attribute(a._attr, m_root);

  a.set_name(name_);

  return a;
}

inline XmlAttribute
XmlNode::insert_attribute_after(string_view_t name_, XmlAttribute const &attr)
{
  if(!impl::allow_insert_attribute(type())) return {};
  if(!attr || !impl::is_attribute_of(attr._attr, m_root)) return {};

  impl::xml_allocator &alloc = impl::get_allocator(m_root);
  if(!alloc.reserve()) return {};

  XmlAttribute a(impl::allocate_attribute(alloc));
  if(!a) return {};

  impl::insert_attribute_after(a._attr, attr._attr, m_root);

  a.set_name(name_);

  return a;
}

inline XmlAttribute
XmlNode::insert_attribute_before(string_view_t name_, XmlAttribute const &attr)
{
  if(!impl::allow_insert_attribute(type())) return {};
  if(!attr || !impl::is_attribute_of(attr._attr, m_root)) return {};

  impl::xml_allocator &alloc = impl::get_allocator(m_root);
  if(!alloc.reserve()) return {};

  XmlAttribute a(impl::allocate_attribute(alloc));
  if(!a) return {};

  impl::insert_attribute_before(a._attr, attr._attr, m_root);

  a.set_name(name_);

  return a;
}
#endif

inline XmlAttribute
XmlNode::append_copy(const XmlAttribute &proto)
{
  if(!proto) return {};
  if(!impl::allow_insert_attribute(type())) return {};

  impl::xml_allocator &alloc = impl::get_allocator(m_root);
  if(!alloc.reserve()) return {};

  XmlAttribute a(impl::allocate_attribute(alloc));
  if(!a) return {};

  impl::append_attribute(a._attr, m_root);
  impl::node_copy_attribute(a._attr, proto._attr);

  return a;
}

inline XmlAttribute
XmlNode::prepend_copy(XmlAttribute const &proto)
{
  if(!proto) return {};
  if(!impl::allow_insert_attribute(type())) return {};

  impl::xml_allocator &alloc = impl::get_allocator(m_root);
  if(!alloc.reserve()) return {};

  XmlAttribute a(impl::allocate_attribute(alloc));
  if(!a) return {};

  impl::prepend_attribute(a._attr, m_root);
  impl::node_copy_attribute(a._attr, proto._attr);

  return a;
}

inline XmlAttribute
XmlNode::insert_copy_after(XmlAttribute const &proto, XmlAttribute const &attr)
{
  if(!proto) return {};
  if(!impl::allow_insert_attribute(type())) return {};
  if(!attr || !impl::is_attribute_of(attr._attr, m_root)) return {};

  impl::xml_allocator &alloc = impl::get_allocator(m_root);
  if(!alloc.reserve()) return {};

  XmlAttribute a(impl::allocate_attribute(alloc));
  if(!a) return {};

  impl::insert_attribute_after(a._attr, attr._attr, m_root);
  impl::node_copy_attribute(a._attr, proto._attr);

  return a;
}

inline XmlAttribute
XmlNode::insert_copy_before(XmlAttribute const &proto, XmlAttribute const &attr)
{
  if(!proto) return {};
  if(!impl::allow_insert_attribute(type())) return {};
  if(!attr || !impl::is_attribute_of(attr._attr, m_root)) return {};

  impl::xml_allocator &alloc = impl::get_allocator(m_root);
  if(!alloc.reserve()) return {};

  XmlAttribute a(impl::allocate_attribute(alloc));
  if(!a) return {};

  impl::insert_attribute_before(a._attr, attr._attr, m_root);
  impl::node_copy_attribute(a._attr, proto._attr);

  return a;
}

inline XmlNode
XmlNode::append_child(xml_node_type type_)
{
  if(!impl::allow_insert_child(type(), type_)) return XmlNode();

  impl::xml_allocator &alloc = impl::get_allocator(m_root);
  if(!alloc.reserve()) return XmlNode();

  XmlNode n(impl::allocate_node(alloc, type_));
  if(!n) return XmlNode();

  impl::append_node(n.m_root, m_root);

  if(type_ == node_declaration) n.set_name(LUMEX_XML_TEXT("xml"));

  return n;
}

inline XmlNode
XmlNode::prepend_child(xml_node_type type_)
{
  if(!impl::allow_insert_child(type(), type_)) return XmlNode();

  impl::xml_allocator &alloc = impl::get_allocator(m_root);
  if(!alloc.reserve()) return XmlNode();

  XmlNode n(impl::allocate_node(alloc, type_));
  if(!n) return XmlNode();

  impl::prepend_node(n.m_root, m_root);

  if(type_ == node_declaration) n.set_name(LUMEX_XML_TEXT("xml"));

  return n;
}

inline XmlNode
XmlNode::insert_child_before(xml_node_type type_, XmlNode const &node)
{
  if(!impl::allow_insert_child(type(), type_)) return XmlNode();
  if(!node.m_root || node.m_root->parent != m_root) return XmlNode();

  impl::xml_allocator &alloc = impl::get_allocator(m_root);
  if(!alloc.reserve()) return XmlNode();

  XmlNode n(impl::allocate_node(alloc, type_));
  if(!n) return XmlNode();

  impl::insert_node_before(n.m_root, node.m_root);

  if(type_ == node_declaration) n.set_name(LUMEX_XML_TEXT("xml"));

  return n;
}

inline XmlNode
XmlNode::insert_child_after(xml_node_type type_, XmlNode const &node)
{
  if(!impl::allow_insert_child(type(), type_)) return XmlNode();
  if(!node.m_root || node.m_root->parent != m_root) return XmlNode();

  impl::xml_allocator &alloc = impl::get_allocator(m_root);
  if(!alloc.reserve()) return XmlNode();

  XmlNode n(impl::allocate_node(alloc, type_));
  if(!n) return XmlNode();

  impl::insert_node_after(n.m_root, node.m_root);

  if(type_ == node_declaration) n.set_name(LUMEX_XML_TEXT("xml"));

  return n;
}

inline XmlNode
XmlNode::append_child(char_t const *name_)
{
  XmlNode result = append_child(node_element);

  result.set_name(name_);

  return result;
}

inline XmlNode
XmlNode::prepend_child(char_t const *name_)
{
  XmlNode result = prepend_child(node_element);

  result.set_name(name_);

  return result;
}

inline XmlNode
XmlNode::insert_child_after(char_t const *name_, XmlNode const &node)
{
  XmlNode result = insert_child_after(node_element, node);

  result.set_name(name_);

  return result;
}

inline XmlNode
XmlNode::insert_child_before(char_t const *name_, XmlNode const &node)
{
  XmlNode result = insert_child_before(node_element, node);

  result.set_name(name_);

  return result;
}

#if __cplusplus >= 201703L
inline XmlNode
XmlNode::append_child(string_view_t name_)
{
  XmlNode result = append_child(node_element);

  result.set_name(name_);

  return result;
}

inline XmlNode
XmlNode::prepend_child(string_view_t name_)
{
  XmlNode result = prepend_child(node_element);

  result.set_name(name_);

  return result;
}

inline XmlNode
XmlNode::insert_child_after(string_view_t name_, XmlNode const &node)
{
  XmlNode result = insert_child_after(node_element, node);

  result.set_name(name_);

  return result;
}

inline XmlNode
XmlNode::insert_child_before(string_view_t name_, XmlNode const &node)
{
  XmlNode result = insert_child_before(node_element, node);

  result.set_name(name_);

  return result;
}
#endif

inline XmlNode
XmlNode::append_copy(const XmlNode &proto)
{
  xml_node_type type_ = proto.type();
  if(!impl::allow_insert_child(type(), type_)) return XmlNode();

  impl::xml_allocator &alloc = impl::get_allocator(m_root);
  if(!alloc.reserve()) return XmlNode();

  XmlNode n(impl::allocate_node(alloc, type_));
  if(!n) return XmlNode();

  impl::append_node(n.m_root, m_root);
  impl::node_copy_tree(n.m_root, proto.m_root);

  return n;
}

inline XmlNode
XmlNode::prepend_copy(XmlNode const &proto)
{
  xml_node_type type_ = proto.type();
  if(!impl::allow_insert_child(type(), type_)) return XmlNode();

  impl::xml_allocator &alloc = impl::get_allocator(m_root);
  if(!alloc.reserve()) return XmlNode();

  XmlNode n(impl::allocate_node(alloc, type_));
  if(!n) return XmlNode();

  impl::prepend_node(n.m_root, m_root);
  impl::node_copy_tree(n.m_root, proto.m_root);

  return n;
}

inline XmlNode
XmlNode::insert_copy_after(XmlNode const &proto, XmlNode const &node)
{
  xml_node_type type_ = proto.type();
  if(!impl::allow_insert_child(type(), type_)) return XmlNode();
  if(!node.m_root || node.m_root->parent != m_root) return XmlNode();

  impl::xml_allocator &alloc = impl::get_allocator(m_root);
  if(!alloc.reserve()) return XmlNode();

  XmlNode n(impl::allocate_node(alloc, type_));
  if(!n) return XmlNode();

  impl::insert_node_after(n.m_root, node.m_root);
  impl::node_copy_tree(n.m_root, proto.m_root);

  return n;
}

inline XmlNode
XmlNode::insert_copy_before(XmlNode const &proto, XmlNode const &node)
{
  xml_node_type type_ = proto.type();
  if(!impl::allow_insert_child(type(), type_)) return XmlNode();
  if(!node.m_root || node.m_root->parent != m_root) return XmlNode();

  impl::xml_allocator &alloc = impl::get_allocator(m_root);
  if(!alloc.reserve()) return XmlNode();

  XmlNode n(impl::allocate_node(alloc, type_));
  if(!n) return XmlNode();

  impl::insert_node_before(n.m_root, node.m_root);
  impl::node_copy_tree(n.m_root, proto.m_root);

  return n;
}

inline XmlNode
XmlNode::append_move(XmlNode const &moved)
{
  if(!impl::allow_move(*this, moved)) return XmlNode();

  impl::xml_allocator &alloc = impl::get_allocator(m_root);
  if(!alloc.reserve()) return XmlNode();

  // disable document_buffer_order optimization since moving nodes around changes document order without changing
  // buffer pointers
  impl::get_document(m_root).header |= impl::xml_memory_page_contents_shared_mask;

  impl::remove_node(moved.m_root);
  impl::append_node(moved.m_root, m_root);

  return moved;
}

inline XmlNode
XmlNode::prepend_move(XmlNode const &moved)
{
  if(!impl::allow_move(*this, moved)) return XmlNode();

  impl::xml_allocator &alloc = impl::get_allocator(m_root);
  if(!alloc.reserve()) return XmlNode();

  // disable document_buffer_order optimization since moving nodes around changes document order without changing
  // buffer pointers
  impl::get_document(m_root).header |= impl::xml_memory_page_contents_shared_mask;

  impl::remove_node(moved.m_root);
  impl::prepend_node(moved.m_root, m_root);

  return moved;
}

inline XmlNode
XmlNode::insert_move_after(XmlNode const &moved, XmlNode const &node)
{
  if(!impl::allow_move(*this, moved)) return XmlNode();
  if(!node.m_root || node.m_root->parent != m_root) return XmlNode();
  if(moved.m_root == node.m_root) return XmlNode();

  impl::xml_allocator &alloc = impl::get_allocator(m_root);
  if(!alloc.reserve()) return XmlNode();

  // disable document_buffer_order optimization since moving nodes around changes document order without changing
  // buffer pointers
  impl::get_document(m_root).header |= impl::xml_memory_page_contents_shared_mask;

  impl::remove_node(moved.m_root);
  impl::insert_node_after(moved.m_root, node.m_root);

  return moved;
}

inline XmlNode
XmlNode::insert_move_before(XmlNode const &moved, XmlNode const &node)
{
  if(!impl::allow_move(*this, moved)) return XmlNode();
  if(!node.m_root || node.m_root->parent != m_root) return XmlNode();
  if(moved.m_root == node.m_root) return XmlNode();

  impl::xml_allocator &alloc = impl::get_allocator(m_root);
  if(!alloc.reserve()) return XmlNode();

  // disable document_buffer_order optimization since moving nodes around changes document order without changing
  // buffer pointers
  impl::get_document(m_root).header |= impl::xml_memory_page_contents_shared_mask;

  impl::remove_node(moved.m_root);
  impl::insert_node_before(moved.m_root, node.m_root);

  return moved;
}

inline bool
XmlNode::remove_attribute(char_t const *name_)
{
  return remove_attribute(attribute(name_));
}

#if __cplusplus >= 201703L
inline bool
XmlNode::remove_attribute(string_view_t name_)
{
  return remove_attribute(attribute(name_));
}
#endif

inline bool
XmlNode::remove_attribute(const XmlAttribute &attr)
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
XmlNode::remove_attributes()
{
  if(!m_root) return false;

  impl::xml_allocator &alloc = impl::get_allocator(m_root);
  if(!alloc.reserve()) return false;

  for(XmlAttributeBase *attr = m_root->first_attribute; attr;)
  {
    XmlAttributeBase *next = attr->next_attribute;

    impl::destroy_attribute(attr, alloc);

    attr = next;
  }

  m_root->first_attribute = nullptr;

  return true;
}

inline bool
XmlNode::remove_child(char_t const *name_)
{
  return remove_child(child(name_));
}

#if __cplusplus >= 201703L
inline bool
XmlNode::remove_child(string_view_t name_)
{
  return remove_child(child(name_));
}
#endif

inline bool
XmlNode::remove_child(const XmlNode &n)
{
  if(!m_root || !n.m_root || n.m_root->parent != m_root) return false;

  impl::xml_allocator &alloc = impl::get_allocator(m_root);
  if(!alloc.reserve()) return false;

  impl::remove_node(n.m_root);
  impl::destroy_node(n.m_root, alloc);

  return true;
}

inline bool
XmlNode::remove_children()
{
  if(!m_root) return false;

  impl::xml_allocator &alloc = impl::get_allocator(m_root);
  if(!alloc.reserve()) return false;

  for(XmlNodeBase *cur = m_root->first_child; cur;)
  {
    XmlNodeBase *next = cur->next_sibling;

    impl::destroy_node(cur, alloc);

    cur = next;
  }

  m_root->first_child = nullptr;

  return true;
}

inline xml_parse_result
XmlNode::append_buffer(void const *contents, size_t size, unsigned int options, xml_encoding encoding)
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

inline XmlNode
XmlNode::find_child_by_attribute(char_t const *name_, char_t const *attr_name, char_t const *attr_value) const
{
  if(!m_root) return XmlNode();

  for(XmlNodeBase *i = m_root->first_child; i; i = i->next_sibling)
  {
    char_t const *iname = i->name;
    if(iname && Utility::strequal(name_, iname))
    {
      for(XmlAttributeBase *a = i->first_attribute; a; a = a->next_attribute)
      {
        char_t const *aname = a->name;
        if(aname && Utility::strequal(attr_name, aname))
        {
          char_t const *avalue = a->value;
          if(Utility::strequal(attr_value, avalue ? avalue : LUMEX_XML_TEXT(""))) return XmlNode(i);
        }
      }
    }
  }

  return XmlNode();
}

inline XmlNode
XmlNode::find_child_by_attribute(char_t const *attr_name, char_t const *attr_value) const
{
  if(!m_root) return XmlNode();

  for(XmlNodeBase *i = m_root->first_child; i; i = i->next_sibling)
    for(XmlAttributeBase *a = i->first_attribute; a; a = a->next_attribute)
    {
      char_t const *aname = a->name;
      if(aname && Utility::strequal(attr_name, aname))
      {
        char_t const *avalue = a->value;
        if(Utility::strequal(attr_value, avalue ? avalue : LUMEX_XML_TEXT(""))) return XmlNode(i);
      }
    }

  return XmlNode();
}

inline string_t
XmlNode::path(char_t delimiter) const
{
  if(!m_root) return string_t();

  size_t offset = 0;

  for(XmlNodeBase *i = m_root; i; i = i->parent)
  {
    char_t const *iname = i->name;
    offset += (i != m_root);
    offset += iname ? impl::strlength(iname) : 0;
  }

  string_t result;
  result.resize(offset);

  for(XmlNodeBase *j = m_root; j; j = j->parent)
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

inline XmlNode
XmlNode::first_element_by_path(char_t const *path_, char_t delimiter) const
{
  XmlNode context = path_[0] == delimiter ? root() : *this;

  if(!context.m_root) return XmlNode();

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
    for(XmlNodeBase *j = context.m_root->first_child; j; j = j->next_sibling)
    {
      char_t const *jname = j->name;
      if(jname && impl::strequalrange(jname, path_segment, static_cast<size_t>(path_segment_end - path_segment)))
      {
        XmlNode subsearch = XmlNode(j).first_element_by_path(next_segment, delimiter);

        if(subsearch) return subsearch;
      }
    }

    return XmlNode();
  }
}

inline bool
XmlNode::traverse(xml_tree_walker &walker)
{
  walker._depth = -1;

  XmlNode arg_begin(m_root);
  if(!walker.begin(arg_begin)) return false;

  XmlNodeBase *cur = m_root ? m_root->first_child + 0 : nullptr;

  if(cur)
  {
    ++walker._depth;

    do {
      XmlNode arg_for_each(cur);
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

  XmlNode arg_end(m_root);
  return walker.end(arg_end);
}

inline size_t
XmlNode::hash_value() const
{
  return reinterpret_cast<uintptr_t>(m_root) / sizeof(XmlNodeBase);
}

inline XmlNodeBase *
XmlNode::get() const
{
  return m_root;
}

inline void
XmlNode::print(xml_writer &writer, char_t const *indent, unsigned int flags, xml_encoding encoding,
               unsigned int depth) const
{
  if(!m_root) return;

  impl::xml_buffered_writer buffered_writer(writer, encoding);

  impl::node_output(buffered_writer, m_root, indent, flags, depth);

  buffered_writer.flush();
}

inline void
XmlNode::print(std::basic_ostream<char> &stream, char_t const *indent, unsigned int flags, xml_encoding encoding,
               unsigned int depth) const
{
  xml_writer_stream writer(stream);

  print(writer, indent, flags, encoding, depth);
}

inline void
XmlNode::print(std::basic_ostream<wchar_t> &stream, char_t const *indent, unsigned int flags, unsigned int depth) const
{
  xml_writer_stream writer(stream);

  print(writer, indent, flags, encoding_wchar, depth);
}

inline ptrdiff_t
XmlNode::offset_debug() const
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
XmlNode::select_node(char_t const *query, xpath_variable_set *variables) const
{
  xpath_query q(query, variables);
  return q.evaluate_node(*this);
}

inline xpath_node
XmlNode::select_node(xpath_query const &query) const
{
  return query.evaluate_node(*this);
}

inline xpath_node_set
XmlNode::select_nodes(char_t const *query, xpath_variable_set *variables) const
{
  xpath_query q(query, variables);
  return q.evaluate_node_set(*this);
}

inline xpath_node_set
XmlNode::select_nodes(xpath_query const &query) const
{
  return query.evaluate_node_set(*this);
}

inline xpath_node
XmlNode::select_single_node(char_t const *query, xpath_variable_set *variables) const
{
  xpath_query q(query, variables);
  return q.evaluate_node(*this);
}

inline xpath_node
XmlNode::select_single_node(xpath_query const &query) const
{
  return query.evaluate_node(*this);
}
