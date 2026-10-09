// A differential test of expected against the std::expected of the standard
// library in use: the same question is asked of both and the answers are
// compared. It runs where the library has std::expected (__cpp_lib_expected:
// libstdc++ 12 and newer, libc++ 16 and newer, MSVC STL 19.33 and newer), so
// from the C++23 suite; elsewhere the file has one skipped test.
//
// What is compared:
//  - the traits of the special member functions (is_copy_constructible,
//    is_trivially_*, is_nothrow_*, is_trivially_copyable, swappable) of
//    expected<T, E> and expected<void, E> for every pair of the type zoo of
//    ExpectedMembersSupport.hpp;
//  - what the constructors accept (is_constructible, is_convertible,
//    is_nothrow_constructible) for a list of argument kinds and for the
//    converting constructors;
//  - the exception specification (noexcept) of the observers and modifiers;
//  - the order of the operations on a probe type that logs every
//    construction, assignment, swap and destruction, for the constructors, the
//    assignments, emplace, swap, value_or, error_or and the monadic
//    operations, in every combination of the states and with the operation
//    that throws;
//  - the types and values of the observers and the comparisons, and what
//    value () throws.
//
// Where the standard leaves room, the implementation may differ, and these
// differences are not failures: a constructor or an observer that the standard
// declares without noexcept may be noexcept ([res.on.exception.handling]/5),
// so every "nothrow" answer is compared as "std nothrow implies lumex
// nothrow". Every other difference fails the test and names the pair of types.

#include <cstddef>
#include <initializer_list>
#include <memory>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>
#include <version>

#if __has_include(<expected>)
#include <expected>
#endif

#include <gtest/gtest.h>

#include "lumex/core/expected/Expected"
#include "lumex/tests/core/expected/ExpectedMembersSupport.hpp"
#include "lumex/tests/core/expected/ExpectedProbe.hpp"

#if defined(__cpp_lib_expected)

