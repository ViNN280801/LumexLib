#define LUMEX_IMPLEMENTATION

#include "lumex/core/utility/LumexAssert.hpp"

#include "XPathException.hpp"

using namespace Lumex::Xml::XPath::Exception;

LUMEX_PUBLIC_API
inline XPathException::XPathException(XPathParseResult const &result_) : m_result(result_)
{
  LUMEX_ASSERT(m_result.error);
}

LUMEX_PUBLIC_API
inline char const *
XPathException::what() const noexcept
{
  return m_result.error;
}

LUMEX_PUBLIC_API
inline XPathParseResult const &
XPathException::result() const
{
  return m_result;
}
