// lumex/tests/core/utility/traits/LumexTypeTraitsTwins.cxx20.tests.cpp
// The C++11 traits of LumexTypeTraitsTwins.cxx11.tests.cpp against the C++20
// concepts they stand for (and std::derived_from), on the same types: each
// pair must give the same answer. The header declares the concepts only where
// the compiler and the standard library have them (LUMEX_HAS_STD_CONCEPTS for
// <concepts>): GCC 8 accepts -std=c++2a without them, so there the tests skip.
// The suites from C++20 up compile this file together with the C++11 file.
#if __has_include(<concepts>)
#include <concepts>
#endif
#include <cstddef>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"
#include "lumex/core/utility/traits/LumexTypeTraits.hpp"

namespace traits = lumex::core::utility::traits;

namespace
{
struct agree_class_t
{
  int value;
};

struct agree_base_t
{
  virtual ~agree_base_t () = default;
};

struct agree_derived_t : agree_base_t
{
};

struct agree_private_t : private agree_base_t
{
};

struct agree_left_t : agree_base_t
{
};

struct agree_right_t : agree_base_t
{
};

struct agree_diamond_t : agree_left_t, agree_right_t
{
};

union agree_union_t
{
  int integer;
  float real;
};

enum class agree_enum_t
{
  first
};

struct agree_incomplete_t;

struct agree_to_string_t
{
  operator std::string () const { return std::string (); }
};

struct agree_explicit_to_string_t
{
  explicit
  operator std::string () const
  {
    return std::string ();
  }
};
} // namespace

#if LUMEX_HAS_STD_CONCEPTS
// One pair, one type: the trait and the concept must give the same bool.
#define LUMEX_TRAIT_AGREES(TRAIT, CONCEPT, TYPE)                              \
  EXPECT_EQ (traits::meta::TRAIT<TYPE>::value, traits::meta::CONCEPT<TYPE>)

#define LUMEX_TRAIT2_AGREES(TRAIT, CONCEPT, FIRST, SECOND)                    \
  EXPECT_EQ ((traits::meta::TRAIT<FIRST, SECOND>::value),                     \
             (traits::meta::CONCEPT<FIRST, SECOND>))
#endif

TEST (LumexTypeTraitsTwinsConceptTest,
      GivenTypes_WhenPointerToClass_ThenTraitAgreesWithConcept)
{
#if LUMEX_HAS_STD_CONCEPTS
  LUMEX_TRAIT_AGREES (is_pointer_to_class, PointerToClass, agree_class_t *);
  LUMEX_TRAIT_AGREES (is_pointer_to_class, PointerToClass,
                      agree_class_t const *);
  LUMEX_TRAIT_AGREES (is_pointer_to_class, PointerToClass,
                      agree_class_t *const);
  LUMEX_TRAIT_AGREES (is_pointer_to_class, PointerToClass,
                      agree_incomplete_t *);
  LUMEX_TRAIT_AGREES (is_pointer_to_class, PointerToClass, agree_union_t *);
  LUMEX_TRAIT_AGREES (is_pointer_to_class, PointerToClass, agree_enum_t *);
  LUMEX_TRAIT_AGREES (is_pointer_to_class, PointerToClass, int *);
  LUMEX_TRAIT_AGREES (is_pointer_to_class, PointerToClass, void *);
  LUMEX_TRAIT_AGREES (is_pointer_to_class, PointerToClass, void (*) ());
  LUMEX_TRAIT_AGREES (is_pointer_to_class, PointerToClass,
                      int agree_class_t::*);
  LUMEX_TRAIT_AGREES (is_pointer_to_class, PointerToClass, agree_class_t **);
  LUMEX_TRAIT_AGREES (is_pointer_to_class, PointerToClass, agree_class_t);
  LUMEX_TRAIT_AGREES (is_pointer_to_class, PointerToClass, agree_class_t &);
  LUMEX_TRAIT_AGREES (is_pointer_to_class, PointerToClass, agree_class_t[2]);
  LUMEX_TRAIT_AGREES (is_pointer_to_class, PointerToClass, std::nullptr_t);
  LUMEX_TRAIT_AGREES (is_pointer_to_class, PointerToClass, int);
  LUMEX_TRAIT_AGREES (is_pointer_to_class, PointerToClass, void);
#else
  GTEST_SKIP () << "the standard library has no <concepts>";
#endif
}

