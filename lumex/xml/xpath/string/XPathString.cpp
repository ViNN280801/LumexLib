#include "lumex/core/utility/LumexAssert.hpp"

#include "lumex/xml/utility/XmlUtils.hpp"

#include "XPathString.hpp"

using namespace Lumex::Xml::Utility;
using namespace Lumex::Xml::XPath::String;

XPathString
XPathString::from_const(char_t const *str)
{
  return {str, false, 0};
}

XPathString
XPathString::from_heap_preallocated(char_t const *begin, char_t const *end)
{
  LUMEX_ASSERT(begin <= end && *end == 0);

  return {begin, true, static_cast<size_t>(end - begin)};
}

XPathString
XPathString::from_heap(char_t const *begin, char_t const *end, XPathAllocator *alloc)
{
  LUMEX_ASSERT(begin <= end);

  if(begin == end) return {};

  auto length        = static_cast<size_t>(end - begin);
  char_t const *data = duplicate_string(begin, length, alloc);

  return (data != nullptr) ? XPathString(data, true, length) : XPathString();
}

XPathString::XPathString() : m_buffer(LUMEX_XML_TEXT("")), m_uses_heap(false), m_length_heap(0) {}

void
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

char_t const *
XPathString::c_str() const
{
  return m_buffer;
}

size_t
XPathString::length() const
{
  return m_uses_heap ? m_length_heap : Utility::strlength(m_buffer);
}

char_t *
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

bool
XPathString::empty() const
{
  return *m_buffer == 0;
}

bool
XPathString::operator==(XPathString const &other) const
{
  return Utility::strequal(m_buffer, other.m_buffer);
}

bool
XPathString::operator!=(XPathString const &other) const
{
  return !Utility::strequal(m_buffer, other.m_buffer);
}

bool
XPathString::uses_heap() const
{
  return m_uses_heap;
}

char_t *
XPathString::duplicate_string(char_t const *string, size_t length, XPathAllocator *alloc)
{
  auto *result = static_cast<char_t *>(alloc->allocate((length + 1) * sizeof(char_t)));
  if(result == nullptr) return nullptr;

  std::memcpy(result, string, length * sizeof(char_t));
  result[length] = 0;

  return result;
}

XPathString::XPathString(char_t const *buffer, bool uses_heap_, size_t length_heap)
    : m_buffer(buffer), m_uses_heap(uses_heap_), m_length_heap(length_heap)
{}