namespace
{
namespace lx = lumex::core::expected;
using namespace expected_members;
using namespace expected_probe;

// ---------------------------------------------------------------------
// The two worlds: the same code runs on std::expected and on lumex::expected.
// ---------------------------------------------------------------------

struct std_world
{
  template <typename T, typename E> using ex = std::expected<T, E>;
  template <typename E> using un = std::unexpected<E>;
  template <typename E> using bad_access = std::bad_expected_access<E>;
  using bad_access_void = std::bad_expected_access<void>;
  using in_place_type = std::in_place_t;
  using unexpect_type = std::unexpect_t;
  static in_place_type
  in_place ()
  {
    return std::in_place;
  }
  static unexpect_type
  unexpect ()
  {
    return std::unexpect;
  }
};

struct lumex_world
{
  template <typename T, typename E> using ex = lx::result::expected<T, E>;
  template <typename E> using un = lx::error::unexpected<E>;
  template <typename E> using bad_access = lx::error::bad_expected_access<E>;
  using bad_access_void = lx::error::bad_expected_access<void>;
  using in_place_type = lx::result::in_place_tag;
  using unexpect_type = lx::result::unexpect_t;
  static in_place_type
  in_place ()
  {
    return lx::result::in_place;
  }
  static unexpect_type
  unexpect ()
  {
    return lx::result::unexpect;
  }
};

// ---------------------------------------------------------------------
// Answers: a list of named yes/no questions, the same in both worlds
// ---------------------------------------------------------------------

using answers_t = std::vector<std::pair<std::string, bool>>;

void
note (answers_t &answers, std::string name, bool answer)
{
  answers.emplace_back (std::move (name), answer);
}

bool
ends_with (std::string const &text, std::string const &suffix)
{
  return text.size () >= suffix.size ()
         && text.compare (text.size () - suffix.size (), suffix.size (),
                          suffix)
                == 0;
}

/// A difference that is known and explained: the answer in the standard
/// library and the answer in lumex.
struct known_difference
{
  std::string name;
  bool in_std;
  bool in_lumex;
};

/// The questions on which the worlds differ. A "nothrow" answer may be
/// stronger in lumex (see the comment of the file), a difference in `known`
/// is explained there, everything else must agree.
std::string
differences (answers_t const &std_answers, answers_t const &lumex_answers,
             std::vector<known_difference> const &known = {})
{
  if (std_answers.size () != lumex_answers.size ())
    return "lists of different length; ";
  std::string report;
  for (std::size_t i = 0; i < std_answers.size (); ++i)
    {
      bool const in_std = std_answers[i].second;
      bool const in_lumex = lumex_answers[i].second;
      bool const nothrow = ends_with (std_answers[i].first, "nothrow")
                           || ends_with (std_answers[i].first, "noexcept");
      bool differs = nothrow ? (in_std && !in_lumex) : (in_std != in_lumex);
      for (known_difference const &entry : known)
        if (entry.name == std_answers[i].first && entry.in_std == in_std
            && entry.in_lumex == in_lumex)
          differs = false;
      if (differs)
        report += std_answers[i].first + " [std " + (in_std ? "yes" : "no")
                  + ", lumex " + (in_lumex ? "yes" : "no") + "]; ";
    }
  return report;
}

/// `expected` in libstdc++ 13 (and in C++23 as published) leaves the copy and
/// the move assignment non-trivial; LWG 4026 (C++26) makes them trivial when
/// the operations of `T` and `E` are, which lumex follows
/// ([expected.object.assign]/5 and /10, [expected.void.assign]/4 and /9), and
/// `is_trivially_copyable` follows from them. The entries match only a
/// standard library that has not made the change; once it has, the answers
/// agree and the entries do nothing.
std::vector<known_difference>
lwg_4026_differences ()
{
  return { { "is_trivially_copy_assignable", false, true },
           { "is_trivially_move_assignable", false, true },
           { "is_trivially_copyable", false, true } };
}

/// Runs `Check<World, Pair>::answers ()` for both worlds over every pair and
/// collects the differences, one line for a pair.
template <template <typename, typename, typename> class Check,
          typename... Pairs>
std::string
compare_over (type_list<Pairs...>)
{
  std::string report;
  ((report +=
    [&]
      {
        std::string const diff
            = differences (Check<std_world, typename Pairs::value_type,
                                 typename Pairs::error_type>::answers (),
                           Check<lumex_world, typename Pairs::value_type,
                                 typename Pairs::error_type>::answers (),
                           lwg_4026_differences ());
        return diff.empty () ? std::string ()
                             : pair_name<typename Pairs::value_type,
                                         typename Pairs::error_type> ()
                                   + ": " + diff + "\n";
      }()),
   ...);
  return report;
}

/// The same for `expected<void, E>`; `Value` is `void`.
template <template <typename, typename, typename> class Check,
          typename... Errors>
std::string
compare_over_void (type_list<Errors...>)
{
  std::string report;
  ((report +=
    [&]
      {
        std::string const diff
            = differences (Check<std_world, void, Errors>::answers (),
                           Check<lumex_world, void, Errors>::answers (),
                           lwg_4026_differences ());
        return diff.empty () ? std::string ()
                             : error_name<Errors> () + ": " + diff + "\n";
      }()),
   ...);
  return report;
}

// ---------------------------------------------------------------------
// Part 1: the traits of the special member functions
// ---------------------------------------------------------------------

template <typename World, typename T, typename E> struct special_member_traits
{
  static answers_t
  answers ()
  {
    using X = typename World::template ex<T, E>;
    answers_t result;
#define LUMEX_DIFF_TRAIT(trait) note (result, #trait, std::trait<X>::value)
    LUMEX_DIFF_TRAIT (is_copy_constructible);
    LUMEX_DIFF_TRAIT (is_trivially_copy_constructible);
    LUMEX_DIFF_TRAIT (is_nothrow_copy_constructible);
    LUMEX_DIFF_TRAIT (is_move_constructible);
    LUMEX_DIFF_TRAIT (is_trivially_move_constructible);
    LUMEX_DIFF_TRAIT (is_nothrow_move_constructible);
    LUMEX_DIFF_TRAIT (is_copy_assignable);
    LUMEX_DIFF_TRAIT (is_trivially_copy_assignable);
    LUMEX_DIFF_TRAIT (is_nothrow_copy_assignable);
    LUMEX_DIFF_TRAIT (is_move_assignable);
    LUMEX_DIFF_TRAIT (is_trivially_move_assignable);
    LUMEX_DIFF_TRAIT (is_nothrow_move_assignable);
    LUMEX_DIFF_TRAIT (is_destructible);
    LUMEX_DIFF_TRAIT (is_trivially_destructible);
    LUMEX_DIFF_TRAIT (is_nothrow_destructible);
    LUMEX_DIFF_TRAIT (is_trivially_copyable);
    LUMEX_DIFF_TRAIT (is_swappable);
    LUMEX_DIFF_TRAIT (is_nothrow_swappable);
#undef LUMEX_DIFF_TRAIT
    note (result, "is_default_constructible",
          std::is_default_constructible_v<X>);
    note (result, "default constructor nothrow",
          std::is_nothrow_default_constructible_v<X>);
    note (result, "member swap", has_member_swap<X>::value);
    note (result, "member swap noexcept", member_swap_nothrow<X>::value);
    return result;
  }
};

// ---------------------------------------------------------------------
// Part 2: what the constructors accept
// ---------------------------------------------------------------------

template <typename X, typename... Args>
void
note_constructible (answers_t &answers, std::string const &name)
{
  note (answers, name + " constructible", std::is_constructible_v<X, Args...>);
  note (answers, name + " nothrow",
        std::is_nothrow_constructible_v<X, Args...>);
}

template <typename X, typename Arg>
void
note_convertible (answers_t &answers, std::string const &name)
{
  note (answers, name + " convertible", std::is_convertible_v<Arg, X>);
}

template <typename World, typename T, typename E> struct construction_traits
{
  static answers_t
  answers ()
  {
    using X = typename World::template ex<T, E>;
    using Un = typename World::template un<E>;
    using Tag = typename World::in_place_type;
    using Unex = typename World::unexpect_type;
    using V = std::conditional_t<std::is_void_v<T>, int, std::remove_cv_t<T>>;
    answers_t result;
    note_constructible<X, V> (result, "V");
    note_constructible<X, V const &> (result, "V const&");
    note_constructible<X, V &> (result, "V&");
    note_constructible<X, V &&> (result, "V&&");
    note_constructible<X, int> (result, "int");
    note_constructible<X, Un> (result, "unexpected");
    note_constructible<X, Un const &> (result, "unexpected const&");
    note_constructible<X, Un &> (result, "unexpected&");
    note_constructible<X, Tag> (result, "in_place");
    note_constructible<X, Tag, int> (result, "in_place, int");
    note_constructible<X, Tag, V> (result, "in_place, V");
    note_constructible<X, Tag, std::initializer_list<int>> (result,
                                                            "in_place, list");
    note_constructible<X, Tag, std::initializer_list<int>, int> (
        result, "in_place, list, int");
    note_constructible<X, Unex> (result, "unexpect");
    note_constructible<X, Unex, int> (result, "unexpect, int");
    note_constructible<X, Unex, E> (result, "unexpect, E");
    note_constructible<X, Unex, E const &> (result, "unexpect, E const&");
    note_constructible<X, Unex, std::initializer_list<int>> (result,
                                                             "unexpect, list");
    note_constructible<X, Unex, std::initializer_list<int>, int> (
        result, "unexpect, list, int");
    note_constructible<X, X &> (result, "X&");
    note_constructible<X, X const &> (result, "X const&");
    note_constructible<X, X &&> (result, "X&&");
    note_constructible<X, X const &&> (result, "X const&&");
    note_convertible<X, V> (result, "V");
    note_convertible<X, V const &> (result, "V const&");
    note_convertible<X, Un> (result, "unexpected");
    note_convertible<X, Un const &> (result, "unexpected const&");
    note_convertible<X, int> (result, "int");
    note_convertible<X, X> (result, "X");
    return result;
  }
};

struct explicit_from_int_t
{
  int value;
  explicit explicit_from_int_t (int v) : value (v) {}
};

struct narrowing_t
{
  long value;
  narrowing_t (int v) : value (v) {}
};

/// One converting case: the target and the source, in the four categories of
/// the source.
template <typename World, typename TargetValue, typename TargetError,
          typename SourceValue, typename SourceError>
void
note_conversion (answers_t &answers, std::string const &name)
{
  using Target = typename World::template ex<TargetValue, TargetError>;
  using Source = typename World::template ex<SourceValue, SourceError>;
  note_constructible<Target, Source &> (answers, name + " from &");
  note_constructible<Target, Source const &> (answers, name + " from const&");
  note_constructible<Target, Source> (answers, name + " from &&");
  note_constructible<Target, Source const> (answers, name + " from const&&");
  note_convertible<Target, Source const &> (answers, name + " from const&");
  note_convertible<Target, Source> (answers, name + " from &&");
}

template <typename World>
answers_t
conversion_answers ()
{
  answers_t result;
  note_conversion<World, long, long, int, int> (
      result, "ex<long,long> <- ex<int,int>");
  note_conversion<World, int, int, long, long> (
      result, "ex<int,int> <- ex<long,long>");
  note_conversion<World, std::string, std::string, char const *,
                  char const *> (result,
                                 "ex<string,string> <- ex<char const*,...>");
  note_conversion<World, bool, int, int, int> (result,
                                               "ex<bool,int> <- ex<int,int>");
  note_conversion<World, bool, int, bool, int> (
      result, "ex<bool,int> <- ex<bool,int>");
  note_conversion<World, int, int, bool, int> (result,
                                               "ex<int,int> <- ex<bool,int>");
  note_conversion<World, explicit_from_int_t, int, int, int> (
      result, "ex<explicit,int> <- ex<int,int>");
  note_conversion<World, narrowing_t, long, int, int> (
      result, "ex<narrowing,long> <- ex<int,int>");
  note_conversion<World, std::unique_ptr<int>, int, std::unique_ptr<int>,
                  int> (result, "ex<unique_ptr> <- ex<unique_ptr>");
  note_conversion<World, void, long, void, int> (
      result, "ex<void,long> <- ex<void,int>");
  note_conversion<World, void, int, int, int> (result,
                                               "ex<void,int> <- ex<int,int>");
  note_conversion<World, int, int, void, int> (result,
                                               "ex<int,int> <- ex<void,int>");
  note_conversion<World, copy_only_t, int, copy_only_t, int> (
      result, "ex<copy_only> <- ex<copy_only>");
  note_conversion<World, std::string, int, std::string, long> (
      result, "ex<string,int> <- ex<string,long>");
  note_conversion<World, int const, int, int, int> (
      result, "ex<const int,int> <- ex<int,int>");
  return result;
}

// ---------------------------------------------------------------------
// Part 3: the exception specification of the observers and modifiers
// ---------------------------------------------------------------------

template <typename World, typename T, typename E> struct noexcept_traits
{
  static answers_t
  answers ()
  {
    using X = typename World::template ex<T, E>;
    answers_t result;
    note (result, "has_value noexcept",
          noexcept (std::declval<X const &> ().has_value ()));
    note (result, "bool noexcept",
          noexcept (static_cast<bool> (std::declval<X const &> ())));
    note (result, "error& noexcept", noexcept (std::declval<X &> ().error ()));
    note (result, "error const& noexcept",
          noexcept (std::declval<X const &> ().error ()));
    note (result, "error&& noexcept",
          noexcept (std::declval<X &&> ().error ()));
    note (result, "error const&& noexcept",
          noexcept (std::declval<X const &&> ().error ()));
    if constexpr (!std::is_void_v<T>)
      {
        note (result, "* & noexcept", noexcept (*std::declval<X &> ()));
        note (result, "* const& noexcept",
              noexcept (*std::declval<X const &> ()));
        note (result, "* && noexcept", noexcept (*std::declval<X &&> ()));
        note (result, "* const&& noexcept",
              noexcept (*std::declval<X const &&> ()));
        note (result, "-> noexcept",
              noexcept (std::declval<X &> ().operator->()));
        note (result, "-> const noexcept",
              noexcept (std::declval<X const &> ().operator->()));
      }
    else
      note (result, "* noexcept", noexcept (*std::declval<X const &> ()));
    note (result, "value() noexcept",
          noexcept (std::declval<X &> ().value ()));
    note (result, "value() const& noexcept",
          noexcept (std::declval<X const &> ().value ()));
    note (result, "value() && noexcept",
          noexcept (std::declval<X &&> ().value ()));
    return result;
  }
};

/// The members that the standard declares `noexcept` must be, in lumex too.
template <typename World, typename T, typename E> struct modifier_noexcept
{
  static answers_t
  answers ()
  {
    using X = typename World::template ex<T, E>;
    answers_t result;
    if constexpr (has_member_swap<X>::value)
      note (result, "swap noexcept",
            noexcept (std::declval<X &> ().swap (std::declval<X &> ())));
    else
      note (result, "swap noexcept", false);
    if constexpr (std::is_void_v<T>)
      note (result, "emplace noexcept",
            noexcept (std::declval<X &> ().emplace ()));
    else if constexpr (std::is_nothrow_constructible_v<std::remove_cv_t<T>,
                                                       int>)
      note (result, "emplace noexcept",
            noexcept (std::declval<X &> ().emplace (1)));
    return result;
  }
};

// ---------------------------------------------------------------------
// Part 4: the order of the operations, on probe types
// ---------------------------------------------------------------------

/// The pairs of probe types the scenarios run on: from every operation safe
/// to none that is.
using probe_pairs = type_list<type_pair<v_nothrow_t, e_nothrow_t>,
                              type_pair<v_nothrow_t, e_copy_throws_t>,
                              type_pair<v_nothrow_t, e_all_throw_t>,
                              type_pair<v_copy_throws_t, e_nothrow_t>,
                              type_pair<v_all_throw_t, e_nothrow_t>,
                              type_pair<v_copy_throws_t, e_copy_throws_t>,
                              type_pair<v_all_throw_t, e_copy_throws_t>,
                              type_pair<v_copy_throws_t, e_all_throw_t>,
                              type_pair<v_all_throw_t, e_all_throw_t>>;

using probe_errors = type_list<e_nothrow_t, e_copy_throws_t, e_all_throw_t>;

/// The operations of a probe that may throw, one after the other: a scenario
/// runs once with each of them failing.
char const *const failure_points[] = { "",
                                       "V:ctor",
                                       "V:copy-ctor",
                                       "V:move-ctor",
                                       "E:ctor",
                                       "E:copy-ctor",
                                       "E:move-ctor",
                                       "V:copy-assign",
                                       "V:move-assign",
                                       "E:copy-assign",
                                       "E:move-assign" };

struct transcript
{
  std::string text;

