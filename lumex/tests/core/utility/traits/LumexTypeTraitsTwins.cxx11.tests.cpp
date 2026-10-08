// lumex/tests/core/utility/traits/LumexTypeTraitsTwins.cxx11.tests.cpp
// The C++11 forms of the C++20 concepts of LumexTypeTraits.hpp that the cast,
// ranges and dump helpers need: is_pointer_to_class, is_lvalue_ref_to_class,
// is_complete_type, preserves_cv, is_derived_from (the form of
// std::derived_from) and is_string_convertible (the form of StringLike). Each
// is a std::integral_constant, so it is tested with its value, its type and
// the address of its value. The C++20 file next to this one checks every trait
// against its concept.
#include <cstddef>
#include <string>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/utility/traits/LumexTypeTraits.hpp"

namespace traits = lumex::core::utility::traits;

namespace
{
struct plain_class_t
{
  int value;
};

class closed_class_t
{
public:
  virtual ~closed_class_t () {}
};

union plain_union_t
{
  int integer;
  float real;
};

enum plain_enum_t
{
  plain_enum_first
};

enum class scoped_enum_t
{
  first
};

/** Declared, never defined. */
struct incomplete_t;

/** An opaque declaration of an enum with a fixed type is a complete type. */
enum opaque_enum_t : int;

struct base_t
{
  virtual ~base_t () {}
};

struct public_derived_t : base_t
{
};

struct private_derived_t : private base_t
{
};

struct protected_derived_t : protected base_t
{
};

struct second_base_t
{
  virtual ~second_base_t () {}
};

struct two_bases_t : base_t, second_base_t
{
};

/** Two copies of base_t: the conversion to base_t is ambiguous. */
struct left_t : base_t
{
};

struct right_t : base_t
{
};

struct diamond_t : left_t, right_t
{
};

/** Virtual inheritance: one base_t, the conversion is unambiguous. */
struct virtual_left_t : virtual base_t
{
};

struct virtual_right_t : virtual base_t
{
};

struct virtual_diamond_t : virtual_left_t, virtual_right_t
{
};

struct final_derived_t final : base_t
{
};

/** Converts implicitly to std::string, like lumex_string_view does. */
struct to_string_t
{
  operator std::string () const { return std::string (); }
};

/** Converts to std::string only through an explicit operator. */
struct explicit_to_string_t
{
  explicit
  operator std::string () const
  {
    return std::string ();
  }
};
} // namespace

// ---------------------------------------------------------------------------
// is_pointer_to_class
// ---------------------------------------------------------------------------

TEST (LumexTypeTraitsTwinsTest, GivenPointers_WhenIsPointerToClass_ThenClasses)
{
  EXPECT_TRUE (traits::meta::is_pointer_to_class<plain_class_t *>::value);
  EXPECT_TRUE (traits::meta::is_pointer_to_class<closed_class_t *>::value);
  EXPECT_TRUE (traits::meta::is_pointer_to_class<base_t *>::value);
  EXPECT_TRUE (traits::meta::is_pointer_to_class<std::string *>::value);
  EXPECT_TRUE (traits::meta::is_pointer_to_class<incomplete_t *>::value);
  // The qualifiers of the pointee and of the pointer itself do not matter.
  EXPECT_TRUE (traits::meta::is_pointer_to_class<base_t const *>::value);
  EXPECT_TRUE (traits::meta::is_pointer_to_class<base_t volatile *>::value);
  EXPECT_TRUE (
      traits::meta::is_pointer_to_class<base_t const volatile *>::value);
  EXPECT_TRUE (traits::meta::is_pointer_to_class<base_t *const>::value);
  EXPECT_TRUE (traits::meta::is_pointer_to_class<base_t const *const>::value);
  EXPECT_TRUE (
      traits::meta::is_pointer_to_class<base_t *volatile const>::value);
}

