#define LUMEX_IMPLEMENTATION

#include "XPathParseResult.hpp"

using namespace lumex::xml::xpath::parser;

LUMEX_PUBLIC_API
inline xpath_parse_result_t::xpath_parse_result_t ()
    : error ("Internal error"), offset (0)
{
}

LUMEX_PUBLIC_API
inline xpath_parse_result_t::
operator bool () const
{
  return error == nullptr;
}

LUMEX_PUBLIC_API
inline char const *
xpath_parse_result_t::description () const
{
  return (error != nullptr) ? error : "No error";
}
