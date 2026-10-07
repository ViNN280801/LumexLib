// The other half of the global names test, for the standard library that has
// std::span: a program with a using-directive for namespace std must still
// name std::span, std::dynamic_extent, std::as_bytes and std::byte without
// an ambiguity after it includes lumex/core/span/LumexSpan. A global alias of
// any of those names in LumexLib makes this file fail to compile.
#include <cstddef>
#include <type_traits>
#if __has_include(<span>)
#include <span>
#endif

#include <gtest/gtest.h>

#include "lumex/core/span/LumexSpan"

#if LUMEX_HAS_STD_SPAN

namespace
{
std::size_t
count_with_std_names (int (&values)[3])
{
  using namespace std;
  span<int> view (values);
  span<int const, 3> fixed (values);
  static_assert (is_same<decltype (view), std::span<int>>::value,
                 "span is std::span");
  static_assert (decltype (view)::extent == dynamic_extent,
                 "dynamic_extent is std::dynamic_extent");
  static_assert (
      is_same<decltype (as_bytes (view)), std::span<std::byte const>>::value,
      "as_bytes is std::as_bytes");
  static_assert (is_same<decltype (as_writable_bytes (view)),
                         std::span<std::byte>>::value,
                 "as_writable_bytes is std::as_writable_bytes");
  return view.size () + fixed.size () + as_bytes (view).size ();
}
} // namespace

TEST (LumexSpanGlobalNamesStdTest,
      GivenUsingNamespaceStd_WhenLumexIncluded_ThenStdNamesAreUnambiguous)
{
  int values[3] = { 1, 2, 3 };
  EXPECT_EQ (count_with_std_names (values), 3 + 3 + 3 * sizeof (int));
}

#else

TEST (LumexSpanGlobalNamesStdTest,
      GivenLibraryWithoutStdSpan_WhenCxx20_ThenSkipped)
{
  GTEST_SKIP () << "the standard library has no std::span";
}

#endif
