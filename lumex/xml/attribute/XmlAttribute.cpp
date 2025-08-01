#include "lumex/xml/utility/XmlUtils.hpp"

#include "XmlAttribute.hpp"

using namespace Lumex::Xml::Types;
using namespace Lumex::Xml::Utility;
using namespace Lumex::Xml::Attribute;

inline XmlAttribute::XmlAttribute() : m_attr(nullptr) {}

inline XmlAttribute::XmlAttribute(XmlAttributeBase *attr) : m_attr(attr) {}

inline static void
unspecified_bool_xml_attribute(XmlAttribute *** /*unused*/) // NOLINT(misc-use-anonymous-namespace)
{}

inline XmlAttribute::
operator XmlAttribute::unspecified_bool_type() const
{
  return (m_attr != nullptr) ? unspecified_bool_xml_attribute : nullptr;
}

inline bool
XmlAttribute::operator!() const
{
  return m_attr == nullptr;
}

inline bool
XmlAttribute::operator==(XmlAttribute const &other) const
{
  return (m_attr == other.m_attr);
}

inline bool
XmlAttribute::operator!=(XmlAttribute const &other) const
{
  return (m_attr != other.m_attr);
}

inline bool
XmlAttribute::operator<(XmlAttribute const &other) const
{
  return (m_attr < other.m_attr);
}

inline bool
XmlAttribute::operator>(XmlAttribute const &other) const
{
  return (m_attr > other.m_attr);
}

inline bool
XmlAttribute::operator<=(XmlAttribute const &other) const
{
  return (m_attr <= other.m_attr);
}

inline bool
XmlAttribute::operator>=(XmlAttribute const &other) const
{
  return (m_attr >= other.m_attr);
}

inline XmlAttribute
XmlAttribute::next_attribute() const
{
  if(m_attr == nullptr) return XmlAttribute();
  return XmlAttribute(m_attr->next_attribute);
}

inline XmlAttribute
XmlAttribute::previous_attribute() const
{
  if(m_attr == nullptr) return XmlAttribute();
  XmlAttributeBase *prev = m_attr->prev_attribute_c;
  return (prev->next_attribute != nullptr) ? XmlAttribute(prev) : XmlAttribute();
}

inline char_t const *
XmlAttribute::as_string(char_t const *def) const
{
  if(m_attr == nullptr) return def;
  char_t const *value = m_attr->value;
  return (value != nullptr) ? value : def;
}

inline int
XmlAttribute::as_int(int def) const
{
  if(m_attr == nullptr) return def;
  char_t const *value = m_attr->value;
  return (value != nullptr) ? get_value_int(value) : def;
}

inline unsigned int
XmlAttribute::as_uint(unsigned int def) const
{
  if(m_attr == nullptr) return def;
  char_t const *value = m_attr->value;
  return (value != nullptr) ? get_value_uint(value) : def;
}

inline double
XmlAttribute::as_double(double def) const
{
  if(m_attr == nullptr) return def;
  char_t const *value = m_attr->value;
  return (value != nullptr) ? get_value_double(value) : def;
}

inline float
XmlAttribute::as_float(float def) const
{
  if(m_attr == nullptr) return def;
  char_t const *value = m_attr->value;
  return (value != nullptr) ? get_value_float(value) : def;
}

inline bool
XmlAttribute::as_bool(bool def) const
{
  if(m_attr == nullptr) return def;
  char_t const *value = m_attr->value;
  return (value != nullptr) ? get_value_bool(value) : def;
}

inline long long
XmlAttribute::as_llong(long long def) const
{
  if(m_attr == nullptr) return def;
  char_t const *value = m_attr->value;
  return (value != nullptr) ? get_value_llong(value) : def;
}

inline unsigned long long
XmlAttribute::as_ullong(unsigned long long def) const
{
  if(m_attr == nullptr) return def;
  char_t const *value = m_attr->value;
  return (value != nullptr) ? get_value_ullong(value) : def;
}

inline bool
XmlAttribute::empty() const
{
  return m_attr == nullptr;
}

inline char_t const *
XmlAttribute::name() const
{
  if(m_attr == nullptr) return LUMEX_XML_TEXT("");
  char_t const *name = m_attr->name;
  return (name != nullptr) ? name : LUMEX_XML_TEXT("");
}

inline char_t const *
XmlAttribute::value() const
{
  if(m_attr == nullptr) return LUMEX_XML_TEXT("");
  char_t const *value = m_attr->value;
  return (value != nullptr) ? value : LUMEX_XML_TEXT("");
}

inline size_t
XmlAttribute::hash_value() const
{
  return reinterpret_cast<uintptr_t>(m_attr) / sizeof(XmlAttributeBase);
}

inline XmlAttributeBase *
XmlAttribute::get() const
{
  return m_attr;
}

inline XmlAttribute &
XmlAttribute::operator=(char_t const *rhs)
{
  set_value(rhs);
  return *this;
}

inline XmlAttribute &
XmlAttribute::operator=(int rhs)
{
  set_value(rhs);
  return *this;
}

inline XmlAttribute &
XmlAttribute::operator=(unsigned int rhs)
{
  set_value(rhs);
  return *this;
}

inline XmlAttribute &
XmlAttribute::operator=(long rhs)
{
  set_value(rhs);
  return *this;
}

inline XmlAttribute &
XmlAttribute::operator=(unsigned long rhs)
{
  set_value(rhs);
  return *this;
}

