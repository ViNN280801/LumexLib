// NOLINTBEGIN
#define LUMEX_IMPLEMENTATION

#include "lumex/core/utility/LumexAssert.hpp"

#include "lumex/xml/constants/XmlConstants.hpp"
#include "lumex/xml/utility/XmlMacros.hpp"
#include "lumex/xml/utility/XmlUtils.hpp"

#include "XmlParser.hpp"

using namespace Lumex::Xml::Utility;
using namespace Lumex::Xml::Text;
using namespace Lumex::Xml::Constants;

struct gap {
  char_t *end;
  size_t size;

  gap() : end(nullptr), size(0) {}

  // Push new gap, move s count bytes further (skipping the gap).
  // Collapse previous gap.
  void
  push(char_t *&s, size_t count)
  {
    if(end) // there was a gap already; collapse it
    {
      // Move [old_gap_end, new_gap_start) to [old_gap_start, ...)
      LUMEX_ASSERT(s >= end);
      memmove(end - size, end, (s - end) * sizeof(char_t));
    }

    s += count; // end of current gap

    // "merge" two gaps
    end = s;
    size += count;
  }

  // Collapse all gaps, return past-the-end pointer
  char_t *
  flush(char_t *s)
  {
    if(end)
    {
      // Move [old_gap_end, current_pos) to [old_gap_start, ...)
      LUMEX_ASSERT(s >= end);
      memmove(end - size, end, (s - end) * sizeof(char_t));

      return s - size;
    }
    else
      return s;
  }
};

inline char_t *
strconv_escape(char_t *s, gap &g)
{
  char_t *stre = s + 1;

  switch(*stre)
  {
  case '#': // &#...
  {
    unsigned int ucsc = 0;

    if(stre[1] == 'x') // &#x... (hex code)
    {
      stre += 2;

      char_t ch = *stre;

      if(ch == ';') return stre;

      for(;;)
      {
        if(static_cast<unsigned int>(ch - '0') <= 9)
          ucsc = 16 * ucsc + (ch - '0');
        else if(static_cast<unsigned int>((ch | ' ') - 'a') <= 5)
          ucsc = 16 * ucsc + ((ch | ' ') - 'a' + 10);
        else if(ch == ';')
          break;
        else // cancel
          return stre;

        ch = *++stre;
      }

      ++stre;
    }
    else // &#... (dec code)
    {
      char_t ch = *++stre;

      if(ch == ';') return stre;

      for(;;)
      {
        if(static_cast<unsigned int>(ch - '0') <= 9)
          ucsc = 10 * ucsc + (ch - '0');
        else if(ch == ';')
          break;
        else // cancel
          return stre;

        ch = *++stre;
      }

      ++stre;
    }

#ifdef LUMEX_XML_WCHAR_MODE
    s = reinterpret_cast<char_t *>(wchar_writer::any(reinterpret_cast<wchar_writer::value_type>(s), ucsc));
#else
    s = reinterpret_cast<char_t *>(utf8_writer::any(reinterpret_cast<uint8_t *>(s), ucsc));
#endif

    g.push(s, stre - s);
    return stre;
  }

  case 'a': // &a
  {
    ++stre;

    if(*stre == 'm') // &am
    {
      if(*++stre == 'p' && *++stre == ';') // &amp;
      {
        *s++ = '&';
        ++stre;

        g.push(s, stre - s);
        return stre;
      }
    }
    else if(*stre == 'p') // &ap
    {
      if(*++stre == 'o' && *++stre == 's' && *++stre == ';') // &apos;
      {
        *s++ = '\'';
        ++stre;

        g.push(s, stre - s);
        return stre;
      }
    }
    break;
  }

  case 'g': // &g
  {
    if(*++stre == 't' && *++stre == ';') // &gt;
    {
      *s++ = '>';
      ++stre;

      g.push(s, stre - s);
      return stre;
    }
    break;
  }

  case 'l': // &l
  {
    if(*++stre == 't' && *++stre == ';') // &lt;
    {
      *s++ = '<';
      ++stre;

      g.push(s, stre - s);
      return stre;
    }
    break;
  }

  case 'q': // &q
  {
    if(*++stre == 'u' && *++stre == 'o' && *++stre == 't' && *++stre == ';') // &quot;
    {
      *s++ = '"';
      ++stre;

      g.push(s, stre - s);
      return stre;
    }
    break;
  }

  default: break;
  }

  return stre;
}

typedef char_t *(*strconv_attribute_t)(char_t *, char_t);

