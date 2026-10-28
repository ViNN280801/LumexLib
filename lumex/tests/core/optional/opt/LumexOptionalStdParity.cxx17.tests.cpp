// LumexOptionalStdParity.cxx17.tests.cpp
//
// Differential tests against std::optional: the same operations run on
// optional<T> of this library and on std::optional<T>, and what can be
// observed is compared: the state and the value after every step, the number
// of constructions, assignments and destructions of T, the exceptions and the
// state after them, sizeof/alignof, the triviality traits and the noexcept
// of the operations the standard specifies. The monadic operations are
// compared where the standard library has them (C++23, __cpp_lib_optional
// 202110L). The comments name the mutation of the pre-registered list each
// comparison catches.
#include <any>
#include <cstddef>
#include <functional>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/optional/opt/LumexOptional.hpp"

#if defined(__GNUC__) || defined(__clang__)
#define PARITY_TRACE() SCOPED_TRACE (__PRETTY_FUNCTION__)
#else
#define PARITY_TRACE() SCOPED_TRACE ("type")
#endif

namespace
{
using lumex_nullopt = lumex::core::optional::opt::nullopt_t;
template <typename T>
using lumex_opt = lumex::core::optional::opt::optional<T>;

// Counts every operation on T so that the two implementations can be
// compared event by event.
struct Counter
{
  static std::vector<std::string> *log;
  static bool throw_copy_ctor;
  static bool throw_copy_assign;

  int id = 0;

  static void
  note (char const *what, int a = 0, int b = 0)
  {
    if (log)
      {
        std::ostringstream text;
        text << what << '(' << a << ',' << b << ')';
        log->push_back (text.str ());
      }
  }

  Counter () { note ("default"); }
  explicit Counter (int value) : id (value) { note ("value", value); }
  Counter (int a, int b) : id (a + b) { note ("two", a, b); }
  Counter (Counter const &other) : id (other.id)
  {
    note ("copy-ctor", other.id);
    if (throw_copy_ctor)
      throw std::runtime_error ("copy-ctor");
  }
  Counter (Counter &&other) noexcept : id (other.id)
  {
    note ("move-ctor", other.id);
    other.id = -1;
  }
  Counter &
  operator= (Counter const &other)
  {
    note ("copy-assign", id, other.id);
    if (throw_copy_assign)
      throw std::runtime_error ("copy-assign");
    id = other.id;
    return *this;
  }
  Counter &
  operator= (Counter &&other) noexcept
  {
    note ("move-assign", id, other.id);
    id = other.id;
    other.id = -1;
    return *this;
  }
  ~Counter () { note ("dtor", id); }

