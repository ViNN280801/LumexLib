#define LUMEX_IMPLEMENTATION

#include "lumex/xml/attribute/XmlAttributeIterator.hpp"
#include "lumex/xml/document/XmlDocumentBase.hpp"
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

LUMEX_PUBLIC_API
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

LUMEX_PUBLIC_API
XmlNode::XmlNode() = default;

LUMEX_PUBLIC_API
inline XmlNode::XmlNode(XmlNodeBase *ptr) : m_root(ptr) {}

LUMEX_PUBLIC_API
inline void
unspecified_bool_xml_node(XmlNode *** /* unused */) // NOLINT(misc-use-internal-linkage)
{}

LUMEX_PUBLIC_API
inline XmlNode::
operator XmlNode::unspecified_bool_type() const
{
  return (m_root != nullptr) ? unspecified_bool_xml_node : nullptr;
}

LUMEX_PUBLIC_API
inline bool
XmlNode::operator!() const
{
  return m_root == nullptr;
}

LUMEX_PUBLIC_API
inline XmlNodeIterator
XmlNode::begin() const
{
  return {(m_root != nullptr) ? m_root->first_child + 0 : nullptr, m_root};
}

LUMEX_PUBLIC_API
inline XmlNodeIterator
XmlNode::end() const
{
  return {nullptr, m_root};
}

LUMEX_PUBLIC_API
inline XmlAttributeIterator
XmlNode::attributes_begin() const
{
  return {m_root != nullptr ? m_root->first_attribute + 0 : nullptr, m_root};
}

LUMEX_PUBLIC_API
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

LUMEX_PUBLIC_API
inline bool
XmlNode::operator==(XmlNode const &other) const
{
  return (m_root == other.m_root);
}

LUMEX_PUBLIC_API
inline bool
XmlNode::operator!=(XmlNode const &other) const
{
  return (m_root != other.m_root);
}

LUMEX_PUBLIC_API
inline bool
XmlNode::operator<(XmlNode const &other) const
{
  return (m_root < other.m_root);
}

LUMEX_PUBLIC_API
inline bool
XmlNode::operator>(XmlNode const &other) const
{
  return (m_root > other.m_root);
}

LUMEX_PUBLIC_API
inline bool
XmlNode::operator<=(XmlNode const &other) const
{
  return (m_root <= other.m_root);
}

LUMEX_PUBLIC_API
inline bool
XmlNode::operator>=(XmlNode const &other) const
{
  return (m_root >= other.m_root);
}

LUMEX_PUBLIC_API
inline bool
XmlNode::empty() const
{
  return m_root == nullptr;
}

LUMEX_PUBLIC_API
inline char_t const *
XmlNode::name() const
{
  if(m_root == nullptr) return LUMEX_XML_TEXT("");
  char_t const *name = m_root->name;
  return (name != nullptr) ? name : LUMEX_XML_TEXT("");
}

LUMEX_PUBLIC_API
inline xml_node_type
XmlNode::type() const
{
  return (m_root != nullptr) ? LUMEX_XML_NODETYPE(m_root) : node_null;
}

LUMEX_PUBLIC_API
inline char_t const *
XmlNode::value() const
{
  if(m_root == nullptr) return LUMEX_XML_TEXT("");
  char_t const *value = m_root->value;
  return (value != nullptr) ? value : LUMEX_XML_TEXT("");
}

LUMEX_PUBLIC_API
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

LUMEX_PUBLIC_API
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

LUMEX_PUBLIC_API
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

LUMEX_PUBLIC_API
inline XmlNode
XmlNode::next_sibling() const
{
  return (m_root != nullptr) ? XmlNode(m_root->next_sibling) : XmlNode();
}

LUMEX_PUBLIC_API
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
LUMEX_PUBLIC_API
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

LUMEX_PUBLIC_API
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

LUMEX_PUBLIC_API
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

LUMEX_PUBLIC_API
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

LUMEX_PUBLIC_API
inline XmlAttribute
XmlNode::attribute(char_t const *name_, XmlAttribute &hint_) const
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
LUMEX_PUBLIC_API
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

LUMEX_PUBLIC_API
inline XmlNode
XmlNode::previous_sibling() const
{
  if(m_root == nullptr) return {};
  XmlNodeBase *prev = m_root->prev_sibling_c;
  return (prev->next_sibling != nullptr) ? XmlNode(prev) : XmlNode();
}