TEST (LumexTypeTraitsTwinsTest,
      GivenNonClassPointees_WhenIsPointerToClass_ThenFalse)
{
  EXPECT_FALSE (traits::meta::is_pointer_to_class<int *>::value);
  EXPECT_FALSE (traits::meta::is_pointer_to_class<void *>::value);
  EXPECT_FALSE (traits::meta::is_pointer_to_class<void const *>::value);
  EXPECT_FALSE (traits::meta::is_pointer_to_class<plain_union_t *>::value);
  EXPECT_FALSE (traits::meta::is_pointer_to_class<plain_enum_t *>::value);
  EXPECT_FALSE (traits::meta::is_pointer_to_class<scoped_enum_t *>::value);
  EXPECT_FALSE (traits::meta::is_pointer_to_class<void (*) ()>::value);
  EXPECT_FALSE (traits::meta::is_pointer_to_class<int (*) (int)>::value);
  EXPECT_FALSE (
      traits::meta::is_pointer_to_class<int plain_class_t::*>::value);
  EXPECT_FALSE (
      traits::meta::is_pointer_to_class<void (plain_class_t::*) ()>::value);
  // A pointer to a pointer is a pointer to a pointer, not to a class.
  EXPECT_FALSE (traits::meta::is_pointer_to_class<base_t **>::value);
  EXPECT_FALSE (traits::meta::is_pointer_to_class<int (*)[3]>::value);
}

TEST (LumexTypeTraitsTwinsTest,
      GivenNonPointers_WhenIsPointerToClass_ThenFalse)
{
  EXPECT_FALSE (traits::meta::is_pointer_to_class<base_t>::value);
  EXPECT_FALSE (traits::meta::is_pointer_to_class<base_t &>::value);
  EXPECT_FALSE (traits::meta::is_pointer_to_class<base_t &&>::value);
  EXPECT_FALSE (traits::meta::is_pointer_to_class<base_t *&>::value);
  EXPECT_FALSE (traits::meta::is_pointer_to_class<base_t[2]>::value);
  EXPECT_FALSE (traits::meta::is_pointer_to_class<std::nullptr_t>::value);
  EXPECT_FALSE (traits::meta::is_pointer_to_class<int>::value);
  EXPECT_FALSE (traits::meta::is_pointer_to_class<void>::value);
}

// ---------------------------------------------------------------------------
// is_lvalue_ref_to_class
// ---------------------------------------------------------------------------

TEST (LumexTypeTraitsTwinsTest,
      GivenReferences_WhenIsLvalueRefToClass_ThenLvalueClasses)
{
  EXPECT_TRUE (traits::meta::is_lvalue_ref_to_class<plain_class_t &>::value);
  EXPECT_TRUE (traits::meta::is_lvalue_ref_to_class<base_t &>::value);
  EXPECT_TRUE (traits::meta::is_lvalue_ref_to_class<std::string &>::value);
  EXPECT_TRUE (traits::meta::is_lvalue_ref_to_class<incomplete_t &>::value);
  EXPECT_TRUE (traits::meta::is_lvalue_ref_to_class<base_t const &>::value);
  EXPECT_TRUE (traits::meta::is_lvalue_ref_to_class<base_t volatile &>::value);
  EXPECT_TRUE (
      traits::meta::is_lvalue_ref_to_class<base_t const volatile &>::value);

  EXPECT_FALSE (traits::meta::is_lvalue_ref_to_class<base_t &&>::value);
  EXPECT_FALSE (traits::meta::is_lvalue_ref_to_class<base_t const &&>::value);
  EXPECT_FALSE (traits::meta::is_lvalue_ref_to_class<base_t>::value);
  EXPECT_FALSE (traits::meta::is_lvalue_ref_to_class<base_t *>::value);
  EXPECT_FALSE (traits::meta::is_lvalue_ref_to_class<base_t *&>::value);
}

