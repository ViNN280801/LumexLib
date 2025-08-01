#include "XmlText.hpp"

#include "lumex/xml/constants/XmlConstants.hpp"
#include "lumex/xml/node/XmlNode.hpp"
#include "lumex/xml/node/XmlNodeBase.hpp"
#include "lumex/xml/utility/XmlMacros.hpp"
#include "lumex/xml/utility/XmlUtils.hpp"

using namespace Lumex::Xml::Text;
using namespace Lumex::Xml::Node;
using namespace Lumex::Xml::Utility;
using namespace Lumex::Xml::Constants;

inline XmlText::XmlText(XmlNodeBase *root) : m_root(root) {}

inline XmlNodeBase *
XmlText::_data() const
{
  if((m_root == nullptr) || Node::is_text_node(m_root)) return m_root;

  // element nodes can have value if parse_embed_pcdata was used
  if(LUMEX_XML_NODETYPE(m_root) == node_element && (m_root->value != nullptr)) return m_root;

  for(XmlNodeBase *node = m_root->first_child; node != nullptr; node = node->next_sibling)
    if(Node::is_text_node(node)) return node;

  return nullptr;
}

inline XmlNodeBase *
XmlText::_data_new()
{
  XmlNodeBase *data = _data();
  if(data != nullptr) return data;

  return XmlNode(m_root).append_child(node_pcdata).get();
}

inline XmlText::XmlText() : m_root(nullptr) {}

inline static void
unspecified_bool_xml_text(XmlText *** /*unused*/) // NOLINT(misc-use-anonymous-namespace)
{}

inline XmlText::
operator XmlText::unspecified_bool_type() const
{
  return (_data() != nullptr) ? unspecified_bool_xml_text : nullptr;
}

inline bool
XmlText::operator!() const
{
  return _data() == nullptr;
}

inline bool
XmlText::empty() const
{
  return _data() == nullptr;
}

inline char_t const *
XmlText::get() const
{
  XmlNodeBase *data = _data();
  if(data == nullptr) return LUMEX_XML_TEXT("");
  char_t const *value = data->value;
  return (value != nullptr) ? value : LUMEX_XML_TEXT("");
}

inline char_t const *
XmlText::as_string(char_t const *def) const
{
  XmlNodeBase *data = _data();
  if(data == nullptr) return def;
  char_t const *value = data->value;
  return (value != nullptr) ? value : def;
}

inline int
XmlText::as_int(int def) const
{
  XmlNodeBase *data = _data();
  if(data == nullptr) return def;
  char_t const *value = data->value;
  return (value != nullptr) ? Utility::get_value_int(value) : def;
}

inline unsigned int
XmlText::as_uint(unsigned int def) const
{
  XmlNodeBase *data = _data();
  if(data == nullptr) return def;
  char_t const *value = data->value;
  return (value != nullptr) ? Utility::get_value_uint(value) : def;
}

inline double
XmlText::as_double(double def) const
{
  XmlNodeBase *data = _data();
  if(data == nullptr) return def;
  char_t const *value = data->value;
  return (value != nullptr) ? Utility::get_value_double(value) : def;
}

inline float
XmlText::as_float(float def) const
{
  XmlNodeBase *data = _data();
  if(data == nullptr) return def;
  char_t const *value = data->value;
  return (value != nullptr) ? Utility::get_value_float(value) : def;
}

inline bool
XmlText::as_bool(bool def) const
{
  XmlNodeBase *data = _data();
  if(data == nullptr) return def;
  char_t const *value = data->value;
  return (value != nullptr) ? Utility::get_value_bool(value) : def;
}

inline long long
XmlText::as_llong(long long def) const
{
  XmlNodeBase *data = _data();
  if(data == nullptr) return def;
  char_t const *value = data->value;
  return (value != nullptr) ? Utility::get_value_llong(value) : def;
}

inline unsigned long long
XmlText::as_ullong(unsigned long long def) const
{
  XmlNodeBase *data = _data();
  if(data == nullptr) return def;
  char_t const *value = data->value;
  return (value != nullptr) ? Utility::get_value_ullong(value) : def;
}

inline bool
XmlText::set(char_t const *rhs)
{
  XmlNodeBase *newdata = _data_new();

  return (newdata != nullptr)
           ? Utility::strcpy_insitu(newdata->value, newdata->header, kxml_memory_page_value_allocated_mask, rhs,
                                    Utility::strlength(rhs))
           : false;
}

inline bool
XmlText::set(char_t const *rhs, size_t size)
{
  XmlNodeBase *newdata = _data_new();

  return (newdata != nullptr)
           ? Utility::strcpy_insitu(newdata->value, newdata->header, kxml_memory_page_value_allocated_mask, rhs, size)
           : false;
}

