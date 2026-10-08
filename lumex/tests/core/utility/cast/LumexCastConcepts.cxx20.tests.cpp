// LumexCastConcepts.cxx20.tests.cpp
//
// The SFINAE constraints of downcast and downcast_noexcept against the C++20
// concepts they replaced (DynCastForm and ValidDownCast of the header before
// C++11 support, copied below as the reference), on every pair of a type
// zoo: each pair must give the same answer, for the trait and for the
// function templates themselves. This is the evidence that the C++20
// behavior did not change. The reference needs concepts and <concepts>
// (LUMEX_HAS_STD_CONCEPTS): GCC 8 accepts -std=c++2a without them, so there
// the tests skip. The suites from C++20 up compile this file together with
// the C++11 files.
#if defined(__has_include)
#if __has_include(<concepts>)
#include <concepts>
#endif
#endif
#include <cstddef>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

#include "lumex/core/utility/cast/LumexCast.hpp"
#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"
#include "lumex/core/utility/traits/LumexTypeTraits.hpp"

namespace
{
struct zoo_base
{
  virtual ~zoo_base () {}
};

struct zoo_public : zoo_base
{
};

struct zoo_protected : protected zoo_base
{
};

struct zoo_private : private zoo_base
{
};

struct zoo_final final : zoo_base
{
};

struct zoo_left : zoo_base
{
};

struct zoo_right : zoo_base
{
};

struct zoo_ambiguous : zoo_left, zoo_right
{
};

struct zoo_virtual_root
{
  virtual ~zoo_virtual_root () {}
};

struct zoo_virtual_a : virtual zoo_virtual_root
{
};

struct zoo_virtual_b : virtual zoo_virtual_root
{
};

struct zoo_virtual_diamond : zoo_virtual_a, zoo_virtual_b
{
};

struct zoo_abstract
{
  virtual void work () = 0;
  virtual ~zoo_abstract () {}
};

struct zoo_concrete : zoo_abstract
{
  void
  work () override
  {
  }
};

struct zoo_plain
{
  int value;
};

struct zoo_plain_derived : zoo_plain
{
};

struct zoo_plain_poly_derived : zoo_plain
{
  virtual ~zoo_plain_poly_derived () {}
};

struct zoo_unrelated
{
  virtual ~zoo_unrelated () {}
};

struct zoo_incomplete;

union zoo_union
{
  int integer;
  float real;
};

enum class zoo_enum
{
  first
};
} // namespace

#if LUMEX_HAS_STD_CONCEPTS

