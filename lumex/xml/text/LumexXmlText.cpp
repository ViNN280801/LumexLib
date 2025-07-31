#define LUMEX_IMPLEMENTATION

#include "LumexXmlText.hpp"

#include "lumex/xml/constants/LumexXmlConstants.hpp"
#include "lumex/xml/node/LumexXmlNode.hpp"
#include "lumex/xml/utility/LumexXmlMacros.hpp"
#include "lumex/xml/utility/LumexXmlUtils.hpp"

using namespace Lumex::Xml::Text;
using namespace Lumex::Xml::Node;
using namespace Lumex::Xml::Utility;
using namespace Lumex::Xml::Constants;

inline LumexXmlText::LumexXmlText(xml_node_t *root) : m_root(root) {}

inline Lumex::Xml::Node::xml_node_t *
LumexXmlText::_data() const
{
  if((m_root == nullptr) || Node::is_text_node(m_root)) return m_root;

  // element nodes can have value if parse_embed_pcdata was used
  if(LUMEX_XML_NODETYPE(m_root) == node_element && (m_root->value != nullptr)) return m_root;

  for(xml_node_t *node = m_root->first_child; node != nullptr; node = node->next_sibling)
    if(Node::is_text_node(node)) return node;

  return nullptr;
}

inline Lumex::Xml::Node::xml_node_t *
LumexXmlText::_data_new()
{
  xml_node_t *data = _data();
  if(data != nullptr) return data;

  return Lumex::Xml::Node::LumexXmlNode(m_root).append_child(node_pcdata).internal_object();
}

inline LumexXmlText::LumexXmlText() : m_root(nullptr) {}

inline static void
unspecified_bool_xml_text(LumexXmlText *** /*unused*/) // NOLINT(misc-use-anonymous-namespace)
{}

inline LumexXmlText::
operator LumexXmlText::unspecified_bool_type() const
{
  return (_data() != nullptr) ? unspecified_bool_xml_text : nullptr;
}

inline bool
LumexXmlText::operator!() const
{
  return _data() == nullptr;
}

inline bool
LumexXmlText::empty() const
{
  return _data() == nullptr;
}

inline char_t const *
LumexXmlText::get() const
{
  xml_node_t *data = _data();
  if(data == nullptr) return LUMEX_XML_TEXT("");
  char_t const *value = data->value;
  return (value != nullptr) ? value : LUMEX_XML_TEXT("");
}

inline char_t const *
LumexXmlText::as_string(char_t const *def) const
{
  xml_node_t *data = _data();
  if(data == nullptr) return def;
  char_t const *value = data->value;
  return (value != nullptr) ? value : def;
}

inline int
LumexXmlText::as_int(int def) const
{
  xml_node_t *data = _data();
  if(data == nullptr) return def;
  char_t const *value = data->value;
  return (value != nullptr) ? Utility::get_value_int(value) : def;
}

inline unsigned int
LumexXmlText::as_uint(unsigned int def) const
{
  xml_node_t *data = _data();
  if(data == nullptr) return def;
  char_t const *value = data->value;
  return (value != nullptr) ? Utility::get_value_uint(value) : def;
}

inline double
LumexXmlText::as_double(double def) const
{
  xml_node_t *data = _data();
  if(data == nullptr) return def;
  char_t const *value = data->value;
  return (value != nullptr) ? Utility::get_value_double(value) : def;
}

inline float
LumexXmlText::as_float(float def) const
{
  xml_node_t *data = _data();
  if(data == nullptr) return def;
  char_t const *value = data->value;
  return (value != nullptr) ? Utility::get_value_float(value) : def;
}

inline bool
LumexXmlText::as_bool(bool def) const
{
  xml_node_t *data = _data();
  if(data == nullptr) return def;
  char_t const *value = data->value;
  return (value != nullptr) ? Utility::get_value_bool(value) : def;
}

inline long long
LumexXmlText::as_llong(long long def) const
{
  xml_node_t *data = _data();
  if(data == nullptr) return def;
  char_t const *value = data->value;
  return (value != nullptr) ? Utility::get_value_llong(value) : def;
}

inline unsigned long long
LumexXmlText::as_ullong(unsigned long long def) const
{
  xml_node_t *data = _data();
  if(data == nullptr) return def;
  char_t const *value = data->value;
  return (value != nullptr) ? Utility::get_value_ullong(value) : def;
}

inline bool
LumexXmlText::set(char_t const *rhs)
{
  xml_node_t *newdata = _data_new();

  return (newdata != nullptr)
           ? Utility::strcpy_insitu(newdata->value, newdata->header, kxml_memory_page_value_allocated_mask, rhs,
                                    Utility::strlength(rhs))
           : false;
}

inline bool
LumexXmlText::set(char_t const *rhs, size_t size)
{
  xml_node_t *newdata = _data_new();

  return (newdata != nullptr)
           ? Utility::strcpy_insitu(newdata->value, newdata->header, kxml_memory_page_value_allocated_mask, rhs, size)
           : false;
}

