// LumexTypeTraitsTopics.cxx11.tests.cpp
// Traits gathered into LumexTypeTraits.hpp from other modules (base64,
// expected, cast, mem, dump, reflection, numeric, fmt) and the ones added
// with them: every public trait of the meta / stream / range / string /
// tuple / value / numeric topics that the LumexTypeTraits,
// LumexStreamTraits and LumexRangeTraits test sources do not cover.
// Every utility suite compiles this file; the concepts are in
// LumexTypeTraitsTopics.cxx20.tests.cpp.
#include <array>
#include <cstddef>
#include <cstdint>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>
#if __cplusplus >= 201703L
#include <optional>
#include <string_view>
#endif

#include <gtest/gtest.h>

#include "lumex/core/utility/traits/LumexTypeTraits.hpp"

namespace traits = lumex::core::utility::traits;

namespace
{
struct plain_t
{
};

struct streamable_t
{
};

std::ostream &
operator<< (std::ostream &stream, streamable_t const &)
{
  return stream;
}

/** Only a non-const lvalue can be streamed. */
struct mutable_streamable_t
{
};

std::ostream &
operator<< (std::ostream &stream, mutable_streamable_t &)
{
  return stream;
}

struct byte_buffer_t
{
  std::size_t
  size () const
  {
    return 0;
  }

  unsigned char
  operator[] (std::size_t) const
  {
    return 0;
  }
};

struct signed_size_t
{
  int
  size () const
  {
    return 0;
  }
};

struct string_size_t
{
  std::string
  size () const
  {
    return std::string ();
  }
};

struct pointer_index_t
{
  void *
  operator[] (std::size_t) const
  {
    return nullptr;
  }
};

/** Optional-like: has_value () and unary *. */
struct maybe_int_t
{
  bool
  has_value () const
  {
    return false;
  }

  int
  operator* () const
  {
    return 0;
  }
};

/** has_value () without operator*: not optional-like. */
struct flag_only_t
{
  bool
  has_value () const
  {
    return false;
  }
};
} // namespace

// ---------------------------------------------------------------------------
// meta
// ---------------------------------------------------------------------------

TEST (LumexTypeTraitsTopicsTest,
      GivenIndirections_WhenIndirectionOf_ThenPointee)
{
  EXPECT_TRUE (
      (std::is_same<traits::meta::indirection_of_t<int *>, int>::value));
  EXPECT_TRUE ((std::is_same<traits::meta::indirection_of_t<int const *>,
                             int const>::value));
  EXPECT_TRUE (
      (std::is_same<traits::meta::indirection_of_t<int &>, int>::value));
  EXPECT_TRUE (
      (std::is_same<traits::meta::indirection_of_t<int>, int>::value));
  // One level only.
  EXPECT_TRUE (
      (std::is_same<traits::meta::indirection_of_t<int **>, int *>::value));
}

// ---------------------------------------------------------------------------
// stream: is_ostreamable (plain `os << value`)
// ---------------------------------------------------------------------------

TEST (LumexTypeTraitsTopicsTest, GivenTypes_WhenIsOstreamable_ThenPlainCheck)
{
  EXPECT_TRUE (traits::stream::is_ostreamable<int>::value);
  EXPECT_TRUE (traits::stream::is_ostreamable<std::string>::value);
  EXPECT_TRUE (traits::stream::is_ostreamable<streamable_t const &>::value);
  EXPECT_FALSE (traits::stream::is_ostreamable<plain_t>::value);
  // Unlike is_streamable, no smart-pointer extras in any standard.
  EXPECT_FALSE (traits::stream::is_ostreamable<std::vector<int>>::value);
}

TEST (LumexTypeTraitsTopicsTest,
      GivenValueCategory_WhenIsOstreamable_ThenExact)
{
  // The operator takes a non-const lvalue: only that form passes.
  EXPECT_TRUE (traits::stream::is_ostreamable<mutable_streamable_t &>::value);
  EXPECT_FALSE (
      traits::stream::is_ostreamable<mutable_streamable_t const &>::value);
  EXPECT_FALSE (traits::stream::is_ostreamable<mutable_streamable_t>::value);
}