#if __cplusplus >= 201703L
inline bool
XmlText::set(string_view_t rhs)
{
  XmlNodeBase *newdata = _data_new();

  return (newdata != nullptr) ? Utility::strcpy_insitu(newdata->value, newdata->header,
                                                       kxml_memory_page_value_allocated_mask, rhs.data(), rhs.size())
                              : false;
}
#endif

inline bool
XmlText::set(int rhs)
{
  XmlNodeBase *newdata = _data_new();

  return (newdata != nullptr) ? Utility::set_value_integer<unsigned int>(
                                  newdata->value, newdata->header, kxml_memory_page_value_allocated_mask, rhs, rhs < 0)
                              : false;
}

inline bool
XmlText::set(unsigned int rhs)
{
  XmlNodeBase *newdata = _data_new();

  return (newdata != nullptr) ? Utility::set_value_integer<unsigned int>(
                                  newdata->value, newdata->header, kxml_memory_page_value_allocated_mask, rhs, false)
                              : false;
}

inline bool
XmlText::set(long rhs)
{
  XmlNodeBase *newdata = _data_new();

  return (newdata != nullptr) ? Utility::set_value_integer<unsigned long>(
                                  newdata->value, newdata->header, kxml_memory_page_value_allocated_mask, rhs, rhs < 0)
                              : false;
}

inline bool
XmlText::set(unsigned long rhs)
{
  XmlNodeBase *newdata = _data_new();

  return (newdata != nullptr) ? Utility::set_value_integer<unsigned long>(
                                  newdata->value, newdata->header, kxml_memory_page_value_allocated_mask, rhs, false)
                              : false;
}

inline bool
XmlText::set(float rhs)
{
  XmlNodeBase *newdata = _data_new();

  return (newdata != nullptr)
           ? Utility::set_value_convert(newdata->value, newdata->header, kxml_memory_page_value_allocated_mask, rhs,
                                        kdefault_float_precision)
           : false;
}

inline bool
XmlText::set(float rhs, int precision)
{
  XmlNodeBase *newdata = _data_new();

  return (newdata != nullptr) ? Utility::set_value_convert(newdata->value, newdata->header,
                                                           kxml_memory_page_value_allocated_mask, rhs, precision)
                              : false;
}

inline bool
XmlText::set(double rhs)
{
  XmlNodeBase *newdata = _data_new();

  return (newdata != nullptr)
           ? Utility::set_value_convert(newdata->value, newdata->header, kxml_memory_page_value_allocated_mask, rhs,
                                        kdefault_double_precision)
           : false;
}

inline bool
XmlText::set(double rhs, int precision)
{
  XmlNodeBase *newdata = _data_new();

  return (newdata != nullptr) ? Utility::set_value_convert(newdata->value, newdata->header,
                                                           kxml_memory_page_value_allocated_mask, rhs, precision)
                              : false;
}

inline bool
XmlText::set(bool rhs)
{
  XmlNodeBase *newdata = _data_new();

  return (newdata != nullptr)
           ? Utility::set_value_bool(newdata->value, newdata->header, kxml_memory_page_value_allocated_mask, rhs)
           : false;
}

inline bool
XmlText::set(long long rhs)
{
  XmlNodeBase *newdata = _data_new();

  return (newdata != nullptr) ? Utility::set_value_integer<unsigned long long>(
                                  newdata->value, newdata->header, kxml_memory_page_value_allocated_mask, rhs, rhs < 0)
                              : false;
}

inline bool
XmlText::set(unsigned long long rhs)
{
  XmlNodeBase *newdata = _data_new();

  return (newdata != nullptr) ? Utility::set_value_integer<unsigned long long>(
                                  newdata->value, newdata->header, kxml_memory_page_value_allocated_mask, rhs, false)
                              : false;
}

inline XmlText &
XmlText::operator=(char_t const *rhs)
{
  set(rhs);
  return *this;
}

inline XmlText &
XmlText::operator=(int rhs)
{
  set(rhs);
  return *this;
}

inline XmlText &
XmlText::operator=(unsigned int rhs)
{
  set(rhs);
  return *this;
}

inline XmlText &
XmlText::operator=(long rhs)
{
  set(rhs);
  return *this;
}

inline XmlText &
XmlText::operator=(unsigned long rhs)
{
  set(rhs);
  return *this;
}

inline XmlText &
XmlText::operator=(double rhs)
{
  set(rhs);
  return *this;
}

inline XmlText &
XmlText::operator=(float rhs)
{
  set(rhs);
  return *this;
}

inline XmlText &
XmlText::operator=(bool rhs)
{
  set(rhs);
  return *this;
}

#if __cplusplus >= 201703L
inline XmlText &
XmlText::operator=(string_view_t rhs)
{
  set(rhs);
  return *this;
}
#endif

inline XmlText &
XmlText::operator=(long long rhs)
{
  set(rhs);
  return *this;
}

inline XmlText &
XmlText::operator=(unsigned long long rhs)
{
  set(rhs);
  return *this;
}

inline XmlNode
XmlText::data() const
{
  return XmlNode(_data());
}
