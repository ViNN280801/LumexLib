// LumexCastConstraints.cxx11.tests.cpp
//
// Which calls of downcast and downcast_noexcept exist. The constraints are
// SFINAE in every standard, so a use that is not a real downcast finds no
// overload; the detectors below ask the compiler instead of compiling a
// rejected call. A real downcast is a pointer to a pointer or an lvalue
// reference to an lvalue reference, both of a complete class type, from a
// polymorphic class to a class derived from it through a public and
// unambiguous base (or to the same class), dropping no const or volatile.
// What the calls do at run time is in LumexCast.cxx11.tests.cpp and
// LumexCastBehavior.cxx11.tests.cpp.
#include <cstddef>
#include <exception>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <utility>

#include <gtest/gtest.h>

#include "lumex/core/utility/cast/LumexCast.hpp"

namespace traits = lumex::core::utility::traits;
namespace cast = lumex::core::utility::cast;
using lumex::core::utility::cast::bad_down_cast;
using lumex::core::utility::cast::downcast;
using lumex::core::utility::cast::downcast_noexcept;

namespace
{
// --- class zoo ----------------------------------------------------------

struct poly_base
{
  virtual ~poly_base () {}
};

struct public_derived : poly_base
{
};

struct protected_derived : protected poly_base
{
};

struct private_derived : private poly_base
{
};

struct final_derived final : poly_base
{
};

// poly_base is a base of ambiguous_derived twice.
struct left_branch : poly_base
{
};

struct right_branch : poly_base
{
};

struct ambiguous_derived : left_branch, right_branch
{
};

// A virtual base is a single, unambiguous base.
struct virtual_base
{
  virtual ~virtual_base () {}
};

struct virtual_left : virtual virtual_base
{
};

struct virtual_right : virtual virtual_base
{
};

struct virtual_diamond : virtual_left, virtual_right
{
};

struct abstract_base
{
  virtual void work () = 0;
  virtual ~abstract_base () {}
};

struct concrete_derived : abstract_base
{
  void
  work () override
  {
  }
};

// Not polymorphic: no virtual function anywhere.
struct plain_base
{
  int value;
};

struct plain_derived : plain_base
{
};

// The base is not polymorphic, only the derived class is.
struct plain_base_poly_derived : plain_base
{
  virtual ~plain_base_poly_derived () {}
};

struct unrelated_poly
{
  virtual ~unrelated_poly () {}
};

struct incomplete_t;

union plain_union
{
  int integer;
  float real;
};

enum class plain_enum
{
  first
};

// --- detectors ----------------------------------------------------------

// downcast<To> (declval<From> ()) is well-formed. From = B * is an rvalue
// pointer, From = B & an lvalue of B.
template <typename To, typename From, typename = void>
struct can_downcast : std::false_type
{
};

template <typename To, typename From>
struct can_downcast<
    To, From,
    traits::meta::void_t<decltype (downcast<To> (std::declval<From> ()))>>
    : std::true_type
{
};

template <typename To, typename From, typename = void>
struct can_downcast_noexcept : std::false_type
{
};

template <typename To, typename From>
struct can_downcast_noexcept<
    To, From,
    traits::meta::void_t<decltype (downcast_noexcept<To> (
        std::declval<From> ()))>> : std::true_type
{
};

template <typename From, typename To>
struct is_valid : cast::Detail::is_valid_down_cast<From, To>
{
};

// downcast_noexcept<To, Base> (declval<Argument> ()): Base named explicitly.
template <typename To, typename Base, typename Argument, typename = void>
struct can_downcast_noexcept_with_base : std::false_type
{
};

template <typename To, typename Base, typename Argument>
struct can_downcast_noexcept_with_base<
    To, Base, Argument,
    traits::meta::void_t<decltype (downcast_noexcept<To, Base> (
        std::declval<Argument> ()))>> : std::true_type
{
};
} // namespace

// --- accepted -----------------------------------------------------------