LUMEX_PUBLIC_API
inline XmlNode
XmlNode::parent() const
{
  return (m_root != nullptr) ? XmlNode(m_root->parent) : XmlNode();
}

LUMEX_PUBLIC_API
inline XmlNode
XmlNode::root() const
{
  return (m_root != nullptr) ? XmlNode(&Document::get_document(m_root)) : XmlNode();
}

LUMEX_PUBLIC_API
inline XmlText
XmlNode::text() const
{
  return XmlText(m_root);
}

LUMEX_PUBLIC_API
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

LUMEX_PUBLIC_API
inline char_t const *
XmlNode::child_value(char_t const *name_) const
{
  return child(name_).child_value();
}

LUMEX_PUBLIC_API
inline XmlAttribute
XmlNode::first_attribute() const
{
  if(m_root == nullptr) return {};
  return XmlAttribute(m_root->first_attribute);
}

LUMEX_PUBLIC_API
inline XmlAttribute
XmlNode::last_attribute() const
{
  if(m_root == nullptr) return {};
  XmlAttributeBase *first = m_root->first_attribute;
  return (first != nullptr) ? XmlAttribute(first->prev_attribute_c) : XmlAttribute();
}

LUMEX_PUBLIC_API
inline XmlNode
XmlNode::first_child() const
{
  if(m_root == nullptr) return {};
  return XmlNode(m_root->first_child);
}

LUMEX_PUBLIC_API
inline XmlNode
XmlNode::last_child() const
{
  if(m_root == nullptr) return {};
  XmlNodeBase *first = m_root->first_child;
  return (first != nullptr) ? XmlNode(first->prev_sibling_c) : XmlNode();
}

LUMEX_PUBLIC_API
inline bool
XmlNode::set_name(char_t const *rhs)
{
  xml_node_type type_ = (m_root != nullptr) ? LUMEX_XML_NODETYPE(m_root) : node_null;

  if(type_ != node_element && type_ != node_pi && type_ != node_declaration) return false;

  return Utility::strcpy_insitu(m_root->name, m_root->header, Constants::kxml_memory_page_name_allocated_mask, rhs,
                                Utility::strlength(rhs));
}

LUMEX_PUBLIC_API
inline bool
XmlNode::set_name(char_t const *rhs, size_t size)
{
  xml_node_type type_ = (m_root != nullptr) ? LUMEX_XML_NODETYPE(m_root) : node_null;

  if(type_ != node_element && type_ != node_pi && type_ != node_declaration) return false;

  return Utility::strcpy_insitu(m_root->name, m_root->header, Constants::kxml_memory_page_name_allocated_mask, rhs,
                                size);
}

#if __cplusplus >= 201703L
LUMEX_PUBLIC_API
inline bool
XmlNode::set_name(string_view_t rhs)
{
  xml_node_type type_ = (m_root != nullptr) ? LUMEX_XML_NODETYPE(m_root) : node_null;

  if(type_ != node_element && type_ != node_pi && type_ != node_declaration) return false;

  return Utility::strcpy_insitu(m_root->name, m_root->header, Constants::kxml_memory_page_name_allocated_mask,
                                rhs.data(), rhs.size());
}
#endif

LUMEX_PUBLIC_API
inline bool
XmlNode::set_value(char_t const *rhs)
{
  xml_node_type type_ = (m_root != nullptr) ? LUMEX_XML_NODETYPE(m_root) : node_null;

  if(type_ != node_pcdata && type_ != node_cdata && type_ != node_comment && type_ != node_pi && type_ != node_doctype)
    return false;

  return Utility::strcpy_insitu(m_root->value, m_root->header, Constants::kxml_memory_page_value_allocated_mask, rhs,
                                Utility::strlength(rhs));
}

LUMEX_PUBLIC_API
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
LUMEX_PUBLIC_API
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

LUMEX_PUBLIC_API
inline XmlAttribute
XmlNode::append_attribute(char_t const *name_)
{
  if(!Utility::allow_insert_attribute(type())) return {};

  XmlAllocator &alloc = Memory::get_allocator(m_root);

  XmlAttribute newAttr(Lumex::Xml::Attribute::allocate_attribute(alloc));
  if(!newAttr) return {};

  Lumex::Xml::Node::append_attribute(newAttr.get(), m_root);

  newAttr.set_name(name_);

  return newAttr;
}

LUMEX_PUBLIC_API
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

LUMEX_PUBLIC_API
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