TEST (LumexTypeTraitsTwinsConceptTest,
      GivenTypes_WhenLvalueRefToClass_ThenTraitAgreesWithConcept)
{
#if LUMEX_HAS_STD_CONCEPTS
  LUMEX_TRAIT_AGREES (is_lvalue_ref_to_class, LvalueRefToClass,
                      agree_class_t &);
  LUMEX_TRAIT_AGREES (is_lvalue_ref_to_class, LvalueRefToClass,
                      agree_class_t const &);
  LUMEX_TRAIT_AGREES (is_lvalue_ref_to_class, LvalueRefToClass,
                      agree_incomplete_t &);
  LUMEX_TRAIT_AGREES (is_lvalue_ref_to_class, LvalueRefToClass,
                      agree_class_t &&);
  LUMEX_TRAIT_AGREES (is_lvalue_ref_to_class, LvalueRefToClass, agree_class_t);
  LUMEX_TRAIT_AGREES (is_lvalue_ref_to_class, LvalueRefToClass,
                      agree_class_t *);
  LUMEX_TRAIT_AGREES (is_lvalue_ref_to_class, LvalueRefToClass,
                      agree_class_t *&);
  LUMEX_TRAIT_AGREES (is_lvalue_ref_to_class, LvalueRefToClass, int &);
  LUMEX_TRAIT_AGREES (is_lvalue_ref_to_class, LvalueRefToClass,
                      agree_union_t &);
  LUMEX_TRAIT_AGREES (is_lvalue_ref_to_class, LvalueRefToClass,
                      agree_enum_t &);
  LUMEX_TRAIT_AGREES (is_lvalue_ref_to_class, LvalueRefToClass, void (&) ());
  LUMEX_TRAIT_AGREES (is_lvalue_ref_to_class, LvalueRefToClass,
                      agree_class_t (&)[2]);
  LUMEX_TRAIT_AGREES (is_lvalue_ref_to_class, LvalueRefToClass, int);
  LUMEX_TRAIT_AGREES (is_lvalue_ref_to_class, LvalueRefToClass, void);
#else
  GTEST_SKIP () << "the standard library has no <concepts>";
#endif
}

TEST (LumexTypeTraitsTwinsConceptTest,
      GivenTypes_WhenCompleteType_ThenTraitAgreesWithConcept)
{
#if LUMEX_HAS_STD_CONCEPTS
  LUMEX_TRAIT_AGREES (is_complete_type, CompleteType, int);
  LUMEX_TRAIT_AGREES (is_complete_type, CompleteType, int const);
  LUMEX_TRAIT_AGREES (is_complete_type, CompleteType, agree_class_t);
  LUMEX_TRAIT_AGREES (is_complete_type, CompleteType, agree_base_t);
  LUMEX_TRAIT_AGREES (is_complete_type, CompleteType, agree_union_t);
  LUMEX_TRAIT_AGREES (is_complete_type, CompleteType, agree_enum_t);
  LUMEX_TRAIT_AGREES (is_complete_type, CompleteType, std::string);
  LUMEX_TRAIT_AGREES (is_complete_type, CompleteType, agree_incomplete_t *);
  LUMEX_TRAIT_AGREES (is_complete_type, CompleteType, std::nullptr_t);
  LUMEX_TRAIT_AGREES (is_complete_type, CompleteType, int[3]);
  LUMEX_TRAIT_AGREES (is_complete_type, CompleteType, int &);
  LUMEX_TRAIT_AGREES (is_complete_type, CompleteType, agree_class_t &&);
  LUMEX_TRAIT_AGREES (is_complete_type, CompleteType, agree_incomplete_t);
  LUMEX_TRAIT_AGREES (is_complete_type, CompleteType,
                      agree_incomplete_t const);
  LUMEX_TRAIT_AGREES (is_complete_type, CompleteType, agree_incomplete_t[3]);
  LUMEX_TRAIT_AGREES (is_complete_type, CompleteType, agree_incomplete_t &);
  LUMEX_TRAIT_AGREES (is_complete_type, CompleteType, int[]);
  LUMEX_TRAIT_AGREES (is_complete_type, CompleteType, void);
  LUMEX_TRAIT_AGREES (is_complete_type, CompleteType, void const);
  LUMEX_TRAIT_AGREES (is_complete_type, CompleteType, void ());
  LUMEX_TRAIT_AGREES (is_complete_type, CompleteType, int (int, char));
#else
  GTEST_SKIP () << "the standard library has no <concepts>";
#endif
}

TEST (LumexTypeTraitsTwinsConceptTest,
      GivenCvPairs_WhenPreservesCv_ThenTraitAgreesWithConcept)
{
#if LUMEX_HAS_STD_CONCEPTS
  LUMEX_TRAIT2_AGREES (preserves_cv, PreserveCV, int, int);
  LUMEX_TRAIT2_AGREES (preserves_cv, PreserveCV, int, int const);
  LUMEX_TRAIT2_AGREES (preserves_cv, PreserveCV, int, int volatile);
  LUMEX_TRAIT2_AGREES (preserves_cv, PreserveCV, int, int const volatile);
  LUMEX_TRAIT2_AGREES (preserves_cv, PreserveCV, int const, int);
  LUMEX_TRAIT2_AGREES (preserves_cv, PreserveCV, int const, int const);
  LUMEX_TRAIT2_AGREES (preserves_cv, PreserveCV, int const, int volatile);
  LUMEX_TRAIT2_AGREES (preserves_cv, PreserveCV, int const,
                       int const volatile);
  LUMEX_TRAIT2_AGREES (preserves_cv, PreserveCV, int volatile, int);
  LUMEX_TRAIT2_AGREES (preserves_cv, PreserveCV, int volatile, int const);
  LUMEX_TRAIT2_AGREES (preserves_cv, PreserveCV, int volatile, int volatile);
  LUMEX_TRAIT2_AGREES (preserves_cv, PreserveCV, int volatile,
                       int const volatile);
  LUMEX_TRAIT2_AGREES (preserves_cv, PreserveCV, int const volatile, int);
  LUMEX_TRAIT2_AGREES (preserves_cv, PreserveCV, int const volatile,
                       int const);
  LUMEX_TRAIT2_AGREES (preserves_cv, PreserveCV, int const volatile,
                       int volatile);
  LUMEX_TRAIT2_AGREES (preserves_cv, PreserveCV, int const volatile,
                       int const volatile);
  LUMEX_TRAIT2_AGREES (preserves_cv, PreserveCV, agree_base_t const,
                       agree_derived_t);
  LUMEX_TRAIT2_AGREES (preserves_cv, PreserveCV, agree_base_t,
                       agree_derived_t const);
#else
  GTEST_SKIP () << "the standard library has no <concepts>";
#endif
}