TEST (LumexTypeTraitsTwinsTest,
      GivenNonClassReferents_WhenIsLvalueRefToClass_ThenFalse)
{
  EXPECT_FALSE (traits::meta::is_lvalue_ref_to_class<int &>::value);
  EXPECT_FALSE (traits::meta::is_lvalue_ref_to_class<int const &>::value);
  EXPECT_FALSE (traits::meta::is_lvalue_ref_to_class<plain_union_t &>::value);
  EXPECT_FALSE (traits::meta::is_lvalue_ref_to_class<plain_enum_t &>::value);
  EXPECT_FALSE (traits::meta::is_lvalue_ref_to_class<scoped_enum_t &>::value);
  EXPECT_FALSE (traits::meta::is_lvalue_ref_to_class<void (&) ()>::value);
  EXPECT_FALSE (traits::meta::is_lvalue_ref_to_class<int (&)[3]>::value);
  EXPECT_FALSE (traits::meta::is_lvalue_ref_to_class<base_t (&)[2]>::value);
  EXPECT_FALSE (traits::meta::is_lvalue_ref_to_class<int>::value);
  EXPECT_FALSE (traits::meta::is_lvalue_ref_to_class<void>::value);
}

// ---------------------------------------------------------------------------
// is_complete_type
// ---------------------------------------------------------------------------

TEST (LumexTypeTraitsTwinsTest, GivenCompleteTypes_WhenIsCompleteType_ThenTrue)
{
  EXPECT_TRUE (traits::meta::is_complete_type<int>::value);
  EXPECT_TRUE (traits::meta::is_complete_type<int const>::value);
  EXPECT_TRUE (traits::meta::is_complete_type<double>::value);
  EXPECT_TRUE (traits::meta::is_complete_type<std::nullptr_t>::value);
  EXPECT_TRUE (traits::meta::is_complete_type<int *>::value);
  EXPECT_TRUE (traits::meta::is_complete_type<void *>::value);
  EXPECT_TRUE (traits::meta::is_complete_type<incomplete_t *>::value);
  EXPECT_TRUE (traits::meta::is_complete_type<void (*) ()>::value);
  EXPECT_TRUE (
      traits::meta::is_complete_type<void (plain_class_t::*) ()>::value);
  EXPECT_TRUE (traits::meta::is_complete_type<plain_class_t>::value);
  EXPECT_TRUE (traits::meta::is_complete_type<base_t>::value);
  EXPECT_TRUE (traits::meta::is_complete_type<plain_union_t>::value);
  EXPECT_TRUE (traits::meta::is_complete_type<plain_enum_t>::value);
  EXPECT_TRUE (traits::meta::is_complete_type<scoped_enum_t>::value);
  EXPECT_TRUE (traits::meta::is_complete_type<opaque_enum_t>::value);
  EXPECT_TRUE (traits::meta::is_complete_type<std::string>::value);
  EXPECT_TRUE (traits::meta::is_complete_type<int[3]>::value);
  EXPECT_TRUE (traits::meta::is_complete_type<base_t[2]>::value);
  // A reference is as complete as what it refers to.
  EXPECT_TRUE (traits::meta::is_complete_type<int &>::value);
  EXPECT_TRUE (traits::meta::is_complete_type<base_t &>::value);
  EXPECT_TRUE (traits::meta::is_complete_type<base_t &&>::value);
}

TEST (LumexTypeTraitsTwinsTest,
      GivenIncompleteTypes_WhenIsCompleteType_ThenFalse)
{
  EXPECT_FALSE (traits::meta::is_complete_type<incomplete_t>::value);
  EXPECT_FALSE (traits::meta::is_complete_type<incomplete_t const>::value);
  EXPECT_FALSE (traits::meta::is_complete_type<incomplete_t[3]>::value);
  EXPECT_FALSE (traits::meta::is_complete_type<incomplete_t &>::value);
  EXPECT_FALSE (traits::meta::is_complete_type<int[]>::value);
  EXPECT_FALSE (traits::meta::is_complete_type<base_t[]>::value);
  EXPECT_FALSE (traits::meta::is_complete_type<void>::value);
  EXPECT_FALSE (traits::meta::is_complete_type<void const>::value);
  EXPECT_FALSE (traits::meta::is_complete_type<void volatile>::value);
  EXPECT_FALSE (traits::meta::is_complete_type<void ()>::value);
  EXPECT_FALSE (traits::meta::is_complete_type<int (int, char)>::value);
}

