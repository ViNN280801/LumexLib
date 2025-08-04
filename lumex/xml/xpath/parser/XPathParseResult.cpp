#define LUMEX_IMPLEMENTATION

#include "XPathParseResult.hpp"

using namespace Lumex::Xml::XPath::Parser;

LUMEX_PUBLIC_API
inline XPathParseResult::XPathParseResult() : error("Internal error"), offset(0) {}

LUMEX_PUBLIC_API
inline XPathParseResult::
operator bool() const
{
  return error == nullptr;
}

LUMEX_PUBLIC_API
inline char const *
XPathParseResult::description() const
{
  return (error != nullptr) ? error : "No error";
}