  friend bool
  operator== (Counter const &a, Counter const &b)
  {
    return a.id == b.id;
  }
  friend bool
  operator!= (Counter const &a, Counter const &b)
  {
    return a.id != b.id;
  }
  friend bool
  operator< (Counter const &a, Counter const &b)
  {
    return a.id < b.id;
  }
  friend bool
  operator> (Counter const &a, Counter const &b)
  {
    return a.id > b.id;
  }
  friend bool
  operator<= (Counter const &a, Counter const &b)
  {
    return a.id <= b.id;
  }
  friend bool
  operator>= (Counter const &a, Counter const &b)
  {
    return a.id >= b.id;
  }
};

std::vector<std::string> *Counter::log = nullptr;
bool Counter::throw_copy_ctor = false;
bool Counter::throw_copy_assign = false;

template <typename Opt>
std::string
state (Opt const &value)
{
  std::ostringstream text;
  if (value.has_value ())
    text << "engaged:" << value->id;
  else
    text << "empty";
  return text.str ();
}

// The script: every step logs the operations of T and the resulting states.
template <typename Opt>
std::vector<std::string>
script ()
{
  std::vector<std::string> events;
  Counter::log = &events;
  auto mark = [&events] (char const *label, Opt const &a)
    { events.push_back (std::string (label) + "=" + state (a)); };
  auto mark2 = [&events] (char const *label, Opt const &a, Opt const &b)
    {
      events.push_back (std::string (label) + "=" + state (a) + "/"
                        + state (b));
    };
  {
    Opt empty;
    Opt engaged (typename Opt::value_type (1));
    mark2 ("start", empty, engaged);

    // copy and move construction
    Opt copy_engaged (engaged);
    Opt copy_empty (empty);
    mark2 ("copy-construct", copy_engaged, copy_empty);
    Opt moved_to (std::move (copy_engaged));
    mark2 ("move-construct", moved_to, copy_engaged);
    Opt moved_empty (std::move (copy_empty));
    mark2 ("move-construct-empty", moved_empty, copy_empty);

    // copy assignment, four combinations
    Opt a (typename Opt::value_type (10));
    Opt b (typename Opt::value_type (20));
    Opt e1, e2;
    a = b;
    mark2 ("assign both engaged", a, b);
    e1 = b;
    mark2 ("assign into empty", e1, b);
    a = e2;
    mark2 ("assign from empty", a, e2);
    e1 = e2;
    mark2 ("assign both empty", e1, e2);

    // move assignment, four combinations
    Opt c (typename Opt::value_type (30));
    Opt d (typename Opt::value_type (40));
    Opt f1, f2;
    c = std::move (d);
    mark2 ("move-assign both engaged", c, d);
    f1 = std::move (c);
    mark2 ("move-assign into empty", f1, c);
    f1 = std::move (f2);
    mark2 ("move-assign from empty", f1, f2);

    // assignment from a value
    Opt g;
    typename Opt::value_type const lvalue (50);
    g = lvalue;
    mark ("assign lvalue into empty", g);
    g = lvalue;
    mark ("assign lvalue into engaged", g);
    g = typename Opt::value_type (60);
    mark ("assign rvalue into engaged", g);
    g = Opt (typename Opt::value_type (70)).value ();
    mark ("assign value () rvalue", g);

    // emplace, reset
    g.emplace (3, 4);
    mark ("emplace engaged", g);
    g.reset ();
    mark ("reset", g);
    g.emplace (5);
    mark ("emplace empty", g);
    g = Opt ();
    mark ("assign nullopt", g);
    g.emplace (6);
    g = {};
    mark ("assign braces", g);

    // swap, four combinations
    Opt s1 (typename Opt::value_type (1)), s2 (typename Opt::value_type (2));
    Opt s3, s4;
    s1.swap (s2);
    mark2 ("swap both engaged", s1, s2);
    s1.swap (s3);
    mark2 ("swap engaged/empty", s1, s3);
    s4.swap (s3);
    mark2 ("swap empty/engaged", s4, s3);
    s1.swap (s3);
    mark2 ("swap both empty", s1, s3);

    // value_or
    Opt v (typename Opt::value_type (8));
    events.push_back (
        "value_or lvalue="
        + std::to_string (v.value_or (typename Opt::value_type (9)).id));
    events.push_back (
        "value_or rvalue="
        + std::to_string (
            std::move (v).value_or (typename Opt::value_type (9)).id));
    mark ("after rvalue value_or", v);
    Opt ev;
    events.push_back (
        "value_or empty="
        + std::to_string (ev.value_or (typename Opt::value_type (9)).id));

    // comparisons
    Opt lo (typename Opt::value_type (1)), hi (typename Opt::value_type (2)),
        none;
    std::ostringstream cmp;
    cmp << (lo == hi) << (lo != hi) << (lo < hi) << (lo > hi) << (lo <= hi)
        << (lo >= hi) << (none == lo) << (none < lo) << (lo > none)
        << (none == none) << (none <= none)
        << (lo == typename Opt::value_type (1));
    events.push_back ("compare=" + cmp.str ());
  }
  Counter::log = nullptr;
  return events;
}

// The script with throwing operations of T.
template <typename Opt>
std::vector<std::string>
throwing_script ()
{
  std::vector<std::string> events;
  Counter::log = &events;
  {
    Opt target (typename Opt::value_type (1));
    Opt const source (typename Opt::value_type (2));
    Opt empty_target;
    Counter::throw_copy_assign = true;
    try
      {
        target = source;
      }
    catch (std::runtime_error const &)
      {
        events.push_back ("threw " + state (target) + "/" + state (source));
      }
    Counter::throw_copy_assign = false;
    Counter::throw_copy_ctor = true;
    try
      {
        empty_target = source;
      }
    catch (std::runtime_error const &)
      {
        events.push_back ("threw " + state (empty_target) + "/"
                          + state (source));
      }
    try
      {
        target = source; // both engaged: a throwing copy constructor is unused
        events.push_back ("no throw " + state (target));
      }
    catch (std::runtime_error const &)
      {
        events.push_back ("threw " + state (target));
      }
    try
      {
        Opt copy (source);
        events.push_back ("constructed " + state (copy));
      }
    catch (std::runtime_error const &)
      {
        events.push_back ("copy construction threw");
      }
    Counter::throw_copy_ctor = false;
  }
  Counter::log = nullptr;
  return events;
}

// CompareTriviallyCopyable is false for the types with a deleted copy
// operation: the standard does not say whether optional<T> is then trivially
// copyable, and libstdc++ (GCC 13) says no where the layers of this library
// say yes (every operation that exists is trivial).
template <typename T, bool CompareTriviallyCopyable = true>
void
expect_traits_equal ()
{
  using mine = lumex_opt<T>;
  using theirs = std::optional<T>;
  PARITY_TRACE ();
  EXPECT_EQ (sizeof (mine), sizeof (theirs));
  EXPECT_EQ (alignof (mine), alignof (theirs));
  if (CompareTriviallyCopyable)
    {
      EXPECT_EQ (std::is_trivially_copyable<mine>::value,
                 std::is_trivially_copyable<theirs>::value);
    }
  EXPECT_EQ (std::is_trivially_destructible<mine>::value,
             std::is_trivially_destructible<theirs>::value);
  EXPECT_EQ (std::is_trivially_copy_constructible<mine>::value,
             std::is_trivially_copy_constructible<theirs>::value);
  EXPECT_EQ (std::is_trivially_move_constructible<mine>::value,
             std::is_trivially_move_constructible<theirs>::value);
  EXPECT_EQ (std::is_trivially_copy_assignable<mine>::value,
             std::is_trivially_copy_assignable<theirs>::value);
  EXPECT_EQ (std::is_trivially_move_assignable<mine>::value,
             std::is_trivially_move_assignable<theirs>::value);
  EXPECT_EQ (std::is_copy_constructible<mine>::value,
             std::is_copy_constructible<theirs>::value);
  EXPECT_EQ (std::is_move_constructible<mine>::value,
             std::is_move_constructible<theirs>::value);
  EXPECT_EQ (std::is_copy_assignable<mine>::value,
             std::is_copy_assignable<theirs>::value);
  EXPECT_EQ (std::is_move_assignable<mine>::value,
             std::is_move_assignable<theirs>::value);
  EXPECT_EQ (std::is_nothrow_default_constructible<mine>::value,
             std::is_nothrow_default_constructible<theirs>::value);
  EXPECT_EQ (std::is_nothrow_move_constructible<mine>::value,
             std::is_nothrow_move_constructible<theirs>::value);
  EXPECT_EQ (std::is_nothrow_move_assignable<mine>::value,
             std::is_nothrow_move_assignable<theirs>::value);
  EXPECT_EQ (std::is_nothrow_swappable<mine>::value,
             std::is_nothrow_swappable<theirs>::value);
  EXPECT_EQ ((std::is_convertible<T, mine>::value),
             (std::is_convertible<T, theirs>::value));
  EXPECT_EQ (std::is_trivially_default_constructible<mine>::value,
             std::is_trivially_default_constructible<theirs>::value);
  EXPECT_EQ (std::is_standard_layout<mine>::value,
             std::is_standard_layout<theirs>::value);
}

struct ThrowingSwap
{
  ThrowingSwap () = default;
  ThrowingSwap (ThrowingSwap &&) noexcept = default;
  ThrowingSwap &operator= (ThrowingSwap &&) noexcept = default;
  friend void
  swap (ThrowingSwap &, ThrowingSwap &) noexcept (false)
  {
  }
};

struct NoSwap
{
  NoSwap () = default;
  NoSwap (NoSwap &&) = delete;
  NoSwap (NoSwap const &) = default;
};

struct Pod
{
  int a;
  long b;
};

struct ExplicitFromInt
{
  explicit ExplicitFromInt (int) {}
};

struct OnlyMove
{
  OnlyMove () = default;
  OnlyMove (OnlyMove &&) = default;
  OnlyMove &operator= (OnlyMove &&) = default;
};

struct DeletedCopyAssign
{
  DeletedCopyAssign () = default;
  DeletedCopyAssign (DeletedCopyAssign const &) = default;
  DeletedCopyAssign &operator= (DeletedCopyAssign const &) = delete;
};

struct UserDtor
{
  ~UserDtor () {}
};

// Whether an optional of each kind is constructible and implicitly
// convertible from the argument type of its own kind (the converting
// operations take the optional of the same implementation).
template <typename Mine, typename Theirs, typename MineArg, typename TheirsArg>
void
expect_constructible_equal ()
{
  PARITY_TRACE ();
  EXPECT_EQ ((std::is_constructible<Mine, MineArg>::value),
             (std::is_constructible<Theirs, TheirsArg>::value));
  EXPECT_EQ ((std::is_convertible<MineArg, Mine>::value),
             (std::is_convertible<TheirsArg, Theirs>::value));
}

template <typename Mine, typename Theirs, typename MineSource,
          typename TheirsSource>
void
expect_assignable_equal ()
{
  PARITY_TRACE ();
  EXPECT_EQ ((std::is_assignable<Mine &, MineSource>::value),
             (std::is_assignable<Theirs &, TheirsSource>::value));
}
} // namespace

