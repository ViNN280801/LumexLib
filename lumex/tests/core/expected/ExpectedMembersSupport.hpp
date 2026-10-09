#ifndef LUMEX_TESTS_CORE_EXPECTED_EXPECTED_MEMBERS_SUPPORT_HPP
#define LUMEX_TESTS_CORE_EXPECTED_EXPECTED_MEMBERS_SUPPORT_HPP

#include <cstddef>
#include <memory>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <utility>

#include "lumex/core/expected/Expected"
#include "lumex/core/utility/traits/LumexTypeTraits.hpp"

// The type zoo of the tests that pin the special member functions of
// expected to the standard ([expected.object.cons], [expected.object.dtor],
// [expected.object.assign], [expected.object.swap], [expected.void]), and the
// helpers that run a property over every pair of the zoo. Every file that
// includes it compiles from C++11.
//
// The types differ in the properties the standard looks at: copy and move
// constructible or not, trivial or not, `noexcept` or not, assignable or not,
// destructor trivial or not. Each is named by the property it holds.

namespace expected_members
{

/// Trivial in every way.
struct trivial_t
{
  int value;
};

/// Copy and move user-provided and `noexcept`; assignments too.
struct nothrow_t
{
  int value;

  explicit nothrow_t (int v = 0) noexcept : value (v) {}
  nothrow_t (nothrow_t const &other) noexcept : value (other.value) {}
  nothrow_t (nothrow_t &&other) noexcept : value (other.value) {}
  nothrow_t &
  operator= (nothrow_t const &other) noexcept
  {
    value = other.value;
    return *this;
  }
  nothrow_t &
  operator= (nothrow_t &&other) noexcept
  {
    value = other.value;
    return *this;
  }
};

/// Copy and move user-provided; the move may throw.
struct throwing_move_t
{
  int value;

  explicit throwing_move_t (int v = 0) : value (v) {}
  throwing_move_t (throwing_move_t const &other) : value (other.value) {}
  throwing_move_t (throwing_move_t &&other) noexcept (false)
      : value (other.value)
  {
  }
  throwing_move_t &
  operator= (throwing_move_t const &other)
  {
    value = other.value;
    return *this;
  }
  throwing_move_t &
  operator= (throwing_move_t &&other) noexcept (false)
  {
    value = other.value;
    return *this;
  }
};

/// Copy declared, no move declared: an rvalue is copied (a legacy class).
struct legacy_copy_t
{
  int value;

  explicit legacy_copy_t (int v = 0) : value (v) {}
  legacy_copy_t (legacy_copy_t const &other) : value (other.value) {}
  legacy_copy_t &
  operator= (legacy_copy_t const &other)
  {
    value = other.value;
    return *this;
  }
};

/// Copy declared, move deleted: it cannot be moved, and an rvalue is not
/// copied either (`is_move_constructible` is false).
struct copy_only_t
{
  int value;

  explicit copy_only_t (int v = 0) : value (v) {}
  copy_only_t (copy_only_t const &other) : value (other.value) {}
  copy_only_t (copy_only_t &&) = delete;
  copy_only_t &
  operator= (copy_only_t const &other)
  {
    value = other.value;
    return *this;
  }
  copy_only_t &operator= (copy_only_t &&) = delete;
};

/// Cannot be copied or moved.
struct pinned_t
{
  int value;

  explicit pinned_t (int v = 0) : value (v) {}
  pinned_t (pinned_t const &) = delete;
  pinned_t (pinned_t &&) = delete;
  pinned_t &operator= (pinned_t const &) = delete;
  pinned_t &operator= (pinned_t &&) = delete;
};

/// Copy constructible but not assignable (a constant member).
struct no_assign_t
{
  int const value;

  explicit no_assign_t (int v = 0) : value (v) {}
};

/// Copy assignable, but not copy constructible (and so not movable either).
struct assign_only_t
{
  int value;

  explicit assign_only_t (int v = 0) : value (v) {}
  assign_only_t (assign_only_t const &) = delete;
  assign_only_t &
  operator= (assign_only_t const &other)
  {
    value = other.value;
    return *this;
  }
};

/// The constructors may throw, the assignments cannot.
struct throwing_ctor_t
{
  int value;

