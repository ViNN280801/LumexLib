// The tags in_place_tag and unexpect_t (ExpectedTypes.hpp) against
// std::in_place_t ([utility.syn]) and std::unexpect_t ([expected.syn]): empty
// classes with an explicit default constructor, so a tag is written out and is
// never made from `{}`. The tests compile from C++11, so every expected suite
// runs them.

#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

#include "lumex/core/expected/Expected"
#include "lumex/core/utility/traits/LumexTypeTraits.hpp"

namespace
{
using lumex::core::expected::result::expected;
using lumex::core::expected::result::in_place;
using lumex::core::expected::result::in_place_tag;
using lumex::core::expected::result::unexpect;
using lumex::core::expected::result::unexpect_t;

/// A function that takes a `T` by value can be called with `{}`: the copy-list
/// initialization of the parameter, which an explicit default constructor
/// refuses.
template <typename T, typename = void>
struct copy_list_initializable : std::false_type
{
};

template <typename T>
struct copy_list_initializable<
    T, lumex::core::utility::traits::meta::void_t<
           decltype (std::declval<void (&) (T)> () ({}))>> : std::true_type
{
};

template <typename Tag>
void
check_tag ()
{
  static_assert (std::is_default_constructible<Tag>::value,
                 "the default constructor exists");
  static_assert (std::is_trivially_default_constructible<Tag>::value,
                 "and is trivial");
  static_assert (std::is_trivially_copyable<Tag>::value, "a trivial class");
  static_assert (std::is_empty<Tag>::value, "an empty class");
  static_assert (!copy_list_initializable<Tag>::value,
                 "but it is explicit: `Tag tag = {};` does not compile");
  static_assert (!std::is_convertible<int, Tag>::value,
                 "nothing converts to a tag");
  static_assert (!std::is_constructible<Tag, int>::value,
                 "nothing constructs a tag");
}
} // namespace

TEST (ExpectedTagsTest,
      InPlaceTag_IsAnEmptyClassWithAnExplicitDefaultConstructor)
{
  check_tag<in_place_tag> ();
  in_place_tag const written_out{};
  in_place_tag const value_initialized = in_place_tag ();
  static_cast<void> (written_out);
  static_cast<void> (value_initialized);
  SUCCEED ();
}

TEST (ExpectedTagsTest,
      UnexpectTag_IsAnEmptyClassWithAnExplicitDefaultConstructor)
{
  check_tag<unexpect_t> ();
  unexpect_t const written_out{};
  unexpect_t const value_initialized = unexpect_t ();
  static_cast<void> (written_out);
  static_cast<void> (value_initialized);
  SUCCEED ();
}

TEST (ExpectedTagsTest, TheConstants_SelectTheConstructors)
{
  expected<int, int> const value (in_place, 3);
  expected<int, int> const error (unexpect, 4);
  expected<void, int> const success (in_place);
  expected<void, int> const failure (unexpect, 5);

  ASSERT_TRUE (value.has_value ());
  EXPECT_EQ (*value, 3);
  ASSERT_FALSE (error.has_value ());
  EXPECT_EQ (error.error (), 4);
  EXPECT_TRUE (success.has_value ());
  ASSERT_FALSE (failure.has_value ());
  EXPECT_EQ (failure.error (), 5);
}
