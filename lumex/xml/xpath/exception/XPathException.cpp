#include "lumex/core/utility/LumexAssert.hpp"

#include "XPathException.hpp"

using namespace Lumex::Xml::XPath::Exception;

inline XPathException::XPathException(XPathParseResult const &result_) : m_result(result_)
{
  LUMEX_ASSERT(m_result.error);
}

inline char const *
XPathException::what() const noexcept
{
  return m_result.error;
}

inline XPathParseResult const &
XPathException::result() const
{
  return m_result;
}