template <typename opt_escape> struct strconv_attribute_impl {
  static char_t *
  parse_wnorm(char_t *str, char_t end_quote)
  {
    gap g;

    // trim leading whitespaces
    if(LUMEX_XML_IS_CHARTYPE(*str, ct_space))
    {
      char_t *tmp = str;

      do ++tmp;
      while(LUMEX_XML_IS_CHARTYPE(*tmp, ct_space));

      g.push(str, tmp - str);
    }

    while(true)
    {
      LUMEX_XML_SCANWHILE_UNROLL(!LUMEX_XML_IS_CHARTYPE(ss, ct_parse_attr_ws | ct_space));

      if(*str == end_quote)
      {
        char_t *tmp = g.flush(str);

        do *tmp-- = 0;
        while(LUMEX_XML_IS_CHARTYPE(*tmp, ct_space));

        return str + 1;
      }
      else if(LUMEX_XML_IS_CHARTYPE(*str, ct_space))
      {
        *str++ = ' ';

        if(LUMEX_XML_IS_CHARTYPE(*str, ct_space))
        {
          char_t *tmp = str + 1;
          while(LUMEX_XML_IS_CHARTYPE(*tmp, ct_space)) ++tmp;

          g.push(str, tmp - str);
        }
      }
      else if(opt_escape::value && *str == '&') { str = strconv_escape(str, g); }
      else if(!*str) { return nullptr; }
      else
        ++str;
    }
  }

  static char_t *
  parse_wconv(char_t *str, char_t end_quote)
  {
    gap g;

    while(true)
    {
      LUMEX_XML_SCANWHILE_UNROLL(!LUMEX_XML_IS_CHARTYPE(ss, ct_parse_attr_ws));

      if(*str == end_quote)
      {
        *g.flush(str) = 0;

        return str + 1;
      }
      if(LUMEX_XML_IS_CHARTYPE(*str, ct_space))
      {
        if(*str == '\r')
        {
          *str++ = ' ';

          if(*str == '\n') g.push(str, 1);
        }
        else
          *str++ = ' ';
      }
      else if(opt_escape::value && *str == '&') { str = strconv_escape(str, g); }
      else if(!*str) { return nullptr; }
      else
        ++str;
    }
  }

  static char_t *
  parse_eol(char_t *str, char_t end_quote)
  {
    gap g;

    while(true)
    {
      LUMEX_XML_SCANWHILE_UNROLL(!LUMEX_XML_IS_CHARTYPE(ss, ct_parse_attr));

      if(*str == end_quote)
      {
        *g.flush(str) = 0;

        return str + 1;
      }
      else if(*str == '\r')
      {
        *str++ = '\n';

        if(*str == '\n') g.push(str, 1);
      }
      else if(opt_escape::value && *str == '&') { str = strconv_escape(str, g); }
      else if(!*str) { return nullptr; }
      else
        ++str;
    }
  }

  static char_t *
  parse_simple(char_t *str, char_t end_quote)
  {
    gap g;

    while(true)
    {
      LUMEX_XML_SCANWHILE_UNROLL(!LUMEX_XML_IS_CHARTYPE(ss, ct_parse_attr));

      if(*str == end_quote)
      {
        *g.flush(str) = 0;

        return str + 1;
      }
      else if(opt_escape::value && *str == '&') { str = strconv_escape(str, g); }
      else if(!*str) { return nullptr; }
      else
        ++str;
    }
  }
};

inline strconv_attribute_t
get_strconv_attribute(unsigned int optmask)
{
  static_assert(kparse_escapes == 0x10 && kparse_eol == 0x20 && kparse_wconv_attribute == 0x40
                && kparse_wnorm_attribute == 0x80);

  switch((optmask >> 4) & 15) // get bitmask for flags (wnorm wconv eol escapes); this simultaneously checks 4 options
                              // from LUMEX_ASSERTion above
  {
  case 0: return strconv_attribute_impl<opt_false>::parse_simple;
  case 1: return strconv_attribute_impl<opt_true>::parse_simple;
  case 2: return strconv_attribute_impl<opt_false>::parse_eol;
  case 3: return strconv_attribute_impl<opt_true>::parse_eol;
  case 4: return strconv_attribute_impl<opt_false>::parse_wconv;
  case 5: return strconv_attribute_impl<opt_true>::parse_wconv;
  case 6: return strconv_attribute_impl<opt_false>::parse_wconv;
  case 7: return strconv_attribute_impl<opt_true>::parse_wconv;
  case 8: return strconv_attribute_impl<opt_false>::parse_wnorm;
  case 9: return strconv_attribute_impl<opt_true>::parse_wnorm;
  case 10: return strconv_attribute_impl<opt_false>::parse_wnorm;
  case 11: return strconv_attribute_impl<opt_true>::parse_wnorm;
  case 12: return strconv_attribute_impl<opt_false>::parse_wnorm;
  case 13: return strconv_attribute_impl<opt_true>::parse_wnorm;
  case 14: return strconv_attribute_impl<opt_false>::parse_wnorm;
  case 15: return strconv_attribute_impl<opt_true>::parse_wnorm;
  default: LUMEX_ASSERT(false); return nullptr; // unreachable
  }
}

