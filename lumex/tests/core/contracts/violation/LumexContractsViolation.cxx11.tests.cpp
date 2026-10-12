// The enumerations of <contracts> of C++26 (values, names) and
// contract_violation: accessors, copying, construction from an object with the
// accessors of std::contracts::contract_violation.

#include <cstdint>
#include <string>
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/core/contracts/violation/LumexContractsViolation.hpp"

namespace
{
using namespace lumex::core::contracts;

TEST (ContractsViolationEnums, HaveTheValuesOfTheStandard)
{
  // [support.contract.enum]
  EXPECT_EQ (static_cast<int> (assertion_kind::pre), 1);
  EXPECT_EQ (static_cast<int> (assertion_kind::post), 2);
  EXPECT_EQ (static_cast<int> (assertion_kind::assert), 3);
  EXPECT_EQ (static_cast<int> (evaluation_semantic::ignore), 1);
  EXPECT_EQ (static_cast<int> (evaluation_semantic::observe), 2);
  EXPECT_EQ (static_cast<int> (evaluation_semantic::enforce), 3);
  EXPECT_EQ (static_cast<int> (evaluation_semantic::quick_enforce), 4);
  EXPECT_EQ (static_cast<int> (detection_mode::predicate_false), 1);
  EXPECT_EQ (static_cast<int> (detection_mode::evaluation_exception), 2);
}

TEST (ContractsViolationEnums, AreScopedEnumerations)
{
  EXPECT_FALSE ((std::is_convertible<assertion_kind, int>::value));
  EXPECT_FALSE ((std::is_convertible<evaluation_semantic, int>::value));
  EXPECT_FALSE ((std::is_convertible<detection_mode, int>::value));
}

TEST (ContractsViolationEnums, HaveNames)
{
  EXPECT_STREQ (to_string (assertion_kind::pre), "pre");
  EXPECT_STREQ (to_string (assertion_kind::post), "post");
  EXPECT_STREQ (to_string (assertion_kind::assert), "assert");
  EXPECT_STREQ (to_string (static_cast<assertion_kind> (77)), "unknown");
  EXPECT_STREQ (to_string (evaluation_semantic::ignore), "ignore");
  EXPECT_STREQ (to_string (evaluation_semantic::observe), "observe");
  EXPECT_STREQ (to_string (evaluation_semantic::enforce), "enforce");
  EXPECT_STREQ (to_string (evaluation_semantic::quick_enforce),
                "quick_enforce");
  EXPECT_STREQ (to_string (static_cast<evaluation_semantic> (0)), "unknown");
  EXPECT_STREQ (to_string (detection_mode::predicate_false),
                "predicate_false");
  EXPECT_STREQ (to_string (detection_mode::evaluation_exception),
                "evaluation_exception");
  EXPECT_STREQ (to_string (static_cast<detection_mode> (9)), "unknown");
  static_assert (to_string (assertion_kind::assert)[0] == 'a', "constexpr");
}

TEST (ContractsViolationEnums, OnlyEnforceAndQuickEnforceTerminate)
{
  EXPECT_FALSE (is_terminating (evaluation_semantic::ignore));
  EXPECT_FALSE (is_terminating (evaluation_semantic::observe));
  EXPECT_TRUE (is_terminating (evaluation_semantic::enforce));
  EXPECT_TRUE (is_terminating (evaluation_semantic::quick_enforce));
}

TEST (ContractsViolation, KeepsWhatItWasGiven)
{
  source_location const where ("v.cpp", "void f ()", 12, 3);
  contract_violation const violation ("x > 0", assertion_kind::assert,
                                      evaluation_semantic::observe,
                                      detection_mode::predicate_false, where);
  EXPECT_STREQ (violation.comment (), "x > 0");
  EXPECT_EQ (violation.kind (), assertion_kind::assert);
  EXPECT_EQ (violation.semantic (), evaluation_semantic::observe);
  EXPECT_EQ (violation.detection_mode (), detection_mode::predicate_false);
  EXPECT_FALSE (violation.is_terminating ());
  EXPECT_TRUE (violation.location () == where);
}

TEST (ContractsViolation, IsTerminatingFollowsTheSemantic)
{
  source_location const where;
  for (int value = 1; value <= 4; ++value)
    {
      evaluation_semantic const semantic
          = static_cast<evaluation_semantic> (value);
      contract_violation const violation ("", assertion_kind::pre, semantic,
                                          detection_mode::predicate_false,
                                          where);
      EXPECT_EQ (violation.is_terminating (), is_terminating (semantic))
          << value;
    }
}

TEST (ContractsViolation, DetectionModeEvaluationException)
{
  contract_violation const violation (
      "f ()", assertion_kind::post, evaluation_semantic::enforce,
      detection_mode::evaluation_exception, source_location ());
  EXPECT_EQ (violation.detection_mode (),
             detection_mode::evaluation_exception);
  EXPECT_EQ (violation.kind (), assertion_kind::post);
  EXPECT_TRUE (violation.is_terminating ());
}

TEST (ContractsViolation, NullCommentIsEmpty)
{
  contract_violation const violation (
      nullptr, assertion_kind::assert, evaluation_semantic::observe,
      detection_mode::predicate_false, source_location ());
  EXPECT_STREQ (violation.comment (), "");
}

TEST (ContractsViolation, IsACopyableValue)
{
  contract_violation const original (
      "a == b", assertion_kind::assert, evaluation_semantic::enforce,
      detection_mode::predicate_false, source_location ("c.cpp", "g", 4, 5));
  contract_violation copy = original;
  EXPECT_STREQ (copy.comment (), "a == b");
  EXPECT_TRUE (copy.location () == original.location ());
  copy = contract_violation (
      "other", assertion_kind::pre, evaluation_semantic::ignore,
      detection_mode::predicate_false, source_location ());
  EXPECT_STREQ (copy.comment (), "other");
  EXPECT_STREQ (original.comment (), "a == b");
  EXPECT_TRUE (std::is_trivially_copyable<contract_violation>::value);
  EXPECT_TRUE (
      (std::is_nothrow_constructible<contract_violation, char const *,
                                     assertion_kind, evaluation_semantic,
                                     detection_mode, source_location>::value));
}

TEST (ContractsViolation, IsALiteralValue)
{
  constexpr contract_violation violation (
      "p", assertion_kind::assert, evaluation_semantic::quick_enforce,
      detection_mode::predicate_false, source_location ("l.cpp", "h", 8, 9));
  static_assert (violation.kind () == assertion_kind::assert, "kind");
  static_assert (violation.semantic () == evaluation_semantic::quick_enforce,
                 "semantic");
  static_assert (violation.is_terminating (), "terminating");
  static_assert (violation.location ().line () == 8, "line");
  static_assert (violation.comment ()[0] == 'p', "comment");
  SUCCEED ();
}

// An object with the accessors of std::contracts::contract_violation
// ([support.contract.violation]) but other types: what a C++26 library has.
namespace standard_like
{
enum class assertion_kind : unsigned char
{
  pre = 1,
  post = 2,
  assert = 3
};
enum class evaluation_semantic : unsigned char
{
  ignore = 1,
  observe = 2,
  enforce = 3,
  quick_enforce = 4
};
enum class detection_mode : unsigned char
{
  predicate_false = 1,
  evaluation_exception = 2
};

struct location_t
{
  char const *
  file_name () const noexcept
  {
    return "std.cpp";
  }
  char const *
  function_name () const noexcept
  {
    return "void std_f ()";
  }
  std::uint_least32_t
  line () const noexcept
  {
    return 31;
  }
  std::uint_least32_t
  column () const noexcept
  {
    return 4;
  }
};

struct contract_violation
{
  char const *
  comment () const noexcept
  {
    return "n > 0";
  }
  standard_like::detection_mode
  detection_mode () const noexcept
  {
    return standard_like::detection_mode::evaluation_exception;
  }
  bool
  is_terminating () const noexcept
  {
    return true;
  }
  standard_like::assertion_kind
  kind () const noexcept
  {
    return standard_like::assertion_kind::post;
  }
  location_t
  location () const noexcept
  {
    return location_t ();
  }
  standard_like::evaluation_semantic
  semantic () const noexcept
  {
    return standard_like::evaluation_semantic::quick_enforce;
  }
};

struct null_comment
{
  char const *
  comment () const noexcept
  {
    return nullptr;
  }
  standard_like::detection_mode
  detection_mode () const noexcept
  {
    return standard_like::detection_mode::predicate_false;
  }
  standard_like::assertion_kind
  kind () const noexcept
  {
    return standard_like::assertion_kind::assert;
  }
  location_t
  location () const noexcept
  {
    return location_t ();
  }
  standard_like::evaluation_semantic
  semantic () const noexcept
  {
    return standard_like::evaluation_semantic::observe;
  }
};

struct missing_accessors
{
  char const *
  comment () const noexcept
  {
    return "x";
  }
};
} // namespace standard_like

TEST (ContractsViolation, ConvertsFromTheShapeOfTheStandardClass)
{
  standard_like::contract_violation const other;
  contract_violation const violation (other);
  EXPECT_STREQ (violation.comment (), "n > 0");
  EXPECT_EQ (violation.kind (), assertion_kind::post);
  EXPECT_EQ (violation.semantic (), evaluation_semantic::quick_enforce);
  EXPECT_EQ (violation.detection_mode (),
             detection_mode::evaluation_exception);
  EXPECT_TRUE (violation.is_terminating ());
  EXPECT_STREQ (violation.location ().file_name (), "std.cpp");
  EXPECT_STREQ (violation.location ().function_name (), "void std_f ()");
  EXPECT_EQ (violation.location ().line (), 31u);
  EXPECT_EQ (violation.location ().column (), 4u);
}

TEST (ContractsViolation, ConversionTakesANullCommentAsEmpty)
{
  contract_violation const violation ((standard_like::null_comment ()));
  EXPECT_STREQ (violation.comment (), "");
  EXPECT_EQ (violation.semantic (), evaluation_semantic::observe);
}

TEST (ContractsViolation, ConversionIsExplicitAndSelective)
{
  EXPECT_TRUE (
      (std::is_constructible<contract_violation,
                             standard_like::contract_violation>::value));
  EXPECT_FALSE ((std::is_convertible<standard_like::contract_violation,
                                     contract_violation>::value));
  EXPECT_FALSE ((std::is_constructible<contract_violation, int>::value));
  EXPECT_FALSE (
      (std::is_constructible<contract_violation,
                             standard_like::missing_accessors>::value));
  EXPECT_FALSE (
      (std::is_constructible<contract_violation, std::string>::value));
}
} // namespace