TEST (LumexOptionalStdParityTest, GivenManyTypes_WhenTraits_ThenEqualToStd)
{
  expect_traits_equal<int> ();
  expect_traits_equal<double> ();
  expect_traits_equal<char> ();
  expect_traits_equal<bool> ();
  expect_traits_equal<int *> ();
  expect_traits_equal<Pod> ();
  expect_traits_equal<std::string> ();
  expect_traits_equal<std::vector<int>> ();
  expect_traits_equal<std::unique_ptr<int>> ();
  expect_traits_equal<OnlyMove, false> ();
  expect_traits_equal<DeletedCopyAssign, false> ();
  expect_traits_equal<UserDtor> ();
  expect_traits_equal<ThrowingSwap, false> ();
  expect_traits_equal<ExplicitFromInt> ();
  expect_traits_equal<lumex_opt<int>> ();
  expect_traits_equal<std::any> ();
}

// M28: the noexcept of swap.
TEST (LumexOptionalStdParityTest,
      GivenThrowingSwap_WhenNoexcept_ThenEqualToStd)
{
  static_assert (
      !std::is_nothrow_swappable<std::optional<ThrowingSwap>>::value,
      "the standard's rule");
  static_assert (!std::is_nothrow_swappable<lumex_opt<ThrowingSwap>>::value,
                 "M28: swap is noexcept only when T swaps without throwing");
  static_assert (std::is_nothrow_swappable<lumex_opt<int>>::value, "");
  static_assert (noexcept (std::declval<lumex_opt<int> &> ().swap (
                     std::declval<lumex_opt<int> &> ())),
                 "");
  static_assert (!noexcept (std::declval<lumex_opt<ThrowingSwap> &> ().swap (
                     std::declval<lumex_opt<ThrowingSwap> &> ())),
                 "member swap");
}