  void
  mark (std::string const &label)
  {
    text += label + ": " + joined (the_log ()) + "\n";
    the_log ().clear ();
  }
};

std::string
text_of (int value)
{
  return std::to_string (value);
}

template <char Name, bool CopyNoexcept, bool MoveNoexcept>
std::string
text_of (probe_t<Name, CopyNoexcept, MoveNoexcept> const &probe)
{
  return std::string (1, Name) + std::to_string (probe.id);
}

/// "V3" for a value, "E4" for an error, "V()" for the success of void.
template <typename X>
std::string
describe (X const &uut)
{
  if (uut.has_value ())
    {
      if constexpr (std::is_void_v<typename X::value_type>)
        return "V()";
      else
        return "V:" + text_of (*uut);
    }
  return "E:" + text_of (uut.error ());
}

template <typename W, typename V, typename E>
auto
make_state (bool value, int id)
{
  using X = typename W::template ex<V, E>;
  if constexpr (std::is_void_v<V>)
    {
      if (value)
        return X (W::in_place ());
      return X (W::unexpect (), id);
    }
  else
    {
      if (value)
        return X (W::in_place (), id);
      return X (W::unexpect (), id);
    }
}

template <typename Function>
std::string
attempt (char const *failure_point, Function &&function)
{
  fail_on () = failure_point;
  std::string result = "ok";
  try
    {
      function ();
    }
  catch (injected_failure const &)
    {
      result = "threw";
    }
  fail_on ().clear ();
  return result;
}

std::string
label (char const *operation, bool destination_value, bool source_value,
       char const *failure_point)
{
  return std::string (operation) + " " + (destination_value ? "V" : "E") + "<-"
         + (source_value ? "V" : "E") + " fail[" + failure_point + "]";
}

std::string
label (char const *operation, bool state, char const *failure_point)
{
  return std::string (operation) + " " + (state ? "V" : "E") + " fail["
         + failure_point + "]";
}

struct assignments
{
  template <typename W, typename V, typename E>
  static std::string
  run ()
  {
    using X = typename W::template ex<V, E>;
    transcript t;
    for (bool dst_value : { true, false })
      for (bool src_value : { true, false })
        for (char const *point : failure_points)
          {
            if constexpr (std::is_copy_assignable_v<X>)
              {
                X dst = make_state<W, V, E> (dst_value, 1);
                X const src = make_state<W, V, E> (src_value, 2);
                the_log ().clear ();
                std::string const result = attempt (point, [&] { dst = src; });
                t.mark (label ("copy-assign", dst_value, src_value, point)
                        + " " + result + " -> " + describe (dst));
              }
            if constexpr (std::is_move_assignable_v<X>)
              {
                X dst = make_state<W, V, E> (dst_value, 1);
                X src = make_state<W, V, E> (src_value, 2);
                the_log ().clear ();
                std::string const result
                    = attempt (point, [&] { dst = std::move (src); });
                t.mark (label ("move-assign", dst_value, src_value, point)
                        + " " + result + " -> " + describe (dst) + " / "
                        + describe (src));
              }
          }
    return t.text;
  }
};

struct assignments_of_contents
{
  template <typename W, typename V, typename E>
  static std::string
  run ()
  {
    using X = typename W::template ex<V, E>;
    using U = typename W::template un<E>;
    transcript t;
    for (bool state : { true, false })
      for (char const *point : failure_points)
        {
          if constexpr (!std::is_void_v<V>)
            {
              if constexpr (requires (X &x, V const &v) { x = v; })
                {
                  X dst = make_state<W, V, E> (state, 1);
                  V const source (3);
                  the_log ().clear ();
                  std::string const result
                      = attempt (point, [&] { dst = source; });
                  t.mark (label ("assign V const&", state, point) + " "
                          + result + " -> " + describe (dst));
                }
              if constexpr (requires (X &x, V &&v) { x = std::move (v); })
                {
                  X dst = make_state<W, V, E> (state, 1);
                  V source (3);
                  the_log ().clear ();
                  std::string const result
                      = attempt (point, [&] { dst = std::move (source); });
                  t.mark (label ("assign V&&", state, point) + " " + result
                          + " -> " + describe (dst));
                }
            }
          if constexpr (requires (X &x, U const &u) { x = u; })
            {
              X dst = make_state<W, V, E> (state, 1);
              U const source (W::in_place (), 4);
              the_log ().clear ();
              std::string const result
                  = attempt (point, [&] { dst = source; });
              t.mark (label ("assign unexpected const&", state, point) + " "
                      + result + " -> " + describe (dst));
            }
          if constexpr (requires (X &x, U &&u) { x = std::move (u); })
            {
              X dst = make_state<W, V, E> (state, 1);
              U source (W::in_place (), 4);
              the_log ().clear ();
              std::string const result
                  = attempt (point, [&] { dst = std::move (source); });
              t.mark (label ("assign unexpected&&", state, point) + " "
                      + result + " -> " + describe (dst));
            }
        }
    return t.text;
  }
};

struct swaps
{
  template <typename W, typename V, typename E>
  static std::string
  run ()
  {
    using X = typename W::template ex<V, E>;
    transcript t;
    for (bool a_value : { true, false })
      for (bool b_value : { true, false })
        for (char const *point : failure_points)
          if constexpr (has_member_swap<X>::value)
            {
              {
                X a = make_state<W, V, E> (a_value, 1);
                X b = make_state<W, V, E> (b_value, 2);
                the_log ().clear ();
                std::string const result
                    = attempt (point, [&] { a.swap (b); });
                t.mark (label ("swap", a_value, b_value, point) + " " + result
                        + " -> " + describe (a) + " / " + describe (b));
              }
              {
                X a = make_state<W, V, E> (a_value, 1);
                X b = make_state<W, V, E> (b_value, 2);
                the_log ().clear ();
                std::string const result
                    = attempt (point, [&] { swap (a, b); });
                t.mark (label ("free swap", a_value, b_value, point) + " "
                        + result + " -> " + describe (a) + " / "
                        + describe (b));
              }
            }
    return t.text;
  }
};

struct emplacements
{
  template <typename W, typename V, typename E>
  static std::string
  run ()
  {
    using X = typename W::template ex<V, E>;
    transcript t;
    for (bool state : { true, false })
      {
        if constexpr (std::is_void_v<V>)
          {
            X uut = make_state<W, V, E> (state, 1);
            the_log ().clear ();
            uut.emplace ();
            t.mark (label ("emplace", state, "") + " -> " + describe (uut));
          }
        else if constexpr (requires (X &x) { x.emplace (5); })
          {
            X uut = make_state<W, V, E> (state, 1);
            the_log ().clear ();
            V &built = uut.emplace (5);
            t.mark (label ("emplace", state, "") + " -> " + describe (uut)
                    + (std::addressof (built) == std::addressof (*uut)
                           ? " same"
                           : " other"));
          }
        if constexpr (requires (X &x) { x.emplace (V (5)); })
          {
            X uut = make_state<W, V, E> (state, 1);
            the_log ().clear ();
            uut.emplace (V (5));
            t.mark (label ("emplace V&&", state, "") + " -> "
                    + describe (uut));
          }
      }
    return t.text;
  }
};

struct constructions
{
  template <typename W, typename V, typename E>
  static std::string
  run ()
  {
    using X = typename W::template ex<V, E>;
    using U = typename W::template un<E>;
    transcript t;
    for (bool state : { true, false })
      for (char const *point : failure_points)
        {
          if constexpr (std::is_copy_constructible_v<X>)
            {
              X const source = make_state<W, V, E> (state, 2);
              the_log ().clear ();
              std::string const result = attempt (
                  point,
                  [&]
                    {
                      X copy (source);
                      t.text += "  copy holds " + describe (copy) + "\n";
                    });
              t.mark (label ("copy-construct", state, point) + " " + result);
            }
          if constexpr (std::is_move_constructible_v<X>)
            {
              X source = make_state<W, V, E> (state, 2);
              the_log ().clear ();
              std::string const result = attempt (
                  point,
                  [&]
                    {
                      X moved (std::move (source));
                      t.text += "  moved holds " + describe (moved) + "\n";
                    });
              t.mark (label ("move-construct", state, point) + " " + result
                      + " / " + describe (source));
            }
        }
    {
      if constexpr (!std::is_void_v<V>)
        {
          if constexpr (std::is_constructible_v<X, V const &>)
            {
              V const source (6);
              the_log ().clear ();
              X uut (source);
              t.mark ("from V const& -> " + describe (uut));
            }
          if constexpr (std::is_constructible_v<X, V &&>)
            {
              V source (6);
              the_log ().clear ();
              X uut (std::move (source));
              t.mark ("from V&& -> " + describe (uut));
            }
          if constexpr (std::is_constructible_v<X, typename W::in_place_type,
                                                int>)
            {
              the_log ().clear ();
              X uut (W::in_place (), 7);
              t.mark ("in_place, 7 -> " + describe (uut));
            }
        }
      if constexpr (std::is_constructible_v<X, typename W::unexpect_type, int>)
        {
          the_log ().clear ();
          X uut (W::unexpect (), 8);
          t.mark ("unexpect, 8 -> " + describe (uut));
        }
      if constexpr (std::is_constructible_v<X, U const &>)
        {
          U const source (W::in_place (), 9);
          the_log ().clear ();
          X uut (source);
          t.mark ("from unexpected const& -> " + describe (uut));
        }
      if constexpr (std::is_constructible_v<X, U &&>)
        {
          U source (W::in_place (), 9);
          the_log ().clear ();
          X uut (std::move (source));
          t.mark ("from unexpected&& -> " + describe (uut));
        }
    }
    return t.text;
  }
};

struct defaulted_or_not
{
  template <typename W, typename V, typename E>
  static std::string
  run ()
  {
    using X = typename W::template ex<V, E>;
    transcript t;
    if constexpr (std::is_default_constructible_v<X>)
      {
        the_log ().clear ();
        X uut;
        t.mark ("default -> " + describe (uut));
      }
    return t.text;
  }
};

struct observers
{
  template <typename W, typename V, typename E>
  static std::string
  run ()
  {
    using X = typename W::template ex<V, E>;
    transcript t;
    for (bool state : { true, false })
      {
        if constexpr (!std::is_void_v<V>)
          {
            if constexpr (std::is_copy_constructible_v<V>)
              {
                X const uut = make_state<W, V, E> (state, 1);
                the_log ().clear ();
                V got = uut.value_or (V (9));
                t.mark (label ("value_or const&", state, "") + " -> "
                        + text_of (got));
              }
            if constexpr (std::is_move_constructible_v<V>)
              {
                X uut = make_state<W, V, E> (state, 1);
                the_log ().clear ();
                V got = std::move (uut).value_or (V (9));
                t.mark (label ("value_or &&", state, "") + " -> "
                        + text_of (got) + " / " + describe (uut));
              }
            if constexpr (std::is_copy_constructible_v<V>)
              {
                X const uut = make_state<W, V, E> (state, 1);
                the_log ().clear ();
                if (state)
                  {
                    V copy (uut.value ());
                    t.mark (label ("value const&", state, "") + " -> "
                            + text_of (copy));
                    V other (*uut);
                    t.mark (label ("* const&", state, "") + " -> "
                            + text_of (other));
                  }
              }
            if constexpr (std::is_move_constructible_v<V>)
              {
                X uut = make_state<W, V, E> (state, 1);
                the_log ().clear ();
                if (state)
                  {
                    V moved (std::move (uut).value ());
                    t.mark (label ("value &&", state, "") + " -> "
                            + text_of (moved) + " / " + describe (uut));
                  }
                X second = make_state<W, V, E> (state, 1);
                the_log ().clear ();
                if (state)
                  {
                    V moved (*std::move (second));
                    t.mark (label ("* &&", state, "") + " -> "
                            + text_of (moved) + " / " + describe (second));
                  }
              }
          }
        if constexpr (std::is_copy_constructible_v<E>)
          {
            X const uut = make_state<W, V, E> (state, 1);
            the_log ().clear ();
            E got = uut.error_or (E (9));
            t.mark (label ("error_or const&", state, "") + " -> "
                    + text_of (got));
          }
        if constexpr (std::is_move_constructible_v<E>)
          {
            X uut = make_state<W, V, E> (state, 1);
            the_log ().clear ();
            E got = std::move (uut).error_or (E (9));
            t.mark (label ("error_or &&", state, "") + " -> " + text_of (got)
                    + " / " + describe (uut));
            if (!state)
              {
                X second = make_state<W, V, E> (state, 1);
                the_log ().clear ();
                E moved (std::move (second).error ());
                t.mark (label ("error &&", state, "") + " -> "
                        + text_of (moved) + " / " + describe (second));
              }
          }
        if constexpr (std::is_copy_constructible_v<E>)
          if (!state)
            {
              X const uut = make_state<W, V, E> (state, 1);
              the_log ().clear ();
              E copy (uut.error ());
              t.mark (label ("error const&", state, "") + " -> "
                      + text_of (copy));
              E other (std::move (uut).error ());
              t.mark (label ("error const&&", state, "") + " -> "
                      + text_of (other));
            }
      }
    return t.text;
  }
};

/// The object that a monadic operation received, as text.
template <typename Argument>
std::string
category_of ()
{
  std::string text;
  text += std::is_const_v<std::remove_reference_t<Argument>> ? "const " : "";
  text += std::is_lvalue_reference_v<Argument> ? "&" : "&&";
  return text;
}

struct monadic_operations
{
  template <typename W, typename V, typename E>
  static std::string
  run ()
  {
    using X = typename W::template ex<V, E>;
    using Int = typename W::template ex<int, E>;
    using Same = typename W::template ex<V, int>;
    transcript t;
    std::string seen;
    auto value_fn = [&] (auto &&...arguments)
      {
        seen += "f("
                + ((category_of<decltype (arguments)> ()) + ...
                   + std::string ())
                + ") ";
        return Int (W::in_place (), 5);
      };
    auto plain_fn = [&] (auto &&...arguments)
      {
        seen += "f("
                + ((category_of<decltype (arguments)> ()) + ...
                   + std::string ())
                + ") ";
        return 5;
      };
    auto error_fn = [&] (auto &&argument)
      {
        seen += "g(" + category_of<decltype (argument)> () + ") ";
        if constexpr (std::is_void_v<V>)
          return Same (W::in_place ());
        else
          return Same (W::unexpect (), 6);
      };
    auto error_plain_fn = [&] (auto &&argument)
      {
        seen += "g(" + category_of<decltype (argument)> () + ") ";
        return 6;
      };
    for (bool state : { true, false })
      {
        X lvalue = make_state<W, V, E> (state, 1);
        X const &const_lvalue = lvalue;
        if constexpr (std::is_copy_constructible_v<E>)
          {
            if constexpr (std::is_void_v<V>)
              {
                seen.clear ();
                the_log ().clear ();
                auto r1 = lvalue.and_then (value_fn);
                t.mark (label ("and_then &", state, "") + " " + seen + "-> "
                        + describe (r1));
                seen.clear ();
                the_log ().clear ();
                auto r2 = const_lvalue.and_then (value_fn);
                t.mark (label ("and_then const&", state, "") + " " + seen
                        + "-> " + describe (r2));
              }
            else if constexpr (requires { lvalue.and_then (value_fn); })
              {
                seen.clear ();
                the_log ().clear ();
                auto r1 = lvalue.and_then (value_fn);
                t.mark (label ("and_then &", state, "") + " " + seen + "-> "
                        + describe (r1));
                seen.clear ();
                the_log ().clear ();
                auto r2 = const_lvalue.and_then (value_fn);
                t.mark (label ("and_then const&", state, "") + " " + seen
                        + "-> " + describe (r2));
              }
          }
        {
          X rvalue = make_state<W, V, E> (state, 1);
          seen.clear ();
          the_log ().clear ();
          auto r1 = std::move (rvalue).and_then (value_fn);
          t.mark (label ("and_then &&", state, "") + " " + seen + "-> "
                  + describe (r1) + " / " + describe (rvalue));
        }
        {
          X rvalue = make_state<W, V, E> (state, 1);
          X const &as_const = rvalue;
          seen.clear ();
          the_log ().clear ();
          auto r1 = std::move (as_const).and_then (value_fn);
          t.mark (label ("and_then const&&", state, "") + " " + seen + "-> "
                  + describe (r1));
        }
        // transform
        {
          seen.clear ();
          the_log ().clear ();
          auto r1 = lvalue.transform (plain_fn);
          t.mark (label ("transform &", state, "") + " " + seen + "-> "
                  + describe (r1));
          seen.clear ();
          the_log ().clear ();
          auto r2 = const_lvalue.transform (plain_fn);
          t.mark (label ("transform const&", state, "") + " " + seen + "-> "
                  + describe (r2));
        }
        {
          X rvalue = make_state<W, V, E> (state, 1);
          seen.clear ();
          the_log ().clear ();
          auto r1 = std::move (rvalue).transform (plain_fn);
          t.mark (label ("transform &&", state, "") + " " + seen + "-> "
                  + describe (r1) + " / " + describe (rvalue));
        }
        {
          X rvalue = make_state<W, V, E> (state, 1);
          X const &as_const = rvalue;
          seen.clear ();
          the_log ().clear ();
          auto r1 = std::move (as_const).transform (plain_fn);
          t.mark (label ("transform const&&", state, "") + " " + seen + "-> "
                  + describe (r1));
        }
        // or_else
        {
          seen.clear ();
          the_log ().clear ();
          auto r1 = lvalue.or_else (error_fn);
          t.mark (label ("or_else &", state, "") + " " + seen + "-> "
                  + describe (r1));
          seen.clear ();
          the_log ().clear ();
          auto r2 = const_lvalue.or_else (error_fn);
          t.mark (label ("or_else const&", state, "") + " " + seen + "-> "
                  + describe (r2));
        }
        {
          X rvalue = make_state<W, V, E> (state, 1);
          seen.clear ();
          the_log ().clear ();
          auto r1 = std::move (rvalue).or_else (error_fn);
          t.mark (label ("or_else &&", state, "") + " " + seen + "-> "
                  + describe (r1) + " / " + describe (rvalue));
        }
        {
          X rvalue = make_state<W, V, E> (state, 1);
          X const &as_const = rvalue;
          seen.clear ();
          the_log ().clear ();
          auto r1 = std::move (as_const).or_else (error_fn);
          t.mark (label ("or_else const&&", state, "") + " " + seen + "-> "
                  + describe (r1));
        }
        // transform_error
        {
          seen.clear ();
          the_log ().clear ();
          auto r1 = lvalue.transform_error (error_plain_fn);
          t.mark (label ("transform_error &", state, "") + " " + seen + "-> "
                  + describe (r1));
          seen.clear ();
          the_log ().clear ();
          auto r2 = const_lvalue.transform_error (error_plain_fn);
          t.mark (label ("transform_error const&", state, "") + " " + seen
                  + "-> " + describe (r2));
        }
        {
          X rvalue = make_state<W, V, E> (state, 1);
          seen.clear ();
          the_log ().clear ();
          auto r1 = std::move (rvalue).transform_error (error_plain_fn);
          t.mark (label ("transform_error &&", state, "") + " " + seen + "-> "
                  + describe (r1) + " / " + describe (rvalue));
        }
        {
          X rvalue = make_state<W, V, E> (state, 1);
          X const &as_const = rvalue;
          seen.clear ();
          the_log ().clear ();
          auto r1 = std::move (as_const).transform_error (error_plain_fn);
          t.mark (label ("transform_error const&&", state, "") + " " + seen
                  + "-> " + describe (r1));
        }
      }
    return t.text;
  }
};

// ---------------------------------------------------------------------
// Part 5: comparisons, the types of the observers, bad_expected_access
// ---------------------------------------------------------------------

/// What `==` and `!=` give for every pair of states, between expected objects
/// of the same and of different types, with a value and with an unexpected,
/// both ways round.
template <typename W>
std::string
comparison_transcript ()
{
  using EI = typename W::template ex<int, int>;
  using EL = typename W::template ex<long, long>;
  using VI = typename W::template ex<void, int>;
  using VL = typename W::template ex<void, long>;
  using UI = typename W::template un<int>;
  using UL = typename W::template un<long>;
  std::string text;
  auto both = [&] (char const *name, auto const &lhs, auto const &rhs)
    {
      text += std::string (name) + ' ' + (lhs == rhs ? 'T' : 'F')
              + (lhs != rhs ? 'T' : 'F') + (rhs == lhs ? 'T' : 'F')
              + (rhs != lhs ? 'T' : 'F') + "\n";
    };
  EI const states[] = { EI (W::in_place (), 1), EI (W::in_place (), 2),
                        EI (W::unexpect (), 1), EI (W::unexpect (), 2) };
  for (EI const &a : states)
    {
      for (EI const &b : states)
        both ("ex<int,int> ex<int,int>", a, b);
      for (EI const &b : states)
        both ("ex<int,int> ex<long,long>", a,
              b.has_value ()
                  ? EL (W::in_place (), static_cast<long> (*b))
                  : EL (W::unexpect (), static_cast<long> (b.error ())));
      both ("ex<int,int> int 1", a, 1);
      both ("ex<int,int> long 2", a, 2L);
      both ("ex<int,int> unexpected<int> 1", a, UI (W::in_place (), 1));
      both ("ex<int,int> unexpected<long> 2", a, UL (W::in_place (), 2));
    }
  VI const voids[] = { VI (W::in_place ()), VI (W::unexpect (), 1),
                       VI (W::unexpect (), 2) };
  for (VI const &a : voids)
    {
      for (VI const &b : voids)
        both ("ex<void,int> ex<void,int>", a, b);
      for (VI const &b : voids)
        both ("ex<void,int> ex<void,long>", a,
              b.has_value ()
                  ? VL (W::in_place ())
                  : VL (W::unexpect (), static_cast<long> (b.error ())));
      both ("ex<void,int> unexpected<int> 1", a, UI (W::in_place (), 1));
    }
  return text;
}

/// Which `==` are well-formed.
template <typename W>
answers_t
comparison_answers ()
{
  using EI = typename W::template ex<int, int>;
  using ES = typename W::template ex<std::string, int>;
  using VI = typename W::template ex<void, int>;
  using UI = typename W::template un<int>;
  using US = typename W::template un<std::string>;
  using UN = typename W::template un<std::unique_ptr<int>>;
  using EU = typename W::template ex<int, std::unique_ptr<int>>;
  answers_t result;
  note (
      result, "ex<int,int> == ex<int,int>",
      requires (EI const &a) { a == a; });
  note (
      result, "ex<int,int> == ex<string,int>",
      requires (EI const &a, ES const &b) { a == b; });
  note (
      result, "ex<int,int> == ex<void,int>",
      requires (EI const &a, VI const &b) { a == b; });
  note (
      result, "ex<void,int> == ex<int,int>",
      requires (VI const &a, EI const &b) { a == b; });
  note (result, "ex<int,int> == int", requires (EI const &a) { a == 1; });
  note (
      result, "ex<int,int> == string",
      requires (EI const &a, std::string const &b) { a == b; });
  note (
      result, "ex<string,int> == const char*",
      requires (ES const &a) { a == "x"; });
  note (result, "ex<void,int> == int", requires (VI const &a) { a == 1; });
  note (
      result, "ex<int,int> == unexpected<int>",
      requires (EI const &a, UI const &b) { a == b; });
  note (
      result, "ex<int,int> == unexpected<string>",
      requires (EI const &a, US const &b) { a == b; });
  note (
      result, "ex<void,int> == unexpected<int>",
      requires (VI const &a, UI const &b) { a == b; });
  note (
      result, "unexpected<int> == ex<int,int>",
      requires (EI const &a, UI const &b) { b == a; });
  note (
      result, "ex<int,unique_ptr> == ex<int,unique_ptr>",
      requires (EU const &a) { a == a; });
  note (
      result, "ex<int,unique_ptr> == unexpected<unique_ptr>",
      requires (EU const &a, UN const &b) { a == b; });
  note (result, "ex<int,int> != int", requires (EI const &a) { a != 1; });
  return result;
}

/// The types the observers and the modifiers return.
template <typename W>
answers_t
observer_type_answers ()
{
  using EI = typename W::template ex<int, long>;
  using CI = typename W::template ex<int const, long>;
  using VI = typename W::template ex<void, long>;
  using ES = typename W::template ex<std::string, long>;
  answers_t result;
  note (result, "value &",
        std::is_same_v<decltype (std::declval<EI &> ().value ()), int &>);
  note (result, "value const&",
        std::is_same_v<decltype (std::declval<EI const &> ().value ()),
                       int const &>);
  note (result, "value &&",
        std::is_same_v<decltype (std::declval<EI &&> ().value ()), int &&>);
  note (result, "value const&&",
        std::is_same_v<decltype (std::declval<EI const &&> ().value ()),
                       int const &&>);
  note (result, "* &",
        std::is_same_v<decltype (*std::declval<EI &> ()), int &>);
  note (result, "* const&",
        std::is_same_v<decltype (*std::declval<EI const &> ()), int const &>);
  note (result, "* &&",
        std::is_same_v<decltype (*std::declval<EI &&> ()), int &&>);
  note (
      result, "* const&&",
      std::is_same_v<decltype (*std::declval<EI const &&> ()), int const &&>);
  note (result, "->",
        std::is_same_v<decltype (std::declval<EI &> ().operator->()), int *>);
  note (result, "-> const",
        std::is_same_v<decltype (std::declval<EI const &> ().operator->()),
                       int const *>);
  note (result, "error &",
        std::is_same_v<decltype (std::declval<EI &> ().error ()), long &>);
  note (result, "error const&",
        std::is_same_v<decltype (std::declval<EI const &> ().error ()),
                       long const &>);
  note (result, "error &&",
        std::is_same_v<decltype (std::declval<EI &&> ().error ()), long &&>);
  note (result, "error const&&",
        std::is_same_v<decltype (std::declval<EI const &&> ().error ()),
                       long const &&>);
  note (result, "value_or const&",
        std::is_same_v<decltype (std::declval<EI const &> ().value_or (1)),
                       int>);
  note (result, "value_or &&",
        std::is_same_v<decltype (std::declval<EI &&> ().value_or (1)), int>);
  note (result, "error_or const&",
        std::is_same_v<decltype (std::declval<EI const &> ().error_or (1L)),
                       long>);
  note (result, "error_or &&",
        std::is_same_v<decltype (std::declval<EI &&> ().error_or (1L)), long>);
  note (
      result, "const T: value &",
      std::is_same_v<decltype (std::declval<CI &> ().value ()), int const &>);
  note (result, "const T: value &&",
        std::is_same_v<decltype (std::declval<CI &&> ().value ()),
                       int const &&>);
  note (result, "const T: value_or",
        std::is_same_v<decltype (std::declval<CI const &> ().value_or (1)),
                       int const>);
  note (result, "const T: emplace",
        std::is_same_v<decltype (std::declval<CI &> ().emplace (1)),
                       int const &>);
  note (result, "emplace",
        std::is_same_v<decltype (std::declval<EI &> ().emplace (1)), int &>);
  note (result, "void: value const&",
        std::is_same_v<decltype (std::declval<VI const &> ().value ()), void>);
  note (result, "void: value &&",
        std::is_same_v<decltype (std::declval<VI &&> ().value ()), void>);
  note (result, "void: *",
        std::is_same_v<decltype (*std::declval<VI const &> ()), void>);
  note (result, "void: error &",
        std::is_same_v<decltype (std::declval<VI &> ().error ()), long &>);
  note (result, "void: emplace",
        std::is_same_v<decltype (std::declval<VI &> ().emplace ()), void>);
  note (result, "copy assign",
        std::is_same_v<decltype (std::declval<EI &> ()
                                 = std::declval<EI const &> ()),
                       EI &>);
  note (
      result, "move assign",
      std::is_same_v<decltype (std::declval<EI &> () = std::declval<EI &&> ()),
                     EI &>);
  note (result, "assign value",
        std::is_same_v<decltype (std::declval<EI &> () = 1), EI &>);
  note (result, "assign unexpected",
        std::is_same_v<decltype (std::declval<EI &> () = std::declval<
                                     typename W::template un<long>> ()),
                       EI &>);
  note (result, "swap",
        std::is_same_v<decltype (std::declval<EI &> ().swap (
                           std::declval<EI &> ())),
                       void>);
  note (result, "and_then",
        std::is_same_v<
            decltype (std::declval<EI &> ().and_then (
                [] (int) { return typename W::template ex<char, long> (); })),
            typename W::template ex<char, long>>);
  note (result, "transform",
        std::is_same_v<decltype (std::declval<EI &> ().transform (
                           [] (int) { return 1.5; })),
                       typename W::template ex<double, long>>);
  note (
      result, "transform to void",
      std::is_same_v<decltype (std::declval<EI &> ().transform ([] (int) {})),
                     typename W::template ex<void, long>>);
  note (
      result, "transform to expected",
      std::is_same_v<
          decltype (std::declval<EI &> ().transform (
              [] (int) { return typename W::template ex<char, char> (); })),
          typename W::template ex<typename W::template ex<char, char>, long>>);
  note (result, "transform const result",
        std::is_same_v<decltype (std::declval<EI &> ().transform (
                           [] (int) -> int const { return 1; })),
                       typename W::template ex<int, long>>);
  note (result, "or_else",
        std::is_same_v<
            decltype (std::declval<EI &> ().or_else (
                [] (long) { return typename W::template ex<int, char> (); })),
            typename W::template ex<int, char>>);
  note (result, "transform_error",
        std::is_same_v<decltype (std::declval<EI &> ().transform_error (
                           [] (long) { return 'c'; })),
                       typename W::template ex<int, char>>);
  note (result, "void: and_then",
        std::is_same_v<
            decltype (std::declval<VI &> ().and_then (
                [] { return typename W::template ex<char, long> (); })),
            typename W::template ex<char, long>>);
  note (result, "void: transform",
        std::is_same_v<decltype (std::declval<VI &> ().transform (
                           [] { return 1.5; })),
                       typename W::template ex<double, long>>);
  note (result, "void: or_else",
        std::is_same_v<
            decltype (std::declval<VI &> ().or_else (
                [] (long) { return typename W::template ex<void, char> (); })),
            typename W::template ex<void, char>>);
  note (result, "void: transform_error",
        std::is_same_v<decltype (std::declval<VI &> ().transform_error (
                           [] (long) { return 'c'; })),
                       typename W::template ex<void, char>>);
  using V = typename W::template ex<int, long>::value_type;
  using Er = typename W::template ex<int, long>::error_type;
  using Un = typename W::template ex<int, long>::unexpected_type;
  note (result, "value_type", std::is_same_v<V, int>);
  note (result, "error_type", std::is_same_v<Er, long>);
  note (result, "unexpected_type",
        std::is_same_v<Un, typename W::template un<long>>);
  note (result, "rebind",
        std::is_same_v<typename EI::template rebind<char>,
                       typename W::template ex<char, long>>);
  note (result, "void: value_type",
        std::is_same_v<typename VI::value_type, void>);
  note (result, "void: rebind",
        std::is_same_v<typename VI::template rebind<char>,
                       typename W::template ex<char, long>>);
  return result;
}

/// What `value ()` throws, in every category, and what can catch it.
template <typename W>
std::string
bad_access_transcript ()
{
  using EP = typename W::template ex<int, e_nothrow_t>;
  using VP = typename W::template ex<void, e_nothrow_t>;
  using B = typename W::template bad_access<e_nothrow_t>;
  using Base = typename W::bad_access_void;
  transcript t;
  auto record = [&] (char const *name, auto &&call)
    {
      the_log ().clear ();
      try
        {
          call ();
          t.mark (std::string (name) + " no throw");
        }
      catch (B &caught)
        {
          t.mark (
              std::string (name)
              + " B id=" + std::to_string (caught.error ().id)
              + (std::is_base_of_v<Base, B> ? " derived" : " unrelated")
              + (std::is_base_of_v<std::exception, B> ? " exception" : ""));
        }
    };
  EP lvalue (W::unexpect (), 3);
  EP const const_lvalue (W::unexpect (), 4);
  record ("value &", [&] { static_cast<void> (lvalue.value ()); });
  record ("value const&", [&] { static_cast<void> (const_lvalue.value ()); });
  record ("value &&",
          [&] { static_cast<void> (std::move (lvalue).value ()); });
  t.text += "  after: " + describe (lvalue) + "\n";
  record ("value const&&",
          [&] { static_cast<void> (std::move (const_lvalue).value ()); });
  VP void_lvalue (W::unexpect (), 5);
  VP const void_const (W::unexpect (), 6);
  record ("void value const&", [&] { void_const.value (); });
  record ("void value &&", [&] { std::move (void_lvalue).value (); });
  t.text += "  after: " + describe (void_lvalue) + "\n";
  EP ok (W::in_place (), 1);
  record ("value of a value", [&] { static_cast<void> (ok.value ()); });
  try
    {
      static_cast<void> (lvalue.value ());
    }
  catch (Base const &caught)
    {
      t.text += std::string ("base catch: ")
                + (caught.what () != nullptr ? "what" : "null") + "\n";
    }
  try
    {
      static_cast<void> (const_lvalue.value ());
    }
  catch (std::exception const &caught)
    {
      t.text += std::string ("std::exception catch: ")
                + (caught.what () != nullptr ? "what" : "null") + "\n";
    }
  static_assert (
      std::is_same_v<decltype (std::declval<B &> ().error ()), e_nothrow_t &>);
  static_assert (std::is_same_v<decltype (std::declval<B const &> ().error ()),
                                e_nothrow_t const &>);
  static_assert (std::is_same_v<decltype (std::declval<B &&> ().error ()),
                                e_nothrow_t &&>);
  static_assert (
      std::is_same_v<decltype (std::declval<B const &&> ().error ()),
                     e_nothrow_t const &&>);
  static_assert (std::is_nothrow_copy_constructible_v<Base>
                 == std::is_nothrow_copy_constructible_v<Base>);
  return t.text;
}

/// The access exception itself: what can be built, copied and caught.
template <typename W>
answers_t
bad_access_answers ()
{
  using B = typename W::template bad_access<int>;
  using Base = typename W::bad_access_void;
  answers_t result;
  note (result, "explicit from E", std::is_constructible_v<B, int>);
  note (result, "implicit from E", std::is_convertible_v<int, B>);
  note (result, "copy constructible", std::is_copy_constructible_v<B>);
  note (result, "move constructible", std::is_move_constructible_v<B>);
  note (result, "copy assignable", std::is_copy_assignable_v<B>);
  note (result, "nothrow copy", std::is_nothrow_copy_constructible_v<B>);
  note (result, "derived from the void one", std::is_base_of_v<Base, B>);
  note (result, "derived from exception",
        std::is_base_of_v<std::exception, B>);
  note (result, "void: default constructible",
        std::is_default_constructible_v<Base>);
  note (result, "void: copy constructible",
        std::is_copy_constructible_v<Base>);
  note (result, "void: move constructible",
        std::is_move_constructible_v<Base>);
  note (result, "void: nothrow copy",
        std::is_nothrow_copy_constructible_v<Base>);
  note (result, "void: nothrow move",
        std::is_nothrow_move_constructible_v<Base>);
  note (result, "void: nothrow copy assign",
        std::is_nothrow_copy_assignable_v<Base>);
  note (result, "void: nothrow default",
        std::is_nothrow_default_constructible_v<Base>);
  note (result, "void: derived from exception",
        std::is_base_of_v<std::exception, Base>);
  note (result, "what noexcept",
        noexcept (std::declval<B const &> ().what ()));
  note (result, "void: what noexcept",
        noexcept (std::declval<Base const &> ().what ()));
  return result;
}

/// The tags: what can be done with `std::in_place_t` / `std::unexpect_t`.
template <typename Tag>
answers_t
tag_answers ()
{
  answers_t result;
  note (result, "default constructible", std::is_default_constructible_v<Tag>);
  note (result, "trivially default constructible",
        std::is_trivially_default_constructible_v<Tag>);
  note (result, "nothrow default constructible",
        std::is_nothrow_default_constructible_v<Tag>);
  note (result, "trivially copyable", std::is_trivially_copyable_v<Tag>);
  note (result, "empty", std::is_empty_v<Tag>);
  note (result, "standard layout", std::is_standard_layout_v<Tag>);
  note (result, "aggregate", std::is_aggregate_v<Tag>);
  note (
      result, "copy-list-initializable from {}",
      requires (void (&function) (Tag)) { function ({}); });
  note (result, "constructible from int", std::is_constructible_v<Tag, int>);
  note (result, "convertible from int", std::is_convertible_v<int, Tag>);
  return result;
}

/// Move-only value and error (`std::unique_ptr`): what is possible and what
/// is left behind.
template <typename W>
std::string
move_only_transcript ()
{
  using P = std::unique_ptr<int>;
  using X = typename W::template ex<P, P>;
  using V = typename W::template ex<void, P>;
  std::string text;
  auto show = [] (auto const &pointer)
    {
      if constexpr (std::is_same_v<std::remove_cvref_t<decltype (pointer)>,
                                   int>)
        return std::to_string (pointer);
      else
        return pointer ? std::to_string (*pointer) : std::string ("null");
    };
  auto describe_ptr = [&] (auto const &uut)
    {
      if (uut.has_value ())
        {
          if constexpr (std::is_void_v<typename std::remove_cvref_t<
                            decltype (uut)>::value_type>)
            return std::string ("V()");
          else
            return "V" + show (*uut);
        }
      return "E" + show (uut.error ());
    };
  auto make = [&] (bool value, int id)
    {
      return value ? X (W::in_place (), new int (id))
                   : X (W::unexpect (), new int (id));
    };
  for (bool dst_value : { true, false })
    for (bool src_value : { true, false })
      {
        X dst = make (dst_value, 1);
        X src = make (src_value, 2);
        dst = std::move (src);
        text += std::string ("move-assign ") + (dst_value ? "V" : "E") + "<-"
                + (src_value ? "V" : "E") + " " + describe_ptr (dst) + " / "
                + describe_ptr (src) + "\n";
        X a = make (dst_value, 1);
        X b = make (src_value, 2);
        a.swap (b);
        text += std::string ("swap ") + (dst_value ? "V" : "E") + "<->"
                + (src_value ? "V" : "E") + " " + describe_ptr (a) + " / "
                + describe_ptr (b) + "\n";
        X c = make (dst_value, 3);
        X d (std::move (c));
        text += std::string ("move-construct ") + (dst_value ? "V" : "E") + " "
                + describe_ptr (d) + " / " + describe_ptr (c) + "\n";
      }
  static_assert (!std::is_copy_constructible_v<X>);
  static_assert (!std::is_copy_assignable_v<X>);
  static_assert (std::is_nothrow_move_constructible_v<X>);
  static_assert (std::is_nothrow_move_assignable_v<X>);
  static_assert (std::is_nothrow_swappable_v<X>);
  {
    X value = make (true, 4);
    P taken = std::move (value).value_or (P (new int (9)));
    text
        += "value_or && " + show (taken) + " / " + describe_ptr (value) + "\n";
    X error = make (false, 5);
    P other = std::move (error).value_or (P (new int (9)));
    text += "value_or && of an error " + show (other) + " / "
            + describe_ptr (error) + "\n";
    X moved_error = make (false, 6);
    P error_out = std::move (moved_error).error_or (P (new int (9)));
    text += "error_or && " + show (error_out) + " / "
            + describe_ptr (moved_error) + "\n";
    X holding_value = make (true, 7);
    P substitute = std::move (holding_value).error_or (P (new int (8)));
    text += "error_or && of a value " + show (substitute) + "\n";
  }
  {
    X value = make (true, 4);
    auto chained = std::move (value).and_then (
        [] (P &&pointer)
          {
            return typename W::template ex<P, P> (W::in_place (),
                                                  new int (*pointer + 10));
          });
    text += "and_then && " + describe_ptr (chained) + " / "
            + describe_ptr (value) + "\n";
    X error = make (false, 5);
    auto recovered = std::move (error).or_else (
        [] (P &&pointer)
          {
            return typename W::template ex<P, P> (W::in_place (),
                                                  new int (*pointer + 20));
          });
    text += "or_else && " + describe_ptr (recovered) + " / "
            + describe_ptr (error) + "\n";
    X mapped_source = make (true, 6);
    auto mapped = std::move (mapped_source)
                      .transform ([] (P &&pointer) { return *pointer + 1; });
    text += "transform && " + describe_ptr (mapped) + "\n";
    X mapped_error_source = make (false, 7);
    auto mapped_error
        = std::move (mapped_error_source)
              .transform_error ([] (P &&pointer) { return *pointer + 2; });
    text += "transform_error && " + describe_ptr (mapped_error) + "\n";
  }
  {
    V ok (W::in_place ());
    V bad (W::unexpect (), new int (11));
    V sink (W::unexpect (), new int (12));
    sink = std::move (bad);
    text += "void move-assign " + describe_ptr (sink) + " / "
            + describe_ptr (bad) + "\n";
    V another (std::move (sink));
    text += "void move-construct " + describe_ptr (another) + " / "
            + describe_ptr (sink) + "\n";
    ok.swap (another);
    text += "void swap " + describe_ptr (ok) + " / " + describe_ptr (another)
            + "\n";
  }
  return text;
}

/// The first line on which two transcripts differ, with the line of each.
std::string
first_difference (std::string const &in_std, std::string const &in_lumex)
{
  std::size_t begin = 0;
  while (begin < in_std.size () && begin < in_lumex.size ()
         && in_std[begin] == in_lumex[begin])
    ++begin;
  while (begin > 0 && in_std[begin - 1] != '\n')
    --begin;
  auto line_at = [] (std::string const &text, std::size_t from)
    {
      std::size_t const end = text.find ('\n', from);
      return text.substr (from, end == std::string::npos ? end : end - from);
    };
  return "\n   std:   " + line_at (in_std, begin)
         + "\n   lumex: " + line_at (in_lumex, begin);
}

template <typename Scenario, typename... Pairs>
std::string
compare_scenario (type_list<Pairs...>)
{
  std::string report;
  ((report +=
    [&]
      {
        std::string const in_std
            = Scenario::template run<std_world, typename Pairs::value_type,
                                     typename Pairs::error_type> ();
        std::string const in_lumex
            = Scenario::template run<lumex_world, typename Pairs::value_type,
                                     typename Pairs::error_type> ();
        return in_std == in_lumex
                   ? std::string ()
                   : pair_name<typename Pairs::value_type,
                               typename Pairs::error_type> ()
                         + ":" + first_difference (in_std, in_lumex) + "\n";
      }()),
   ...);
  return report;
}

template <typename Scenario, typename... Errors>
std::string
compare_scenario_void (type_list<Errors...>)
{
  std::string report;
  ((report +=
    [&]
      {
        std::string const in_std
            = Scenario::template run<std_world, void, Errors> ();
        std::string const in_lumex
            = Scenario::template run<lumex_world, void, Errors> ();
        return in_std == in_lumex
                   ? std::string ()
                   : error_name<Errors> () + ":"
                         + first_difference (in_std, in_lumex) + "\n";
      }()),
   ...);
  return report;
}

} // namespace