namespace
{
namespace traits = lumex::core::utility::traits;
namespace cast = lumex::core::utility::cast;
using lumex::core::utility::cast::downcast;
using lumex::core::utility::cast::downcast_noexcept;

// The concepts the SFINAE traits replaced, verbatim.
template <typename T>
concept ReferenceDynCastForm
    = traits::meta::PointerToClass<T> || traits::meta::LvalueRefToClass<T>;

template <typename Base, typename Derived>
concept ReferenceValidDownCast
    = ReferenceDynCastForm<Base> && ReferenceDynCastForm<Derived>
      && (std::is_pointer_v<Base> == std::is_pointer_v<Derived>)
      && traits::meta::CompleteType<traits::meta::indirection_of_t<Base>>
      && traits::meta::CompleteType<traits::meta::indirection_of_t<Derived>>
      && std::is_polymorphic_v<
          std::remove_cv_t<traits::meta::indirection_of_t<Base>>>
      && std::derived_from<
          std::remove_cv_t<traits::meta::indirection_of_t<Derived>>,
          std::remove_cv_t<traits::meta::indirection_of_t<Base>>>
      && traits::meta::PreserveCV<traits::meta::indirection_of_t<Base>,
                                  traits::meta::indirection_of_t<Derived>>;

template <typename... Ts> struct type_list
{
};

template <typename First, typename Second> struct concat2;

template <typename... Firsts, typename... Seconds>
struct concat2<type_list<Firsts...>, type_list<Seconds...>>
{
  using type = type_list<Firsts..., Seconds...>;
};

template <typename... Lists> struct concat_all
{
  using type = type_list<>;
};

template <typename List, typename... Rest> struct concat_all<List, Rest...>
{
  using type =
      typename concat2<List, typename concat_all<Rest...>::type>::type;
};

// The operands a downcast is called with: pointers and lvalue references.
template <typename C>
using source_forms
    = type_list<C *, C const *, C volatile *, C &, C const &, C volatile &>;

// Every type that may be asked for as a target.
template <typename C>
using target_forms
    = type_list<C *, C const *, C volatile *, C const volatile *, C &,
                C const &, C volatile &, C const volatile &, C &&, C>;

using extra_types = type_list<int *, int &, void *, void, std::nullptr_t,
                              zoo_base **, zoo_base *&, void (*) ()>;

template <template <typename> class Forms, typename... Classes>
using forms_of_all = typename concat_all<Forms<Classes>...>::type;

#define LUMEX_ZOO_CLASSES                                                     \
  zoo_base, zoo_public, zoo_protected, zoo_private, zoo_final, zoo_ambiguous, \
      zoo_left, zoo_virtual_root, zoo_virtual_diamond, zoo_abstract,          \
      zoo_concrete, zoo_plain, zoo_plain_derived, zoo_plain_poly_derived,     \
      zoo_unrelated, zoo_incomplete, zoo_union, zoo_enum

using all_sources = forms_of_all<source_forms, LUMEX_ZOO_CLASSES>;
using all_targets =
    typename concat2<forms_of_all<target_forms, LUMEX_ZOO_CLASSES>,
                     extra_types>::type;

template <typename To, typename From, typename = void>
struct accepts_downcast : std::false_type
{
};

template <typename To, typename From>
struct accepts_downcast<
    To, From, std::void_t<decltype (downcast<To> (std::declval<From> ()))>>
    : std::true_type
{
};

template <typename To, typename From, typename = void>
struct accepts_downcast_noexcept : std::false_type
{
};

template <typename To, typename From>
struct accepts_downcast_noexcept<
    To, From,
    std::void_t<decltype (downcast_noexcept<To> (std::declval<From> ()))>>
    : std::true_type
{
};

struct tally
{
  std::size_t pairs = 0;
  std::size_t accepted = 0;
  std::size_t trait_differs = 0;
  std::size_t downcast_differs = 0;
  std::size_t noexcept_differs = 0;
};

template <typename From, typename To>
void
check_pair (tally &result)
{
  bool const reference = ReferenceValidDownCast<From, To>;
  ++result.pairs;
  if (reference)
    ++result.accepted;
  if (cast::Detail::is_valid_down_cast<From, To>::value != reference)
    ++result.trait_differs;
  // The function templates took exactly the pairs the concept took.
  if (accepts_downcast<To, From>::value != reference)
    ++result.downcast_differs;
  // downcast_noexcept took pointer operands only: the by-value Base is never
  // a reference, so a lvalue reference operand had no overload. A pointer
  // operand is a pointer form, and a mixed pair fails the shape check, so
  // `reference` already means pointer to pointer there.
  bool const expected_noexcept = reference && std::is_pointer_v<From>;
  if (accepts_downcast_noexcept<To, From>::value != expected_noexcept)
    ++result.noexcept_differs;
}

template <typename From, typename... Targets>
void
check_from (tally &result, type_list<Targets...>)
{
  (check_pair<From, Targets> (result), ...);
}

template <typename... Sources, typename Targets>
void
check_all (tally &result, type_list<Sources...>, Targets targets)
{
  (check_from<Sources> (result, targets), ...);
}

template <typename T> struct form_agrees
{
  static bool
  value ()
  {
    return cast::Detail::is_dyn_cast_form<T>::value == ReferenceDynCastForm<T>;
  }
};

template <typename... Ts>
std::size_t
count_form_differences (type_list<Ts...>)
{
  return (std::size_t{ 0 } + ... + (form_agrees<Ts>::value () ? 0u : 1u));
}
} // namespace

#endif // LUMEX_HAS_STD_CONCEPTS

TEST (LumexCastConceptsTest,
      GivenTypeZoo_WhenFormTrait_ThenAgreesWithTheConcept)
{
#if LUMEX_HAS_STD_CONCEPTS
  EXPECT_EQ (count_form_differences (all_sources{}), 0U);
  EXPECT_EQ (count_form_differences (all_targets{}), 0U);
#else
  GTEST_SKIP () << "the standard library has no <concepts>";
#endif
}

TEST (LumexCastConceptsTest,
      GivenTypeZoo_WhenValidDownCast_ThenTraitAndFunctionsAgreeWithTheConcept)
{
#if LUMEX_HAS_STD_CONCEPTS
  tally result;
  check_all (result, all_sources{}, all_targets{});
  // The zoo is large enough to mean something, and both answers occur.
  EXPECT_EQ (result.pairs, 108U * 188U);
  EXPECT_GT (result.accepted, 50U);
  EXPECT_LT (result.accepted, result.pairs);
  EXPECT_EQ (result.trait_differs, 0U);
  EXPECT_EQ (result.downcast_differs, 0U);
  EXPECT_EQ (result.noexcept_differs, 0U);
#else
  GTEST_SKIP () << "the standard library has no <concepts>";
#endif
}