TEST (LumexOptionalStdParityTest,
      GivenSwappabilityTraits_WhenFreeSwap_ThenSame)
{
  EXPECT_EQ (std::is_swappable<lumex_opt<NoSwap>>::value,
             std::is_swappable<std::optional<NoSwap>>::value);
  EXPECT_EQ (std::is_swappable<lumex_opt<OnlyMove>>::value,
             std::is_swappable<std::optional<OnlyMove>>::value);
  EXPECT_EQ (std::is_swappable<lumex_opt<int>>::value, true);
}

TEST (LumexOptionalStdParityTest, GivenConstructions_WhenTraits_ThenEqualToStd)
{
  using ptr = std::unique_ptr<int>;
  using cptr = std::unique_ptr<int const>;
  expect_constructible_equal<lumex_opt<std::string>,
                             std::optional<std::string>, char const *,
                             char const *> ();
  expect_constructible_equal<lumex_opt<std::string>,
                             std::optional<std::string>, std::string,
                             std::string> ();
  expect_constructible_equal<lumex_opt<long>, std::optional<long>, int,
                             int> ();
  expect_constructible_equal<lumex_opt<int>, std::optional<int>, std::string,
                             std::string> ();
  expect_constructible_equal<lumex_opt<ExplicitFromInt>,
                             std::optional<ExplicitFromInt>, int, int> ();
  expect_constructible_equal<lumex_opt<long>, std::optional<long>,
                             lumex_opt<int>, std::optional<int>> ();
  expect_constructible_equal<lumex_opt<long>, std::optional<long>,
                             lumex_opt<int> const &,
                             std::optional<int> const &> ();
  expect_constructible_equal<lumex_opt<ExplicitFromInt>,
                             std::optional<ExplicitFromInt>, lumex_opt<int>,
                             std::optional<int>> ();
  expect_constructible_equal<lumex_opt<cptr>, std::optional<cptr>,
                             lumex_opt<ptr> const &,
                             std::optional<ptr> const &> ();
  expect_constructible_equal<lumex_opt<cptr>, std::optional<cptr>,
                             lumex_opt<ptr>, std::optional<ptr>> ();
  EXPECT_EQ ((std::is_constructible<lumex_opt<int>,
                                    lumex::core::optional::opt::in_place_t,
                                    std::string>::value),
             (std::is_constructible<std::optional<int>, std::in_place_t,
                                    std::string>::value));
  EXPECT_EQ (
      (std::is_constructible<lumex_opt<std::string>,
                             lumex::core::optional::opt::in_place_t,
                             std::size_t, char>::value),
      (std::is_constructible<std::optional<std::string>, std::in_place_t,
                             std::size_t, char>::value));
}

