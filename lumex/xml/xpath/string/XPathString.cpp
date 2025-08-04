#include "lumex/LumexExport.hpp"
#define LUMEX_IMPLEMENTATION

#include "lumex/core/utility/LumexAssert.hpp"

#include "lumex/xml/utility/XmlUtils.hpp"
#include "lumex/xml/xpath/node/XPathNode.hpp"

#include "XPathString.hpp"

using namespace Lumex::Xml::Utility;
using namespace Lumex::Xml::XPath::Node;
using namespace Lumex::Xml::XPath::String;

LUMEX_PUBLIC_API
inline XPathString
XPathString::from_const(char_t const *str)
{
  return {str, false, 0};
}

LUMEX_PUBLIC_API
inline XPathString
XPathString::from_heap_preallocated(char_t const *begin, char_t const *end)
{
  LUMEX_ASSERT(begin <= end && *end == 0);

  return {begin, true, static_cast<size_t>(end - begin)};
}

LUMEX_PUBLIC_API
inline XPathString
XPathString::from_heap(char_t const *begin, char_t const *end, XPathAllocator *alloc)
{
  LUMEX_ASSERT(begin <= end);

  if(begin == end) return {};

  auto length        = static_cast<size_t>(end - begin);
  char_t const *data = duplicate_string(begin, length, alloc);

  return (data != nullptr) ? XPathString(data, true, length) : XPathString();
}

LUMEX_PUBLIC_API
inline XPathString::XPathString() : m_buffer(LUMEX_XML_TEXT("")), m_uses_heap(false), m_length_heap(0) {}

LUMEX_PUBLIC_API
inline void
XPathString::append(XPathString const &other, XPathAllocator *alloc)
{
  // skip empty sources
  if(*other.m_buffer == 0) return;

  // fast append for constant empty target and constant source
  if((*m_buffer == 0) && !m_uses_heap && !other.m_uses_heap) { m_buffer = other.m_buffer; }
  else
  {
    // need to make heap copy
    size_t target_length = length();
    size_t source_length = other.length();
    size_t result_length = target_length + source_length;

    // allocate new buffer
    auto *result = static_cast<char_t *>(
      alloc->reallocate(m_uses_heap ? const_cast< // NOLINT(cppcoreguidelines-pro-type-const-cast)
                                        char_t *>(m_buffer)
                                    : nullptr,
                        (target_length + 1) * sizeof(char_t), (result_length + 1) * sizeof(char_t)));
    if(result == nullptr) return;

    // append first string to the new buffer in case there was no reallocation
    if(!m_uses_heap) memcpy(result, m_buffer, target_length * sizeof(char_t));

    // append second string to the new buffer
    memcpy(result + target_length, other.m_buffer, source_length * sizeof(char_t));
    result[result_length] = 0;

    // finalize
    m_buffer      = result;
    m_uses_heap   = true;
    m_length_heap = result_length;
  }
}

LUMEX_PUBLIC_API
inline char_t const *
XPathString::c_str() const
{
  return m_buffer;
}

LUMEX_PUBLIC_API
inline size_t
XPathString::length() const
{
  return m_uses_heap ? m_length_heap : Utility::strlength(m_buffer);
}

LUMEX_PUBLIC_API
inline char_t *
XPathString::data(XPathAllocator *alloc)
{
  // make private heap copy
  if(!m_uses_heap)
  {
    size_t length_      = Utility::strlength(m_buffer);
    char_t const *data_ = duplicate_string(m_buffer, length_, alloc);

    if(data_ == nullptr) return nullptr;

    m_buffer      = data_;
    m_uses_heap   = true;
    m_length_heap = length_;
  }

  return const_cast<char_t *>(m_buffer); // NOLINT(cppcoreguidelines-pro-type-const-cast)
}

LUMEX_PUBLIC_API
inline bool
XPathString::empty() const
{
  return *m_buffer == 0;
}

LUMEX_PUBLIC_API
inline bool
XPathString::operator==(XPathString const &other) const
{
  return Utility::strequal(m_buffer, other.m_buffer);
}

LUMEX_PUBLIC_API
inline bool
XPathString::operator!=(XPathString const &other) const
{
  return !Utility::strequal(m_buffer, other.m_buffer);
}

