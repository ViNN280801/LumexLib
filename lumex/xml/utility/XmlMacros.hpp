/*
 * Copyright (c) 2026 Vladislav Semykin <vladislav.semykin@gmail.com>
 *
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of
 * charge, to any person obtaining a copy
 * of this software and associated
 * documentation files (the "Software"), to deal
 * in the Software without
 * restriction, including without limitation the rights
 * to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the
 * Software, and to permit persons to whom the Software is
 * furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice
 * and this permission notice shall be included in
 * all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT
 * WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO
 * THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH
 * THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/**
 * @file XmlMacros.hpp
 * @brief Preprocessor macros of the XML module: the character mode, the
 * decoding of header words and the scanning macros of the parser.
 * @details `LUMEX_XML_CHAR` and `LUMEX_XML_TEXT` select `char` or `wchar_t`
 * (with the `L` prefix for literals) depending on `LUMEX_XML_WCHAR_MODE`.
 * Every node and attribute starts with a header word that holds the byte
 * distance from its memory page, shifted left by 8 bits, and flags in the low
 * 8 bits; `LUMEX_XML_GETHEADER_IMPL` builds that word, `LUMEX_XML_GETPAGE`
 * recovers the page and `LUMEX_XML_NODETYPE` the node type.
 * `LUMEX_XML_IS_CHARTYPE` and `LUMEX_XML_IS_CHARTYPEX` test a character
 * against the class tables of `XmlConstants.hpp`.
 *
 * The scanning macros (`LUMEX_XML_SKIPWS`, `LUMEX_XML_SCANFOR`,
 * `LUMEX_XML_PUSHNODE`, `LUMEX_XML_THROW_ERROR` and the others) are written
 * for the bodies of the parser and refer to its local variables, such as
 * `str`, `cursor`, `alloc` and `optmsk`. Several macros also use names from
 * `XmlConstants.hpp`, `XmlMemoryPage.hpp` and `XmlTypes.hpp` that must be
 * visible where they are expanded.
 */
#ifndef LUMEX_XML_UTILITY_XML_MACROS_HPP
#define LUMEX_XML_UTILITY_XML_MACROS_HPP

#include "lumex/core/utility/attr/LumexAttributes.hpp"

#ifdef LUMEX_XML_WCHAR_MODE
#define LUMEX_XML_TEXT(t) L##t
#define LUMEX_XML_CHAR wchar_t
#else
#define LUMEX_XML_TEXT(t) t
#define LUMEX_XML_CHAR char
#endif

#if defined(_MSC_VER) && !defined(__S3E__) && !defined(_WIN32_WCE)
#define LUMEX_XML_MSVC_CRT_VERSION _MSC_VER
#elif defined(_WIN32_WCE)
#define LUMEX_XML_MSVC_CRT_VERSION 1310 // MSVC7.1
#endif

/* ===== For these 4 macros, we need to use the constants from:
LumexXmlMemoryPage.hpp, LumexXmlTypes.hpp, LumexXmlConstants.hpp ===== */
#define LUMEX_XML_GETHEADER_IMPL(object, page, flags)                         \
  static_cast<uintptr_t> (                                                    \
      ((reinterpret_cast<char *> (object) - reinterpret_cast<char *> (page))  \
       << 8)                                                                  \
      | (flags))
#define LUMEX_XML_GETPAGE_IMPL(header)                                        \
  static_cast<XmlMemoryPage *> (                                              \
      const_cast<void *> (static_cast<const void *> (                         \
          reinterpret_cast<const char *> (&header) - (header >> 8))))

#define LUMEX_XML_GETPAGE(n) LUMEX_XML_GETPAGE_IMPL ((n)->header)
#define LUMEX_XML_NODETYPE(n)                                                 \
  static_cast<xml_node_type> ((n)->header & kxml_memory_page_type_mask)
/* ========================================================================================
 */

#ifdef LUMEX_XML_WCHAR_MODE
#define LUMEX_XML_IS_CHARTYPE_IMPL(c, ct, table)                              \
  ((static_cast<unsigned int> (c) < 128                                       \
        ? table[static_cast<unsigned int> (c)]                                \
        : table[128])                                                         \
   & (ct))
