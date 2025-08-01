#ifndef LUMEX_XML_MACRO_HPP
#define LUMEX_XML_MACRO_HPP

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

#endif // !LUMEX_XML_MACRO_HPP