inline XmlAttribute &
XmlAttribute::operator=(double rhs)
{
  set_value(rhs);
  return *this;
}

inline XmlAttribute &
XmlAttribute::operator=(float rhs)
{
  set_value(rhs);
  return *this;
}

inline XmlAttribute &
XmlAttribute::operator=(bool rhs)
{
  set_value(rhs);
  return *this;
}

#if __cplusplus >= 201703L
inline XmlAttribute &
XmlAttribute::operator=(string_view_t rhs)
{
  set_value(rhs);
  return *this;
}
#endif

inline XmlAttribute &
XmlAttribute::operator=(long long rhs)
{
  set_value(rhs);
  return *this;
}

inline XmlAttribute &
XmlAttribute::operator=(unsigned long long rhs)
{
  set_value(rhs);
  return *this;
}

inline bool
XmlAttribute::set_name(char_t const *rhs)
{
  if(m_attr == nullptr) return false;

  return strcpy_insitu(m_attr->name, m_attr->header, kxml_memory_page_name_allocated_mask, rhs, strlength(rhs));
}

inline bool
XmlAttribute::set_name(char_t const *rhs, size_t size)
{
  if(m_attr == nullptr) return false;

  return strcpy_insitu(m_attr->name, m_attr->header, kxml_memory_page_name_allocated_mask, rhs, size);
}

#if __cplusplus >= 201703L
inline bool
XmlAttribute::set_name(string_view_t rhs)
{
  if(m_attr == nullptr) return false;

  return strcpy_insitu(m_attr->name, m_attr->header, kxml_memory_page_name_allocated_mask, rhs.data(), rhs.size());
}
#endif

inline bool
XmlAttribute::set_value(const char_t *rhs)
{
  if(m_attr == nullptr) return false;

  return strcpy_insitu(m_attr->value, m_attr->header, kxml_memory_page_value_allocated_mask, rhs, strlength(rhs));
}

inline bool
XmlAttribute::set_value(char_t const *rhs, size_t size)
{
  if(m_attr == nullptr) return false;

  return strcpy_insitu(m_attr->value, m_attr->header, kxml_memory_page_value_allocated_mask, rhs, size);
}

#if __cplusplus >= 201703L
inline bool
XmlAttribute::set_value(string_view_t rhs)
{
  if(m_attr == nullptr) return false;

  return strcpy_insitu(m_attr->value, m_attr->header, kxml_memory_page_value_allocated_mask, rhs.data(), rhs.size());
}
#endif

inline bool
XmlAttribute::set_value(int rhs)
{
  if(m_attr == nullptr) return false;

  return set_value_integer<unsigned int>(m_attr->value, m_attr->header, kxml_memory_page_value_allocated_mask, rhs,
                                         rhs < 0);
}

inline bool
XmlAttribute::set_value(unsigned int rhs)
{
  if(m_attr == nullptr) return false;

  return set_value_integer<unsigned int>(m_attr->value, m_attr->header, kxml_memory_page_value_allocated_mask, rhs,
                                         false);
}

inline bool
XmlAttribute::set_value(long rhs)
{
  if(m_attr == nullptr) return false;

  return set_value_integer<unsigned long>(m_attr->value, m_attr->header, kxml_memory_page_value_allocated_mask, rhs,
                                          rhs < 0);
}

inline bool
XmlAttribute::set_value(unsigned long rhs)
{
  if(m_attr == nullptr) return false;

  return set_value_integer<unsigned long>(m_attr->value, m_attr->header, kxml_memory_page_value_allocated_mask, rhs,
                                          false);
}

inline bool
XmlAttribute::set_value(double rhs)
{
  if(m_attr == nullptr) return false;

  return set_value_convert(m_attr->value, m_attr->header, kxml_memory_page_value_allocated_mask, rhs,
                           kdefault_double_precision);
}

inline bool
XmlAttribute::set_value(double rhs, int precision)
{
  if(m_attr == nullptr) return false;

  return set_value_convert(m_attr->value, m_attr->header, kxml_memory_page_value_allocated_mask, rhs, precision);
}

inline bool
XmlAttribute::set_value(float rhs)
{
  if(m_attr == nullptr) return false;

  return set_value_convert(m_attr->value, m_attr->header, kxml_memory_page_value_allocated_mask, rhs,
                           kdefault_float_precision);
}

inline bool
XmlAttribute::set_value(float rhs, int precision)
{
  if(m_attr == nullptr) return false;

  return set_value_convert(m_attr->value, m_attr->header, kxml_memory_page_value_allocated_mask, rhs, precision);
}

inline bool
XmlAttribute::set_value(bool rhs)
{
  if(m_attr == nullptr) return false;

  return set_value_bool(m_attr->value, m_attr->header, kxml_memory_page_value_allocated_mask, rhs);
}

inline bool
XmlAttribute::set_value(long long rhs)
{
  if(m_attr == nullptr) return false;

  return set_value_integer<unsigned long long>(m_attr->value, m_attr->header, kxml_memory_page_value_allocated_mask,
                                               rhs, rhs < 0);
}

inline bool
XmlAttribute::set_value(unsigned long long rhs)
{
  if(m_attr == nullptr) return false;

  return set_value_integer<unsigned long long>(m_attr->value, m_attr->header, kxml_memory_page_value_allocated_mask,
                                               rhs, false);
}
