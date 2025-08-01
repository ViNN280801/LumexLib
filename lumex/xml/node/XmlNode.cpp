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

#include "lumex/xml/xpath/node/XPathNode.hpp"
#include "lumex/xml/xpath/node/XPathNodeSet.hpp"
#include "lumex/xml/xpath/query/XPathQuery.hpp"
#include "lumex/xml/xpath/variable/XPathVariableSet.hpp"

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

using namespace Lumex::Xml::XPath::Node;
using namespace Lumex::Xml::XPath::Query;
using namespace Lumex::Xml::XPath::Variable;

inline bool
allow_move(XmlNode parent, XmlNode child) // NOLINT(misc-use-internal-linkage)
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

XmlNode::XmlNode() = default;

inline XmlNode::XmlNode(XmlNodeBase *ptr) : m_root(ptr) {}

inline void
unspecified_bool_xml_node(XmlNode *** /* unused */) // NOLINT(misc-use-internal-linkage)
{}

inline XmlNode::
operator XmlNode::unspecified_bool_type() const
{
  return (m_root != nullptr) ? unspecified_bool_xml_node : nullptr;
}

inline bool
XmlNode::operator!() const
{
  return m_root == nullptr;
}

inline XmlNodeIterator
XmlNode::begin() const
{
  return {(m_root != nullptr) ? m_root->first_child + 0 : nullptr, m_root};
}

inline XmlNodeIterator
XmlNode::end() const
{
  return {nullptr, m_root};
}

inline XmlAttributeIterator
XmlNode::attributes_begin() const
{
  return {m_root != nullptr ? m_root->first_attribute + 0 : nullptr, m_root};
}

inline XmlAttributeIterator
XmlNode::attributes_end() const
{
  return {nullptr, m_root};
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
  return m_root == nullptr;
}

inline char_t const *
XmlNode::name() const
{
  if(m_root == nullptr) return LUMEX_XML_TEXT("");
  char_t const *name = m_root->name;
  return (name != nullptr) ? name : LUMEX_XML_TEXT("");
}

inline xml_node_type
XmlNode::type() const
{
  return (m_root != nullptr) ? LUMEX_XML_NODETYPE(m_root) : node_null;
}

inline char_t const *
XmlNode::value() const
{
  if(m_root == nullptr) return LUMEX_XML_TEXT("");
  char_t const *value = m_root->value;
  return (value != nullptr) ? value : LUMEX_XML_TEXT("");
}

inline XmlNode
XmlNode::child(char_t const *name_) const
{
  if(m_root == nullptr) return {};

  for(XmlNodeBase *i = m_root->first_child; i != nullptr; i = i->next_sibling)
  {
    char_t const *iname = i->name;
    if((iname != nullptr) && Utility::strequal(name_, iname)) return XmlNode(i);
  }

  return {};
}

inline XmlAttribute
XmlNode::attribute(char_t const *name_) const
{
  if(m_root == nullptr) return {};

  for(XmlAttributeBase *i = m_root->first_attribute; i != nullptr; i = i->next_attribute)
  {
    char_t const *iname = i->name;
    if((iname != nullptr) && Utility::strequal(name_, iname)) return XmlAttribute(i);
  }

  return {};
}

inline XmlNode
XmlNode::next_sibling(char_t const *name_) const
{
  if(m_root == nullptr) return {};

  for(XmlNodeBase *i = m_root->next_sibling; i != nullptr; i = i->next_sibling)
  {
    char_t const *iname = i->name;
    if((iname != nullptr) && Utility::strequal(name_, iname)) return XmlNode(i);
  }

  return {};
}

inline XmlNode
XmlNode::next_sibling() const
{
  return (m_root != nullptr) ? XmlNode(m_root->next_sibling) : XmlNode();
}

inline XmlNode
XmlNode::previous_sibling(char_t const *name_) const
{
  if(m_root == nullptr) return {};

  for(XmlNodeBase *i = m_root->prev_sibling_c; i->next_sibling != nullptr; i = i->prev_sibling_c)
  {
    char_t const *iname = i->name;
    if((iname != nullptr) && Utility::strequal(name_, iname)) return XmlNode(i);
  }

  return {};
}

#if __cplusplus >= 201703L
inline XmlNode
XmlNode::child(string_view_t name_) const
{
  if(m_root == nullptr) return {};

  for(XmlNodeBase *i = m_root->first_child; i != nullptr; i = i->next_sibling)
  {
    char_t const *iname = i->name;
    if((iname != nullptr) && Utility::stringview_equal(name_, iname)) return XmlNode(i);
  }

  return {};
}

inline XmlAttribute
XmlNode::attribute(string_view_t name_) const
{
  if(m_root == nullptr) return {};

  for(XmlAttributeBase *i = m_root->first_attribute; i != nullptr; i = i->next_attribute)
  {
    char_t const *iname = i->name;
    if((iname != nullptr) && Utility::stringview_equal(name_, iname)) return XmlAttribute(i);
  }

  return {};
}

inline XmlNode
XmlNode::next_sibling(string_view_t name_) const
{
  if(m_root == nullptr) return {};

  for(XmlNodeBase *i = m_root->next_sibling; i != nullptr; i = i->next_sibling)
  {
    char_t const *iname = i->name;
    if((iname != nullptr) && Utility::stringview_equal(name_, iname)) return XmlNode(i);
  }

  return {};
}