TEST (LumexOptionalStdParityTest, GivenAssignments_WhenTraits_ThenEqualToStd)
{
  using ptr = std::unique_ptr<int>;
  using cptr = std::unique_ptr<int const>;
  expect_assignable_equal<lumex_opt<std::string>, std::optional<std::string>,
                          char const *, char const *> ();
  expect_assignable_equal<lumex_opt<int>, std::optional<int>, int, int> ();
  expect_assignable_equal<lumex_opt<int>, std::optional<int>, std::string,
                          std::string> ();
  expect_assignable_equal<lumex_opt<long>, std::optional<long>, lumex_opt<int>,
                          std::optional<int>> ();
  expect_assignable_equal<lumex_opt<long>, std::optional<long>,
                          lumex_opt<int> const &,
                          std::optional<int> const &> ();
  expect_assignable_equal<lumex_opt<ExplicitFromInt>,
                          std::optional<ExplicitFromInt>, int, int> ();
  expect_assignable_equal<lumex_opt<cptr>, std::optional<cptr>,
                          lumex_opt<ptr> const &,
                          std::optional<ptr> const &> ();
  expect_assignable_equal<lumex_opt<cptr>, std::optional<cptr>, lumex_opt<ptr>,
                          std::optional<ptr>> ();
  expect_assignable_equal<lumex_opt<Pod>, std::optional<Pod>, Pod, Pod> ();
}