TEST (LumexCastConstraintsTest, GivenPublicBase_WhenDowncastPointer_ThenExists)
{
  static_assert (can_downcast<public_derived *, poly_base *>::value, "");
  static_assert (can_downcast_noexcept<public_derived *, poly_base *>::value,
                 "");
  EXPECT_TRUE ((can_downcast<public_derived *, poly_base *>::value));
  EXPECT_TRUE ((can_downcast_noexcept<public_derived *, poly_base *>::value));
}

TEST (LumexCastConstraintsTest,
      GivenPublicBase_WhenDowncastReference_ThenExists)
{
  static_assert (can_downcast<public_derived &, poly_base &>::value, "");
  EXPECT_TRUE ((can_downcast<public_derived &, poly_base &>::value));
}

TEST (LumexCastConstraintsTest,
      GivenLvaluePointer_WhenDowncast_ThenDeductionDropsTheLvalue)
{
  // A pointer variable, a const pointer variable and an rvalue pointer are
  // all a by-value Base.
  EXPECT_TRUE ((can_downcast<public_derived *, poly_base *&>::value));
  EXPECT_TRUE ((can_downcast<public_derived *, poly_base *const &>::value));
  EXPECT_TRUE ((can_downcast<public_derived *, poly_base *>::value));
  EXPECT_TRUE (
      (can_downcast<public_derived const *, poly_base const *const &>::value));
}

TEST (LumexCastConstraintsTest, GivenSameClass_WhenDowncast_ThenIdentityExists)
{
  // std::derived_from<T, T> holds for a class, so the identity cast is a
  // downcast too.
  EXPECT_TRUE ((can_downcast<poly_base *, poly_base *>::value));
  EXPECT_TRUE ((can_downcast<poly_base &, poly_base &>::value));
  EXPECT_TRUE ((can_downcast_noexcept<poly_base *, poly_base *>::value));
}

TEST (LumexCastConstraintsTest, GivenDeepHierarchy_WhenDowncast_ThenExists)
{
  // Two steps down, and a derived class that is final.
  EXPECT_TRUE ((can_downcast<final_derived *, poly_base *>::value));
  EXPECT_TRUE ((can_downcast<final_derived &, poly_base &>::value));
  EXPECT_TRUE ((can_downcast<left_branch *, poly_base *>::value));
  EXPECT_TRUE ((can_downcast<ambiguous_derived *, left_branch *>::value));
  EXPECT_TRUE ((can_downcast<ambiguous_derived *, right_branch *>::value));
}

TEST (LumexCastConstraintsTest,
      GivenVirtualBase_WhenDowncast_ThenExistsThoughStaticCastCannot)
{
  EXPECT_TRUE ((can_downcast<virtual_left *, virtual_base *>::value));
  EXPECT_TRUE ((can_downcast<virtual_diamond *, virtual_base *>::value));
  EXPECT_TRUE ((can_downcast<virtual_diamond &, virtual_base &>::value));
  EXPECT_TRUE ((can_downcast<virtual_diamond *, virtual_left *>::value));
  EXPECT_TRUE ((can_downcast<virtual_diamond *, virtual_right *>::value));
}

TEST (LumexCastConstraintsTest,
      GivenAbstractBase_WhenDowncast_ThenExistsForPointerAndReference)
{
  EXPECT_TRUE ((can_downcast<concrete_derived *, abstract_base *>::value));
  EXPECT_TRUE ((can_downcast<concrete_derived &, abstract_base &>::value));
  EXPECT_TRUE ((can_downcast<abstract_base *, abstract_base *>::value));
}

// --- cv-qualifiers ------------------------------------------------------