TEST (LumexTypeTraitsTwinsConceptTest,
      GivenClassPairs_WhenIsDerivedFrom_ThenTraitAgreesWithStdDerivedFrom)
{
#if LUMEX_HAS_STD_CONCEPTS
#define LUMEX_DERIVED_AGREES(DERIVED, BASE)                                   \
  EXPECT_EQ ((traits::meta::is_derived_from<DERIVED, BASE>::value),           \
             (std::derived_from<DERIVED, BASE>))
  LUMEX_DERIVED_AGREES (agree_derived_t, agree_base_t);
  LUMEX_DERIVED_AGREES (agree_derived_t const, agree_base_t);
  LUMEX_DERIVED_AGREES (agree_derived_t, agree_base_t const volatile);
  LUMEX_DERIVED_AGREES (agree_base_t, agree_base_t);
  LUMEX_DERIVED_AGREES (agree_base_t, agree_derived_t);
  LUMEX_DERIVED_AGREES (agree_private_t, agree_base_t);
  LUMEX_DERIVED_AGREES (agree_diamond_t, agree_base_t);
  LUMEX_DERIVED_AGREES (agree_diamond_t, agree_left_t);
  LUMEX_DERIVED_AGREES (agree_left_t, agree_right_t);
  LUMEX_DERIVED_AGREES (agree_class_t, agree_base_t);
  LUMEX_DERIVED_AGREES (int, int);
  LUMEX_DERIVED_AGREES (agree_derived_t *, agree_base_t *);
  LUMEX_DERIVED_AGREES (agree_derived_t &, agree_base_t &);
  LUMEX_DERIVED_AGREES (agree_union_t, agree_union_t);
#undef LUMEX_DERIVED_AGREES
#else
  GTEST_SKIP () << "the standard library has no <concepts>";
#endif
}

TEST (LumexTypeTraitsTwinsConceptTest,
      GivenTypes_WhenIsStringConvertible_ThenTraitAgreesWithStringLike)
{
#if LUMEX_HAS_STD_CONCEPTS
#define LUMEX_STRING_AGREES(TYPE)                                             \
  EXPECT_EQ (traits::string::is_string_convertible<TYPE>::value,              \
             traits::string::StringLike<TYPE>)
  LUMEX_STRING_AGREES (std::string);
  LUMEX_STRING_AGREES (std::string &);
  LUMEX_STRING_AGREES (std::string const &);
  LUMEX_STRING_AGREES (std::string &&);
  LUMEX_STRING_AGREES (std::string_view);
  LUMEX_STRING_AGREES (std::string_view const &);
  LUMEX_STRING_AGREES (char const *);
  LUMEX_STRING_AGREES (char *);
  LUMEX_STRING_AGREES (char const (&)[4]);
  LUMEX_STRING_AGREES (char[4]);
  LUMEX_STRING_AGREES (agree_to_string_t);
  LUMEX_STRING_AGREES (agree_to_string_t const &);
  LUMEX_STRING_AGREES (agree_explicit_to_string_t);
  LUMEX_STRING_AGREES (int);
  LUMEX_STRING_AGREES (double);
  LUMEX_STRING_AGREES (int *);
  LUMEX_STRING_AGREES (std::wstring);
  LUMEX_STRING_AGREES (wchar_t const *);
  LUMEX_STRING_AGREES (std::vector<char>);
  LUMEX_STRING_AGREES (agree_class_t);
  LUMEX_STRING_AGREES (void);
#undef LUMEX_STRING_AGREES
  // std::string_view is C++17: the trait of C++20 knows it.
  EXPECT_TRUE (traits::string::is_string_convertible<std::string_view>::value);
  EXPECT_TRUE (
      traits::string::is_string_convertible<std::string_view const &>::value);
#else
  GTEST_SKIP () << "the standard library has no <concepts>";
#endif
}

#if LUMEX_HAS_STD_CONCEPTS
#undef LUMEX_TRAIT_AGREES
#undef LUMEX_TRAIT2_AGREES
#endif