#else
#define LUMEX_XML_IS_CHARTYPE_IMPL(c, ct, table)                              \
  (table[static_cast<unsigned char> (c)] & (ct))
#endif

/* ===== For these 2 macros, we need to use the constants from
 * LumexXmlConstants.hpp ===== */
#define LUMEX_XML_IS_CHARTYPE(c, ct)                                          \
  LUMEX_XML_IS_CHARTYPE_IMPL (c, ct, kchartype_table)
#define LUMEX_XML_IS_CHARTYPEX(c, ct)                                         \
  LUMEX_XML_IS_CHARTYPE_IMPL (c, ct, kchartypex_table)
/* =======================================================================================
 */

#define LUMEX_XML_SCANCHAR(ch)                                                \
  {                                                                           \
    if (offset >= size || data[offset] != ch)                                 \
      return false;                                                           \
    offset++;                                                                 \
  }
#define LUMEX_XML_SCANCHARTYPE(ct)                                            \
  {                                                                           \
    while (offset < size && LUMEX_XML_IS_CHARTYPE (data[offset], ct))         \
      offset++;                                                               \
  }

/* ================== Parser macros ================== */
#define LUMEX_XML_ENDSWITH(c, e) ((c) == (e) || ((c) == 0 && endch == (e)))
#define LUMEX_XML_SKIPWS()                                                    \
  {                                                                           \
    while (LUMEX_XML_IS_CHARTYPE (*str, ct_space))                            \
      ++str;                                                                  \
  }
#define LUMEX_XML_OPTSET(OPT) (optmsk & (OPT))
#define LUMEX_XML_PUSHNODE(TYPE)                                              \
  {                                                                           \
    cursor = append_new_node (cursor, *alloc, TYPE);                          \
    if (!cursor)                                                              \
      LUMEX_XML_THROW_ERROR (status_out_of_memory, str);                      \
  }
#define LUMEX_XML_POPNODE()                                                   \
  {                                                                           \
    cursor = cursor->parent;                                                  \
  }
#define LUMEX_XML_SCANFOR(X)                                                  \
  {                                                                           \
    while (*str != 0 && !(X))                                                 \
      ++str;                                                                  \
  }
#define LUMEX_XML_SCANWHILE(X)                                                \
  {                                                                           \
    while (X)                                                                 \
      ++str;                                                                  \
  }
#define LUMEX_XML_SCANWHILE_UNROLL(X)                                         \
  {                                                                           \
    for (;;)                                                                  \
      {                                                                       \
        LUMEX_ATTRIBUTE_MAYBE_UNUSED char_t ss = str[0];                      \
        if (LUMEX_ATTRIBUTE_UNLIKELY_COND (!(X)))                             \
          {                                                                   \
            break;                                                            \
          }                                                                   \
        ss = str[1];                                                          \
        if (LUMEX_ATTRIBUTE_UNLIKELY_COND (!(X)))                             \
          {                                                                   \
            str += 1;                                                         \
            break;                                                            \
          }                                                                   \
        ss = str[2];                                                          \
        if (LUMEX_ATTRIBUTE_UNLIKELY_COND (!(X)))                             \
          {                                                                   \
            str += 2;                                                         \
            break;                                                            \
          }                                                                   \
        ss = str[3];                                                          \
        if (LUMEX_ATTRIBUTE_UNLIKELY_COND (!(X)))                             \
          {                                                                   \
            str += 3;                                                         \
            break;                                                            \
          }                                                                   \
        str += 4;                                                             \
      }                                                                       \
  }
#define LUMEX_XML_ENDSEG()                                                    \
  {                                                                           \
    ch = *str;                                                                \
    *str = 0;                                                                 \
    ++str;                                                                    \
  }
#define LUMEX_XML_THROW_ERROR(err, m)                                         \
  return error_offset = m, error_status = err, static_cast<char_t *> (nullptr)
#define LUMEX_XML_CHECK_ERROR(err, m)                                         \
  {                                                                           \
    if (*str == 0)                                                            \
      LUMEX_XML_THROW_ERROR (err, m);                                         \
  }
/* ==================================================== */

#endif // !LUMEX_XML_UTILITY_XML_MACROS_HPP