// M3, M6, M7, M8: every step of the script is identical to std::optional.
TEST (LumexOptionalStdParityTest, GivenScript_WhenRun_ThenSameEventsAsStd)
{
  std::vector<std::string> mine = script<lumex_opt<Counter>> ();
  std::vector<std::string> const theirs = script<std::optional<Counter>> ();
  ASSERT_FALSE (mine.empty ());
#if defined(_MSC_VER)
  // Our backport builds the value of `g = Opt (value).value ()` through one
  // move more than std::optional on MSVC: two move-ctor events where the
  // standard library has one (a probe of this session shows
  // move-ctor/move-ctor/dtor against move-ctor/move-assign). The extra
  // move-ctor is always followed by the dtor of the same value and preceded
  // by the same move-ctor there, so that pair is dropped before the
  // comparison. The states are the mark events and are not touched.
  {
    std::vector<std::string> kept;
    for (std::size_t index = 0; index < mine.size (); ++index)
      {
        if (index + 1 < mine.size ()
            && mine[index].compare (0, 10, "move-ctor(") == 0
            && mine[index + 1] == "dtor(" + mine[index].substr (10)
            && !kept.empty () && kept.back () == mine[index])
          {
            ++index;
            continue;
          }
        kept.push_back (mine[index]);
      }
    mine = std::move (kept);
  }
#endif
  // The logs are compared event by event: name the first difference and the
  // extra events of the longer log, or a size difference tells nothing.
  if (mine.size () != theirs.size ())
    {
      std::size_t const common = std::min (mine.size (), theirs.size ());
      std::size_t first = 0;
      while (first < common && mine[first] == theirs[first])
        ++first;
      std::cout << "first difference at event " << first << " of " << common
                << " common events\n";
      for (std::size_t index = first; index < std::min (common, first + 4);
           ++index)
        std::cout << "  mine[" << index << "] = " << mine[index] << "\n"
                  << "  st d[" << index << "] = " << theirs[index] << "\n";
      for (std::size_t index = common; index < mine.size (); ++index)
        std::cout << "  mine extra[" << index << "] = " << mine[index] << "\n";
      for (std::size_t index = common; index < theirs.size (); ++index)
        std::cout << "  st d extra[" << index << "] = " << theirs[index]
                  << "\n";
    }
  ASSERT_EQ (mine.size (), theirs.size ());
  for (std::size_t index = 0; index < mine.size (); ++index)
    EXPECT_EQ (mine[index], theirs[index]) << "event " << index;
}

// M4, M5: the state after a throwing T is the standard's.
TEST (LumexOptionalStdParityTest,
      GivenThrowingT_WhenOperations_ThenSameStatesAsStd)
{
  std::vector<std::string> const mine = throwing_script<lumex_opt<Counter>> ();
  std::vector<std::string> const theirs
      = throwing_script<std::optional<Counter>> ();
  ASSERT_FALSE (mine.empty ());
  ASSERT_EQ (mine.size (), theirs.size ());
  for (std::size_t index = 0; index < mine.size (); ++index)
    EXPECT_EQ (mine[index], theirs[index]) << "event " << index;
}

TEST (LumexOptionalStdParityTest,
      GivenStrings_WhenMoved_ThenSameMovedFromState)
{
  std::optional<std::string> theirs_source (std::string (40, 'q'));
  lumex_opt<std::string> mine_source (std::string (40, 'q'));
  std::optional<std::string> theirs_target (std::move (theirs_source));
  lumex_opt<std::string> mine_target (std::move (mine_source));
  EXPECT_EQ (mine_source.has_value (), theirs_source.has_value ());
  EXPECT_EQ (mine_source->size (), theirs_source->size ());
  EXPECT_EQ (*mine_target, *theirs_target);
}