TEST (LumexTypeTraitsTopicsTest,
      GivenPacks_WhenAllOstreamable_ThenEveryOneChecked)
{
  EXPECT_TRUE ((traits::stream::all_ostreamable<>::value));
  EXPECT_TRUE ((
      traits::stream::all_ostreamable<int, char const *, std::string>::value));
  EXPECT_FALSE ((traits::stream::all_ostreamable<int, plain_t>::value));
  // Decayed: a reference to a streamable type counts.
  EXPECT_TRUE ((traits::stream::all_ostreamable<streamable_t const &>::value));
#if __cplusplus >= 201402L
  EXPECT_TRUE ((traits::stream::is_ostreamable_v<int const &>));
  EXPECT_FALSE ((traits::stream::all_ostreamable_v<int, plain_t>));
#endif
}

#if __cplusplus < 202002L
TEST (LumexTypeTraitsTopicsTest,
      GivenUniquePtr_WhenBelowCxx20_ThenStreamableSuperset)
{
  // is_streamable counts the address stringify prints below C++20;
  // is_ostreamable reports only what the standard library itself streams
  // (MSVC's STL streams unique_ptr before C++20, libstdc++ does not).
  // Whatever the library, is_ostreamable implies is_streamable.
  EXPECT_TRUE (traits::stream::is_streamable<std::unique_ptr<int>>::value);
  EXPECT_TRUE (!traits::stream::is_ostreamable<std::unique_ptr<int>>::value
               || traits::stream::is_streamable<std::unique_ptr<int>>::value);
  EXPECT_FALSE (traits::stream::is_ostreamable<std::vector<int>>::value);
  EXPECT_FALSE (traits::stream::is_streamable<std::vector<int>>::value);
}
#endif

// ---------------------------------------------------------------------------
// range: container shape
// ---------------------------------------------------------------------------

TEST (LumexTypeTraitsTopicsTest,
      GivenContainers_WhenKeyAndMappedType_ThenShape)
{
  EXPECT_TRUE ((traits::range::has_key_type<std::map<int, int>>::value));
  EXPECT_TRUE ((traits::range::has_mapped_type<std::map<int, int>>::value));
  EXPECT_TRUE ((traits::range::has_key_type<std::set<int>>::value));
  EXPECT_FALSE ((traits::range::has_mapped_type<std::set<int>>::value));
  EXPECT_FALSE ((traits::range::has_key_type<std::vector<int>>::value));
  EXPECT_FALSE ((traits::range::has_mapped_type<int>::value));
}

TEST (LumexTypeTraitsTopicsTest, GivenTypes_WhenHasConvertibleSize_ThenSizeT)
{
  EXPECT_TRUE ((traits::range::has_convertible_size<std::string>::value));
  EXPECT_TRUE ((traits::range::has_convertible_size<std::vector<int>>::value));
  EXPECT_TRUE ((traits::range::has_convertible_size<byte_buffer_t>::value));
  EXPECT_TRUE ((traits::range::has_convertible_size<signed_size_t>::value));
  EXPECT_FALSE ((traits::range::has_convertible_size<string_size_t>::value));
  EXPECT_FALSE ((traits::range::has_convertible_size<int>::value));
  EXPECT_FALSE ((traits::range::has_convertible_size<plain_t>::value));
}

TEST (LumexTypeTraitsTopicsTest,
      GivenTypes_WhenIndexedAccess_ThenConvertsToTarget)
{
  EXPECT_TRUE (
      (traits::range::has_convertible_indexed_access<byte_buffer_t,
                                                     unsigned char>::value));
  EXPECT_TRUE (
      (traits::range::has_convertible_indexed_access<std::string,
                                                     unsigned char>::value));
  EXPECT_TRUE ((traits::range::has_convertible_indexed_access<std::vector<int>,
                                                              long>::value));
  EXPECT_TRUE ((traits::range::has_convertible_indexed_access<char const *,
                                                              char>::value));
  EXPECT_FALSE (
      (traits::range::has_convertible_indexed_access<pointer_index_t,
                                                     unsigned char>::value));
  EXPECT_FALSE ((traits::range::has_convertible_indexed_access<std::list<int>,
                                                               int>::value));
  EXPECT_FALSE (
      (traits::range::has_convertible_indexed_access<int, int>::value));
}

// ---------------------------------------------------------------------------
// string
// ---------------------------------------------------------------------------

