#define LUMEX_IMPLEMENTATION

#include "lumex/xml/utility/XmlUtils.hpp"

#include "XPathLexerString.hpp"

using namespace lumex::xml::utility;
using namespace lumex::xml::xpath::parser::lexer;

LUMEX_PUBLIC_API
bool
XPathLexerString::operator== (char_t const *other) const
{
  auto length = static_cast<std::size_t> (end - begin);
  return utility::strequalrange (other, begin, length);
}