// === the traits ==========================================================

TEST (ExpectedStdDifferentialTest, SpecialMemberTraits_EqualThoseOfStdExpected)
{
  EXPECT_EQ (compare_over<special_member_traits> (pair_zoo ()), "");
}

TEST (ExpectedStdDifferentialTest,
      SpecialMemberTraitsOfVoid_EqualThoseOfStdExpected)
{
  EXPECT_EQ (compare_over_void<special_member_traits> (void_zoo ()), "");
}

TEST (ExpectedStdDifferentialTest, Constructibility_EqualsThatOfStdExpected)
{
  EXPECT_EQ (compare_over<construction_traits> (pair_zoo ()), "");
}

TEST (ExpectedStdDifferentialTest,
      ConstructibilityOfVoid_EqualsThatOfStdExpected)
{
  EXPECT_EQ (compare_over_void<construction_traits> (void_zoo ()), "");
}

TEST (ExpectedStdDifferentialTest, ConvertingConstructors_AcceptWhatStdAccepts)
{
  // expected<bool, int> built from an expected<int, int>: libstdc++ 13 comes
  // before the resolution of LWG 3836 and builds the bool from the
  // expected<int, int> as a whole (through its operator bool), so the
  // conversion is explicit there; the resolution converts the value inside
  // and the conversion is implicit, as lumex does.
  std::vector<known_difference> const known
      = { { "ex<bool,int> <- ex<int,int> from const& convertible", false,
            true },
          { "ex<bool,int> <- ex<int,int> from && convertible", false, true } };
  EXPECT_EQ (differences (conversion_answers<std_world> (),
                          conversion_answers<lumex_world> (), known),
             "");
}