TEST (LumexCastConstraintsTest, GivenCv_WhenDowncast_ThenTargetKeepsEveryCv)
{
  // The target may have more cv than the source, never less.
  EXPECT_TRUE ((can_downcast<public_derived *, poly_base *>::value));
  EXPECT_TRUE ((can_downcast<public_derived const *, poly_base *>::value));
  EXPECT_TRUE ((can_downcast<public_derived volatile *, poly_base *>::value));
  EXPECT_TRUE (
      (can_downcast<public_derived const volatile *, poly_base *>::value));
  EXPECT_TRUE (
      (can_downcast<public_derived const *, poly_base const *>::value));
  EXPECT_TRUE (
      (can_downcast<public_derived volatile *, poly_base volatile *>::value));
  EXPECT_TRUE ((can_downcast<public_derived const volatile *,
                             poly_base const *>::value));
  EXPECT_TRUE ((can_downcast<public_derived const volatile *,
                             poly_base const volatile *>::value));

  EXPECT_TRUE ((can_downcast<public_derived const &, poly_base &>::value));
  EXPECT_TRUE (
      (can_downcast<public_derived const &, poly_base const &>::value));
  EXPECT_TRUE (
      (can_downcast<public_derived volatile &, poly_base volatile &>::value));
}

TEST (LumexCastConstraintsTest, GivenCvDropped_WhenDowncast_ThenNoOverload)
{
  EXPECT_FALSE ((can_downcast<public_derived *, poly_base const *>::value));
  EXPECT_FALSE ((can_downcast<public_derived *, poly_base volatile *>::value));
  EXPECT_FALSE (
      (can_downcast<public_derived *, poly_base const volatile *>::value));
  // const kept but volatile dropped, and the other way round.
  EXPECT_FALSE ((can_downcast<public_derived const *,
                              poly_base const volatile *>::value));
  EXPECT_FALSE ((can_downcast<public_derived volatile *,
                              poly_base const volatile *>::value));
  EXPECT_FALSE (
      (can_downcast<public_derived const *, poly_base volatile *>::value));

  EXPECT_FALSE ((can_downcast<public_derived &, poly_base const &>::value));
  EXPECT_FALSE ((can_downcast<public_derived &, poly_base volatile &>::value));
  EXPECT_FALSE ((can_downcast<public_derived const &,
                              poly_base const volatile &>::value));

  EXPECT_FALSE (
      (can_downcast_noexcept<public_derived *, poly_base const *>::value));
}

// --- the base class must be reachable -----------------------------------

TEST (LumexCastConstraintsTest, GivenProtectedBase_WhenDowncast_ThenNoOverload)
{
  EXPECT_FALSE ((can_downcast<protected_derived *, poly_base *>::value));
  EXPECT_FALSE ((can_downcast<protected_derived &, poly_base &>::value));
  EXPECT_FALSE (
      (can_downcast_noexcept<protected_derived *, poly_base *>::value));
}

TEST (LumexCastConstraintsTest, GivenPrivateBase_WhenDowncast_ThenNoOverload)
{
  EXPECT_FALSE ((can_downcast<private_derived *, poly_base *>::value));
  EXPECT_FALSE ((can_downcast<private_derived &, poly_base &>::value));
  EXPECT_FALSE (
      (can_downcast_noexcept<private_derived *, poly_base *>::value));
}

TEST (LumexCastConstraintsTest, GivenAmbiguousBase_WhenDowncast_ThenNoOverload)
{
  // poly_base is reached through left_branch and right_branch.
  EXPECT_FALSE ((can_downcast<ambiguous_derived *, poly_base *>::value));
  EXPECT_FALSE ((can_downcast<ambiguous_derived &, poly_base &>::value));
  EXPECT_FALSE (
      (can_downcast_noexcept<ambiguous_derived *, poly_base *>::value));
}

TEST (LumexCastConstraintsTest,
      GivenInaccessibleBase_WhenIsBaseOf_ThenTrueButNotAValidDownCast)
{
  // std::is_base_of alone would accept these: it is the conversion check that
  // rejects them (the reason for traits::meta::is_derived_from).
  static_assert (std::is_base_of<poly_base, protected_derived>::value, "");
  static_assert (std::is_base_of<poly_base, private_derived>::value, "");
  static_assert (std::is_base_of<poly_base, ambiguous_derived>::value, "");
  EXPECT_TRUE ((std::is_base_of<poly_base, private_derived>::value));
  EXPECT_FALSE ((is_valid<poly_base *, private_derived *>::value));
}

// --- unrelated and reversed classes -------------------------------------

