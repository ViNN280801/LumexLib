#ifndef LUMEX_XML_MACRO_HPP
#define LUMEX_XML_MACRO_HPP

#include "lumex/core/utility/LumexAttributes.hpp"

#ifdef LUMEX_XML_WCHAR_MODE
  #define LUMEX_XML_TEXT(t) L ## t
  #define LUMEX_XML_CHAR wchar_t
#else
  #define LUMEX_XML_TEXT(t) t
  #define LUMEX_XML_CHAR char
#endif

#if defined(__GNUC__) && !defined(__c2__)
  #define LUMEX_XML_UNLIKELY(cond) __builtin_expect(cond, 0)
#else
  #define LUMEX_XML_UNLIKELY(cond) (cond)
#endif

#if defined(_MSC_VER) && !defined(__S3E__) && !defined(_WIN32_WCE)
  #define LUMEX_XML_MSVC_CRT_VERSION _MSC_VER
#elif defined(_WIN32_WCE)
  #define LUMEX_XML_MSVC_CRT_VERSION 1310 // MSVC7.1
#endif

/* ===== For these 4 macros, we need to use the constants from:
LumexXmlMemoryPage.hpp, LumexXmlTypes.hpp, LumexXmlConstants.hpp ===== */
#define LUMEX_XML_GETHEADER_IMPL(object, page, flags) (((reinterpret_cast<char *>(object) - reinterpret_cast<char *>(page)) << 8) | (flags))
#define LUMEX_XML_GETPAGE_IMPL(header) static_cast<XmlMemoryPage *>(const_cast<void *>(static_cast<const void *>(reinterpret_cast<const char *>(&header) - (header >> 8))))

#define LUMEX_XML_GETPAGE(n) LUMEX_XML_GETPAGE_IMPL((n)->header)
#define LUMEX_XML_NODETYPE(n) static_cast<xml_node_type>((n)->header & kxml_memory_page_type_mask)
/* ======================================================================================== */

#ifdef LUMEX_XML_WCHAR_MODE
  #define LUMEX_XML_IS_CHARTYPE_IMPL(c, ct, table) ((static_cast<unsigned int>(c) < 128 ? table[static_cast<unsigned int>(c)] : table[128]) & (ct))
#else
  #define LUMEX_XML_IS_CHARTYPE_IMPL(c, ct, table) (table[static_cast<unsigned char>(c)] & (ct))
#endif

/* ===== For these 2 macros, we need to use the constants from LumexXmlConstants.hpp ===== */
#define LUMEX_XML_IS_CHARTYPE(c, ct) LUMEX_XML_IS_CHARTYPE_IMPL(c, ct, kchartype_table)
#define LUMEX_XML_IS_CHARTYPEX(c, ct) LUMEX_XML_IS_CHARTYPE_IMPL(c, ct, kchartypex_table)
/* ======================================================================================= */

#define LUMEX_XML_SCANCHAR(ch) { if (offset >= size || data[offset] != ch) return false; offset++; }
#define LUMEX_XML_SCANCHARTYPE(ct) { while (offset < size && LUMEX_XML_IS_CHARTYPE(data[offset], ct)) offset++; }

/* ================== Parser macros ================== */
#define LUMEX_XML_ENDSWITH(c, e)        ((c) == (e) || ((c) == 0 && endch == (e)))
#define LUMEX_XML_SKIPWS()              { while (LUMEX_XML_IS_CHARTYPE(*str, ct_space)) ++str; }
#define LUMEX_XML_OPTSET(OPT)           ( optmsk & (OPT) )
#define LUMEX_XML_PUSHNODE(TYPE)        { cursor = append_new_node(cursor, *alloc, TYPE); if (!cursor) LUMEX_XML_THROW_ERROR(status_out_of_memory, str); }
#define LUMEX_XML_POPNODE()             { cursor = cursor->parent; }
#define LUMEX_XML_SCANFOR(X)            { while (*str != 0 && !(X)) ++str; }
#define LUMEX_XML_SCANWHILE(X)          { while (X) ++str; }
#define LUMEX_XML_SCANWHILE_UNROLL(X)   { for (;;) { LUMEX_ATTRIBUTE_MAYBE_UNUSED char_t ss = str[0]; if (LUMEX_XML_UNLIKELY(!(X))) { break; } ss = str[1]; if (LUMEX_XML_UNLIKELY(!(X))) { str += 1; break; } ss = str[2]; if (LUMEX_XML_UNLIKELY(!(X))) { str += 2; break; } ss = str[3]; if (LUMEX_XML_UNLIKELY(!(X))) { str += 3; break; } str += 4; } }
#define LUMEX_XML_ENDSEG()              { ch = *str; *str = 0; ++str; }
#define LUMEX_XML_THROW_ERROR(err, m)   return error_offset = m, error_status = err, static_cast<char_t*>(nullptr)
#define LUMEX_XML_CHECK_ERROR(err, m)   { if (*str == 0) LUMEX_XML_THROW_ERROR(err, m); }
/* ==================================================== */

#endif // !LUMEX_XML_MACRO_HPP
