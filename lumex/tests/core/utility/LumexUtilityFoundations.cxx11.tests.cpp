// LumexUtilityFoundations.cxx11.tests.cpp
// The umbrella LumexUtility declares the shared C++11 building blocks without
// any other include: the index sequences (sequence/), the function objects
// identity and less (functional/) and the trait forms of the concepts of
// traits::meta and traits::string. Each block has its own tests in the
// directory of its header; this file only fails when the umbrella stops
// including one of them.
#include <cstddef>
#include <string>
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/core/utility/LumexUtility"

namespace utility = lumex::core::utility;

namespace
{
struct foundation_base_t
{
};

struct foundation_derived_t : foundation_base_t
{
};
} // namespace

TEST (LumexUtilityFoundationsTest,
      GivenUmbrella_WhenIncluded_ThenSequencesDeclared)
{
  static_assert (
      std::is_same<utility::sequence::make_index_sequence<3>,
                   utility::sequence::index_sequence<0, 1, 2>>::value,
      "make_index_sequence is declared");
  static_assert (std::is_same<utility::sequence::index_sequence_for<int, int>,
                              utility::sequence::index_sequence<0, 1>>::value,
                 "index_sequence_for is declared");
  static_assert (
      std::is_same<utility::sequence::make_integer_sequence<int, 2>,
                   utility::sequence::integer_sequence<int, 0, 1>>::value,
      "make_integer_sequence is declared");
  EXPECT_EQ (utility::sequence::make_index_sequence<5>::size (), 5U);
}

TEST (LumexUtilityFoundationsTest,
      GivenUmbrella_WhenIncluded_ThenFunctionObjectsDeclared)
{
  int value = 3;
  EXPECT_EQ (&utility::functional::identity () (value), &value);
  EXPECT_TRUE (utility::functional::less () (1, 2));
  EXPECT_FALSE (utility::functional::less () (2, 1));
}

TEST (LumexUtilityFoundationsTest,
      GivenUmbrella_WhenIncluded_ThenTraitFormsDeclared)
{
  namespace traits = utility::traits;
  static_assert (traits::meta::is_pointer_to_class<std::string *>::value,
                 "is_pointer_to_class is declared");
  static_assert (traits::meta::is_lvalue_ref_to_class<std::string &>::value,
                 "is_lvalue_ref_to_class is declared");
  static_assert (traits::meta::is_complete_type<std::string>::value,
                 "is_complete_type is declared");
  static_assert (traits::meta::preserves_cv<int, int const>::value,
                 "preserves_cv is declared");
  static_assert (traits::meta::is_derived_from<foundation_derived_t,
                                               foundation_base_t>::value,
                 "is_derived_from is declared");
  static_assert (traits::string::is_string_convertible<char const *>::value,
                 "is_string_convertible is declared");
  SUCCEED ();
}