TEST (LumexCastConstraintsTest, GivenNotDerived_WhenDowncast_ThenNoOverload)
{
  // A cross-cast and an upcast are not downcasts.
  EXPECT_FALSE ((can_downcast<unrelated_poly *, poly_base *>::value));
  EXPECT_FALSE ((can_downcast<unrelated_poly &, poly_base &>::value));
  EXPECT_FALSE ((can_downcast<public_derived *, final_derived *>::value));
  EXPECT_FALSE ((can_downcast<left_branch *, right_branch *>::value));
  EXPECT_FALSE ((can_downcast<poly_base *, public_derived *>::value));
  EXPECT_FALSE ((can_downcast<poly_base &, public_derived &>::value));
  EXPECT_FALSE ((can_downcast<virtual_base *, virtual_diamond *>::value));
  EXPECT_FALSE ((can_downcast_noexcept<unrelated_poly *, poly_base *>::value));
}

TEST (LumexCastConstraintsTest, GivenPlainSource_WhenDowncast_ThenNoOverload)
{
  // No virtual function in the source class: dynamic_cast is not allowed.
  EXPECT_FALSE ((can_downcast<plain_derived *, plain_base *>::value));
  EXPECT_FALSE ((can_downcast<plain_derived &, plain_base &>::value));
  EXPECT_FALSE (
      (can_downcast<plain_base_poly_derived *, plain_base *>::value));
  EXPECT_FALSE ((can_downcast_noexcept<plain_derived *, plain_base *>::value));
}

// --- shapes -------------------------------------------------------------

TEST (LumexCastConstraintsTest, GivenMixedShapes_WhenDowncast_ThenNoOverload)
{
  EXPECT_FALSE ((can_downcast<public_derived &, poly_base *>::value));
  EXPECT_FALSE ((can_downcast<public_derived *, poly_base &>::value));
  EXPECT_FALSE ((can_downcast_noexcept<public_derived &, poly_base *>::value));
  EXPECT_FALSE ((can_downcast_noexcept<public_derived *, poly_base &>::value));
}

TEST (LumexCastConstraintsTest,
      GivenRvalueReferenceOrValueTarget_WhenDowncast_ThenNoOverload)
{
  EXPECT_FALSE ((can_downcast<public_derived &&, poly_base &>::value));
  EXPECT_FALSE ((can_downcast<public_derived, poly_base &>::value));
  EXPECT_FALSE ((can_downcast<public_derived, poly_base *>::value));
}

TEST (LumexCastConstraintsTest,
      GivenTemporaryObject_WhenDowncast_ThenOnlyAConstTargetBinds)
{
  // The reference form binds an lvalue, so a temporary has no overload; a
  // reference to const binds a temporary, as in any function that takes
  // `Base const &`.
  EXPECT_FALSE ((can_downcast<public_derived &, poly_base>::value));
  EXPECT_FALSE ((can_downcast<public_derived &, poly_base &&>::value));
  EXPECT_TRUE ((can_downcast<public_derived const &, poly_base const>::value));
  EXPECT_FALSE ((can_downcast<public_derived &, poly_base const>::value));
}

TEST (LumexCastConstraintsTest,
      GivenPointerToPointerOrNotClass_WhenDowncast_ThenNoOverload)
{
  EXPECT_FALSE ((can_downcast<public_derived **, poly_base **>::value));
  EXPECT_FALSE ((can_downcast<public_derived *&, poly_base *&>::value));
  EXPECT_FALSE ((can_downcast<int *, int *>::value));
  EXPECT_FALSE ((can_downcast<int &, int &>::value));
  EXPECT_FALSE ((can_downcast<void *, void *>::value));
  EXPECT_FALSE ((can_downcast<public_derived *, void *>::value));
  EXPECT_FALSE ((can_downcast<void *, poly_base *>::value));
  EXPECT_FALSE ((can_downcast<plain_union *, plain_union *>::value));
  EXPECT_FALSE ((can_downcast<plain_enum *, plain_enum *>::value));
  EXPECT_FALSE ((can_downcast<void (*) (), void (*) ()>::value));
  EXPECT_FALSE ((can_downcast<int poly_base::*, int poly_base::*>::value));
}