LUMEX_PUBLIC_API
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
LUMEX_PUBLIC_API
inline XmlAttribute
XmlNode::append_attribute(string_view_t name_)
{
  if(!Utility::allow_insert_attribute(type())) return {};

  XmlAllocator &alloc = Memory::get_allocator(m_root);

  XmlAttribute newAttr(Lumex::Xml::Attribute::allocate_attribute(alloc));
  if(!newAttr) return {};

  Lumex::Xml::Node::append_attribute(newAttr.get(), m_root);

  newAttr.set_name(name_);

  return newAttr;
}

LUMEX_PUBLIC_API
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

LUMEX_PUBLIC_API
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

LUMEX_PUBLIC_API
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

LUMEX_PUBLIC_API
inline XmlAttribute
XmlNode::append_copy(XmlAttribute const &proto)
{
  if(!proto) return {};
  if(!Utility::allow_insert_attribute(type())) return {};

  XmlAllocator &alloc = Memory::get_allocator(m_root);

  XmlAttribute newAttr(Lumex::Xml::Attribute::allocate_attribute(alloc));
  if(!newAttr) return {};

  Lumex::Xml::Node::append_attribute(newAttr.get(), m_root);
  Lumex::Xml::Attribute::node_copy_attribute(newAttr.get(), proto.get());

  return newAttr;
}

LUMEX_PUBLIC_API
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

LUMEX_PUBLIC_API
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

LUMEX_PUBLIC_API
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

LUMEX_PUBLIC_API
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

LUMEX_PUBLIC_API
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

LUMEX_PUBLIC_API
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

LUMEX_PUBLIC_API
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

LUMEX_PUBLIC_API
inline XmlNode
XmlNode::append_child(char_t const *name_)
{
  XmlNode result = append_child(node_element);

  result.set_name(name_);

  return result;
}

LUMEX_PUBLIC_API
inline XmlNode
XmlNode::prepend_child(char_t const *name_)
{
  XmlNode result = prepend_child(node_element);

  result.set_name(name_);

  return result;
}

LUMEX_PUBLIC_API
inline XmlNode
XmlNode::insert_child_after(char_t const *name_, XmlNode const &node)
{
  XmlNode result = insert_child_after(node_element, node);

  result.set_name(name_);

  return result;
}

LUMEX_PUBLIC_API
inline XmlNode
XmlNode::insert_child_before(char_t const *name_, XmlNode const &node)
{
  XmlNode result = insert_child_before(node_element, node);

  result.set_name(name_);

  return result;
}

#if __cplusplus >= 201703L
LUMEX_PUBLIC_API
inline XmlNode
XmlNode::append_child(string_view_t name_)
{
  XmlNode result = append_child(node_element);

  result.set_name(name_);

  return result;
}

LUMEX_PUBLIC_API
inline XmlNode
XmlNode::prepend_child(string_view_t name_)
{
  XmlNode result = prepend_child(node_element);

  result.set_name(name_);

  return result;
}

LUMEX_PUBLIC_API
inline XmlNode
XmlNode::insert_child_after(string_view_t name_, XmlNode const &node)
{
  XmlNode result = insert_child_after(node_element, node);

  result.set_name(name_);

  return result;
}

LUMEX_PUBLIC_API
inline XmlNode
XmlNode::insert_child_before(string_view_t name_, XmlNode const &node)
{
  XmlNode result = insert_child_before(node_element, node);

  result.set_name(name_);

  return result;
}
#endif

LUMEX_PUBLIC_API
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

LUMEX_PUBLIC_API
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

LUMEX_PUBLIC_API
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

LUMEX_PUBLIC_API
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

LUMEX_PUBLIC_API
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

LUMEX_PUBLIC_API
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

LUMEX_PUBLIC_API
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

LUMEX_PUBLIC_API
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

LUMEX_PUBLIC_API
inline bool
XmlNode::remove_attribute(char_t const *name_)
{
  return remove_attribute(attribute(name_));
}

#if __cplusplus >= 201703L
LUMEX_PUBLIC_API
inline bool
XmlNode::remove_attribute(string_view_t name_)
{
  return remove_attribute(attribute(name_));
}
#endif