  explicit throwing_ctor_t (int v = 0) : value (v) {}
  throwing_ctor_t (throwing_ctor_t const &other) noexcept (false)
      : value (other.value)
  {
  }
  throwing_ctor_t (throwing_ctor_t &&other) noexcept (false)
      : value (other.value)
  {
  }
  throwing_ctor_t &
  operator= (throwing_ctor_t const &other) noexcept
  {
    value = other.value;
    return *this;
  }
  throwing_ctor_t &
  operator= (throwing_ctor_t &&other) noexcept
  {
    value = other.value;
    return *this;
  }
};

/// Trivial copy and move, a destructor that is not trivial.
struct trivial_copy_t
{
  int value;

  explicit trivial_copy_t (int v = 0) : value (v) {}
  ~trivial_copy_t () {}
};

/// Move only (assignable).
using move_only_t = std::unique_ptr<int>;

/// Spelling of a type for a failure message: the zoo types are named by
/// specializations below, any other type by the (mangled) name of the
/// compiler.
template <typename T> struct type_name_of
{
  static char const *
  get ()
  {
    return typeid (T).name ();
  }
};

#define LUMEX_EXPECTED_MEMBERS_NAME(Type)                                     \
  template <> struct type_name_of<Type>                                       \
  {                                                                           \
    static char const *                                                       \
    get ()                                                                    \
    {                                                                         \
      return #Type;                                                           \
    }                                                                         \
  }

LUMEX_EXPECTED_MEMBERS_NAME (int);
LUMEX_EXPECTED_MEMBERS_NAME (int const);
LUMEX_EXPECTED_MEMBERS_NAME (trivial_t);
LUMEX_EXPECTED_MEMBERS_NAME (nothrow_t);
LUMEX_EXPECTED_MEMBERS_NAME (throwing_move_t);
LUMEX_EXPECTED_MEMBERS_NAME (legacy_copy_t);
LUMEX_EXPECTED_MEMBERS_NAME (copy_only_t);
LUMEX_EXPECTED_MEMBERS_NAME (pinned_t);
LUMEX_EXPECTED_MEMBERS_NAME (no_assign_t);
LUMEX_EXPECTED_MEMBERS_NAME (trivial_copy_t);
LUMEX_EXPECTED_MEMBERS_NAME (assign_only_t);
LUMEX_EXPECTED_MEMBERS_NAME (throwing_ctor_t);
LUMEX_EXPECTED_MEMBERS_NAME (std::string);
LUMEX_EXPECTED_MEMBERS_NAME (std::string const);
LUMEX_EXPECTED_MEMBERS_NAME (move_only_t);
LUMEX_EXPECTED_MEMBERS_NAME (nothrow_t const);

#undef LUMEX_EXPECTED_MEMBERS_NAME

// ---------------------------------------------------------------------
// The conditions of the standard, one function each, written from the text of
// the clauses and not from the implementation. T and E are the types as in
// expected<T, E>.
// ---------------------------------------------------------------------

template <typename T>
constexpr bool
copyable ()
{
  return std::is_copy_constructible<T>::value;
}

template <typename T>
constexpr bool
movable ()
{
  return std::is_move_constructible<T>::value;
}

template <typename T>
constexpr bool
nothrow_movable ()
{
  return std::is_nothrow_move_constructible<T>::value;
}

/// [expected.object.cons]/9: the copy constructor is not deleted.
template <typename T, typename E>
constexpr bool
copy_constructor_exists ()
{
  return copyable<T> () && copyable<E> ();
}

/// [expected.object.cons]/10: the copy constructor is trivial.
template <typename T, typename E>
constexpr bool
copy_constructor_trivial ()
{
  return std::is_trivially_copy_constructible<T>::value
         && std::is_trivially_copy_constructible<E>::value;
}

/// [expected.object.cons]/11: the move constructor takes part.
template <typename T, typename E>
constexpr bool
move_constructor_exists ()
{
  return movable<T> () && movable<E> ();
}

/// [expected.object.cons]/16: the move constructor is trivial.
template <typename T, typename E>
constexpr bool
move_constructor_trivial ()
{
  return std::is_trivially_move_constructible<T>::value
         && std::is_trivially_move_constructible<E>::value;
}

/// [expected.object.assign]/4: the copy assignment is not deleted.
template <typename T, typename E>
constexpr bool
copy_assignment_exists ()
{
  return std::is_copy_assignable<T>::value && copyable<T> ()
         && std::is_copy_assignable<E>::value && copyable<E> ()
         && (nothrow_movable<T> () || nothrow_movable<E> ());
}

/// [expected.object.assign]/5: the copy assignment is trivial.
template <typename T, typename E>
constexpr bool
copy_assignment_trivial ()
{
  return std::is_trivially_copy_constructible<T>::value
         && std::is_trivially_copy_assignable<T>::value
         && std::is_trivially_destructible<T>::value
         && std::is_trivially_copy_constructible<E>::value
         && std::is_trivially_copy_assignable<E>::value
         && std::is_trivially_destructible<E>::value;
}

/// [expected.object.assign]/6: the move assignment takes part.
template <typename T, typename E>
constexpr bool
move_assignment_exists ()
{
  return movable<T> () && std::is_move_assignable<T>::value && movable<E> ()
         && std::is_move_assignable<E>::value
         && (nothrow_movable<T> () || nothrow_movable<E> ());
}

/// [expected.object.assign]/9: the move assignment is `noexcept`.
template <typename T, typename E>
constexpr bool
move_assignment_nothrow ()
{
  return std::is_nothrow_move_assignable<T>::value && nothrow_movable<T> ()
         && std::is_nothrow_move_assignable<E>::value && nothrow_movable<E> ();
}

/// [expected.object.assign]/10: the move assignment is trivial.
template <typename T, typename E>
constexpr bool
move_assignment_trivial ()
{
  return std::is_trivially_move_constructible<T>::value
         && std::is_trivially_move_assignable<T>::value
         && std::is_trivially_destructible<T>::value
         && std::is_trivially_move_constructible<E>::value
         && std::is_trivially_move_assignable<E>::value
         && std::is_trivially_destructible<E>::value;
}

/// [expected.object.dtor]/2: the destructor is trivial.
template <typename T, typename E>
constexpr bool
destructor_trivial ()
{
  return std::is_trivially_destructible<T>::value
         && std::is_trivially_destructible<E>::value;
}

// ---------------------------------------------------------------------
// swap: found by argument-dependent lookup, as the standard writes
// `using std::swap; swap (a, b)`
// ---------------------------------------------------------------------

namespace swap_probe
{
using std::swap;

template <typename X, typename = void> struct swappable : std::false_type
{
};

template <typename X>
struct swappable<X, lumex::core::utility::traits::meta::void_t<decltype (swap (
                        std::declval<X &> (), std::declval<X &> ()))>>
    : std::true_type
{
};

template <typename X, bool = swappable<X>::value>
struct nothrow_swappable : std::false_type
{
};

template <typename X>
struct nothrow_swappable<X, true>
    : std::integral_constant<bool, noexcept (swap (std::declval<X &> (),
                                                   std::declval<X &> ()))>
{
};
} // namespace swap_probe

template <typename T>
constexpr bool
swappable ()
{
  return swap_probe::swappable<T>::value;
}

template <typename T>
constexpr bool
nothrow_swappable ()
{
  return swap_probe::nothrow_swappable<T>::value;
}

/// The member `X::swap (X &)` can be called.
template <typename X, typename = void> struct has_member_swap : std::false_type
{
};

template <typename X>
struct has_member_swap<
    X, lumex::core::utility::traits::meta::void_t<
           decltype (std::declval<X &> ().swap (std::declval<X &> ()))>>
    : std::true_type
{
};

/// The member `X::swap (X &)` is `noexcept`.
template <typename X, bool = has_member_swap<X>::value>
struct member_swap_nothrow : std::false_type
{
};

template <typename X>
struct member_swap_nothrow<X, true>
    : std::integral_constant<bool, noexcept (std::declval<X &> ().swap (
                                       std::declval<X &> ()))>
{
};

/// [expected.object.swap]/1: the member `swap` exists.
template <typename T, typename E>
constexpr bool
swap_exists ()
{
  return swappable<T> () && swappable<E> () && movable<T> () && movable<E> ()
         && (nothrow_movable<T> () || nothrow_movable<E> ());
}

/// [expected.object.swap]/4: the member `swap` is `noexcept`.
template <typename T, typename E>
constexpr bool
swap_nothrow ()
{
  return nothrow_movable<T> () && nothrow_swappable<T> ()
         && nothrow_movable<E> () && nothrow_swappable<E> ();
}

// ---------------------------------------------------------------------
// Running a property over every pair
// ---------------------------------------------------------------------

template <typename... Types> struct type_list
{
};

template <typename Value, typename Error> struct type_pair
{
  using value_type = Value;
  using error_type = Error;
};

template <typename Value, typename ErrorList> struct pairs_of;

template <typename Value, typename... Errors>
struct pairs_of<Value, type_list<Errors...>>
{
  using type = type_list<type_pair<Value, Errors>...>;
};

template <typename... Lists> struct concat;

template <> struct concat<>
{
  using type = type_list<>;
};

template <typename... Types> struct concat<type_list<Types...>>
{
  using type = type_list<Types...>;
};

template <typename... Left, typename... Right, typename... Rest>
struct concat<type_list<Left...>, type_list<Right...>, Rest...>
{
  using type = typename concat<type_list<Left..., Right...>, Rest...>::type;
};

template <typename ValueList, typename ErrorList> struct product;

template <typename... Values, typename ErrorList>
struct product<type_list<Values...>, ErrorList>
{
  using type =
      typename concat<typename pairs_of<Values, ErrorList>::type...>::type;
};

/// Value types of the zoo (the `T` of `expected<T, E>`).
using value_zoo
    = type_list<int, int const, trivial_t, nothrow_t, nothrow_t const,
                throwing_move_t, legacy_copy_t, copy_only_t, pinned_t,
                no_assign_t, trivial_copy_t, assign_only_t, throwing_ctor_t,
                std::string, std::string const, move_only_t>;

/// Error types of the zoo (the `E`, never cv-qualified).
using error_zoo
    = type_list<int, trivial_t, nothrow_t, throwing_move_t, legacy_copy_t,
                copy_only_t, pinned_t, no_assign_t, trivial_copy_t,
                assign_only_t, throwing_ctor_t, std::string, move_only_t>;

/// Every pair of the zoo.
using pair_zoo = typename product<value_zoo, error_zoo>::type;

/// The pairs of a value type `void`: the error types alone.
using void_zoo = error_zoo;

/// "Value | Error" for a message.
template <typename Value, typename Error>
std::string
pair_name ()
{
  return std::string (type_name_of<Value>::get ()) + " | "
         + type_name_of<Error>::get ();
}

template <typename Error>
std::string
error_name ()
{
  return std::string ("void | ") + type_name_of<Error>::get ();
}

/// Collects the pairs on which `Property::actual` and `Property::expected`
/// differ.
template <typename Property, typename... Pairs>
std::string
mismatches (type_list<Pairs...>)
{
  std::string report;
  bool const results[] = { (
      Property::template actual<typename Pairs::value_type,
                                typename Pairs::error_type> ()
              == Property::template expected<typename Pairs::value_type,
                                             typename Pairs::error_type> ()
          ? true
          : (report
             += pair_name<typename Pairs::value_type,
                          typename Pairs::error_type> ()
                + (Property::template actual<typename Pairs::value_type,
                                             typename Pairs::error_type> ()
                       ? " is, the standard says it is not; "
                       : " is not, the standard says it is; "),
             false))... };
  (void)results;
  return report;
}

/// The same for `expected<void, E>`: `Property::actual<Error>`.
template <typename Property, typename... Errors>
std::string
void_mismatches (type_list<Errors...>)
{
  std::string report;
  bool const results[]
      = { (Property::template actual_void<Errors> ()
                   == Property::template expected_void<Errors> ()
               ? true
               : (report += error_name<Errors> ()
                            + (Property::template actual_void<Errors> ()
                                   ? " is, the standard says it is not; "
                                   : " is not, the standard says it is; "),
                  false))... };
  (void)results;
  return report;
}

} // namespace expected_members

#endif // !LUMEX_TESTS_CORE_EXPECTED_EXPECTED_MEMBERS_SUPPORT_HPP