TEST (LumexCastConstraintsTest,
      GivenNullptrLiteral_WhenDowncast_ThenNoOverload)
{
  // A null pointer variable is a pointer and converts to a null pointer;
  // the literal nullptr is not a pointer to a class.
  EXPECT_FALSE ((can_downcast<public_derived *, std::nullptr_t>::value));
  EXPECT_FALSE (
      (can_downcast_noexcept<public_derived *, std::nullptr_t>::value));
}

// --- incomplete types ---------------------------------------------------

TEST (LumexCastConstraintsTest,
      GivenIncompleteClass_WhenDowncast_ThenNoOverloadAndNoHardError)
{
  EXPECT_FALSE ((can_downcast<incomplete_t *, poly_base *>::value));
  EXPECT_FALSE ((can_downcast<public_derived *, incomplete_t *>::value));
  EXPECT_FALSE ((can_downcast<incomplete_t *, incomplete_t *>::value));
  EXPECT_FALSE ((can_downcast<incomplete_t &, incomplete_t &>::value));
  EXPECT_FALSE ((can_downcast<incomplete_t &, poly_base &>::value));
  EXPECT_FALSE ((can_downcast<public_derived &, incomplete_t &>::value));
  EXPECT_FALSE ((can_downcast_noexcept<incomplete_t *, poly_base *>::value));
  EXPECT_FALSE (
      (can_downcast_noexcept<public_derived *, incomplete_t *>::value));
  EXPECT_FALSE ((is_valid<incomplete_t *, incomplete_t *>::value));
  EXPECT_FALSE ((is_valid<incomplete_t &, incomplete_t &>::value));
}

// --- the noexcept variant takes pointers --------------------------------

TEST (LumexCastConstraintsTest,
      GivenReferenceTarget_WhenDowncastNoexcept_ThenNoOverload)
{
  // A reference cannot report a failure with nullptr. Explicit template
  // arguments name both types, as the function takes Base by value.
  EXPECT_FALSE ((can_downcast_noexcept<public_derived &, poly_base &>::value));
  EXPECT_FALSE ((can_downcast_noexcept<public_derived &, poly_base *>::value));
  EXPECT_FALSE ((can_downcast_noexcept<public_derived const &,
                                       poly_base const &>::value));
}

TEST (LumexCastConstraintsTest,
      GivenReferenceBaseNamedExplicitly_WhenDowncastNoexcept_ThenNoOverload)
{
  // With the Base type spelled out the argument is a reference operand; the
  // target must still be a pointer, otherwise the body (`return nullptr`)
  // could not compile.
  EXPECT_FALSE ((can_downcast_noexcept_with_base<public_derived &, poly_base &,
                                                 poly_base &>::value));
  EXPECT_FALSE ((can_downcast_noexcept_with_base<public_derived const &,
                                                 poly_base const &,
                                                 poly_base const &>::value));
  EXPECT_TRUE ((can_downcast_noexcept_with_base<public_derived *, poly_base *,
                                                poly_base *>::value));
}

// --- the trait ----------------------------------------------------------

TEST (LumexCastConstraintsTest,
      GivenOperandPairs_WhenIsValidDownCast_ThenAsDocumented)
{
  EXPECT_TRUE ((is_valid<poly_base *, public_derived *>::value));
  EXPECT_TRUE ((is_valid<poly_base &, public_derived &>::value));
  EXPECT_TRUE ((is_valid<poly_base const *, public_derived const *>::value));
  EXPECT_TRUE ((is_valid<poly_base *, public_derived const *>::value));
  EXPECT_TRUE ((is_valid<poly_base *, poly_base *>::value));
  EXPECT_FALSE ((is_valid<poly_base const *, public_derived *>::value));
  EXPECT_FALSE ((is_valid<poly_base *, public_derived &>::value));
  EXPECT_FALSE ((is_valid<poly_base &, public_derived *>::value));
  EXPECT_FALSE ((is_valid<poly_base, public_derived>::value));
  EXPECT_FALSE ((is_valid<poly_base *, private_derived *>::value));
  EXPECT_FALSE ((is_valid<poly_base *, protected_derived *>::value));
  EXPECT_FALSE ((is_valid<poly_base *, ambiguous_derived *>::value));
  EXPECT_FALSE ((is_valid<plain_base *, plain_derived *>::value));
  EXPECT_FALSE ((is_valid<poly_base *, unrelated_poly *>::value));
  EXPECT_FALSE ((is_valid<int *, int *>::value));
  EXPECT_FALSE ((is_valid<void, void>::value));
  EXPECT_FALSE ((is_valid<poly_base &&, public_derived &&>::value));
}