#if __cplusplus >= 201703L
inline bool
LumexXmlText::set(string_view_t rhs)
{
  xml_node_t *newdata = _data_new();

  return (newdata != nullptr) ? Utility::strcpy_insitu(newdata->value, newdata->header,
                                                       kxml_memory_page_value_allocated_mask, rhs.data(), rhs.size())
                              : false;
}
#endif

inline bool
LumexXmlText::set(int rhs)
{
  xml_node_t *newdata = _data_new();

  return (newdata != nullptr) ? Utility::set_value_integer<unsigned int>(
                                  newdata->value, newdata->header, kxml_memory_page_value_allocated_mask, rhs, rhs < 0)
                              : false;
}

inline bool
LumexXmlText::set(unsigned int rhs)
{
  xml_node_t *newdata = _data_new();

  return (newdata != nullptr) ? Utility::set_value_integer<unsigned int>(
                                  newdata->value, newdata->header, kxml_memory_page_value_allocated_mask, rhs, false)
                              : false;
}

inline bool
LumexXmlText::set(long rhs)
{
  xml_node_t *newdata = _data_new();

  return (newdata != nullptr) ? Utility::set_value_integer<unsigned long>(
                                  newdata->value, newdata->header, kxml_memory_page_value_allocated_mask, rhs, rhs < 0)
                              : false;
}

inline bool
LumexXmlText::set(unsigned long rhs)
{
  xml_node_t *newdata = _data_new();

  return (newdata != nullptr) ? Utility::set_value_integer<unsigned long>(
                                  newdata->value, newdata->header, kxml_memory_page_value_allocated_mask, rhs, false)
                              : false;
}

inline bool
LumexXmlText::set(float rhs)
{
  xml_node_t *newdata = _data_new();

  return (newdata != nullptr)
           ? Utility::set_value_convert(newdata->value, newdata->header, kxml_memory_page_value_allocated_mask, rhs,
                                        kdefault_float_precision)
           : false;
}

inline bool
LumexXmlText::set(float rhs, int precision)
{
  xml_node_t *newdata = _data_new();

  return (newdata != nullptr) ? Utility::set_value_convert(newdata->value, newdata->header,
                                                           kxml_memory_page_value_allocated_mask, rhs, precision)
                              : false;
}

inline bool
LumexXmlText::set(double rhs)
{
  xml_node_t *newdata = _data_new();

  return (newdata != nullptr)
           ? Utility::set_value_convert(newdata->value, newdata->header, kxml_memory_page_value_allocated_mask, rhs,
                                        kdefault_double_precision)
           : false;
}

inline bool
LumexXmlText::set(double rhs, int precision)
{
  xml_node_t *newdata = _data_new();

  return (newdata != nullptr) ? Utility::set_value_convert(newdata->value, newdata->header,
                                                           kxml_memory_page_value_allocated_mask, rhs, precision)
                              : false;
}

inline bool
LumexXmlText::set(bool rhs)
{
  xml_node_t *newdata = _data_new();

  return (newdata != nullptr)
           ? Utility::set_value_bool(newdata->value, newdata->header, kxml_memory_page_value_allocated_mask, rhs)
           : false;
}

inline bool
LumexXmlText::set(long long rhs)
{
  xml_node_t *newdata = _data_new();

  return (newdata != nullptr) ? Utility::set_value_integer<unsigned long long>(
                                  newdata->value, newdata->header, kxml_memory_page_value_allocated_mask, rhs, rhs < 0)
                              : false;
}

inline bool
LumexXmlText::set(unsigned long long rhs)
{
  xml_node_t *newdata = _data_new();

  return (newdata != nullptr) ? Utility::set_value_integer<unsigned long long>(
                                  newdata->value, newdata->header, kxml_memory_page_value_allocated_mask, rhs, false)
                              : false;
}

inline LumexXmlText &
LumexXmlText::operator=(char_t const *rhs)
{
  set(rhs);
  return *this;
}

inline LumexXmlText &
LumexXmlText::operator=(int rhs)
{
  set(rhs);
  return *this;
}

inline LumexXmlText &
LumexXmlText::operator=(unsigned int rhs)
{
  set(rhs);
  return *this;
}

inline LumexXmlText &
LumexXmlText::operator=(long rhs)
{
  set(rhs);
  return *this;
}

inline LumexXmlText &
LumexXmlText::operator=(unsigned long rhs)
{
  set(rhs);
  return *this;
}

inline LumexXmlText &
LumexXmlText::operator=(double rhs)
{
  set(rhs);
  return *this;
}

inline LumexXmlText &
LumexXmlText::operator=(float rhs)
{
  set(rhs);
  return *this;
}

inline LumexXmlText &
LumexXmlText::operator=(bool rhs)
{
  set(rhs);
  return *this;
}

#if __cplusplus >= 201703L
inline LumexXmlText &
LumexXmlText::operator=(string_view_t rhs)
{
  set(rhs);
  return *this;
}
#endif

inline LumexXmlText &
LumexXmlText::operator=(long long rhs)
{
  set(rhs);
  return *this;
}

inline LumexXmlText &
LumexXmlText::operator=(unsigned long long rhs)
{
  set(rhs);
  return *this;
}

inline Lumex::Xml::Node::LumexXmlNode
LumexXmlText::data() const
{
  return Lumex::Xml::Node::LumexXmlNode(_data());
}
