// lumex/tests/core/unicode/LumexUnicodeGlobalNames.cxx11.tests.cpp
//
// Including the unicode umbrella adds no name to the global namespace. This
// file declares global names spelled like the module's and uses them
// unqualified. A header that writes `using namespace lumex::core::unicode...;`
// (or a using declaration of one of its names) at file scope makes every such
// use ambiguous, and this file stops compiling. The probes stand in for a
// consumer's own declarations, so they carry the library's spellings.
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/core/unicode/LumexUnicode"

/// @brief Base of every global type probe of this file.
struct own_name_t
{
};

// lumex::core::unicode::utf
struct utf8_counter : own_name_t
{
};
struct utf16_counter : own_name_t
{
};
struct utf32_counter : own_name_t
{
};
struct utf8_writer : own_name_t
{
};
struct utf16_writer : own_name_t
{
};
struct utf32_writer : own_name_t
{
};
struct latin1_writer : own_name_t
{
};
struct utf8_decoder : own_name_t
{
};
struct latin1_decoder : own_name_t
{
};
struct wchar_decoder : own_name_t
{
};
template <bool SwapBytes> struct utf16_decoder : own_name_t
{
};
template <bool SwapBytes> struct utf32_decoder : own_name_t
{
};
template <unsigned int Size> struct wchar_selector : own_name_t
{
};

// lumex::core::unicode::convert: global functions with the same names; an
// unqualified call is ambiguous if the module's overloads were visible.
inline int
to_utf8 (int value)
{
  return value + 1;
}
inline int
to_wide (int value)
{
  return value + 2;
}

// The namespaces of the module itself.
namespace utf
{
struct probe_t : own_name_t
{
};
} // namespace utf
namespace convert
{
struct probe_t : own_name_t
{
};
} // namespace convert

namespace
{
template <typename T>
bool
is_own_name ()
{
  return std::is_base_of<own_name_t, T>::value;
}

TEST (UnicodeGlobalNames,
      GivenUmbrellaIncluded_WhenPolicyNamesUsed_ThenOwnTypes)
{
  EXPECT_TRUE (is_own_name<utf8_counter> ());
  EXPECT_TRUE (is_own_name<utf16_counter> ());
  EXPECT_TRUE (is_own_name<utf32_counter> ());
  EXPECT_TRUE (is_own_name<utf8_writer> ());
  EXPECT_TRUE (is_own_name<utf16_writer> ());
  EXPECT_TRUE (is_own_name<utf32_writer> ());
  EXPECT_TRUE (is_own_name<latin1_writer> ());
  EXPECT_TRUE (is_own_name<utf8_decoder> ());
  EXPECT_TRUE (is_own_name<latin1_decoder> ());
  EXPECT_TRUE (is_own_name<wchar_decoder> ());
  EXPECT_TRUE (is_own_name<utf16_decoder<false>> ());
  EXPECT_TRUE (is_own_name<utf32_decoder<true>> ());
  EXPECT_TRUE (is_own_name<wchar_selector<2>> ());
}

TEST (UnicodeGlobalNames,
      GivenUmbrellaIncluded_WhenConversionNamesCalled_ThenOwnFunctions)
{
  EXPECT_EQ (to_utf8 (1), 2);
  EXPECT_EQ (to_wide (1), 3);
}

TEST (UnicodeGlobalNames,
      GivenUmbrellaIncluded_WhenNamespaceNamesUsed_ThenOwnNamespaces)
{
  EXPECT_TRUE (is_own_name<utf::probe_t> ());
  EXPECT_TRUE (is_own_name<convert::probe_t> ());
}

TEST (UnicodeGlobalNames,
      GivenUmbrellaIncluded_WhenLibraryNamesQualified_ThenLibraryEntities)
{
  EXPECT_FALSE (is_own_name<lumex::core::unicode::utf::utf8_decoder> ());
  EXPECT_FALSE (is_own_name<lumex::core::unicode::utf::utf8_writer> ());
  EXPECT_EQ (lumex::core::unicode::convert::to_utf8 (L"ab"),
             std::string ("ab"));
  EXPECT_EQ (lumex::core::unicode::convert::to_wide ("ab"),
             std::wstring (L"ab"));
}
} // namespace