LUMEX_PUBLIC_API
inline bool
XPathString::uses_heap() const
{
  return m_uses_heap;
}

LUMEX_PUBLIC_API
inline char_t *
XPathString::duplicate_string(char_t const *string, size_t length, XPathAllocator *alloc)
{
  auto *result = static_cast<char_t *>(alloc->allocate((length + 1) * sizeof(char_t)));
  if(result == nullptr) return nullptr;

  std::memcpy(result, string, length * sizeof(char_t));
  result[length] = 0;

  return result;
}

LUMEX_PUBLIC_API
inline XPathString::XPathString(char_t const *buffer, bool uses_heap_, size_t length_heap)
    : m_buffer(buffer), m_uses_heap(uses_heap_), m_length_heap(length_heap)
{}

LUMEX_PUBLIC_API
inline XPathString
Lumex::Xml::XPath::String::string_value(XPathNode const &node,
                                        XPathAllocator *alloc) // NOLINT(misc-use-internal-linkage)
{
  if(node.attribute() != nullptr) return XPathString::from_const(node.attribute().value());

  XmlNode tmp = node.node();

  switch(tmp.type())
  {
  case node_pcdata:
  case node_cdata:
  case node_comment:
  case node_pi: return XPathString::from_const(tmp.value());

  case node_document:
  case node_element: {
    XPathString result;

    // element nodes can have value if parse_embed_pcdata was used
    if(tmp.value()[0] != 0) result.append(XPathString::from_const(tmp.value()), alloc);

    XmlNode cur = tmp.first_child();

    while((cur != nullptr) && cur != tmp)
    {
      if(cur.type() == node_pcdata || cur.type() == node_cdata)
        result.append(XPathString::from_const(cur.value()), alloc);

      if(cur.first_child() != nullptr)
        cur = cur.first_child();
      else if(cur.next_sibling() != nullptr)
        cur = cur.next_sibling();
      else
      {
        while(!cur.next_sibling() && cur != tmp) cur = cur.parent();
        if(cur != tmp) cur = cur.next_sibling();
      }
    }
    return result;
  }
  default: return {};
  }
}

LUMEX_PUBLIC_API
inline XPathString
Lumex::Xml::XPath::String::convert_number_to_string(double value,
                                                    XPathAllocator *alloc) // NOLINT(misc-use-internal-linkage)
{
  // try special number conversion
  char_t const *special = convert_number_to_string_special(value);
  if(special != nullptr) return XPathString::from_const(special);

  // get mantissa + exponent form
  char mantissa_buffer[32]; // NOLINT(cppcoreguidelines-avoid-c-arrays, modernize-avoid-c-arrays)

  char *mantissa{};
  int exponent = 0;
  Utility::convert_number_to_mantissa_exponent(value, mantissa_buffer, &mantissa, &exponent);

  // allocate a buffer of suitable length for the number
  size_t result_size = strlen(mantissa_buffer // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
                              )
                       + (exponent > 0 ? exponent : -exponent) + 4;
  auto *result = static_cast<char_t *>(alloc->allocate(sizeof(char_t) * result_size));
  if(result == nullptr) return {};

  // make the number!
  char_t *str = result;

  // sign
  if(value < 0) *str++ = '-';

  // integer part
  if(exponent <= 0) { *str++ = '0'; }
  else
  {
    while(exponent > 0)
    {
      LUMEX_ASSERT(*mantissa == 0 || static_cast<unsigned int>(*mantissa - '0') <= 9);
      *str++ = (*mantissa != 0) ? *mantissa++ : '0';
      exponent--;
    }
  }

  // fractional part
  if(*mantissa != 0)
  {
    // decimal point
    *str++ = '.';

    // extra zeroes from negative exponent
    while(exponent < 0)
    {
      *str++ = '0';
      exponent++;
    }

    // extra mantissa digits
    while(*mantissa != 0)
    {
      LUMEX_ASSERT(static_cast<unsigned int>(*mantissa - '0') <= 9);
      *str++ = *mantissa++;
    }
  }

  // zero-terminate
  LUMEX_ASSERT(str < result + result_size);
  *str = 0;

  return XPathString::from_heap_preallocated(result, str);
}
