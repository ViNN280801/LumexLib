#define LUMEX_IMPLEMENTATION

#include "lumex/core/utility/LumexAssert.hpp"

#include "lumex/xml/constants/XmlConstants.hpp" // NOLINT(used in LUMEX_XML_IS_CHARTYPE)

#include "XPathLexer.hpp"

using namespace Lumex::Xml::Constants;
using namespace Lumex::Xml::XPath::Parser::Lexer;

LUMEX_PUBLIC_API
XPathLexer::XPathLexer(char_t const *query) : _cur(query) { next(); }

LUMEX_PUBLIC_API
char_t const *
XPathLexer::state() const
{
  return _cur;
}

LUMEX_PUBLIC_API
void
XPathLexer::next() // NOLINT(readability-function-cognitive-complexity)
{
  char_t const *cur = _cur;

  while(LUMEX_XML_IS_CHARTYPE( // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
    *cur, ct_space))
    ++cur;

  // save lexeme position for error reporting
  _cur_lexeme_pos = cur;

  switch(*cur)
  {
  case 0: _cur_lexeme = lex_eof; break;

  case '>':
    if(*(cur + 1) == '=')
    {
      cur += 2;
      _cur_lexeme = lex_greater_or_equal;
    }
    else
    {
      cur += 1;
      _cur_lexeme = lex_greater;
    }
    break;

  case '<':
    if(*(cur + 1) == '=')
    {
      cur += 2;
      _cur_lexeme = lex_less_or_equal;
    }
    else
    {
      cur += 1;
      _cur_lexeme = lex_less;
    }
    break;

  case '!':
    if(*(cur + 1) == '=')
    {
      cur += 2;
      _cur_lexeme = lex_not_equal;
    }
    else { _cur_lexeme = lex_none; }
    break;

  case '=':
    cur += 1;
    _cur_lexeme = lex_equal;

    break;

  case '+':
    cur += 1;
    _cur_lexeme = lex_plus;

    break;

  case '-':
    cur += 1;
    _cur_lexeme = lex_minus;

    break;

  case '*':
    cur += 1;
    _cur_lexeme = lex_multiply;

    break;

  case '|':
    cur += 1;
    _cur_lexeme = lex_union;

    break;

  case '$':
    cur += 1;

    if(LUMEX_XML_IS_CHARTYPEX( // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
         *cur, ctx_start_symbol))
    {
      _cur_lexeme_contents.begin = cur;

      while(LUMEX_XML_IS_CHARTYPEX( // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
        *cur, ctx_symbol))
        cur++;

      if(cur[0] == ':'
         && LUMEX_XML_IS_CHARTYPEX( // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
           cur[1], ctx_symbol))     // qname
      {
        cur++; // :

        while(LUMEX_XML_IS_CHARTYPEX( // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
          *cur, ctx_symbol))
          cur++;
      }

      _cur_lexeme_contents.end = cur;

      _cur_lexeme              = lex_var_ref;
    }
    else { _cur_lexeme = lex_none; }

    break;

  case '(':
    cur += 1;
    _cur_lexeme = lex_open_brace;

    break;

  case ')':
    cur += 1;
    _cur_lexeme = lex_close_brace;

    break;

  case '[':
    cur += 1;
    _cur_lexeme = lex_open_square_brace;

    break;

  case ']':
    cur += 1;
    _cur_lexeme = lex_close_square_brace;

    break;

  case ',':
    cur += 1;
    _cur_lexeme = lex_comma;

    break;

  case '/':
    if(*(cur + 1) == '/')
    {
      cur += 2;
      _cur_lexeme = lex_double_slash;
    }
    else
    {
      cur += 1;
      _cur_lexeme = lex_slash;
    }
    break;

  case '.':
    if(*(cur + 1) == '.')
    {
      cur += 2;
      _cur_lexeme = lex_double_dot;
    }
    else if(LUMEX_XML_IS_CHARTYPEX( // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
              *(cur + 1), ctx_digit))
    {
      _cur_lexeme_contents.begin = cur; // .

      ++cur;

      while(LUMEX_XML_IS_CHARTYPEX( // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
        *cur, ctx_digit))
        cur++;

      _cur_lexeme_contents.end = cur;

      _cur_lexeme              = lex_number;
    }
    else
    {
      cur += 1;
      _cur_lexeme = lex_dot;
    }
    break;

  case '@':
    cur += 1;
    _cur_lexeme = lex_axis_attribute;

    break;

  case '"':
  case '\'': {
    char_t terminator = *cur;

    ++cur;

    _cur_lexeme_contents.begin = cur;
    while((*cur != 0) && *cur != terminator) cur++;
    _cur_lexeme_contents.end = cur;

    if(*cur == 0)
      _cur_lexeme = lex_none;
    else
    {
      cur += 1;
      _cur_lexeme = lex_quoted_string;
    }

    break;
  }

  case ':':
    if(*(cur + 1) == ':')
    {
      cur += 2;
      _cur_lexeme = lex_double_colon;
    }
    else { _cur_lexeme = lex_none; }
    break;

  default:
    if(LUMEX_XML_IS_CHARTYPEX( // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
         *cur, ctx_digit))
    {
      _cur_lexeme_contents.begin = cur;

      while(LUMEX_XML_IS_CHARTYPEX( // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
        *cur, ctx_digit))
        cur++;

      if(*cur == '.')
      {
        cur++;

        while(LUMEX_XML_IS_CHARTYPEX( // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
          *cur, ctx_digit))
          cur++;
      }

      _cur_lexeme_contents.end = cur;

      _cur_lexeme              = lex_number;
    }
    else if(LUMEX_XML_IS_CHARTYPEX( // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
              *cur, ctx_start_symbol))
    {
      _cur_lexeme_contents.begin = cur;

      while(LUMEX_XML_IS_CHARTYPEX( // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
        *cur, ctx_symbol))
        cur++;

      if(cur[0] == ':')
      {
        if(cur[1] == '*') // namespace test ncname:*
        {
          cur += 2; // :*
        }
        else if(LUMEX_XML_IS_CHARTYPEX( // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                  cur[1], ctx_symbol))  // namespace test qname
        {
          cur++; // :

          while(LUMEX_XML_IS_CHARTYPEX( // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
            *cur, ctx_symbol))
            cur++;
        }
      }

      _cur_lexeme_contents.end = cur;
      _cur_lexeme              = lex_string;
    }
    else { _cur_lexeme = lex_none; }
  }

  _cur = cur;
}

LUMEX_PUBLIC_API
lexeme_t
XPathLexer::current() const
{
  return _cur_lexeme;
}

LUMEX_PUBLIC_API
char_t const *
XPathLexer::current_pos() const
{
  return _cur_lexeme_pos;
}

LUMEX_PUBLIC_API
XPathLexerString const &
XPathLexer::contents() const
{
  LUMEX_ASSERT(_cur_lexeme == lex_var_ref || _cur_lexeme == lex_number || _cur_lexeme == lex_string
               || _cur_lexeme == lex_quoted_string);

  return _cur_lexeme_contents;
}