inline XmlNode
XmlNode::previous_sibling(string_view_t name_) const
{
  if(m_root == nullptr) return {};

  for(XmlNodeBase *i = m_root->prev_sibling_c; i->next_sibling != nullptr; i = i->prev_sibling_c)
  {
    char_t const *iname = i->name;
    if((iname != nullptr) && Utility::stringview_equal(name_, iname)) return XmlNode(i);
  }

  return {};
}
#endif

inline XmlAttribute
XmlNode::attribute(const char_t *name_, XmlAttribute &hint_) const
{
  XmlAttributeBase *hint = hint_.get();

  // if hint is not an attribute of node, behavior is not defined
  LUMEX_ASSERT((hint == nullptr) || (m_root != nullptr && Utility::is_attribute_of(hint, m_root)));

  if(m_root == nullptr) return {};

  // optimistically search from hint up until the end
  for(XmlAttributeBase *i = hint; i != nullptr; i = i->next_attribute)
  {
    char_t const *iname = i->name;
    if((iname != nullptr) && Utility::strequal(name_, iname))
    {
      // update hint to maximize efficiency of searching for consecutive attributes
      hint_.set(i->next_attribute);

      return XmlAttribute(i);
    }
  }

  // wrap around and search from the first attribute until the hint
  // 'j' null pointer check is technically redundant, but it prevents a crash in case the LUMEX_ASSERTion above fails
  for(XmlAttributeBase *j = m_root->first_attribute; j != nullptr && j != hint; j = j->next_attribute)
  {
    char_t const *jname = j->name;
    if((jname != nullptr) && Utility::strequal(name_, jname))
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
  LUMEX_ASSERT((hint == nullptr) || (m_root != nullptr && Utility::is_attribute_of(hint, m_root)));

  if(m_root == nullptr) return {};

  // optimistically search from hint up until the end
  for(XmlAttributeBase *i = hint; i != nullptr; i = i->next_attribute)
  {
    char_t const *iname = i->name;
    if((iname != nullptr) && Utility::stringview_equal(name_, iname))
    {
      // update hint to maximize efficiency of searching for consecutive attributes
      hint_.set(i->next_attribute);

      return XmlAttribute(i);
    }
  }

  // wrap around and search from the first attribute until the hint
  // 'j' null pointer check is technically redundant, but it prevents a crash in case the LUMEX_ASSERTion above fails
  for(XmlAttributeBase *j = m_root->first_attribute; j != nullptr && j != hint; j = j->next_attribute)
  {
    char_t const *jname = j->name;
    if((jname != nullptr) && Utility::stringview_equal(name_, jname))
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
  if(m_root == nullptr) return {};
  XmlNodeBase *prev = m_root->prev_sibling_c;
  return (prev->next_sibling != nullptr) ? XmlNode(prev) : XmlNode();
}

inline XmlNode
XmlNode::parent() const
{
  return (m_root != nullptr) ? XmlNode(m_root->parent) : XmlNode();
}

inline XmlNode
XmlNode::root() const
{
  return (m_root != nullptr) ? XmlNode(&Document::get_document(m_root)) : XmlNode();
}

inline XmlText
XmlNode::text() const
{
  return XmlText(m_root);
}

inline char_t const *
XmlNode::child_value() const
{
  if(m_root == nullptr) return LUMEX_XML_TEXT("");

  // element nodes can have value if parse_embed_pcdata was used
  if(LUMEX_XML_NODETYPE(m_root) == node_element && m_root->value != nullptr) return m_root->value;

  for(XmlNodeBase *i = m_root->first_child; i != nullptr; i = i->next_sibling)
  {
    char_t const *ivalue = i->value;
    if(Node::is_text_node(i) && ivalue != nullptr) return ivalue;
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
  return (first != nullptr) ? XmlAttribute(first->prev_attribute_c) : XmlAttribute();
}

inline XmlNode
XmlNode::first_child() const
{
  if(m_root == nullptr) return {};
  return XmlNode(m_root->first_child);
}

inline XmlNode
XmlNode::last_child() const
{
  if(m_root == nullptr) return {};
  XmlNodeBase *first = m_root->first_child;
  return (first != nullptr) ? XmlNode(first->prev_sibling_c) : XmlNode();
}

inline bool
XmlNode::set_name(char_t const *rhs)
{
  xml_node_type type_ = (m_root != nullptr) ? LUMEX_XML_NODETYPE(m_root) : node_null;

  if(type_ != node_element && type_ != node_pi && type_ != node_declaration) return false;

  return Utility::strcpy_insitu(m_root->name, m_root->header, Constants::kxml_memory_page_name_allocated_mask, rhs,
                                Utility::strlength(rhs));
}

inline bool
XmlNode::set_name(char_t const *rhs, size_t size)
{
  xml_node_type type_ = (m_root != nullptr) ? LUMEX_XML_NODETYPE(m_root) : node_null;

  if(type_ != node_element && type_ != node_pi && type_ != node_declaration) return false;

  return Utility::strcpy_insitu(m_root->name, m_root->header, Constants::kxml_memory_page_name_allocated_mask, rhs,
                                size);
}

#if __cplusplus >= 201703L
inline bool
XmlNode::set_name(string_view_t rhs)
{
  xml_node_type type_ = (m_root != nullptr) ? LUMEX_XML_NODETYPE(m_root) : node_null;

  if(type_ != node_element && type_ != node_pi && type_ != node_declaration) return false;

  return Utility::strcpy_insitu(m_root->name, m_root->header, Constants::kxml_memory_page_name_allocated_mask,
                                rhs.data(), rhs.size());
}
#endif

inline bool
XmlNode::set_value(const char_t *rhs)
{
  xml_node_type type_ = (m_root != nullptr) ? LUMEX_XML_NODETYPE(m_root) : node_null;

  if(type_ != node_pcdata && type_ != node_cdata && type_ != node_comment && type_ != node_pi && type_ != node_doctype)
    return false;

  return Utility::strcpy_insitu(m_root->value, m_root->header, Constants::kxml_memory_page_value_allocated_mask, rhs,
                                Utility::strlength(rhs));
}

inline bool
XmlNode::set_value(char_t const *rhs, size_t size)
{
  xml_node_type type_ = (m_root != nullptr) ? LUMEX_XML_NODETYPE(m_root) : node_null;

  if(type_ != node_pcdata && type_ != node_cdata && type_ != node_comment && type_ != node_pi && type_ != node_doctype)
    return false;

  return Utility::strcpy_insitu(m_root->value, m_root->header, Constants::kxml_memory_page_value_allocated_mask, rhs,
                                size);
}

#if __cplusplus >= 201703L
inline bool
XmlNode::set_value(string_view_t rhs)
{
  xml_node_type type_ = (m_root != nullptr) ? LUMEX_XML_NODETYPE(m_root) : node_null;

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

  XmlAttribute newAttr(Lumex::Xml::Attribute::allocate_attribute(alloc));
  if(!newAttr) return {};

  Lumex::Xml::Attribute::append_attribute(newAttr.get(), m_root);

  newAttr.set_name(name_);

  return newAttr;
}

inline XmlAttribute
XmlNode::prepend_attribute(char_t const *name_)
{
  if(!Utility::allow_insert_attribute(type())) return {};

  XmlAllocator &alloc = Memory::get_allocator(m_root);

  XmlAttribute attr(Lumex::Xml::Attribute::allocate_attribute(alloc));
  if(!attr) return {};

  Lumex::Xml::Attribute::prepend_attribute(attr.get(), m_root);

  attr.set_name(name_);

  return attr;
}

inline XmlAttribute
XmlNode::insert_attribute_after(char_t const *name_, XmlAttribute const &attr)
{
  if(!Utility::allow_insert_attribute(type())) return {};
  if(!attr || !Utility::is_attribute_of(attr.get(), m_root)) return {};

  XmlAllocator &alloc = Memory::get_allocator(m_root);

  XmlAttribute newAttr(Lumex::Xml::Attribute::allocate_attribute(alloc));
  if(!newAttr) return {};

  Lumex::Xml::Attribute::insert_attribute_after(newAttr.get(), attr.get(), m_root);

  newAttr.set_name(name_);

  return newAttr;
}

inline XmlAttribute
XmlNode::insert_attribute_before(char_t const *name_, XmlAttribute const &attr)
{
  if(!Utility::allow_insert_attribute(type())) return {};
  if(!attr || !Utility::is_attribute_of(attr.get(), m_root)) return {};

  XmlAllocator &alloc = Memory::get_allocator(m_root);

  XmlAttribute newAttr(Lumex::Xml::Attribute::allocate_attribute(alloc));
  if(!newAttr) return {};

  Lumex::Xml::Attribute::insert_attribute_before(newAttr.get(), attr.get(), m_root);

  newAttr.set_name(name_);

  return newAttr;
}

#if __cplusplus >= 201703L
inline XmlAttribute
XmlNode::append_attribute(string_view_t name_)
{
  if(!Utility::allow_insert_attribute(type())) return {};

  XmlAllocator &alloc = Memory::get_allocator(m_root);

  XmlAttribute newAttr(Lumex::Xml::Attribute::allocate_attribute(alloc));
  if(!newAttr) return {};

  Lumex::Xml::Attribute::append_attribute(newAttr.get(), m_root);

  newAttr.set_name(name_);

  return newAttr;
}

inline XmlAttribute
XmlNode::prepend_attribute(string_view_t name_)
{
  if(!Utility::allow_insert_attribute(type())) return {};

  XmlAllocator &alloc = Memory::get_allocator(m_root);

  XmlAttribute newAttr(Lumex::Xml::Attribute::allocate_attribute(alloc));
  if(!newAttr) return {};

  Lumex::Xml::Attribute::prepend_attribute(newAttr.get(), m_root);

  newAttr.set_name(name_);

  return newAttr;
}

inline XmlAttribute
XmlNode::insert_attribute_after(string_view_t name_, XmlAttribute const &attr)
{
  if(!Utility::allow_insert_attribute(type())) return {};
  if(!attr || !Utility::is_attribute_of(attr.get(), m_root)) return {};

  XmlAllocator &alloc = Memory::get_allocator(m_root);

  XmlAttribute newAttr(Lumex::Xml::Attribute::allocate_attribute(alloc));
  if(!newAttr) return {};

  Lumex::Xml::Attribute::insert_attribute_after(newAttr.get(), attr.get(), m_root);

  newAttr.set_name(name_);

  return newAttr;
}

inline XmlAttribute
XmlNode::insert_attribute_before(string_view_t name_, XmlAttribute const &attr)
{
  if(!Utility::allow_insert_attribute(type())) return {};
  if(!attr || !Utility::is_attribute_of(attr.get(), m_root)) return {};

  XmlAllocator &alloc = Memory::get_allocator(m_root);

  XmlAttribute newAttr(Lumex::Xml::Attribute::allocate_attribute(alloc));
  if(!newAttr) return {};

  Lumex::Xml::Attribute::insert_attribute_before(newAttr.get(), attr.get(), m_root);

  newAttr.set_name(name_);

  return newAttr;
}
#endif

inline XmlAttribute
XmlNode::append_copy(const XmlAttribute &proto)
{
  if(!proto) return {};
  if(!Utility::allow_insert_attribute(type())) return {};

  XmlAllocator &alloc = Memory::get_allocator(m_root);

  XmlAttribute newAttr(Lumex::Xml::Attribute::allocate_attribute(alloc));
  if(!newAttr) return {};

  Lumex::Xml::Attribute::append_attribute(newAttr.get(), m_root);
  Lumex::Xml::Attribute::node_copy_attribute(newAttr.get(), proto.get());

  return newAttr;
}

inline XmlAttribute
XmlNode::prepend_copy(XmlAttribute const &proto)
{
  if(!proto) return {};
  if(!Utility::allow_insert_attribute(type())) return {};

  XmlAllocator &alloc = Memory::get_allocator(m_root);

  XmlAttribute newAttr(Lumex::Xml::Attribute::allocate_attribute(alloc));
  if(!newAttr) return {};

  Lumex::Xml::Attribute::prepend_attribute(newAttr.get(), m_root);
  Lumex::Xml::Attribute::node_copy_attribute(newAttr.get(), proto.get());

  return newAttr;
}

inline XmlAttribute
XmlNode::insert_copy_after(XmlAttribute const &proto, XmlAttribute const &attr)
{
  if(!proto) return {};
  if(!Utility::allow_insert_attribute(type())) return {};
  if(!attr || !Utility::is_attribute_of(attr.get(), m_root)) return {};

  XmlAllocator &alloc = Memory::get_allocator(m_root);

  XmlAttribute newAttr(Lumex::Xml::Attribute::allocate_attribute(alloc));
  if(!newAttr) return {};

  Lumex::Xml::Attribute::insert_attribute_after(newAttr.get(), attr.get(), m_root);
  Lumex::Xml::Attribute::node_copy_attribute(newAttr.get(), proto.get());

  return newAttr;
}

inline XmlAttribute
XmlNode::insert_copy_before(XmlAttribute const &proto, XmlAttribute const &attr)
{
  if(!proto) return {};
  if(!Utility::allow_insert_attribute(type())) return {};
  if(!attr || !Utility::is_attribute_of(attr.get(), m_root)) return {};

  XmlAllocator &alloc = Memory::get_allocator(m_root);

  XmlAttribute newAttr(Lumex::Xml::Attribute::allocate_attribute(alloc));
  if(!newAttr) return {};

  Lumex::Xml::Attribute::insert_attribute_before(newAttr.get(), attr.get(), m_root);
  Lumex::Xml::Attribute::node_copy_attribute(newAttr.get(), proto.get());

  return newAttr;
}

inline XmlNode
XmlNode::append_child(xml_node_type type_)
{
  if(!Utility::allow_insert_child(type(), type_)) return {};

  XmlAllocator &alloc = Memory::get_allocator(m_root);

  XmlNode newNode(Lumex::Xml::Node::allocate_node(alloc, type_));
  if(!newNode) return {};

  Lumex::Xml::Node::append_node(newNode.m_root, m_root);

  if(type_ == node_declaration) newNode.set_name(LUMEX_XML_TEXT("xml"));

  return newNode;
}

inline XmlNode
XmlNode::prepend_child(xml_node_type type_)
{
  if(!Utility::allow_insert_child(type(), type_)) return {};

  XmlAllocator &alloc = Memory::get_allocator(m_root);

  XmlNode newNode(Lumex::Xml::Node::allocate_node(alloc, type_));
  if(!newNode) return {};

  Lumex::Xml::Node::prepend_node(newNode.m_root, m_root);

  if(type_ == node_declaration) newNode.set_name(LUMEX_XML_TEXT("xml"));

  return newNode;
}

inline XmlNode
XmlNode::insert_child_before(xml_node_type type_, XmlNode const &node)
{
  if(!Utility::allow_insert_child(type(), type_)) return {};
  if((node.m_root == nullptr) || node.m_root->parent != m_root) return {};

  XmlAllocator &alloc = Memory::get_allocator(m_root);

  XmlNode newNode(Lumex::Xml::Node::allocate_node(alloc, type_));
  if(!newNode) return {};

  Lumex::Xml::Node::insert_node_before(newNode.m_root, node.m_root);

  if(type_ == node_declaration) newNode.set_name(LUMEX_XML_TEXT("xml"));

  return newNode;
}

inline XmlNode
XmlNode::insert_child_after(xml_node_type type_, XmlNode const &node)
{
  if(!Utility::allow_insert_child(type(), type_)) return {};
  if((node.m_root == nullptr) || node.m_root->parent != m_root) return {};

  XmlAllocator &alloc = Memory::get_allocator(m_root);

  XmlNode newNode(Lumex::Xml::Node::allocate_node(alloc, type_));
  if(!newNode) return {};

  Lumex::Xml::Node::insert_node_after(newNode.m_root, node.m_root);

  if(type_ == node_declaration) newNode.set_name(LUMEX_XML_TEXT("xml"));

  return newNode;
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
XmlNode::append_copy(XmlNode const &proto)
{
  xml_node_type type_ = proto.type();
  if(!Utility::allow_insert_child(type(), type_)) return {};

  XmlAllocator &alloc = Memory::get_allocator(m_root);

  XmlNode newNode(Lumex::Xml::Node::allocate_node(alloc, type_));
  if(!newNode) return {};

  Lumex::Xml::Node::append_node(newNode.m_root, m_root);
  Lumex::Xml::Node::node_copy_tree(newNode.m_root, proto.m_root);

  return newNode;
}

inline XmlNode
XmlNode::prepend_copy(XmlNode const &proto)
{
  xml_node_type type_ = proto.type();
  if(!Utility::allow_insert_child(type(), type_)) return {};

  XmlAllocator &alloc = Memory::get_allocator(m_root);

  XmlNode newNode(Lumex::Xml::Node::allocate_node(alloc, type_));
  if(!newNode) return {};

  Lumex::Xml::Node::prepend_node(newNode.m_root, m_root);
  Lumex::Xml::Node::node_copy_tree(newNode.m_root, proto.m_root);

  return newNode;
}

inline XmlNode
XmlNode::insert_copy_after(XmlNode const &proto, XmlNode const &node)
{
  xml_node_type type_ = proto.type();
  if(!Utility::allow_insert_child(type(), type_)) return {};
  if((node.m_root == nullptr) || node.m_root->parent != m_root) return {};

  XmlAllocator &alloc = Memory::get_allocator(m_root);

  XmlNode newNode(Lumex::Xml::Node::allocate_node(alloc, type_));
  if(!newNode) return {};

  Lumex::Xml::Node::insert_node_after(newNode.m_root, node.m_root);
  Lumex::Xml::Node::node_copy_tree(newNode.m_root, proto.m_root);

  return newNode;
}

inline XmlNode
XmlNode::insert_copy_before(XmlNode const &proto, XmlNode const &node)
{
  xml_node_type type_ = proto.type();
  if(!Utility::allow_insert_child(type(), type_)) return {};
  if((node.m_root == nullptr) || node.m_root->parent != m_root) return {};

  XmlAllocator &alloc = Memory::get_allocator(m_root);

  XmlNode newNode(Lumex::Xml::Node::allocate_node(alloc, type_));
  if(!newNode) return {};

  Lumex::Xml::Node::insert_node_before(newNode.m_root, node.m_root);
  Lumex::Xml::Node::node_copy_tree(newNode.m_root, proto.m_root);

  return newNode;
}

inline XmlNode
XmlNode::append_move(XmlNode const &moved)
{
  if(!allow_move(*this, moved)) return {};

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
  if(!allow_move(*this, moved)) return {};

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
  if(!allow_move(*this, moved)) return {};
  if((node.m_root == nullptr) || node.m_root->parent != m_root) return {};
  if(moved.m_root == node.m_root) return {};

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
  if(!allow_move(*this, moved)) return {};
  if((node.m_root == nullptr) || node.m_root->parent != m_root) return {};
  if(moved.m_root == node.m_root) return {};

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
  if((m_root == nullptr) || (attr.get() == nullptr)) return false;
  if(!Utility::is_attribute_of(attr.get(), m_root)) return false;

  XmlAllocator &alloc = Memory::get_allocator(m_root);

  Lumex::Xml::Attribute::remove_attribute(attr.get(), m_root);
  Lumex::Xml::Attribute::destroy_attribute(attr.get(), alloc);

  return true;
}

inline bool
XmlNode::remove_attributes()
{
  if(m_root == nullptr) return false;

  XmlAllocator &alloc = Memory::get_allocator(m_root);

  for(XmlAttributeBase *attr = m_root->first_attribute; attr != nullptr;)
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
XmlNode::remove_child(XmlNode const &n)
{
  if((m_root == nullptr) || (n.m_root == nullptr) || n.m_root->parent != m_root) return false;

  XmlAllocator &alloc = Memory::get_allocator(m_root);

  Lumex::Xml::Node::remove_node(n.m_root);
  Lumex::Xml::Node::destroy_node(n.m_root, alloc);

  return true;
}

inline bool
XmlNode::remove_children()
{
  if(m_root == nullptr) return false;

  XmlAllocator &alloc = Memory::get_allocator(m_root);

  for(XmlNodeBase *cur = m_root->first_child; cur != nullptr;)
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
  if(!Utility::allow_insert_child(type(), node_element)) return {Types::xml_parse_status::status_append_invalid_root};

  // append buffer can not merge PCDATA into existing PCDATA nodes
  if((options & kparse_merge_pcdata) != 0 && last_child().type() == node_pcdata)
    return {Types::xml_parse_status::status_append_invalid_root};

  // get document node
  Lumex::Xml::Document::XmlDocumentBase *doc = &Document::get_document(m_root);

  // disable document_buffer_order optimization since in a document with multiple buffers comparing buffer pointers
  // does not make sense
  doc->header |= Lumex::Xml::Constants::kxml_memory_page_contents_shared_mask;

  // get extra buffer element (we'll store the document fragment buffer there so that we can deallocate it later)
  XmlMemoryPage *page = nullptr;
  auto *extra         = static_cast<Types::xml_extra_buffer *>(
    doc->allocate_memory(sizeof(Types::xml_extra_buffer) + sizeof(void *), page));
  (void)page;

  if(extra == nullptr) return Text::make_parse_result(Types::xml_parse_status::status_out_of_memory);

  // add extra buffer to the list
  extra->buffer      = nullptr;
  extra->next        = doc->extra_buffers;
  doc->extra_buffers = extra;

  // name of the root has to be nullptr before parsing - otherwise closing node mismatches will not be detected at the
  // top level
  Node::name_null_sentry sentry(m_root);

  return Text::load_buffer_impl(doc, m_root,
                                const_cast< // NOLINT(cppcoreguidelines-pro-type-const-cast)
                                  void *>(contents),
                                size, options, encoding, false, false, &extra->buffer);
}

inline XmlNode
XmlNode::find_child_by_attribute(char_t const *name_, char_t const *attr_name, char_t const *attr_value) const
{
  if(m_root == nullptr) return {};

  for(XmlNodeBase *i = m_root->first_child; i != nullptr; i = i->next_sibling)
  {
    char_t const *iname = i->name;
    if((iname != nullptr) && Utility::strequal(name_, iname))
    {
      for(XmlAttributeBase *newAttr = i->first_attribute; newAttr != nullptr; newAttr = newAttr->next_attribute)
      {
        char_t const *aname = newAttr->name;
        if((aname != nullptr) && Utility::strequal(attr_name, aname))
        {
          char_t const *avalue = newAttr->value;
          if(Utility::strequal(attr_value, avalue != nullptr ? avalue : LUMEX_XML_TEXT(""))) return XmlNode(i);
        }
      }
    }
  }

  return {};
}

inline XmlNode
XmlNode::find_child_by_attribute(char_t const *attr_name, char_t const *attr_value) const
{
  if(m_root == nullptr) return {};

  for(XmlNodeBase *i = m_root->first_child; i != nullptr; i = i->next_sibling)
    for(XmlAttributeBase *newAttr = i->first_attribute; newAttr != nullptr; newAttr = newAttr->next_attribute)
    {
      char_t const *aname = newAttr->name;
      if((aname != nullptr) && Utility::strequal(attr_name, aname))
      {
        char_t const *avalue = newAttr->value;
        if(Utility::strequal(attr_value, avalue != nullptr ? avalue : LUMEX_XML_TEXT(""))) return XmlNode(i);
      }
    }

  return {};
}

inline string_t
XmlNode::path(char_t delimiter) const
{
  if(m_root == nullptr) return {};

  size_t offset = 0;

  for(XmlNodeBase *i = m_root; i != nullptr; i = i->parent)
  {
    char_t const *iname = i->name;
    offset += static_cast<size_t>(i != m_root);
    offset += (iname != nullptr) ? Utility::strlength(iname) : 0;
  }

  string_t result;
  result.resize(offset);

  for(XmlNodeBase *j = m_root; j != nullptr; j = j->parent)
  {
    if(j != m_root) result[--offset] = delimiter;

    char_t const *jname = j->name;
    if(jname != nullptr)
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
XmlNode::first_element_by_path(char_t const *path_, char_t delimiter) const // NOLINT(misc-no-recursion)
{
  XmlNode context = path_[0] == delimiter ? root() : *this;

  if(context.m_root == nullptr) return {};

  char_t const *path_segment = path_;

  while(*path_segment == delimiter) ++path_segment;

  char_t const *path_segment_end = path_segment;

  while((*path_segment_end != 0) && *path_segment_end != delimiter) ++path_segment_end;

  if(path_segment == path_segment_end) return context;

  char_t const *next_segment = path_segment_end;

  while(*next_segment == delimiter) ++next_segment;

  if(*path_segment == '.' && path_segment + 1 == path_segment_end)
    return context.first_element_by_path(next_segment, delimiter);

  if(*path_segment == '.' && *(path_segment + 1) == '.' && path_segment + 2 == path_segment_end)
    return context.parent().first_element_by_path(next_segment, delimiter);

  for(XmlNodeBase *j = context.m_root->first_child; j != nullptr; j = j->next_sibling)
  {
    char_t const *jname = j->name;
    if((jname != nullptr)
       && Utility::strequalrange(jname, path_segment, static_cast<size_t>(path_segment_end - path_segment)))
    {
      XmlNode subsearch = XmlNode(j).first_element_by_path(next_segment, delimiter);

      if(subsearch != nullptr) return subsearch;
    }
  }

  return {};
}

inline bool
XmlNode::traverse(XmlTreeWalker &walker)
{
  walker.m_depth = -1;

  XmlNode arg_begin(m_root);
  if(!walker.begin(arg_begin)) return false;

  XmlNodeBase *cur = (m_root != nullptr) ? m_root->first_child + 0 : nullptr;

  if(cur != nullptr)
  {
    ++walker.m_depth;

    do { // NOLINT(cppcoreguidelines-avoid-do-while)
      XmlNode arg_for_each(cur);
      if(!walker.for_each(arg_for_each)) return false;

      if(cur->first_child != nullptr)
      {
        ++walker.m_depth;
        cur = cur->first_child;
      }
      else if(cur->next_sibling != nullptr)
        cur = cur->next_sibling;
      else
      {
        while((cur->next_sibling == nullptr) && cur != m_root && (cur->parent != nullptr))
        {
          --walker.m_depth;
          cur = cur->parent;
        }

        if(cur != m_root) cur = cur->next_sibling;
      }
    } while((cur != nullptr) && cur != m_root);
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
text_output_escaped( // NOLINT(misc-use-internal-linkage, readability-function-cognitive-complexity)
  XmlBufferedWriter &writer, char_t const *str, chartypex_t type, unsigned int flags)
{
  while(*str != 0)
  {
    char_t const *prev = str;

    // While *s is a usual symbol
    LUMEX_XML_SCANWHILE_UNROLL( // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index,
                                // readability-identifier-length)
      !LUMEX_XML_IS_CHARTYPEX(*str, type));

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
      if((flags & Constants::kformat_attribute_single_quote) != 0)
        writer.write('"');
      else
        writer.write('&', 'q', 'u', 'o', 't', ';');
      ++str;
      break;
    case '\'':
      if((flags & Constants::kformat_attribute_single_quote) != 0)
        writer.write('&', 'a', 'p', 'o', 's', ';');
      else
        writer.write('\'');
      ++str;
      break;
    default: // s is not a usual symbol
    {
      auto chr = static_cast<unsigned int>(*str++); // NOLINT(bugprone-signed-char-misuse)
      LUMEX_ASSERT(chr < 32);

      if((flags & Constants::kformat_skip_control_chars) == 0)
        writer.write('&', '#', static_cast<char_t>((chr / 10) + '0'), static_cast<char_t>((chr % 10) + '0'), ';');
    }
    }
  }
}

inline void
text_output(XmlBufferedWriter &writer, char_t const *str, // NOLINT(misc-use-internal-linkage)
            chartypex_t type, unsigned int flags)
{
  if((flags & Constants::kformat_no_escapes) != 0)
    writer.write_string(str);
  else
    text_output_escaped(writer, str, type, flags);
}

inline void
text_output_cdata(XmlBufferedWriter &writer, char_t const *str) // NOLINT(misc-use-internal-linkage)
{
  do { // NOLINT(cppcoreguidelines-avoid-do-while)
    writer.write('<', '!', '[', 'C', 'D');
    writer.write('A', 'T', 'A', '[');

    char_t const *prev = str;

    // look for ]]> sequence - we can't output it as is since it terminates CDATA
    while((*str != 0) && (str[0] != ']' || str[1] != ']' || str[2] != '>')) ++str;

    // skip ]] if we stopped at ]]>, > will go to the next CDATA section
    if(*str != 0) str += 2;

    writer.write_buffer(prev, static_cast<size_t>(str - prev));

    writer.write(']', ']', '>');
  } while(*str != 0);
}

inline void
text_output_indent(XmlBufferedWriter &writer,                  // NOLINT(misc-use-internal-linkage)
                   char_t const *indent, size_t indent_length, // NOLINT(bugprone-easily-swappable-parameters)
                   unsigned int depth)
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
node_output_comment(XmlBufferedWriter &writer, char_t const *str) // NOLINT(misc-use-internal-linkage)
{
  writer.write('<', '!', '-', '-');

  while(*str != 0)
  {
    char_t const *prev = str;

    // look for -\0 or -- sequence - we can't output it since -- is illegal in comment body
    while((*str != 0) && (str[0] != '-' || (str[1] != '-' && str[1] != 0))) ++str;

    writer.write_buffer(prev, static_cast<size_t>(str - prev));

    if(*str != 0)
    {
      LUMEX_ASSERT(*str == '-');

      writer.write('-', ' ');
      ++str;
    }
  }

  writer.write('-', '-', '>');
}

inline void
node_output_pi_value(XmlBufferedWriter &writer, char_t const *str) // NOLINT(misc-use-internal-linkage)
{
  while(*str != 0)
  {
    char_t const *prev = str;

    // look for ?> sequence - we can't output it since ?> terminates PI
    while((*str != 0) && (str[0] != '?' || str[1] != '>')) ++str;

    writer.write_buffer(prev, static_cast<size_t>(str - prev));

    if(*str != 0)
    {
      LUMEX_ASSERT(str[0] == '?' && str[1] == '>');

      writer.write('?', ' ', '>');
      str += 2;
    }
  }
}

inline void
node_output_attributes(XmlBufferedWriter &writer, // NOLINT(misc-use-internal-linkage)
                       XmlNodeBase *node, char_t const *indent,
                       size_t indent_length, // NOLINT(bugprone-easily-swappable-parameters)
                       unsigned int flags, unsigned int depth)
{
  char_t const *default_name    = LUMEX_XML_TEXT(":anonymous");
  char_t const enquotation_char = ((flags & Constants::kformat_attribute_single_quote) != 0) ? '\'' : '"';

  for(XmlAttributeBase *attr = node->first_attribute; attr != nullptr; attr = attr->next_attribute)
  {
    if((flags & (Constants::kformat_indent_attributes | Constants::kformat_raw))
       == Constants::kformat_indent_attributes)
    {
      writer.write('\n');

      text_output_indent(writer, indent, indent_length, depth + 1);
    }
    else { writer.write(' '); }

    writer.write_string((attr->name != nullptr) ? attr->name + 0 : default_name);
    writer.write('=', enquotation_char);

    if(attr->value != nullptr) text_output(writer, attr->value, ctx_special_attr, flags);

    writer.write(enquotation_char);
  }
}

inline bool
node_output_start(XmlBufferedWriter &writer, // NOLINT(misc-use-internal-linkage)
                  XmlNodeBase *node, char_t const *indent, size_t indent_length, unsigned int flags, unsigned int depth)
{
  char_t const *default_name = LUMEX_XML_TEXT(":anonymous");
  char_t const *name         = (node->name != nullptr) ? node->name + 0 : default_name;

  writer.write('<');
  writer.write_string(name);

  if(node->first_attribute != nullptr) node_output_attributes(writer, node, indent, indent_length, flags, depth);

  // element nodes can have value if parse_embed_pcdata was used
  if(node->value == nullptr)
  {
    if(node->first_child == nullptr)
    {
      if((flags & Constants::kformat_no_empty_element_tags) != 0)
      {
        writer.write('>', '<', '/');
        writer.write_string(name);
        writer.write('>');

        return false;
      }

      if((flags & Constants::kformat_raw) == 0) writer.write(' ');

      writer.write('/', '>');

      return false;
    }
    writer.write('>');
    return true;
  }

  writer.write('>');

  text_output(writer, node->value, ctx_special_pcdata, flags);

  if(node->first_child == nullptr)
  {
    writer.write('<', '/');
    writer.write_string(name);
    writer.write('>');

    return false;
  }
  return true;
}

inline void
node_output_end(XmlBufferedWriter &writer, XmlNodeBase *node) // NOLINT(misc-use-internal-linkage)
{
  char_t const *default_name = LUMEX_XML_TEXT(":anonymous");
  char_t const *name         = (node->name != nullptr) ? node->name + 0 : default_name;

  writer.write('<', '/');
  writer.write_string(name);
  writer.write('>');
}

inline void
node_output_simple(XmlBufferedWriter &writer, // NOLINT(misc-use-internal-linkage)
                   XmlNodeBase *node, unsigned int flags)
{
  char_t const *default_name = LUMEX_XML_TEXT(":anonymous");

  switch(LUMEX_XML_NODETYPE(node))
  {
  case node_pcdata:
    text_output(writer, (node->value != nullptr) ? node->value + 0 : LUMEX_XML_TEXT(""), ctx_special_pcdata, flags);
    break;

  case node_cdata: text_output_cdata(writer, (node->value != nullptr) ? node->value + 0 : LUMEX_XML_TEXT("")); break;

  case node_comment:
    node_output_comment(writer, (node->value != nullptr) ? node->value + 0 : LUMEX_XML_TEXT(""));
    break;

  case node_pi:
    writer.write('<', '?');
    writer.write_string((node->name != nullptr) ? node->name + 0 : default_name);

    if(node->value != nullptr)
    {
      writer.write(' ');
      node_output_pi_value(writer, node->value);
    }

    writer.write('?', '>');
    break;

  case node_declaration:
    writer.write('<', '?');
    writer.write_string((node->name != nullptr) ? node->name + 0 : default_name);
    node_output_attributes(writer, node, LUMEX_XML_TEXT(""), 0, flags | Constants::kformat_raw, 0);
    writer.write('?', '>');
    break;

  case node_doctype:
    writer.write('<', '!', 'D', 'O', 'C');
    writer.write('T', 'Y', 'P', 'E');

    if(node->value != nullptr)
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
node_output(XmlBufferedWriter &writer, // NOLINT(misc-use-internal-linkage, readability-function-cognitive-complexity)
            XmlNodeBase *root, char_t const *indent, unsigned int flags, unsigned int depth)
{
  size_t indent_length      = (((flags & (Constants::kformat_indent | Constants::kformat_indent_attributes)) != 0)
                          && (flags & Constants::kformat_raw) == 0)
                                ? strlength(indent)
                                : 0;
  unsigned int indent_flags = indent_indent;

  XmlNodeBase *node         = root;

  do { // NOLINT(cppcoreguidelines-avoid-do-while)
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

  if(((indent_flags & indent_newline) != 0) && (flags & Constants::kformat_raw) == 0) writer.write('\n');
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

inline XPathNode
XmlNode::select_node(char_t const *query, XPathVariableSet *variables) const
{
  XPathQuery query_obj(query, variables);
  return query_obj.evaluate_node(*this);
}

inline XPathNode
XmlNode::select_node(XPathQuery const &query) const
{
  return query.evaluate_node(*this);
}

inline XPathNodeSet
XmlNode::select_nodes(char_t const *query, XPathVariableSet *variables) const
{
  XPathQuery query_obj(query, variables);
  return query_obj.evaluate_node_set(*this);
}

inline XPathNodeSet
XmlNode::select_nodes(XPathQuery const &query) const
{
  return query.evaluate_node_set(*this);
}

inline XPathNode
XmlNode::select_single_node(char_t const *query, XPathVariableSet *variables) const
{
  XPathQuery query_obj(query, variables);
  return query_obj.evaluate_node(*this);
}

inline XPathNode
XmlNode::select_single_node(XPathQuery const &query) const
{
  return query.evaluate_node(*this);
}