TEST (ExpectedStdDifferentialTest, ObserverSpecifications_AreNotWeakerThanStd)
{
  EXPECT_EQ (compare_over<noexcept_traits> (pair_zoo ()), "");
  EXPECT_EQ (compare_over_void<noexcept_traits> (void_zoo ()), "");
}

TEST (ExpectedStdDifferentialTest, ModifierSpecifications_AreNotWeakerThanStd)
{
  EXPECT_EQ (compare_over<modifier_noexcept> (pair_zoo ()), "");
  EXPECT_EQ (compare_over_void<modifier_noexcept> (void_zoo ()), "");
}

// === the order of the operations =========================================

#define LUMEX_DIFF_SCENARIO(Test, Scenario)                                   \
  TEST (ExpectedStdDifferentialTest, Test)                                    \
  {                                                                           \
    EXPECT_EQ (compare_scenario<Scenario> (probe_pairs ()), "");              \
    EXPECT_EQ (compare_scenario_void<Scenario> (probe_errors ()), "");        \
  }

LUMEX_DIFF_SCENARIO (Assignment_OfAnotherExpected_DoesWhatStdDoes, assignments)
LUMEX_DIFF_SCENARIO (Assignment_OfAValueOrAnUnexpected_DoesWhatStdDoes,
                     assignments_of_contents)