LUMEX_PUBLIC_API
inline bool
XmlNode::remove_attribute(XmlAttribute const &attr)
{
  if((m_root == nullptr) || (attr.get() == nullptr)) return false;
  if(!Utility::is_attribute_of(attr.get(), m_root)) return false;

  XmlAllocator &alloc = Memory::get_allocator(m_root);

  Lumex::Xml::Attribute::remove_attribute(attr.get(), m_root);
  Lumex::Xml::Attribute::destroy_attribute(attr.get(), alloc);

  return true;
}

LUMEX_PUBLIC_API
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

LUMEX_PUBLIC_API
inline bool
XmlNode::remove_child(char_t const *name_)
{
  return remove_child(child(name_));
}

#if __cplusplus >= 201703L
LUMEX_PUBLIC_API
inline bool
XmlNode::remove_child(string_view_t name_)
{
  return remove_child(child(name_));
}
#endif

LUMEX_PUBLIC_API
inline bool
XmlNode::remove_child(XmlNode const &n)
{
  if((m_root == nullptr) || (n.m_root == nullptr) || n.m_root->parent != m_root) return false;

  XmlAllocator &alloc = Memory::get_allocator(m_root);

  Lumex::Xml::Node::remove_node(n.m_root);
  Lumex::Xml::Node::destroy_node(n.m_root, alloc);

  return true;
}

LUMEX_PUBLIC_API
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

LUMEX_PUBLIC_API
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

LUMEX_PUBLIC_API
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

LUMEX_PUBLIC_API
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

LUMEX_PUBLIC_API
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

LUMEX_PUBLIC_API
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

LUMEX_PUBLIC_API
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

LUMEX_PUBLIC_API
inline size_t
XmlNode::hash_value() const
{
  return reinterpret_cast<uintptr_t>(m_root) / sizeof(XmlNodeBase);
}

LUMEX_PUBLIC_API
inline XmlNodeBase *
XmlNode::get() const
{
  return m_root;
}

LUMEX_PUBLIC_API
inline void
XmlNode::print(IXmlWriter &writer, char_t const *indent, unsigned int flags, xml_encoding encoding,
               unsigned int depth) const
{
  if(m_root == nullptr) return;

  XmlBufferedWriter buffered_writer(writer, encoding);

  node_output(buffered_writer, m_root, indent, flags, depth);

  buffered_writer.flush();
}

LUMEX_PUBLIC_API
inline void
XmlNode::print(std::basic_ostream<char> &stream, char_t const *indent, unsigned int flags, xml_encoding encoding,
               unsigned int depth) const
{
  XmlWriterStream writer(stream);

  print(writer, indent, flags, encoding, depth);
}

LUMEX_PUBLIC_API
inline void
XmlNode::print(std::basic_ostream<wchar_t> &stream, char_t const *indent, unsigned int flags, unsigned int depth) const
{
  XmlWriterStream writer(stream);

  print(writer, indent, flags, encoding_wchar, depth);
}

LUMEX_PUBLIC_API
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

LUMEX_PUBLIC_API
inline XPathNode
XmlNode::select_node(char_t const *query, XPathVariableSet *variables) const
{
  XPathQuery query_obj(query, variables);
  return query_obj.evaluate_node(*this);
}

LUMEX_PUBLIC_API
inline XPathNode
XmlNode::select_node(XPathQuery const &query) const
{
  return query.evaluate_node(*this);
}

LUMEX_PUBLIC_API
inline XPathNodeSet
XmlNode::select_nodes(char_t const *query, XPathVariableSet *variables) const
{
  XPathQuery query_obj(query, variables);
  return query_obj.evaluate_node_set(*this);
}

LUMEX_PUBLIC_API
inline XPathNodeSet
XmlNode::select_nodes(XPathQuery const &query) const
{
  return query.evaluate_node_set(*this);
}

LUMEX_PUBLIC_API
inline XPathNode
XmlNode::select_single_node(char_t const *query, XPathVariableSet *variables) const
{
  XPathQuery query_obj(query, variables);
  return query_obj.evaluate_node(*this);
}

LUMEX_PUBLIC_API
inline XPathNode
XmlNode::select_single_node(XPathQuery const &query) const
{
  return query.evaluate_node(*this);
}

LUMEX_PUBLIC_API
inline bool
operator&&(XmlNode const &lhs, bool rhs) // NOLINT(misc-use-internal-linkage)
{
  return (bool)lhs && rhs;
}

LUMEX_PUBLIC_API
inline bool
operator||(XmlNode const &lhs, bool rhs) // NOLINT(misc-use-internal-linkage)
{
  return (bool)lhs || rhs;
}
