#include "lumex/xml/attribute/XmlAttributeIterator.hpp"
#include "lumex/xml/document/XmlDocument.hpp"
#include "lumex/xml/memory/XmlAllocator.hpp"
#include "lumex/xml/text/XmlParseResult.hpp"
#include "lumex/xml/text/XmlText.hpp"
#include "lumex/xml/tree/XmlTreeWalker.hpp"
#include "lumex/xml/types/XmlTypes.hpp"
#include "lumex/xml/utility/XmlUtils.hpp"
#include "lumex/xml/writer/IXmlWriter.hpp"
#include "lumex/xml/writer/XmlBufferedWriter.hpp"
#include "lumex/xml/writer/XmlWriterStream.hpp"

#include "XmlNode.hpp"
#include "XmlNodeIterator.hpp"

using namespace Lumex::Xml::Writer;
using namespace Lumex::Xml::Tree;
using namespace Lumex::Xml::Memory;
using namespace Lumex::Xml::Attribute;
using namespace Lumex::Xml::Node;
using namespace Lumex::Xml::Utility;
using namespace Lumex::Xml::Document;
using namespace Lumex::Xml::Text;

inline bool
allow_move(XmlNode parent, XmlNode child)
{
  // check that child can be a child of parent
  if(!allow_insert_child(parent.type(), child.type())) return false;

  // check that node is not moved between documents
  if(parent.root() != child.root()) return false;

  // check that new parent is not in the child subtree
  XmlNode cur = parent;

  while(cur != nullptr)
  {
    if(cur == child) return false;

    cur = cur.parent();
  }

  return true;
}

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
  XmlAttributeBase *hint = hint_.get();

  // if hint is not an attribute of node, behavior is not defined
  LUMEX_ASSERT(!hint || (m_root && Utility::is_attribute_of(hint, m_root)));

  if(m_root == nullptr) return {};

  // optimistically search from hint up until the end
  for(XmlAttributeBase *i = hint; i; i = i->next_attribute)
  {
    char_t const *iname = i->name;
    if(iname && Utility::strequal(name_, iname))
    {
      // update hint to maximize efficiency of searching for consecutive attributes
      hint_.set(i->next_attribute);

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
      hint_.set(j->next_attribute);

      return XmlAttribute(j);
    }
  }

  return {};
}