LUMEX_DIFF_SCENARIO (Swap_DoesWhatStdDoes, swaps)
LUMEX_DIFF_SCENARIO (Emplace_DoesWhatStdDoes, emplacements)
LUMEX_DIFF_SCENARIO (Construction_DoesWhatStdDoes, constructions)
LUMEX_DIFF_SCENARIO (DefaultConstruction_DoesWhatStdDoes, defaulted_or_not)
LUMEX_DIFF_SCENARIO (Observers_DoWhatStdDoes, observers)
LUMEX_DIFF_SCENARIO (MonadicOperations_DoWhatStdDoes, monadic_operations)

#undef LUMEX_DIFF_SCENARIO

// === results and exceptions ================================================

TEST (ExpectedStdDifferentialTest, Comparisons_GiveWhatStdGives)
{
  EXPECT_EQ (comparison_transcript<std_world> (),
             comparison_transcript<lumex_world> ());
}

TEST (ExpectedStdDifferentialTest, Comparisons_AreWellFormedWhereStdsAre)
{
  // The Constraints of [expected.object.eq]/1, /3 and /5 and of
  // [expected.void.eq]/1 and /3 (the expressions *x == *y, *x == v and
  // x.error () == e.error () are well-formed) are missing from libstdc++ 13:
  // its == accepts any pair and fails inside the body. lumex has them, so
  // `==` of a pair that cannot be compared is not a candidate. The entries
  // match only a library without the Constraints.
  std::vector<known_difference> const known
      = { { "ex<int,int> == ex<string,int>", true, false },
          { "ex<int,int> == ex<void,int>", true, false },
          { "ex<void,int> == ex<int,int>", true, false },
          { "ex<int,int> == string", true, false },
          { "ex<int,int> == unexpected<string>", true, false } };
  EXPECT_EQ (differences (comparison_answers<std_world> (),
                          comparison_answers<lumex_world> (), known),
             "");
}