// ---------------------------------------------------------------------------
// preserves_cv
// ---------------------------------------------------------------------------

TEST (LumexTypeTraitsTwinsTest, GivenSameOrMoreCv_WhenPreservesCv_ThenTrue)
{
  EXPECT_TRUE ((traits::meta::preserves_cv<int, int>::value));
  EXPECT_TRUE ((traits::meta::preserves_cv<int, int const>::value));
  EXPECT_TRUE ((traits::meta::preserves_cv<int, int volatile>::value));
  EXPECT_TRUE ((traits::meta::preserves_cv<int, int const volatile>::value));
  EXPECT_TRUE ((traits::meta::preserves_cv<int const, int const>::value));
  EXPECT_TRUE (
      (traits::meta::preserves_cv<int const, int const volatile>::value));
  EXPECT_TRUE (
      (traits::meta::preserves_cv<int volatile, int volatile>::value));
  EXPECT_TRUE (
      (traits::meta::preserves_cv<int volatile, int const volatile>::value));
  EXPECT_TRUE ((traits::meta::preserves_cv<int const volatile,
                                           int const volatile>::value));
  // Only the qualifiers count, not the types.
  EXPECT_TRUE ((traits::meta::preserves_cv<base_t, public_derived_t>::value));
  EXPECT_TRUE ((traits::meta::preserves_cv<base_t const,
                                           public_derived_t const>::value));
}

TEST (LumexTypeTraitsTwinsTest, GivenDroppedCv_WhenPreservesCv_ThenFalse)
{
  EXPECT_FALSE ((traits::meta::preserves_cv<int const, int>::value));
  EXPECT_FALSE ((traits::meta::preserves_cv<int volatile, int>::value));
  EXPECT_FALSE ((traits::meta::preserves_cv<int const volatile, int>::value));
  EXPECT_FALSE (
      (traits::meta::preserves_cv<int const volatile, int const>::value));
  EXPECT_FALSE (
      (traits::meta::preserves_cv<int const volatile, int volatile>::value));
  // Swapping const for volatile drops both.
  EXPECT_FALSE ((traits::meta::preserves_cv<int const, int volatile>::value));
  EXPECT_FALSE ((traits::meta::preserves_cv<int volatile, int const>::value));
  EXPECT_FALSE (
      (traits::meta::preserves_cv<base_t const, public_derived_t>::value));
}

// ---------------------------------------------------------------------------
// is_derived_from
// ---------------------------------------------------------------------------

TEST (LumexTypeTraitsTwinsTest, GivenPublicBases_WhenIsDerivedFrom_ThenTrue)
{
  EXPECT_TRUE (
      (traits::meta::is_derived_from<public_derived_t, base_t>::value));
  EXPECT_TRUE (
      (traits::meta::is_derived_from<final_derived_t, base_t>::value));
  EXPECT_TRUE ((traits::meta::is_derived_from<two_bases_t, base_t>::value));
  EXPECT_TRUE (
      (traits::meta::is_derived_from<two_bases_t, second_base_t>::value));
  EXPECT_TRUE (
      (traits::meta::is_derived_from<virtual_diamond_t, base_t>::value));
  EXPECT_TRUE ((traits::meta::is_derived_from<virtual_diamond_t,
                                              virtual_left_t>::value));
  // A class derives from itself, as for std::is_base_of.
  EXPECT_TRUE ((traits::meta::is_derived_from<base_t, base_t>::value));
  // The qualifiers do not matter.
  EXPECT_TRUE (
      (traits::meta::is_derived_from<public_derived_t const, base_t>::value));
  EXPECT_TRUE ((traits::meta::is_derived_from<public_derived_t,
                                              base_t const volatile>::value));
}