inline char_t *
strconv_comment(char_t *str, char_t endch)
{
  gap g;

  while(true)
  {
    LUMEX_XML_SCANWHILE_UNROLL(!LUMEX_XML_IS_CHARTYPE(ss, ct_parse_comment));

    if(*str == '\r') // Either a single 0x0d or 0x0d 0x0a pair
    {
      *str++ = '\n'; // replace first one with 0x0a

      if(*str == '\n') g.push(str, 1);
    }
    else if(str[0] == '-' && str[1] == '-' && LUMEX_XML_ENDSWITH(str[2], '>')) // comment ends here
    {
      *g.flush(str) = 0;

      return str + (str[2] == '>' ? 3 : 2);
    }
    else if(*str == 0) { return nullptr; }
    else
      ++str;
  }
}

inline char_t *
strconv_cdata(char_t *str, char_t endch)
{
  gap g;

  while(true)
  {
    LUMEX_XML_SCANWHILE_UNROLL(!LUMEX_XML_IS_CHARTYPE(ss, ct_parse_cdata));

    if(*str == '\r') // Either a single 0x0d or 0x0d 0x0a pair
    {
      *str++ = '\n'; // replace first one with 0x0a

      if(*str == '\n') g.push(str, 1);
    }
    else if(str[0] == ']' && str[1] == ']' && LUMEX_XML_ENDSWITH(str[2], '>')) // CDATA ends here
    {
      *g.flush(str) = 0;

      return str + 1;
    }
    else if(*str == 0) { return nullptr; }
    else
      ++str;
  }
}

typedef char_t *(*strconv_pcdata_t)(char_t *);

