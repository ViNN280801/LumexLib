// lumex/tests/core/units/api/LumexUnitsLookup.cxx20.tests.cpp
//
// The lookup of UTF-8 text of char8_t (the u8 literals of C++20).
#include <string>

#include <gtest/gtest.h>

#include "lumex/core/units/LumexUnits"
#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"

using namespace lumex::core::units;

#if LUMEX_HAS_CHAR8_T
using u8_view_t = u8_text_view_t;
#endif

TEST (LumexUnitsLookup20Test,
      GivenUtf8Text_WhenLookedUp_ThenTheSameUnitIsFound)
{
#if LUMEX_HAS_CHAR8_T
  result_t<prefixed_unit_t, lookup_error> const micrometre
      = lookup (u8_view_t (u8"\u00B5m"), script::international);
  ASSERT_TRUE (micrometre.has_value ());
  EXPECT_EQ (unit_id::metre, micrometre.value ().unit);
  EXPECT_EQ (prefix_id::micro, micrometre.value ().prefix);
  result_t<prefixed_unit_t, lookup_error> const pascal
      = lookup (u8_view_t (u8"\u041F\u0430"));
  ASSERT_TRUE (pascal.has_value ());
  EXPECT_EQ (unit_id::pascal_unit, pascal.value ().unit);
  EXPECT_FALSE (lookup (u8_view_t (u8"\u00B0\u0421")).has_value ());
  EXPECT_FALSE (lookup (u8_view_t (u8"")).has_value ());
  // the symbols come back as char8_t copies of the same bytes
  u8_symbol_buffer_t const ohm
      = canonical_symbol_u8 (unit_id::ohm, script::international);
  EXPECT_EQ (2u, ohm.size);
  EXPECT_EQ (u8_view_t (u8"\u03A9"), ohm.view ());
  u8_symbol_buffer_t const micro
      = canonical_symbol_u8 (prefix_id::micro, script::russian);
  EXPECT_EQ (u8_view_t (u8"\u043C\u043A"), micro.view ());
  result_t<symbol_buffer_t, format_error> const text
      = format_symbol ({ unit_id::gram, prefix_id::kilo }, script::russian);
  ASSERT_TRUE (text.has_value ());
  EXPECT_EQ (u8_view_t (u8"\u043A\u0433"), text.value ().view_u8 ().view ());
  EXPECT_EQ (4u, text.value ().view_u8 ().size);
  // a text of char8_t is read byte by byte, in either script
  EXPECT_TRUE (lookup (u8_view_t (u8"\u043C\u043A\u043C")).has_value ());
  EXPECT_FALSE (lookup (u8_view_t (u8"\u043C\u043Ag")).has_value ());
#else
  GTEST_SKIP () << "char8_t is not available";
#endif
}