TEST (LumexTypeTraitsTwinsTest,
      GivenInaccessibleOrAmbiguousBases_WhenIsDerivedFrom_ThenFalse)
{
  EXPECT_FALSE (
      (traits::meta::is_derived_from<private_derived_t, base_t>::value));
  EXPECT_FALSE (
      (traits::meta::is_derived_from<protected_derived_t, base_t>::value));
  // Two copies of base_t: std::is_base_of is true, the conversion is not.
  EXPECT_TRUE ((std::is_base_of<base_t, diamond_t>::value));
  EXPECT_FALSE ((traits::meta::is_derived_from<diamond_t, base_t>::value));
  EXPECT_TRUE ((traits::meta::is_derived_from<diamond_t, left_t>::value));
}

TEST (LumexTypeTraitsTwinsTest,
      GivenUnrelatedTypes_WhenIsDerivedFrom_ThenFalse)
{
  // The wrong direction.
  EXPECT_FALSE (
      (traits::meta::is_derived_from<base_t, public_derived_t>::value));
  EXPECT_FALSE ((traits::meta::is_derived_from<left_t, right_t>::value));
  EXPECT_FALSE ((traits::meta::is_derived_from<base_t, second_base_t>::value));
  EXPECT_FALSE ((traits::meta::is_derived_from<plain_class_t, base_t>::value));
  // Not classes: pointers, references and scalars are never derived.
  EXPECT_FALSE ((traits::meta::is_derived_from<int, int>::value));
  EXPECT_FALSE (
      (traits::meta::is_derived_from<public_derived_t *, base_t *>::value));
  EXPECT_FALSE (
      (traits::meta::is_derived_from<public_derived_t &, base_t &>::value));
  EXPECT_FALSE (
      (traits::meta::is_derived_from<plain_union_t, plain_union_t>::value));
}

// ---------------------------------------------------------------------------
// is_string_convertible
// ---------------------------------------------------------------------------

TEST (LumexTypeTraitsTwinsTest,
      GivenStringSources_WhenIsStringConvertible_ThenTrue)
{
  EXPECT_TRUE (traits::string::is_string_convertible<std::string>::value);
  EXPECT_TRUE (traits::string::is_string_convertible<std::string &>::value);
  EXPECT_TRUE (
      traits::string::is_string_convertible<std::string const &>::value);
  EXPECT_TRUE (traits::string::is_string_convertible<std::string &&>::value);
  EXPECT_TRUE (traits::string::is_string_convertible<char const *>::value);
  EXPECT_TRUE (traits::string::is_string_convertible<char *>::value);
  EXPECT_TRUE (
      traits::string::is_string_convertible<char const (&)[4]>::value);
  EXPECT_TRUE (traits::string::is_string_convertible<char[4]>::value);
  // A class with an implicit conversion operator, as lumex_string_view has.
  EXPECT_TRUE (traits::string::is_string_convertible<to_string_t>::value);
  EXPECT_TRUE (
      traits::string::is_string_convertible<to_string_t const &>::value);
}

TEST (LumexTypeTraitsTwinsTest,
      GivenOtherTypes_WhenIsStringConvertible_ThenFalse)
{
  EXPECT_FALSE (traits::string::is_string_convertible<int>::value);
  EXPECT_FALSE (traits::string::is_string_convertible<double>::value);
  EXPECT_FALSE (traits::string::is_string_convertible<void *>::value);
  EXPECT_FALSE (traits::string::is_string_convertible<int *>::value);
  EXPECT_FALSE (traits::string::is_string_convertible<std::wstring>::value);
  EXPECT_FALSE (traits::string::is_string_convertible<wchar_t const *>::value);
  EXPECT_FALSE (
      traits::string::is_string_convertible<std::vector<char>>::value);
  EXPECT_FALSE (traits::string::is_string_convertible<plain_class_t>::value);
  EXPECT_FALSE (traits::string::is_string_convertible<void>::value);
  // An explicit conversion operator is not an implicit conversion.
  EXPECT_FALSE (
      traits::string::is_string_convertible<explicit_to_string_t>::value);
}

// ---------------------------------------------------------------------------
// Shape common to the traits
// ---------------------------------------------------------------------------