TEST (LumexTypeTraitsTopicsTest, GivenTypes_WhenIsAnyString_ThenOwnCharType)
{
  EXPECT_TRUE (traits::string::is_any_string<std::string>::value);
  EXPECT_TRUE (traits::string::is_any_string<std::wstring>::value);
  EXPECT_TRUE (traits::string::is_any_string<std::u16string>::value);
#if __cplusplus >= 201703L
  EXPECT_TRUE (traits::string::is_any_string<std::string_view>::value);
#endif
  EXPECT_FALSE (traits::string::is_any_string<std::vector<char>>::value);
  EXPECT_FALSE (traits::string::is_any_string<char const *>::value);
  EXPECT_FALSE (traits::string::is_any_string<int>::value);
}

// ---------------------------------------------------------------------------
// tuple
// ---------------------------------------------------------------------------

TEST (LumexTypeTraitsTopicsTest, GivenTypes_WhenIsPairLike_ThenTwoElementsOnly)
{
  EXPECT_TRUE ((traits::tuple::is_pair_like<std::pair<int, char>>::value));
  EXPECT_TRUE ((traits::tuple::is_pair_like<std::tuple<int, char>>::value));
  EXPECT_TRUE (
      (traits::tuple::is_pair_like<std::pair<int const, std::string>>::value));
  EXPECT_FALSE ((traits::tuple::is_pair_like<std::tuple<int>>::value));
  EXPECT_FALSE (
      (traits::tuple::is_pair_like<std::tuple<int, int, int>>::value));
  EXPECT_FALSE ((traits::tuple::is_pair_like<std::array<int, 2>>::value));
  EXPECT_FALSE ((traits::tuple::is_pair_like<int>::value));
}

// ---------------------------------------------------------------------------
// value
// ---------------------------------------------------------------------------

TEST (LumexTypeTraitsTopicsTest,
      GivenTypes_WhenIsOptionalLike_ThenShapeChecked)
{
  EXPECT_TRUE (traits::value::is_optional_like<maybe_int_t>::value);
#if __cplusplus >= 201703L
  EXPECT_TRUE (traits::value::is_optional_like<std::optional<int>>::value);
#endif
  EXPECT_FALSE (traits::value::is_optional_like<flag_only_t>::value);
  EXPECT_FALSE (traits::value::is_optional_like<int>::value);
  // A raw pointer dereferences but has no has_value ().
  EXPECT_FALSE (traits::value::is_optional_like<int *>::value);
}

TEST (LumexTypeTraitsTopicsTest, GivenTypes_WhenIsExpected_ThenOnlyExpected)
{
  // The trait needs only the forward declaration of expected.
  typedef lumex::core::expected::result::expected<int, std::string>
      expected_type;
  typedef lumex::core::expected::result::expected<void, int> void_expected;
  EXPECT_TRUE (traits::value::is_expected<expected_type>::value);
  EXPECT_TRUE (traits::value::is_expected<void_expected>::value);
  EXPECT_FALSE (traits::value::is_expected<int>::value);
  EXPECT_FALSE ((traits::value::is_expected<std::pair<int, int>>::value));
  EXPECT_TRUE (traits::value::is_expected_v<expected_type>);
#if __cplusplus >= 202002L
  EXPECT_TRUE (traits::value::is_expected_concept<expected_type>);
  EXPECT_FALSE (traits::value::is_expected_concept<int>);
#endif
}

// ---------------------------------------------------------------------------
// numeric
// ---------------------------------------------------------------------------

TEST (LumexTypeTraitsTopicsTest,
      GivenTypes_WhenIsSafeComparable_ThenArithmetic)
{
  EXPECT_TRUE ((traits::numeric::is_safe_comparable<int, double>::value));
  EXPECT_TRUE (
      (traits::numeric::is_safe_comparable<unsigned char, long long>::value));
  // cv and references are stripped.
  EXPECT_TRUE (
      (traits::numeric::is_safe_comparable<int const &, float &&>::value));
  EXPECT_TRUE ((traits::numeric::is_safe_comparable<bool, char>::value));
  EXPECT_FALSE ((traits::numeric::is_safe_comparable<int, int *>::value));
  EXPECT_FALSE (
      (traits::numeric::is_safe_comparable<std::string, int>::value));
}

TEST (LumexTypeTraitsTopicsTest,
      GivenSafeComparableValue_WhenAddressTaken_ThenLinks)
{
  // A volatile pointer keeps the reference to the `value` symbol in an
  // optimized build too, so a member without a definition fails to link
  // here in every build type, not only without optimization.
  bool const *volatile address
      = &traits::numeric::is_safe_comparable<int, double>::value;
  EXPECT_TRUE (*address);
  address = &traits::numeric::is_safe_comparable<int, int *>::value;
  EXPECT_FALSE (*address);
}