TEST (LumexCastConstraintsTest,
      GivenAnyType_WhenIsValidDownCast_ThenNoHardError)
{
  // Every operand in one list: the trait must answer, never fail to compile.
  EXPECT_FALSE ((is_valid<void, int>::value));
  EXPECT_FALSE ((is_valid<int[3], int[3]>::value));
  EXPECT_FALSE ((is_valid<void (&) (), void (&) ()>::value));
  EXPECT_FALSE ((is_valid<void (), void ()>::value));
  EXPECT_FALSE ((is_valid<std::nullptr_t, std::nullptr_t>::value));
  EXPECT_FALSE ((is_valid<plain_union &, plain_union &>::value));
  EXPECT_FALSE ((is_valid<incomplete_t, incomplete_t>::value));
  EXPECT_FALSE ((is_valid<abstract_base, concrete_derived>::value));
}

// --- result and exception types -----------------------------------------

TEST (LumexCastConstraintsTest,
      GivenCalls_WhenDecltype_ThenResultTypesFollowTarget)
{
  static_assert (std::is_same<decltype (downcast<public_derived *> (
                                  std::declval<poly_base *> ())),
                              public_derived *>::value,
                 "");
  static_assert (std::is_same<decltype (downcast<public_derived const *> (
                                  std::declval<poly_base *> ())),
                              public_derived const *>::value,
                 "");
  static_assert (std::is_same<decltype (downcast<public_derived &> (
                                  std::declval<poly_base &> ())),
                              public_derived &>::value,
                 "");
  static_assert (std::is_same<decltype (downcast<public_derived const &> (
                                  std::declval<poly_base &> ())),
                              public_derived const &>::value,
                 "");
  static_assert (std::is_same<decltype (downcast_noexcept<public_derived *> (
                                  std::declval<poly_base *> ())),
                              public_derived *>::value,
                 "");
  SUCCEED ();
}

TEST (LumexCastConstraintsTest,
      GivenCalls_WhenNoexceptOperator_ThenOnlyTheNoexceptOneIs)
{
  static_assert (noexcept (downcast_noexcept<public_derived *> (
                     std::declval<poly_base *> ())),
                 "downcast_noexcept must be noexcept");
  static_assert (
      !noexcept (downcast<public_derived *> (std::declval<poly_base *> ())),
      "downcast may throw");
  static_assert (
      !noexcept (downcast<public_derived &> (std::declval<poly_base &> ())),
      "downcast may throw");
  EXPECT_TRUE (noexcept (
      downcast_noexcept<public_derived *> (std::declval<poly_base *> ())));
  EXPECT_FALSE (
      noexcept (downcast<public_derived *> (std::declval<poly_base *> ())));
}

TEST (LumexCastConstraintsTest,
      GivenBadDownCast_WhenInspected_ThenIsAStdBadCast)
{
  static_assert (std::is_base_of<std::bad_cast, bad_down_cast>::value, "");
  static_assert (std::is_base_of<std::exception, bad_down_cast>::value, "");
  static_assert (std::is_copy_constructible<bad_down_cast>::value, "");
  static_assert (std::is_constructible<bad_down_cast, std::type_info const &,
                                       std::type_info const &>::value,
                 "");
  static_assert (
      std::is_constructible<bad_down_cast, std::type_info const &,
                            std::type_info const &, char const *>::value,
      "");
  static_assert (noexcept (std::declval<bad_down_cast const &> ().what ()),
                 "");
  EXPECT_TRUE ((std::is_base_of<std::bad_cast, bad_down_cast>::value));
}
