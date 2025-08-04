#define LUMEX_IMPLEMENTATION

#include "lumex/xml/utility/XmlUtils.hpp"

#include "XPathLexerString.hpp"

using namespace Lumex::Xml::Utility;
using namespace Lumex::Xml::XPath::Parser::Lexer;

LUMEX_PUBLIC_API
bool
XPathLexerString::operator==(char_t const *other) const
{
  auto length = static_cast<size_t>(end - begin);
  return Utility::strequalrange(other, begin, length);
}