TEST (ExpectedStdDifferentialTest, ObserverAndMonadicTypes_AreThoseOfStd)
{
  EXPECT_EQ (differences (observer_type_answers<std_world> (),
                          observer_type_answers<lumex_world> ()),
             "");
}

TEST (ExpectedStdDifferentialTest, Value_ThrowsWhatStdThrows)
{
  EXPECT_EQ (bad_access_transcript<std_world> (),
             bad_access_transcript<lumex_world> ());
}

TEST (ExpectedStdDifferentialTest, Tags_AreWhatStdsAre)
{
  EXPECT_EQ (differences (tag_answers<std::in_place_t> (),
                          tag_answers<lx::result::in_place_tag> ()),
             "");
  EXPECT_EQ (differences (tag_answers<std::unexpect_t> (),
                          tag_answers<lx::result::unexpect_t> ()),
             "");
}

TEST (ExpectedStdDifferentialTest, MoveOnlyContents_BehaveAsInStd)
{
  EXPECT_EQ (move_only_transcript<std_world> (),
             move_only_transcript<lumex_world> ());
}

TEST (ExpectedStdDifferentialTest, BadExpectedAccess_IsWhatStdsIs)
{
  EXPECT_EQ (differences (bad_access_answers<std_world> (),
                          bad_access_answers<lumex_world> ()),
             "");
}

#else // defined(__cpp_lib_expected)

TEST (ExpectedStdDifferentialTest, StdExpectedIsAvailable)
{
  GTEST_SKIP () << "this standard library has no std::expected "
                   "(__cpp_lib_expected)";
}

#endif