TEST (LumexTypeTraitsTwinsTest,
      GivenTraits_WhenInspected_ThenIntegralConstants)
{
  static_assert (
      std::is_base_of<std::true_type,
                      traits::meta::is_pointer_to_class<base_t *>>::value,
      "a true trait derives from std::true_type's integral_constant");
  static_assert (traits::meta::is_pointer_to_class<base_t *>::value
                     && !traits::meta::is_pointer_to_class<base_t>::value,
                 "is_pointer_to_class is usable in a constant expression");
  static_assert (
      traits::meta::is_lvalue_ref_to_class<base_t &>::value
          && !traits::meta::is_lvalue_ref_to_class<base_t &&>::value,
      "is_lvalue_ref_to_class is usable in a constant expression");
  static_assert (traits::meta::is_complete_type<int>::value
                     && !traits::meta::is_complete_type<incomplete_t>::value,
                 "is_complete_type is usable in a constant expression");
  static_assert (traits::meta::preserves_cv<int, int const>::value
                     && !traits::meta::preserves_cv<int const, int>::value,
                 "preserves_cv is usable in a constant expression");
  static_assert (
      traits::meta::is_derived_from<public_derived_t, base_t>::value
          && !traits::meta::is_derived_from<base_t, public_derived_t>::value,
      "is_derived_from is usable in a constant expression");
  static_assert (traits::string::is_string_convertible<char const *>::value
                     && !traits::string::is_string_convertible<int>::value,
                 "is_string_convertible is usable in a constant expression");

  static_assert (
      std::is_same<traits::meta::is_pointer_to_class<int *>::value_type,
                   bool>::value,
      "value_type is bool");
  static_assert (
      std::is_convertible<traits::meta::is_complete_type<int>, bool>::value,
      "the trait converts to bool like the standard ones");
  EXPECT_TRUE (traits::meta::is_complete_type<int> ());
  EXPECT_FALSE (traits::meta::is_complete_type<incomplete_t> ());
}

TEST (LumexTypeTraitsTwinsTest, GivenTraits_WhenAddressTaken_ThenLinks)
{
  // A volatile pointer keeps the reference to the `value` symbol in an
  // optimized build too, so a member without a definition fails to link here
  // in every build type, not only without optimization.
  bool const *volatile address
      = &traits::meta::is_pointer_to_class<base_t *>::value;
  EXPECT_TRUE (*address);
  address = &traits::meta::is_lvalue_ref_to_class<base_t>::value;
  EXPECT_FALSE (*address);
  address = &traits::meta::is_complete_type<int>::value;
  EXPECT_TRUE (*address);
  address = &traits::meta::preserves_cv<int const, int>::value;
  EXPECT_FALSE (*address);
  address = &traits::meta::is_derived_from<public_derived_t, base_t>::value;
  EXPECT_TRUE (*address);
  address = &traits::string::is_string_convertible<std::string>::value;
  EXPECT_TRUE (*address);
}

TEST (LumexTypeTraitsTwinsTest, GivenTraits_WhenAsCastGuard_ThenDowncastRules)
{
  // The way the cast helpers combine the traits: the pointer or reference to
  // a class, a complete and polymorphic class, a public base, and no cv
  // dropped.
  typedef traits::meta::indirection_of_t<base_t *> pointee_t;
  static_assert (
      traits::meta::is_pointer_to_class<base_t *>::value
          && traits::meta::is_complete_type<pointee_t>::value
          && std::is_polymorphic<pointee_t>::value
          && traits::meta::is_derived_from<public_derived_t, pointee_t>::value
          && traits::meta::preserves_cv<pointee_t, public_derived_t>::value,
      "base_t * to public_derived_t * is a valid downcast");
  typedef traits::meta::indirection_of_t<base_t const &> const_pointee_t;
  static_assert (traits::meta::is_lvalue_ref_to_class<base_t const &>::value
                     && !traits::meta::preserves_cv<const_pointee_t,
                                                    public_derived_t>::value,
                 "base_t const & to public_derived_t & drops const");
  SUCCEED ();
}