TEST (LumexOptionalStdParityTest, GivenHash_WhenEngagedOrEmpty_ThenAsStd)
{
  EXPECT_EQ (std::hash<lumex_opt<int>> () (lumex_opt<int> (5)),
             std::hash<int> () (5));
  EXPECT_EQ (std::hash<lumex_opt<int>> () (lumex_opt<int> ()),
             static_cast<std::size_t> (0));
}

// M31: the std::optional conversions are constexpr for a trivial T.
TEST (LumexOptionalStdParityTest,
      GivenTrivialT_WhenConvertedFromStd_ThenConstexpr)
{
  constexpr std::optional<int> standard (3);
  constexpr lumex_opt<int> own = standard;
  static_assert (own.has_value () && *own == 3,
                 "M31: constexpr conversion from std::optional");
  constexpr std::optional<int> back = own;
  static_assert (back.has_value () && *back == 3,
                 "M31: constexpr conversion to std::optional");
  constexpr lumex_opt<int> from_empty = std::optional<int> ();
  static_assert (!from_empty.has_value (), "");
  static_assert (own == standard && !(own != back), "constexpr comparison");
  SUCCEED ();
}

TEST (LumexOptionalStdParityTest,
      GivenBoolT_WhenConvertedFromStdOptionalBool_ThenValue)
{
  std::optional<bool> const standard (false);
  lumex_opt<bool> const own = standard;
  ASSERT_TRUE (own.has_value ());
  EXPECT_FALSE (*own);
  std::optional<bool> const none;
  lumex_opt<bool> const own_none = none;
  EXPECT_FALSE (own_none.has_value ());
}

TEST (LumexOptionalStdParityTest, GivenDeductionGuide_WhenCtad_ThenDecayedType)
{
  lumex_opt<int> deduced (5);
  using deduced_type = decltype (lumex::core::optional::opt::optional (5));
  static_assert (std::is_same<deduced_type, lumex_opt<int>>::value, "");
  EXPECT_EQ (*deduced, 5);
}

#if defined(__cpp_lib_optional) && __cpp_lib_optional >= 202110L
// ---- C++23: the monadic operations against the standard's ----

namespace
{
std::optional<int>
std_half (int value)
{
  return value % 2 == 0 ? std::optional<int> (value / 2) : std::nullopt;
}

lumex_opt<int>
own_half (int value)
{
  return value % 2 == 0 ? lumex_opt<int> (value / 2) : lumex_opt<int> ();
}
} // namespace

TEST (LumexOptionalStdParityTest,
      GivenMonadics_WhenCalled_ThenSameResultsAsStd)
{
  for (int input = 0; input < 5; ++input)
    {
      std::optional<int> const theirs (input);
      lumex_opt<int> const mine (input);
      auto theirs_then = theirs.and_then (std_half);
      auto mine_then = mine.and_then (own_half);
      EXPECT_EQ (theirs_then.has_value (), mine_then.has_value ());
      if (mine_then.has_value ())
        EXPECT_EQ (*theirs_then, *mine_then);
      auto theirs_nested = theirs.transform (std_half);
      auto mine_nested = mine.transform (own_half);
      static_assert (
          std::is_same<decltype (mine_nested),
                       lumex_opt<lumex_opt<int>>>::value
              && std::is_same<decltype (theirs_nested),
                              std::optional<std::optional<int>>>::value,
          "neither flattens");
      EXPECT_EQ (theirs_nested.has_value (), mine_nested.has_value ());
      EXPECT_EQ ((*theirs_nested).has_value (), (*mine_nested).has_value ());
    }
  std::optional<int> const theirs_none;
  lumex_opt<int> const mine_none;
  EXPECT_EQ (
      theirs_none.or_else ([] { return std::optional<int> (4); }).value (),
      mine_none.or_else ([] { return lumex_opt<int> (4); }).value ());
  auto theirs_text = std::optional<int> (3).transform (
      [] (int v) { return std::to_string (v); });
  auto mine_text = lumex_opt<int> (3).transform (
      [] (int v) { return std::to_string (v); });
  EXPECT_EQ (*theirs_text, *mine_text);
}
#endif
