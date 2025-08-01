#include "XPathParseResult.hpp"

using namespace Lumex::Xml::XPath::Parser;

inline XPathParseResult::XPathParseResult() : error("Internal error"), offset(0) {}

inline XPathParseResult::
operator bool() const
{
  return error == nullptr;
}

inline char const *
XPathParseResult::description() const
{
  return (error != nullptr) ? error : "No error";
}