#if __cplusplus >= 201703L
inline XmlAttribute
XmlNode::attribute(string_view_t name_, XmlAttribute &hint_) const
{
  XmlAttributeBase *hint = hint_.get();

  // if hint is not an attribute of node, behavior is not defined
  LUMEX_ASSERT(!hint || (m_root && Utility::is_attribute_of(hint, m_root)));

  if(m_root == nullptr) return {};

  // optimistically search from hint up until the end
  for(XmlAttributeBase *i = hint; i; i = i->next_attribute)
  {
    char_t const *iname = i->name;
    if(iname && Utility::stringview_equal(name_, iname))
    {
      // update hint to maximize efficiency of searching for consecutive attributes
      hint_.set(i->next_attribute);

      return XmlAttribute(i);
    }
  }

  // wrap around and search from the first attribute until the hint
  // 'j' null pointer check is technically redundant, but it prevents a crash in case the LUMEX_ASSERTion above fails
  for(XmlAttributeBase *j = m_root->first_attribute; j && j != hint; j = j->next_attribute)
  {
    char_t const *jname = j->name;
    if(jname && Utility::stringview_equal(name_, jname))
    {
      // update hint to maximize efficiency of searching for consecutive attributes
      hint_.set(j->next_attribute);

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
  return m_root ? XmlNode(&Document::get_document(m_root)) : XmlNode();
}

inline XmlText
XmlNode::text() const
{
  return XmlText(m_root);
}

inline char_t const *
XmlNode::child_value() const
{
  if(!m_root) return LUMEX_XML_TEXT("");

  // element nodes can have value if parse_embed_pcdata was used
  if(LUMEX_XML_NODETYPE(m_root) == node_element && m_root->value) return m_root->value;

  for(XmlNodeBase *i = m_root->first_child; i; i = i->next_sibling)
  {
    char_t const *ivalue = i->value;
    if(Node::is_text_node(i) && ivalue) return ivalue;
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
  xml_node_type type_ = m_root ? LUMEX_XML_NODETYPE(m_root) : node_null;

  if(type_ != node_element && type_ != node_pi && type_ != node_declaration) return false;

  return Utility::strcpy_insitu(m_root->name, m_root->header, Constants::kxml_memory_page_name_allocated_mask, rhs,
                                Utility::strlength(rhs));
}

inline bool
XmlNode::set_name(char_t const *rhs, size_t size)
{
  xml_node_type type_ = m_root ? LUMEX_XML_NODETYPE(m_root) : node_null;

  if(type_ != node_element && type_ != node_pi && type_ != node_declaration) return false;

  return Utility::strcpy_insitu(m_root->name, m_root->header, Constants::kxml_memory_page_name_allocated_mask, rhs,
                                size);
}

#if __cplusplus >= 201703L
inline bool
XmlNode::set_name(string_view_t rhs)
{
  xml_node_type type_ = m_root ? LUMEX_XML_NODETYPE(m_root) : node_null;

  if(type_ != node_element && type_ != node_pi && type_ != node_declaration) return false;

  return Utility::strcpy_insitu(m_root->name, m_root->header, Constants::kxml_memory_page_name_allocated_mask,
                                rhs.data(), rhs.size());
}
#endif

inline bool
XmlNode::set_value(const char_t *rhs)
{
  xml_node_type type_ = m_root ? LUMEX_XML_NODETYPE(m_root) : node_null;

  if(type_ != node_pcdata && type_ != node_cdata && type_ != node_comment && type_ != node_pi && type_ != node_doctype)
    return false;

  return Utility::strcpy_insitu(m_root->value, m_root->header, Constants::kxml_memory_page_value_allocated_mask, rhs,
                                Utility::strlength(rhs));
}

inline bool
XmlNode::set_value(char_t const *rhs, size_t size)
{
  xml_node_type type_ = m_root ? LUMEX_XML_NODETYPE(m_root) : node_null;

  if(type_ != node_pcdata && type_ != node_cdata && type_ != node_comment && type_ != node_pi && type_ != node_doctype)
    return false;

  return Utility::strcpy_insitu(m_root->value, m_root->header, Constants::kxml_memory_page_value_allocated_mask, rhs,
                                size);
}

#if __cplusplus >= 201703L
inline bool
XmlNode::set_value(string_view_t rhs)
{
  xml_node_type type_ = m_root ? LUMEX_XML_NODETYPE(m_root) : node_null;

  if(type_ != node_pcdata && type_ != node_cdata && type_ != node_comment && type_ != node_pi && type_ != node_doctype)
    return false;

  return Utility::strcpy_insitu(m_root->value, m_root->header, Constants::kxml_memory_page_value_allocated_mask,
                                rhs.data(), rhs.size());
}
#endif

inline XmlAttribute
XmlNode::append_attribute(const char_t *name_)
{
  if(!Utility::allow_insert_attribute(type())) return {};

  XmlAllocator &alloc = Memory::get_allocator(m_root);

  XmlAttribute a(Lumex::Xml::Attribute::allocate_attribute(alloc));
  if(!a) return {};

  Lumex::Xml::Attribute::append_attribute(a.get(), m_root);

  a.set_name(name_);

  return a;
}

inline XmlAttribute
XmlNode::prepend_attribute(char_t const *name_)
{
  if(!Utility::allow_insert_attribute(type())) return {};

  XmlAllocator &alloc = Memory::get_allocator(m_root);

  XmlAttribute a(Lumex::Xml::Attribute::allocate_attribute(alloc));
  if(!a) return {};

  Lumex::Xml::Attribute::prepend_attribute(a.get(), m_root);

  a.set_name(name_);

  return a;
}

inline XmlAttribute
XmlNode::insert_attribute_after(char_t const *name_, XmlAttribute const &attr)
{
  if(!Utility::allow_insert_attribute(type())) return {};
  if(!attr || !Utility::is_attribute_of(attr.get(), m_root)) return {};

  XmlAllocator &alloc = Memory::get_allocator(m_root);

  XmlAttribute a(Lumex::Xml::Attribute::allocate_attribute(alloc));
  if(!a) return {};

  Lumex::Xml::Attribute::insert_attribute_after(a.get(), attr.get(), m_root);

  a.set_name(name_);

  return a;
}

inline XmlAttribute
XmlNode::insert_attribute_before(char_t const *name_, XmlAttribute const &attr)
{
  if(!Utility::allow_insert_attribute(type())) return {};
  if(!attr || !Utility::is_attribute_of(attr.get(), m_root)) return {};

  XmlAllocator &alloc = Memory::get_allocator(m_root);

  XmlAttribute a(Lumex::Xml::Attribute::allocate_attribute(alloc));
  if(!a) return {};

  Lumex::Xml::Attribute::insert_attribute_before(a.get(), attr.get(), m_root);

  a.set_name(name_);

  return a;
}

#if __cplusplus >= 201703L
inline XmlAttribute
XmlNode::append_attribute(string_view_t name_)
{
  if(!Utility::allow_insert_attribute(type())) return {};

  XmlAllocator &alloc = Memory::get_allocator(m_root);

  XmlAttribute a(Lumex::Xml::Attribute::allocate_attribute(alloc));
  if(!a) return {};

  Lumex::Xml::Attribute::append_attribute(a.get(), m_root);

  a.set_name(name_);

  return a;
}

inline XmlAttribute
XmlNode::prepend_attribute(string_view_t name_)
{
  if(!Utility::allow_insert_attribute(type())) return {};

  XmlAllocator &alloc = Memory::get_allocator(m_root);

  XmlAttribute a(Lumex::Xml::Attribute::allocate_attribute(alloc));
  if(!a) return {};

  Lumex::Xml::Attribute::prepend_attribute(a.get(), m_root);

  a.set_name(name_);

  return a;
}

inline XmlAttribute
XmlNode::insert_attribute_after(string_view_t name_, XmlAttribute const &attr)
{
  if(!Utility::allow_insert_attribute(type())) return {};
  if(!attr || !Utility::is_attribute_of(attr.get(), m_root)) return {};

  XmlAllocator &alloc = Memory::get_allocator(m_root);

  XmlAttribute a(Lumex::Xml::Attribute::allocate_attribute(alloc));
  if(!a) return {};

  Lumex::Xml::Attribute::insert_attribute_after(a.get(), attr.get(), m_root);

  a.set_name(name_);

  return a;
}

inline XmlAttribute
XmlNode::insert_attribute_before(string_view_t name_, XmlAttribute const &attr)
{
  if(!Utility::allow_insert_attribute(type())) return {};
  if(!attr || !Utility::is_attribute_of(attr.get(), m_root)) return {};

  XmlAllocator &alloc = Memory::get_allocator(m_root);

  XmlAttribute a(Lumex::Xml::Attribute::allocate_attribute(alloc));
  if(!a) return {};

  Lumex::Xml::Attribute::insert_attribute_before(a.get(), attr.get(), m_root);

  a.set_name(name_);

  return a;
}
#endif

inline XmlAttribute
XmlNode::append_copy(const XmlAttribute &proto)
{
  if(!proto) return {};
  if(!Utility::allow_insert_attribute(type())) return {};

  XmlAllocator &alloc = Memory::get_allocator(m_root);

  XmlAttribute a(Lumex::Xml::Attribute::allocate_attribute(alloc));
  if(!a) return {};

  Lumex::Xml::Attribute::append_attribute(a.get(), m_root);
  Lumex::Xml::Attribute::node_copy_attribute(a.get(), proto.get());

  return a;
}

inline XmlAttribute
XmlNode::prepend_copy(XmlAttribute const &proto)
{
  if(!proto) return {};
  if(!Utility::allow_insert_attribute(type())) return {};

  XmlAllocator &alloc = Memory::get_allocator(m_root);

  XmlAttribute a(Lumex::Xml::Attribute::allocate_attribute(alloc));
  if(!a) return {};

  Lumex::Xml::Attribute::prepend_attribute(a.get(), m_root);
  Lumex::Xml::Attribute::node_copy_attribute(a.get(), proto.get());

  return a;
}

inline XmlAttribute
XmlNode::insert_copy_after(XmlAttribute const &proto, XmlAttribute const &attr)
{
  if(!proto) return {};
  if(!Utility::allow_insert_attribute(type())) return {};
  if(!attr || !Utility::is_attribute_of(attr.get(), m_root)) return {};

  XmlAllocator &alloc = Memory::get_allocator(m_root);

  XmlAttribute a(Lumex::Xml::Attribute::allocate_attribute(alloc));
  if(!a) return {};

  Lumex::Xml::Attribute::insert_attribute_after(a.get(), attr.get(), m_root);
  Lumex::Xml::Attribute::node_copy_attribute(a.get(), proto.get());

  return a;
}

inline XmlAttribute
XmlNode::insert_copy_before(XmlAttribute const &proto, XmlAttribute const &attr)
{
  if(!proto) return {};
  if(!Utility::allow_insert_attribute(type())) return {};
  if(!attr || !Utility::is_attribute_of(attr.get(), m_root)) return {};

  XmlAllocator &alloc = Memory::get_allocator(m_root);

  XmlAttribute a(Lumex::Xml::Attribute::allocate_attribute(alloc));
  if(!a) return {};

  Lumex::Xml::Attribute::insert_attribute_before(a.get(), attr.get(), m_root);
  Lumex::Xml::Attribute::node_copy_attribute(a.get(), proto.get());

  return a;
}

inline XmlNode
XmlNode::append_child(xml_node_type type_)
{
  if(!Utility::allow_insert_child(type(), type_)) return XmlNode();

  XmlAllocator &alloc = Memory::get_allocator(m_root);

  XmlNode n(Lumex::Xml::Node::allocate_node(alloc, type_));
  if(!n) return XmlNode();

  Lumex::Xml::Node::append_node(n.m_root, m_root);

  if(type_ == node_declaration) n.set_name(LUMEX_XML_TEXT("xml"));

  return n;
}

inline XmlNode
XmlNode::prepend_child(xml_node_type type_)
{
  if(!Utility::allow_insert_child(type(), type_)) return XmlNode();

  XmlAllocator &alloc = Memory::get_allocator(m_root);

  XmlNode n(Lumex::Xml::Node::allocate_node(alloc, type_));
  if(!n) return XmlNode();

  Lumex::Xml::Node::prepend_node(n.m_root, m_root);

  if(type_ == node_declaration) n.set_name(LUMEX_XML_TEXT("xml"));

  return n;
}

inline XmlNode
XmlNode::insert_child_before(xml_node_type type_, XmlNode const &node)
{
  if(!Utility::allow_insert_child(type(), type_)) return XmlNode();
  if(!node.m_root || node.m_root->parent != m_root) return XmlNode();

  XmlAllocator &alloc = Memory::get_allocator(m_root);

  XmlNode n(Lumex::Xml::Node::allocate_node(alloc, type_));
  if(!n) return XmlNode();

  Lumex::Xml::Node::insert_node_before(n.m_root, node.m_root);

  if(type_ == node_declaration) n.set_name(LUMEX_XML_TEXT("xml"));

  return n;
}

inline XmlNode
XmlNode::insert_child_after(xml_node_type type_, XmlNode const &node)
{
  if(!Utility::allow_insert_child(type(), type_)) return XmlNode();
  if(!node.m_root || node.m_root->parent != m_root) return XmlNode();

  XmlAllocator &alloc = Memory::get_allocator(m_root);

  XmlNode n(Lumex::Xml::Node::allocate_node(alloc, type_));
  if(!n) return XmlNode();

  Lumex::Xml::Node::insert_node_after(n.m_root, node.m_root);

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
  if(!Utility::allow_insert_child(type(), type_)) return XmlNode();

  XmlAllocator &alloc = Memory::get_allocator(m_root);

  XmlNode n(Lumex::Xml::Node::allocate_node(alloc, type_));
  if(!n) return XmlNode();

  Lumex::Xml::Node::append_node(n.m_root, m_root);
  Lumex::Xml::Node::node_copy_tree(n.m_root, proto.m_root);

  return n;
}

inline XmlNode
XmlNode::prepend_copy(XmlNode const &proto)
{
  xml_node_type type_ = proto.type();
  if(!Utility::allow_insert_child(type(), type_)) return XmlNode();

  XmlAllocator &alloc = Memory::get_allocator(m_root);

  XmlNode n(Lumex::Xml::Node::allocate_node(alloc, type_));
  if(!n) return XmlNode();

  Lumex::Xml::Node::prepend_node(n.m_root, m_root);
  Lumex::Xml::Node::node_copy_tree(n.m_root, proto.m_root);

  return n;
}

inline XmlNode
XmlNode::insert_copy_after(XmlNode const &proto, XmlNode const &node)
{
  xml_node_type type_ = proto.type();
  if(!Utility::allow_insert_child(type(), type_)) return XmlNode();
  if(!node.m_root || node.m_root->parent != m_root) return XmlNode();

  XmlAllocator &alloc = Memory::get_allocator(m_root);

  XmlNode n(Lumex::Xml::Node::allocate_node(alloc, type_));
  if(!n) return XmlNode();

  Lumex::Xml::Node::insert_node_after(n.m_root, node.m_root);
  Lumex::Xml::Node::node_copy_tree(n.m_root, proto.m_root);

  return n;
}

inline XmlNode
XmlNode::insert_copy_before(XmlNode const &proto, XmlNode const &node)
{
  xml_node_type type_ = proto.type();
  if(!Utility::allow_insert_child(type(), type_)) return XmlNode();
  if(!node.m_root || node.m_root->parent != m_root) return XmlNode();

  XmlAllocator &alloc = Memory::get_allocator(m_root);

  XmlNode n(Lumex::Xml::Node::allocate_node(alloc, type_));
  if(!n) return XmlNode();

  Lumex::Xml::Node::insert_node_before(n.m_root, node.m_root);
  Lumex::Xml::Node::node_copy_tree(n.m_root, proto.m_root);

  return n;
}

inline XmlNode
XmlNode::append_move(XmlNode const &moved)
{
  if(!allow_move(*this, moved)) return XmlNode();

  // disable document_buffer_order optimization since moving nodes around changes document order without changing
  // buffer pointers
  Document::get_document(m_root).header |= Lumex::Xml::Constants::kxml_memory_page_contents_shared_mask;

  Lumex::Xml::Node::remove_node(moved.m_root);
  Lumex::Xml::Node::append_node(moved.m_root, m_root);

  return moved;
}

inline XmlNode
XmlNode::prepend_move(XmlNode const &moved)
{
  if(!allow_move(*this, moved)) return XmlNode();

  // disable document_buffer_order optimization since moving nodes around changes document order without changing
  // buffer pointers
  Document::get_document(m_root).header |= Lumex::Xml::Constants::kxml_memory_page_contents_shared_mask;

  Lumex::Xml::Node::remove_node(moved.m_root);
  Lumex::Xml::Node::prepend_node(moved.m_root, m_root);

  return moved;
}

inline XmlNode
XmlNode::insert_move_after(XmlNode const &moved, XmlNode const &node)
{
  if(!allow_move(*this, moved)) return XmlNode();
  if(!node.m_root || node.m_root->parent != m_root) return XmlNode();
  if(moved.m_root == node.m_root) return XmlNode();

  // disable document_buffer_order optimization since moving nodes around changes document order without changing
  // buffer pointers
  Document::get_document(m_root).header |= Lumex::Xml::Constants::kxml_memory_page_contents_shared_mask;

  Lumex::Xml::Node::remove_node(moved.m_root);
  Lumex::Xml::Node::insert_node_after(moved.m_root, node.m_root);

  return moved;
}

inline XmlNode
XmlNode::insert_move_before(XmlNode const &moved, XmlNode const &node)
{
  if(!allow_move(*this, moved)) return XmlNode();
  if(!node.m_root || node.m_root->parent != m_root) return XmlNode();
  if(moved.m_root == node.m_root) return XmlNode();

  // disable document_buffer_order optimization since moving nodes around changes document order without changing
  // buffer pointers
  Document::get_document(m_root).header |= Lumex::Xml::Constants::kxml_memory_page_contents_shared_mask;

  Lumex::Xml::Node::remove_node(moved.m_root);
  Lumex::Xml::Node::insert_node_before(moved.m_root, node.m_root);

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
  if(!m_root || !attr.get()) return false;
  if(!Utility::is_attribute_of(attr.get(), m_root)) return false;

  XmlAllocator &alloc = Memory::get_allocator(m_root);

  Lumex::Xml::Attribute::remove_attribute(attr.get(), m_root);
  Lumex::Xml::Attribute::destroy_attribute(attr.get(), alloc);

  return true;
}

inline bool
XmlNode::remove_attributes()
{
  if(!m_root) return false;

  XmlAllocator &alloc = Memory::get_allocator(m_root);

  for(XmlAttributeBase *attr = m_root->first_attribute; attr;)
  {
    XmlAttributeBase *next = attr->next_attribute;

    Lumex::Xml::Attribute::destroy_attribute(attr, alloc);

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

  XmlAllocator &alloc = Memory::get_allocator(m_root);

  Lumex::Xml::Node::remove_node(n.m_root);
  Lumex::Xml::Node::destroy_node(n.m_root, alloc);

  return true;
}

inline bool
XmlNode::remove_children()
{
  if(!m_root) return false;

  XmlAllocator &alloc = Memory::get_allocator(m_root);

  for(XmlNodeBase *cur = m_root->first_child; cur;)
  {
    XmlNodeBase *next = cur->next_sibling;

    Lumex::Xml::Node::destroy_node(cur, alloc);

    cur = next;
  }

  m_root->first_child = nullptr;

  return true;
}

inline XmlParseResult
XmlNode::append_buffer(void const *contents, size_t size, unsigned int options, xml_encoding encoding)
{
  // append_buffer is only valid for elements/documents
  if(!Utility::allow_insert_child(type(), node_element))
    return XmlParseResult(Types::xml_parse_status::status_append_invalid_root);

  // append buffer can not merge PCDATA into existing PCDATA nodes
  if((options & kparse_merge_pcdata) != 0 && last_child().type() == node_pcdata)
    return XmlParseResult(Types::xml_parse_status::status_append_invalid_root);

  // get document node
  Lumex::Xml::Document::XmlDocumentBase *doc = &Document::get_document(m_root);

  // disable document_buffer_order optimization since in a document with multiple buffers comparing buffer pointers
  // does not make sense
  doc->header |= Lumex::Xml::Constants::kxml_memory_page_contents_shared_mask;

  // get extra buffer element (we'll store the document fragment buffer there so that we can deallocate it later)
  XmlMemoryPage *page            = nullptr;
  Types::xml_extra_buffer *extra = static_cast<Types::xml_extra_buffer *>(
    doc->allocate_memory(sizeof(Types::xml_extra_buffer) + sizeof(void *), page));
  (void)page;

  if(!extra) return Text::make_parse_result(Types::xml_parse_status::status_out_of_memory);

  // add extra buffer to the list
  extra->buffer      = nullptr;
  extra->next        = doc->extra_buffers;
  doc->extra_buffers = extra;

  // name of the root has to be nullptr before parsing - otherwise closing node mismatches will not be detected at the
  // top level
  Node::name_null_sentry sentry(m_root);

  return Text::load_buffer_impl(doc, m_root, const_cast<void *>(contents), size, options, encoding, false, false,
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
    offset += iname ? Utility::strlength(iname) : 0;
  }

  string_t result;
  result.resize(offset);

  for(XmlNodeBase *j = m_root; j; j = j->parent)
  {
    if(j != m_root) result[--offset] = delimiter;

    char_t const *jname = j->name;
    if(jname)
    {
      size_t length = Utility::strlength(jname);

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
      if(jname && Utility::strequalrange(jname, path_segment, static_cast<size_t>(path_segment_end - path_segment)))
      {
        XmlNode subsearch = XmlNode(j).first_element_by_path(next_segment, delimiter);

        if(subsearch) return subsearch;
      }
    }

    return XmlNode();
  }
}

inline bool
XmlNode::traverse(XmlTreeWalker &walker)
{
  walker.m_depth = -1;

  XmlNode arg_begin(m_root);
  if(!walker.begin(arg_begin)) return false;

  XmlNodeBase *cur = m_root ? m_root->first_child + 0 : nullptr;

  if(cur)
  {
    ++walker.m_depth;

    do {
      XmlNode arg_for_each(cur);
      if(!walker.for_each(arg_for_each)) return false;

      if(cur->first_child)
      {
        ++walker.m_depth;
        cur = cur->first_child;
      }
      else if(cur->next_sibling)
        cur = cur->next_sibling;
      else
      {
        while(!cur->next_sibling && cur != m_root && cur->parent)
        {
          --walker.m_depth;
          cur = cur->parent;
        }

        if(cur != m_root) cur = cur->next_sibling;
      }
    } while(cur && cur != m_root);
  }

  LUMEX_ASSERT(walker.m_depth == -1);

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
text_output_escaped(XmlBufferedWriter &writer, char_t const *str, chartypex_t type, unsigned int flags)
{
  while(*str)
  {
    char_t const *prev = str;

    // While *s is a usual symbol
    LUMEX_XML_SCANWHILE_UNROLL(!LUMEX_XML_IS_CHARTYPEX(*str, type));

    writer.write_buffer(prev, static_cast<size_t>(str - prev));

    switch(*str)
    {
    case 0: break;
    case '&':
      writer.write('&', 'a', 'm', 'p', ';');
      ++str;
      break;
    case '<':
      writer.write('&', 'l', 't', ';');
      ++str;
      break;
    case '>':
      writer.write('&', 'g', 't', ';');
      ++str;
      break;
    case '"':
      if(flags & Constants::kformat_attribute_single_quote)
        writer.write('"');
      else
        writer.write('&', 'q', 'u', 'o', 't', ';');
      ++str;
      break;
    case '\'':
      if(flags & Constants::kformat_attribute_single_quote)
        writer.write('&', 'a', 'p', 'o', 's', ';');
      else
        writer.write('\'');
      ++str;
      break;
    default: // s is not a usual symbol
    {
      unsigned int ch = static_cast<unsigned int>(*str++);
      LUMEX_ASSERT(ch < 32);

      if(!(flags & Constants::kformat_skip_control_chars))
        writer.write('&', '#', static_cast<char_t>((ch / 10) + '0'), static_cast<char_t>((ch % 10) + '0'), ';');
    }
    }
  }
}

inline void
text_output(XmlBufferedWriter &writer, char_t const *s, chartypex_t type, unsigned int flags)
{
  if(flags & Constants::kformat_no_escapes)
    writer.write_string(s);
  else
    text_output_escaped(writer, s, type, flags);
}

inline void
text_output_cdata(XmlBufferedWriter &writer, char_t const *s)
{
  do {
    writer.write('<', '!', '[', 'C', 'D');
    writer.write('A', 'T', 'A', '[');

    char_t const *prev = s;

    // look for ]]> sequence - we can't output it as is since it terminates CDATA
    while(*s && !(s[0] == ']' && s[1] == ']' && s[2] == '>')) ++s;

    // skip ]] if we stopped at ]]>, > will go to the next CDATA section
    if(*s) s += 2;

    writer.write_buffer(prev, static_cast<size_t>(s - prev));

    writer.write(']', ']', '>');
  } while(*s);
}

inline void
text_output_indent(XmlBufferedWriter &writer, char_t const *indent, size_t indent_length, unsigned int depth)
{
  switch(indent_length)
  {
  case 1: {
    for(unsigned int i = 0; i < depth; ++i) writer.write(indent[0]);
    break;
  }

  case 2: {
    for(unsigned int i = 0; i < depth; ++i) writer.write(indent[0], indent[1]);
    break;
  }

  case 3: {
    for(unsigned int i = 0; i < depth; ++i) writer.write(indent[0], indent[1], indent[2]);
    break;
  }

  case 4: {
    for(unsigned int i = 0; i < depth; ++i) writer.write(indent[0], indent[1], indent[2], indent[3]);
    break;
  }

  default: {
    for(unsigned int i = 0; i < depth; ++i) writer.write_buffer(indent, indent_length);
  }
  }
}

inline void
node_output_comment(XmlBufferedWriter &writer, char_t const *s)
{
  writer.write('<', '!', '-', '-');

  while(*s)
  {
    char_t const *prev = s;

    // look for -\0 or -- sequence - we can't output it since -- is illegal in comment body
    while(*s && !(s[0] == '-' && (s[1] == '-' || s[1] == 0))) ++s;

    writer.write_buffer(prev, static_cast<size_t>(s - prev));

    if(*s)
    {
      LUMEX_ASSERT(*s == '-');

      writer.write('-', ' ');
      ++s;
    }
  }

  writer.write('-', '-', '>');
}

inline void
node_output_pi_value(XmlBufferedWriter &writer, char_t const *s)
{
  while(*s)
  {
    char_t const *prev = s;

    // look for ?> sequence - we can't output it since ?> terminates PI
    while(*s && !(s[0] == '?' && s[1] == '>')) ++s;

    writer.write_buffer(prev, static_cast<size_t>(s - prev));

    if(*s)
    {
      LUMEX_ASSERT(s[0] == '?' && s[1] == '>');

      writer.write('?', ' ', '>');
      s += 2;
    }
  }
}

inline void
node_output_attributes(XmlBufferedWriter &writer, XmlNodeBase *node, char_t const *indent, size_t indent_length,
                       unsigned int flags, unsigned int depth)
{
  char_t const *default_name    = LUMEX_XML_TEXT(":anonymous");
  char_t const enquotation_char = (flags & Constants::kformat_attribute_single_quote) ? '\'' : '"';

  for(XmlAttributeBase *a = node->first_attribute; a; a = a->next_attribute)
  {
    if((flags & (Constants::kformat_indent_attributes | Constants::kformat_raw))
       == Constants::kformat_indent_attributes)
    {
      writer.write('\n');

      text_output_indent(writer, indent, indent_length, depth + 1);
    }
    else { writer.write(' '); }

    writer.write_string(a->name ? a->name + 0 : default_name);
    writer.write('=', enquotation_char);

    if(a->value) text_output(writer, a->value, ctx_special_attr, flags);

    writer.write(enquotation_char);
  }
}

inline bool
node_output_start(XmlBufferedWriter &writer, XmlNodeBase *node, char_t const *indent, size_t indent_length,
                  unsigned int flags, unsigned int depth)
{
  char_t const *default_name = LUMEX_XML_TEXT(":anonymous");
  char_t const *name         = node->name ? node->name + 0 : default_name;

  writer.write('<');
  writer.write_string(name);

  if(node->first_attribute) node_output_attributes(writer, node, indent, indent_length, flags, depth);

  // element nodes can have value if parse_embed_pcdata was used
  if(!node->value)
  {
    if(!node->first_child)
    {
      if(flags & Constants::kformat_no_empty_element_tags)
      {
        writer.write('>', '<', '/');
        writer.write_string(name);
        writer.write('>');

        return false;
      }
      else
      {
        if((flags & Constants::kformat_raw) == 0) writer.write(' ');

        writer.write('/', '>');

        return false;
      }
    }
    else
    {
      writer.write('>');

      return true;
    }
  }
  else
  {
    writer.write('>');

    text_output(writer, node->value, ctx_special_pcdata, flags);

    if(!node->first_child)
    {
      writer.write('<', '/');
      writer.write_string(name);
      writer.write('>');

      return false;
    }
    else { return true; }
  }
}

inline void
node_output_end(XmlBufferedWriter &writer, XmlNodeBase *node)
{
  char_t const *default_name = LUMEX_XML_TEXT(":anonymous");
  char_t const *name         = node->name ? node->name + 0 : default_name;

  writer.write('<', '/');
  writer.write_string(name);
  writer.write('>');
}

inline void
node_output_simple(XmlBufferedWriter &writer, XmlNodeBase *node, unsigned int flags)
{
  char_t const *default_name = LUMEX_XML_TEXT(":anonymous");

  switch(LUMEX_XML_NODETYPE(node))
  {
  case node_pcdata:
    text_output(writer, node->value ? node->value + 0 : LUMEX_XML_TEXT(""), ctx_special_pcdata, flags);
    break;

  case node_cdata: text_output_cdata(writer, node->value ? node->value + 0 : LUMEX_XML_TEXT("")); break;

  case node_comment: node_output_comment(writer, node->value ? node->value + 0 : LUMEX_XML_TEXT("")); break;

  case node_pi:
    writer.write('<', '?');
    writer.write_string(node->name ? node->name + 0 : default_name);

    if(node->value)
    {
      writer.write(' ');
      node_output_pi_value(writer, node->value);
    }

    writer.write('?', '>');
    break;

  case node_declaration:
    writer.write('<', '?');
    writer.write_string(node->name ? node->name + 0 : default_name);
    node_output_attributes(writer, node, LUMEX_XML_TEXT(""), 0, flags | Constants::kformat_raw, 0);
    writer.write('?', '>');
    break;

  case node_doctype:
    writer.write('<', '!', 'D', 'O', 'C');
    writer.write('T', 'Y', 'P', 'E');

    if(node->value)
    {
      writer.write(' ');
      writer.write_string(node->value);
    }

    writer.write('>');
    break;

  default: LUMEX_ASSERT(false && "Invalid node type"); // unreachable
  }
}

inline void
node_output(XmlBufferedWriter &writer, XmlNodeBase *root, char_t const *indent, unsigned int flags, unsigned int depth)
{
  size_t indent_length      = ((flags & (Constants::kformat_indent | Constants::kformat_indent_attributes))
                          && (flags & Constants::kformat_raw) == 0)
                                ? strlength(indent)
                                : 0;
  unsigned int indent_flags = indent_indent;

  XmlNodeBase *node         = root;

  do {
    LUMEX_ASSERT(node);

    // begin writing current node
    if(LUMEX_XML_NODETYPE(node) == node_pcdata || LUMEX_XML_NODETYPE(node) == node_cdata)
    {
      node_output_simple(writer, node, flags);

      indent_flags = 0;
    }
    else
    {
      if(((indent_flags & indent_newline) != 0) && (flags & Constants::kformat_raw) == 0) writer.write('\n');

      if(((indent_flags & indent_indent) != 0) && (indent_length != 0))
        text_output_indent(writer, indent, indent_length, depth);

      if(LUMEX_XML_NODETYPE(node) == node_element)
      {
        indent_flags = indent_newline | indent_indent;

        if(node_output_start(writer, node, indent, indent_length, flags, depth))
        {
          // element nodes can have value if parse_embed_pcdata was used
          if(node->value != nullptr) indent_flags = 0;

          node = node->first_child;
          depth++;
          continue;
        }
      }
      else if(LUMEX_XML_NODETYPE(node) == node_document)
      {
        indent_flags = indent_indent;

        if(node->first_child != nullptr)
        {
          node = node->first_child;
          continue;
        }
      }
      else
      {
        node_output_simple(writer, node, flags);

        indent_flags = indent_newline | indent_indent;
      }
    }

    // continue to the next node
    while(node != root)
    {
      if(node->next_sibling != nullptr)
      {
        node = node->next_sibling;
        break;
      }

      node = node->parent;

      // write closing node
      if(LUMEX_XML_NODETYPE(node) == node_element)
      {
        depth--;

        if(((indent_flags & indent_newline) != 0) && (flags & Constants::kformat_raw) == 0) writer.write('\n');

        if(((indent_flags & indent_indent) != 0) && (indent_length != 0))
          text_output_indent(writer, indent, indent_length, depth);

        node_output_end(writer, node);

        indent_flags = indent_newline | indent_indent;
      }
    }
  } while(node != root);

  if((indent_flags & indent_newline) && (flags & Constants::kformat_raw) == 0) writer.write('\n');
}

inline void
XmlNode::print(IXmlWriter &writer, char_t const *indent, unsigned int flags, xml_encoding encoding,
               unsigned int depth) const
{
  if(m_root == nullptr) return;

  XmlBufferedWriter buffered_writer(writer, encoding);

  node_output(buffered_writer, m_root, indent, flags, depth);

  buffered_writer.flush();
}

inline void
XmlNode::print(std::basic_ostream<char> &stream, char_t const *indent, unsigned int flags, xml_encoding encoding,
               unsigned int depth) const
{
  XmlWriterStream writer(stream);

  print(writer, indent, flags, encoding, depth);
}

inline void
XmlNode::print(std::basic_ostream<wchar_t> &stream, char_t const *indent, unsigned int flags, unsigned int depth) const
{
  XmlWriterStream writer(stream);

  print(writer, indent, flags, encoding_wchar, depth);
}

inline ptrdiff_t
XmlNode::offset_debug() const
{
  if(m_root == nullptr) return -1;

  XmlDocumentBase &doc = Document::get_document(m_root);

  // we can determine the offset reliably only if there is exactly once parse buffer
  if((doc.buffer == nullptr) || (doc.extra_buffers != nullptr)) return -1;

  switch(type())
  {
  case node_document: return 0;

  case node_element: // NOLINT(bugprone-branch-clone)
  case node_declaration:
  case node_pi:
    return (m_root->name != nullptr)
               && (m_root->header & Constants::kxml_memory_page_name_allocated_or_shared_mask) == 0
             ? m_root->name - doc.buffer
             : -1;

  case node_pcdata:
  case node_cdata:
  case node_comment:
  case node_doctype:
    return (m_root->value != nullptr)
               && (m_root->header & Constants::kxml_memory_page_value_allocated_or_shared_mask) == 0
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