template <typename opt_trim, typename opt_eol, typename opt_escape> struct strconv_pcdata_impl {
  static char_t *
  parse(char_t *str)
  {
    gap g;

    char_t *begin = str;

    while(true)
    {
      LUMEX_XML_SCANWHILE_UNROLL(!LUMEX_XML_IS_CHARTYPE(ss, ct_parse_pcdata));

      if(*str == '<') // PCDATA ends here
      {
        char_t *end = g.flush(str);

        if(opt_trim::value)
          while(end > begin && LUMEX_XML_IS_CHARTYPE(end[-1], ct_space)) --end;

        *end = 0;

        return str + 1;
      }
      else if(opt_eol::value && *str == '\r') // Either a single 0x0d or 0x0d 0x0a pair
      {
        *str++ = '\n'; // replace first one with 0x0a

        if(*str == '\n') g.push(str, 1);
      }
      else if(opt_escape::value && *str == '&') { str = strconv_escape(str, g); }
      else if(*str == 0)
      {
        char_t *end = g.flush(str);

        if(opt_trim::value)
          while(end > begin
                && LUMEX_XML_IS_CHARTYPE( // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                  end[-1], ct_space))
            --end;

        *end = 0;

        return str;
      }
      else
        ++str;
    }
  }
};

inline strconv_pcdata_t
get_strconv_pcdata(unsigned int optmask)
{
  static_assert(Constants::kparse_escapes == 0x10 && Constants::kparse_eol == 0x20
                && Constants::kparse_trim_pcdata == 0x0800);

  switch(((optmask >> 4) & 3) | ((optmask >> 9) & 4)) // get bitmask for flags (trim eol escapes); this simultaneously
                                                      // checks 3 options from LUMEX_ASSERTion above
  {
  case 0: return strconv_pcdata_impl<opt_false, opt_false, opt_false>::parse;
  case 1: return strconv_pcdata_impl<opt_false, opt_false, opt_true>::parse;
  case 2: return strconv_pcdata_impl<opt_false, opt_true, opt_false>::parse;
  case 3: return strconv_pcdata_impl<opt_false, opt_true, opt_true>::parse;
  case 4: return strconv_pcdata_impl<opt_true, opt_false, opt_false>::parse;
  case 5: return strconv_pcdata_impl<opt_true, opt_false, opt_true>::parse;
  case 6: return strconv_pcdata_impl<opt_true, opt_true, opt_false>::parse;
  case 7: return strconv_pcdata_impl<opt_true, opt_true, opt_true>::parse;
  default: LUMEX_ASSERT(false); return nullptr; // unreachable
  }
}

LUMEX_PUBLIC_API
inline XmlParser::XmlParser(XmlAllocator *alloc_) : alloc(alloc_), error_status(status_ok) {}

LUMEX_PUBLIC_API
char_t *
XmlParser::parse_doctype_primitive(char_t *str)
{
  if(*str == '"' || *str == '\'')
  {
    // quoted string
    char_t ch = *str++;
    LUMEX_XML_SCANFOR(*str == ch);
    if(!*str) LUMEX_XML_THROW_ERROR(status_bad_doctype, str);

    str++;
  }
  else if(str[0] == '<' && str[1] == '?')
  {
    // <? ... ?>
    str += 2;
    LUMEX_XML_SCANFOR(str[0] == '?' && str[1] == '>'); // no need for ENDSWITH because ?> can't terminate proper doctype
    if(!*str) LUMEX_XML_THROW_ERROR(status_bad_doctype, str);

    str += 2;
  }
  else if(str[0] == '<' && str[1] == '!' && str[2] == '-' && str[3] == '-')
  {
    str += 4;
    LUMEX_XML_SCANFOR(str[0] == '-' && str[1] == '-'
                      && str[2] == '>'); // no need for ENDSWITH because --> can't terminate proper doctype
    if(!*str) LUMEX_XML_THROW_ERROR(status_bad_doctype, str);

    str += 3;
  }
  else
    LUMEX_XML_THROW_ERROR(status_bad_doctype, str);

  return str;
}

LUMEX_PUBLIC_API
char_t *
XmlParser::parse_doctype_ignore(char_t *str)
{
  size_t depth = 0;

  LUMEX_ASSERT(str[0] == '<' && str[1] == '!' && str[2] == '[');
  str += 3;

  while(*str)
  {
    if(str[0] == '<' && str[1] == '!' && str[2] == '[')
    {
      // nested ignore section
      str += 3;
      depth++;
    }
    else if(str[0] == ']' && str[1] == ']' && str[2] == '>')
    {
      // ignore section end
      str += 3;

      if(depth == 0) return str;

      depth--;
    }
    else
      str++;
  }

  LUMEX_XML_THROW_ERROR(status_bad_doctype, str);
}

LUMEX_PUBLIC_API
char_t *
XmlParser::parse_doctype_group(char_t *str, char_t endch)
{
  size_t depth = 0;

  LUMEX_ASSERT((str[0] == '<' || str[0] == 0) && str[1] == '!');
  str += 2;

  while(*str)
  {
    if(str[0] == '<' && str[1] == '!' && str[2] != '-')
    {
      if(str[2] == '[')
      {
        // ignore
        str = parse_doctype_ignore(str);
        if(!str) return str;
      }
      else
      {
        // some control group
        str += 2;
        depth++;
      }
    }
    else if(str[0] == '<' || str[0] == '"' || str[0] == '\'')
    {
      // unknown tag (forbidden), or some primitive group
      str = parse_doctype_primitive(str);
      if(!str) return str;
    }
    else if(*str == '>')
    {
      if(depth == 0) return str;

      depth--;
      str++;
    }
    else
      str++;
  }

  if(depth != 0 || endch != '>') LUMEX_XML_THROW_ERROR(status_bad_doctype, str);

  return str;
}

LUMEX_PUBLIC_API
char_t *
XmlParser::parse_exclamation(char_t *str, XmlNodeBase *cursor, unsigned int optmsk, char_t endch)
{
  // parse node contents, starting with exclamation mark
  ++str;

  if(*str == '-') // '<!-...'
  {
    ++str;

    if(*str == '-') // '<!--...'
    {
      ++str;

      if(LUMEX_XML_OPTSET(Constants::kparse_comments))
      {
        LUMEX_XML_PUSHNODE(node_comment); // Append a new node on the tree.
        cursor->value = str;              // Save the offset.
      }

      if(LUMEX_XML_OPTSET(Constants::kparse_eol) && LUMEX_XML_OPTSET(Constants::kparse_comments))
      {
        str = strconv_comment(str, endch);

        if(!str) LUMEX_XML_THROW_ERROR(status_bad_comment, cursor->value);
      }
      else
      {
        // Scan for terminating '-->'.
        LUMEX_XML_SCANFOR(str[0] == '-' && str[1] == '-' && LUMEX_XML_ENDSWITH(str[2], '>'));
        LUMEX_XML_CHECK_ERROR(status_bad_comment, str);

        if(LUMEX_XML_OPTSET(Constants::kparse_comments))
          *str = 0; // Zero-terminate this segment at the first terminating '-'.

        str += (str[2] == '>' ? 3 : 2); // Step over the '\0->'.
      }
    }
    else
      LUMEX_XML_THROW_ERROR(status_bad_comment, str);
  }
  else if(*str == '[')
  {
    // '<![CDATA[...'
    if(*++str == 'C' && *++str == 'D' && *++str == 'A' && *++str == 'T' && *++str == 'A' && *++str == '[')
    {
      ++str;

      if(LUMEX_XML_OPTSET(Constants::kparse_cdata))
      {
        LUMEX_XML_PUSHNODE(node_cdata); // Append a new node on the tree.
        cursor->value = str;            // Save the offset.

        if(LUMEX_XML_OPTSET(Constants::kparse_eol))
        {
          str = strconv_cdata(str, endch);

          if(!str) LUMEX_XML_THROW_ERROR(status_bad_cdata, cursor->value);
        }
        else
        {
          // Scan for terminating ']]>'.
          LUMEX_XML_SCANFOR(str[0] == ']' && str[1] == ']' && LUMEX_XML_ENDSWITH(str[2], '>'));
          LUMEX_XML_CHECK_ERROR(status_bad_cdata, str);

          *str++ = 0; // Zero-terminate this segment.
        }
      }
      else // Flagged for discard, but we still have to scan for the terminator.
      {
        // Scan for terminating ']]>'.
        LUMEX_XML_SCANFOR(str[0] == ']' && str[1] == ']' && LUMEX_XML_ENDSWITH(str[2], '>'));
        LUMEX_XML_CHECK_ERROR(status_bad_cdata, str);

        ++str;
      }

      str += (str[1] == '>' ? 2 : 1); // Step over the last ']>'.
    }
    else
      LUMEX_XML_THROW_ERROR(status_bad_cdata, str);
  }
  else if(str[0] == 'D' && str[1] == 'O' && str[2] == 'C' && str[3] == 'T' && str[4] == 'Y' && str[5] == 'P'
          && LUMEX_XML_ENDSWITH(str[6], 'E'))
  {
    str -= 2;

    if(cursor->parent) LUMEX_XML_THROW_ERROR(status_bad_doctype, str);

    char_t *mark = str + 9;

    str          = parse_doctype_group(str, endch);
    if(!str) return str;

    LUMEX_ASSERT((*str == 0 && endch == '>') || *str == '>');
    if(*str) *str++ = 0;

    if(LUMEX_XML_OPTSET(Constants::kparse_doctype))
    {
      while(LUMEX_XML_IS_CHARTYPE(*mark, ct_space)) ++mark;

      LUMEX_XML_PUSHNODE(node_doctype);

      cursor->value = mark;
    }
  }
  else if(*str == 0 && endch == '-')
    LUMEX_XML_THROW_ERROR(status_bad_comment, str);
  else if(*str == 0 && endch == '[')
    LUMEX_XML_THROW_ERROR(status_bad_cdata, str);
  else
    LUMEX_XML_THROW_ERROR(status_unrecognized_tag, str);

  return str;
}

LUMEX_PUBLIC_API
char_t *
XmlParser::parse_question(char_t *str, XmlNodeBase *&ref_cursor, unsigned int optmsk, char_t endch)
{
  // load into registers
  XmlNodeBase *cursor = ref_cursor;
  char_t ch           = 0;

  // parse node contents, starting with question mark
  ++str;

  // read PI target
  char_t *target = str;

  if(!LUMEX_XML_IS_CHARTYPE(*str, ct_start_symbol)) LUMEX_XML_THROW_ERROR(status_bad_pi, str);

  LUMEX_XML_SCANWHILE(LUMEX_XML_IS_CHARTYPE(*str, ct_symbol));
  LUMEX_XML_CHECK_ERROR(status_bad_pi, str);

  // determine node type; stricmp / strcasecmp is not portable
  bool declaration
    = (target[0] | ' ') == 'x' && (target[1] | ' ') == 'm' && (target[2] | ' ') == 'l' && target + 3 == str;

  if(declaration ? LUMEX_XML_OPTSET(Constants::kparse_declaration) : LUMEX_XML_OPTSET(Constants::kparse_pi))
  {
    if(declaration)
    {
      // disallow non top-level declarations
      if(cursor->parent) LUMEX_XML_THROW_ERROR(status_bad_pi, str);

      LUMEX_XML_PUSHNODE(node_declaration);
    }
    else { LUMEX_XML_PUSHNODE(node_pi); }

    cursor->name = target;

    LUMEX_XML_ENDSEG();

    // parse value/attributes
    if(ch == '?')
    {
      // empty node
      if(!LUMEX_XML_ENDSWITH(*str, '>')) LUMEX_XML_THROW_ERROR(status_bad_pi, str);
      str += (*str == '>');

      LUMEX_XML_POPNODE();
    }
    else if(LUMEX_XML_IS_CHARTYPE(ch, ct_space))
    {
      LUMEX_XML_SKIPWS();

      // scan for tag end
      char_t *value = str;

      LUMEX_XML_SCANFOR(str[0] == '?' && LUMEX_XML_ENDSWITH(str[1], '>'));
      LUMEX_XML_CHECK_ERROR(status_bad_pi, str);

      if(declaration)
      {
        // replace ending ? with / so that 'element' terminates properly
        *str = '/';

        // we exit from this function with cursor at node_declaration, which is a signal to parse() to go to
        // LOC_ATTRIBUTES
        str = value;
      }
      else
      {
        // store value and step over >
        cursor->value = value;

        LUMEX_XML_POPNODE();

        LUMEX_XML_ENDSEG();

        str += (*str == '>');
      }
    }
    else
      LUMEX_XML_THROW_ERROR(status_bad_pi, str);
  }
  else
  {
    // scan for tag end
    LUMEX_XML_SCANFOR(str[0] == '?' && LUMEX_XML_ENDSWITH(str[1], '>'));
    LUMEX_XML_CHECK_ERROR(status_bad_pi, str);

    str += (str[1] == '>' ? 2 : 1);
  }

  // store from registers
  ref_cursor = cursor;

  return str;
}

LUMEX_PUBLIC_API
char_t *
XmlParser::parse_tree(char_t *str, XmlNodeBase *root, unsigned int optmsk, char_t endch)
{
  strconv_attribute_t strconv_attribute = get_strconv_attribute(optmsk);
  strconv_pcdata_t strconv_pcdata       = get_strconv_pcdata(optmsk);

  char_t ch                             = 0;
  XmlNodeBase *cursor                   = root;
  char_t *mark                          = str;
  char_t *merged_pcdata                 = str;

  while(*str != 0)
  {
    if(*str == '<')
    {
      ++str;

    LOC_TAG:
      if(LUMEX_XML_IS_CHARTYPE(*str, ct_start_symbol)) // '<#...'
      {
        LUMEX_XML_PUSHNODE(node_element); // Append a new node to the tree.

        cursor->name = str;

        LUMEX_XML_SCANWHILE_UNROLL(LUMEX_XML_IS_CHARTYPE(ss, ct_symbol)); // Scan for a terminator.
        LUMEX_XML_ENDSEG();                                               // Save char in 'ch', terminate & step over.

        if(ch == '>')
        {
          // end of tag
        }
        else if(LUMEX_XML_IS_CHARTYPE(ch, ct_space))
        {
        LOC_ATTRIBUTES:
          while(true)
          {
            LUMEX_XML_SKIPWS(); // Eat any whitespace.

            if(LUMEX_XML_IS_CHARTYPE(*str, ct_start_symbol)) // <... #...
            {
              XmlAttributeBase *a = append_new_attribute(cursor, *alloc); // Make space for this attribute.
              if(!a) LUMEX_XML_THROW_ERROR(status_out_of_memory, str);

              a->name = str; // Save the offset.

              LUMEX_XML_SCANWHILE_UNROLL(LUMEX_XML_IS_CHARTYPE(ss, ct_symbol)); // Scan for a terminator.
              LUMEX_XML_ENDSEG(); // Save char in 'ch', terminate & step over.

              if(LUMEX_XML_IS_CHARTYPE(ch, ct_space))
              {
                LUMEX_XML_SKIPWS(); // Eat any whitespace.

                ch = *str;
                ++str;
              }

              if(ch == '=') // '<... #=...'
              {
                LUMEX_XML_SKIPWS(); // Eat any whitespace.

                if(*str == '"' || *str == '\'') // '<... #="...'
                {
                  ch = *str;      // Save quote char to avoid breaking on "''" -or- '""'.
                  ++str;          // Step over the quote.
                  a->value = str; // Save the offset.

                  str      = strconv_attribute(str, ch);

                  if(!str) LUMEX_XML_THROW_ERROR(status_bad_attribute, a->value);

                  // After this line the loop continues from the start;
                  // Whitespaces, / and > are ok, symbols and EOF are wrong,
                  // everything else will be detected
                  if(LUMEX_XML_IS_CHARTYPE(*str, ct_start_symbol)) LUMEX_XML_THROW_ERROR(status_bad_attribute, str);
                }
                else
                  LUMEX_XML_THROW_ERROR(status_bad_attribute, str);
              }
              else
                LUMEX_XML_THROW_ERROR(status_bad_attribute, str);
            }
            else if(*str == '/')
            {
              ++str;

              if(*str == '>')
              {
                LUMEX_XML_POPNODE();
                str++;
                break;
              }
              else if(*str == 0 && endch == '>')
              {
                LUMEX_XML_POPNODE();
                break;
              }
              else
                LUMEX_XML_THROW_ERROR(status_bad_start_element, str);
            }
            else if(*str == '>')
            {
              ++str;

              break;
            }
            else if(*str == 0 && endch == '>') { break; }
            else
              LUMEX_XML_THROW_ERROR(status_bad_start_element, str);
          }

          // !!!
        }
        else if(ch == '/') // '<#.../'
        {
          if(!LUMEX_XML_ENDSWITH(*str, '>')) LUMEX_XML_THROW_ERROR(status_bad_start_element, str);

          LUMEX_XML_POPNODE(); // Pop.

          str += (*str == '>');
        }
        else if(ch == 0)
        {
          // we stepped over null terminator, backtrack & handle closing tag
          --str;

          if(endch != '>') LUMEX_XML_THROW_ERROR(status_bad_start_element, str);
        }
        else
          LUMEX_XML_THROW_ERROR(status_bad_start_element, str);
      }
      else if(*str == '/')
      {
        ++str;

        mark         = str;

        char_t *name = cursor->name;
        if(!name) LUMEX_XML_THROW_ERROR(status_end_element_mismatch, mark);

        while(LUMEX_XML_IS_CHARTYPE(*str, ct_symbol))
          if(*str++ != *name++) LUMEX_XML_THROW_ERROR(status_end_element_mismatch, mark);

        if(*name)
        {
          if(*str == 0 && name[0] == endch && name[1] == 0)
            LUMEX_XML_THROW_ERROR(status_bad_end_element, str);
          else
            LUMEX_XML_THROW_ERROR(status_end_element_mismatch, mark);
        }

        LUMEX_XML_POPNODE(); // Pop.

        LUMEX_XML_SKIPWS();

        if(*str == 0)
        {
          if(endch != '>') LUMEX_XML_THROW_ERROR(status_bad_end_element, str);
        }
        else
        {
          if(*str != '>') LUMEX_XML_THROW_ERROR(status_bad_end_element, str);
          ++str;
        }
      }
      else if(*str == '?') // '<?...'
      {
        str = parse_question(str, cursor, optmsk, endch);
        if(!str) return str;

        LUMEX_ASSERT(cursor);
        if(LUMEX_XML_NODETYPE(cursor) == node_declaration) goto LOC_ATTRIBUTES;
      }
      else if(*str == '!') // '<!...'
      {
        str = parse_exclamation(str, cursor, optmsk, endch);
        if(!str) return str;
      }
      else if(*str == 0 && endch == '?')
        LUMEX_XML_THROW_ERROR(status_bad_pi, str);
      else
        LUMEX_XML_THROW_ERROR(status_unrecognized_tag, str);
    }
    else
    {
      mark = str; // Save this offset while searching for a terminator.

      LUMEX_XML_SKIPWS(); // Eat whitespace if no genuine PCDATA here.

      if(*str == '<' || !*str)
      {
        // We skipped some whitespace characters because otherwise we would take the tag branch instead of
        // PCDATA one
        LUMEX_ASSERT(mark != str);

        if(!LUMEX_XML_OPTSET(Constants::kparse_ws_pcdata | Constants::kparse_ws_pcdata_single)
           || LUMEX_XML_OPTSET(Constants::kparse_trim_pcdata))
        {
          continue;
        }
        else if(LUMEX_XML_OPTSET(Constants::kparse_ws_pcdata_single))
        {
          if(str[0] != '<' || str[1] != '/' || cursor->first_child) continue;
        }
      }

      if(!LUMEX_XML_OPTSET(Constants::kparse_trim_pcdata)) str = mark;

      if(cursor->parent || LUMEX_XML_OPTSET(Constants::kparse_fragment))
      {
        char_t *parsed_pcdata = str;

        str                   = strconv_pcdata(str);

        if(LUMEX_XML_OPTSET(Constants::kparse_embed_pcdata) && cursor->parent && !cursor->first_child && !cursor->value)
        {
          cursor->value = parsed_pcdata; // Save the offset.
        }
        else if(LUMEX_XML_OPTSET(Constants::kparse_merge_pcdata) && cursor->first_child
                && LUMEX_XML_NODETYPE(cursor->first_child->prev_sibling_c) == node_pcdata)
        {
          LUMEX_ASSERT(merged_pcdata >= cursor->first_child->prev_sibling_c->value);

          // Catch up to the end of last parsed value; only needed for the first fragment.
          merged_pcdata += strlength(merged_pcdata);

          size_t length = strlength(parsed_pcdata);

          // Must use memmove instead of memcpy as this move may overlap
          memmove(merged_pcdata, parsed_pcdata, (length + 1) * sizeof(char_t));
          merged_pcdata += length;
        }
        else
        {
          XmlNodeBase *prev_cursor = cursor;
          LUMEX_XML_PUSHNODE(node_pcdata); // Append a new node on the tree.

          cursor->value = parsed_pcdata; // Save the offset.
          merged_pcdata = parsed_pcdata; // Used for parse_merge_pcdata above, cheaper to save unconditionally

          cursor        = prev_cursor; // Pop since this is a standalone.
        }

        if(!*str) break;
      }
      else
      {
        LUMEX_XML_SCANFOR(*str == '<'); // '...<'
        if(!*str) break;

        ++str;
      }

      // We're after '<'
      goto LOC_TAG;
    }
  }

  // check that last tag is closed
  if(cursor != root) LUMEX_XML_THROW_ERROR(status_end_element_mismatch, str);

  return str;
}

LUMEX_PUBLIC_API
#ifdef LUMEX_XML_WCHAR_MODE
char_t *
XmlParser::parse_skip_bom(char_t *str)
{
  unsigned int bom = 0xfeff;
  return (str[0] == static_cast<wchar_t>(bom)) ? str + 1 : str;
}
#else
char_t *
XmlParser::parse_skip_bom(char_t *str)
{
  return (str[0] == '\xef' && str[1] == '\xbb' && str[2] == '\xbf') ? str + 3 : str;
}
#endif

LUMEX_PUBLIC_API
bool
XmlParser::has_element_node_siblings(XmlNodeBase *node)
{
  while(node)
  {
    if(LUMEX_XML_NODETYPE(node) == node_element) return true;

    node = node->next_sibling;
  }

  return false;
}

LUMEX_PUBLIC_API
XmlParseResult
XmlParser::parse(char_t *buffer, size_t length, Document::XmlDocumentBase *xmldoc, XmlNodeBase *root,
                 unsigned int optmsk)
{
  // early-out for empty documents
  if(length == 0)
    return make_parse_result(LUMEX_XML_OPTSET(Constants::kparse_fragment) ? status_ok : status_no_document_element);

  // get last child of the root before parsing
  XmlNodeBase *last_root_child = (root->first_child != nullptr) ? root->first_child->prev_sibling_c + 0 : nullptr;

  // create parser on stack
  XmlParser parser(static_cast<XmlAllocator *>(xmldoc));

  // save last character and make buffer zero-terminated (speeds up parsing)
  char_t endch       = buffer[length - 1];
  buffer[length - 1] = 0;

  // skip BOM to make sure it does not end up as part of parse output
  char_t *buffer_data = parse_skip_bom(buffer);

  // perform actual parsing
  parser.parse_tree(buffer_data, root, optmsk, endch);

  XmlParseResult result
    = make_parse_result(parser.error_status, (parser.error_offset != nullptr) ? parser.error_offset - buffer : 0);
  LUMEX_ASSERT(result.offset >= 0 && static_cast<size_t>(result.offset) <= length);

  if(result)
  {
    // since we removed last character, we have to handle the only possible false positive (stray <)
    if(endch == '<') return make_parse_result(status_unrecognized_tag, length - 1);

    // check if there are any element nodes parsed
    XmlNodeBase *first_root_child_parsed
      = (last_root_child != nullptr) ? last_root_child->next_sibling + 0 : root->first_child + 0;

    if(!LUMEX_XML_OPTSET(Constants::kparse_fragment) && !has_element_node_siblings(first_root_child_parsed))
      return make_parse_result(status_no_document_element, length - 1);
  }
  else
  {
    // roll back offset if it occurs on a null terminator in the source buffer
    if(result.offset > 0 && static_cast<size_t>(result.offset) == length - 1 && endch == 0) result.offset--;
  }

  return result;
}
// NOLINTEND
