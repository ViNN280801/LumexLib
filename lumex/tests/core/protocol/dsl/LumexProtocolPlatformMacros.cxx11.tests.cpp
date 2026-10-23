// The collision guard of the protocol tables (lumex/core/protocol/dsl/
// LumexProtocolPlatformMacros.hpp): the list of platform macro names is
// sorted (the binary search depends on it), every listed name is found, the
// names the design names are listed, and ordinary row identifiers pass. The
// failing side (a table row DELETE without X_AS) is in
// cmake.protocol_compile_checks.

#include <cstddef>
#include <cstring>
#include <string>

#include <gtest/gtest.h>

#include "lumex/core/protocol/LumexProtocol"
#include "lumex/tests/core/protocol/LumexProtocolTestTables.hpp"

namespace
{
namespace dsl = lumex::core::protocol::dsl;

// A valid list passes the standalone check, at namespace scope.
LUMEX_PROTOCOL_CHECK_IDENTIFIERS (WIDGET_ROWS);
LUMEX_PROTOCOL_CHECK_IDENTIFIERS (LARGE_ROWS);

struct holder
{
  // And at class scope.
  LUMEX_PROTOCOL_CHECK_IDENTIFIERS (WIDGET_V2_ROWS);
};
} // namespace

TEST (LumexProtocolPlatformMacrosTest,
      GivenTheList_WhenRead_ThenItIsStrictlySortedByByteValue)
{
  ASSERT_GT (dsl::platform_macro_count (), 100u);
  for (std::size_t index = 1; index < dsl::platform_macro_count (); ++index)
    EXPECT_LT (std::strcmp (dsl::platform_macro_name (index - 1),
                            dsl::platform_macro_name (index)),
               0)
        << dsl::platform_macro_name (index - 1) << " / "
        << dsl::platform_macro_name (index);
}

TEST (LumexProtocolPlatformMacrosTest,
      GivenEveryListedName_WhenChecked_ThenItIsFound)
{
  for (std::size_t index = 0; index < dsl::platform_macro_count (); ++index)
    EXPECT_TRUE (
        dsl::is_platform_macro_name (dsl::platform_macro_name (index)))
        << dsl::platform_macro_name (index);
}

TEST (LumexProtocolPlatformMacrosTest,
      GivenTheNamesOfTheDesign_WhenChecked_ThenEveryOneIsRejected)
{
  char const *const names[]
      = { "ERROR", "DELETE", "IN",    "OUT",   "OPTIONAL", "interface",
          "min",   "max",    "near",  "far",   "TRUE",     "FALSE",
          "ECHO",  "major",  "minor", "small", "VOID",     "CONST" };
  for (char const *name : names)
    EXPECT_TRUE (dsl::is_platform_macro_name (name)) << name;
}

TEST (LumexProtocolPlatformMacrosTest,
      GivenOrdinaryIdentifiers_WhenChecked_ThenTheyAreAccepted)
{
  char const *const names[]
      = { "PING",    "ERASE_LOG", "DELETE_RECORD", "DELET",
          "DELETEX", "Delete",    "delete_",       "MINIMUM",
          "error",   "",          "zzzzzzz",       "AAAAAAA",
          "_",       "CMD_099" };
  for (char const *name : names)
    EXPECT_FALSE (dsl::is_platform_macro_name (name)) << name;
}

TEST (LumexProtocolPlatformMacrosTest,
      GivenTheCheck_WhenUsedInAStaticAssert_ThenItIsAConstantExpression)
{
  static_assert (dsl::is_platform_macro_name ("DELETE"), "DELETE is listed");
  static_assert (!dsl::is_platform_macro_name ("ERASE_LOG"),
                 "ERASE_LOG is not");
  SUCCEED ();
}
