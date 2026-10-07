// unexpected<E> next to std::unexpected: the constructor that takes anything E
// can be built from, the in-place constructors, noexcept error(), swap and the
// comparisons. The source is compiled into every suite of the module (C++11,
// C++17 and C++20).

#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/expected/Expected"
#include "lumex/tests/core/expected/ExpectedParitySupport.hpp"

using namespace lumex::core::expected::result;
using namespace lumex::core::expected::error;
using namespace expected_parity;

namespace
{

/// Swapping two of these can throw.
struct throwing_swap_t
{
  throwing_swap_t () {}
  throwing_swap_t (throwing_swap_t const &) {}
  throwing_swap_t (throwing_swap_t &&) noexcept (false) {}
  throwing_swap_t &
  operator= (throwing_swap_t const &)
  {
    return *this;
  }
  throwing_swap_t &
  operator= (throwing_swap_t &&) noexcept (false)
  {
    return *this;
  }
};

constexpr unexpected<int> kConstant (5);
constexpr int kMoveOnlyValue = 11;

} // namespace

// === the forwarding constructor ==========================================

TEST (UnexpectedParityTest, Constructor_ConvertibleArgument_BuildsTheError)
{
  unexpected<std::string> const from_literal ("text");
  unexpected<std::string> const from_string{ std::string ("text") };
  unexpected<std::string> const from_pieces (std::string (3, 'x'));

  EXPECT_EQ (from_literal.error (), "text");
  EXPECT_EQ (from_string.error (), "text");
  EXPECT_EQ (from_pieces.error (), "xxx");
}

TEST (UnexpectedParityTest, Constructor_IsExplicitAndTakesWhatTheErrorTakes)
{
  static_assert (
      std::is_constructible<unexpected<std::string>, char const *>::value,
      "a string is made from a literal");
  static_assert (
      !std::is_convertible<char const *, unexpected<std::string>>::value,
      "the constructor is explicit");
  static_assert (std::is_constructible<unexpected<explicit_int_t>, int>::value,
                 "an explicit constructor of the error is reachable");
  static_assert (!std::is_constructible<unexpected<std::string>, int>::value,
                 "a string is not made from an int");
  SUCCEED ();
}

TEST (UnexpectedParityTest,
      Constructor_ExplicitOnlyErrorType_IsBuiltFromTheArgument)
{
  unexpected<explicit_int_t> const uut (5);

  EXPECT_EQ (uut.error ().value, 5);
}

TEST (UnexpectedParityTest, Constructor_MoveOnlyError_IsMovedIn)
{
  unexpected<move_only_t> uut{ move_only_t (kMoveOnlyValue) };

  EXPECT_EQ (*uut.error ().pointer, kMoveOnlyValue);
  move_only_t taken (std::move (uut).error ());
  EXPECT_EQ (*taken.pointer, kMoveOnlyValue);
  EXPECT_EQ (uut.error ().pointer, nullptr);
}

TEST (UnexpectedParityTest,
      CopyAndMove_OfTheUnexpectedItself_AreNotTakenByTheForwardingConstructor)
{
  unexpected<std::string> original (
      "long enough to live on the heap, not in a small buffer");
  unexpected<std::string> const const_original ("const");

  unexpected<std::string> copy (original);
  unexpected<std::string> const_copy (const_original);
  unexpected<std::string> moved (std::move (original));

  EXPECT_EQ (copy.error (),
             "long enough to live on the heap, not in a small buffer");
  EXPECT_EQ (const_copy.error (), "const");
  EXPECT_EQ (moved.error (),
             "long enough to live on the heap, not in a small buffer");
  static_assert (std::is_copy_constructible<unexpected<std::string>>::value,
                 "copyable");
  static_assert (std::is_move_constructible<unexpected<move_only_t>>::value,
                 "movable");
  static_assert (!std::is_copy_constructible<unexpected<move_only_t>>::value,
                 "not copyable when the error is not");
}

TEST (UnexpectedParityTest, Constructor_InPlace_ForwardsTheArguments)
{
  unexpected<std::string> const uut (in_place, 3u, 'z');

  static_assert (!std::is_constructible<unexpected<int>, in_place_tag,
                                        std::string>::value,
                 "an int is not made from a string");
  EXPECT_EQ (uut.error (), "zzz");
}

TEST (UnexpectedParityTest, Constructor_Noexcept_FollowsTheErrorType)
{
  static_assert (std::is_nothrow_constructible<unexpected<int>, int>::value,
                 "an int is made without throwing");
  static_assert (
      std::is_nothrow_constructible<unexpected<int>, in_place_tag, int>::value,
      "an int is made without throwing");
  static_assert (!std::is_nothrow_constructible<unexpected<std::string>,
                                                char const *>::value,
                 "a string allocates");
  SUCCEED ();
}

TEST (UnexpectedParityTest,
      Constexpr_ConstructorAndErrorAndComparison_AreConstantExpressions)
{
  static_assert (kConstant.error () == 5, "error () is constexpr");
  static_assert (kConstant == unexpected<int> (5), "== is constexpr");
  static_assert (kConstant != unexpected<int> (6), "!= is constexpr");
  SUCCEED ();
}

// === error () ============================================================

TEST (UnexpectedParityTest, Error_EveryValueCategory_ReturnsTheMatchingType)
{
  using uut_t = unexpected<int>;

  static_assert (
      std::is_same<decltype (std::declval<uut_t &> ().error ()), int &>::value,
      "&");
  static_assert (
      std::is_same<decltype (std::declval<uut_t const &> ().error ()),
                   int const &>::value,
      "const &");
  static_assert (std::is_same<decltype (std::declval<uut_t &&> ().error ()),
                              int &&>::value,
                 "&&");
  static_assert (
      std::is_same<decltype (std::declval<uut_t const &&> ().error ()),
                   int const &&>::value,
      "const &&");
  SUCCEED ();
}

TEST (UnexpectedParityTest, Error_EveryValueCategory_IsNoexcept)
{
  using uut_t = unexpected<std::string>;

  static_assert (noexcept (std::declval<uut_t &> ().error ()), "&");
  static_assert (noexcept (std::declval<uut_t const &> ().error ()),
                 "const &");
  static_assert (noexcept (std::declval<uut_t &&> ().error ()), "&&");
  static_assert (noexcept (std::declval<uut_t const &&> ().error ()),
                 "const &&");
  SUCCEED ();
}

// === swap ================================================================

TEST (UnexpectedParityTest, Swap_MemberAndFree_ExchangeTheErrors)
{
  unexpected<std::string> first ("first");
  unexpected<std::string> second ("second");

  first.swap (second);
  EXPECT_EQ (first.error (), "second");
  EXPECT_EQ (second.error (), "first");

  swap (first, second);
  EXPECT_EQ (first.error (), "first");
  EXPECT_EQ (second.error (), "second");
}

TEST (UnexpectedParityTest, Swap_Noexcept_FollowsTheErrorType)
{
  static_assert (noexcept (std::declval<unexpected<int> &> ().swap (
                     std::declval<unexpected<int> &> ())),
                 "ints swap without throwing");
  static_assert (noexcept (swap (std::declval<unexpected<int> &> (),
                                 std::declval<unexpected<int> &> ())),
                 "ints swap without throwing");
  static_assert (
      !noexcept (std::declval<unexpected<throwing_swap_t> &> ().swap (
          std::declval<unexpected<throwing_swap_t> &> ())),
      "a throwing move makes the swap throwing");
  static_assert (
      !noexcept (swap (std::declval<unexpected<throwing_swap_t> &> (),
                       std::declval<unexpected<throwing_swap_t> &> ())),
      "the free swap is noexcept exactly when the member swap is");
  SUCCEED ();
}

// === comparison ==========================================================

TEST (UnexpectedParityTest, Compare_SameAndOtherErrorTypes_ComparesTheErrors)
{
  EXPECT_TRUE (unexpected<int> (1) == unexpected<int> (1));
  EXPECT_FALSE (unexpected<int> (1) == unexpected<int> (2));
  EXPECT_TRUE (unexpected<int> (1) != unexpected<int> (2));
  EXPECT_FALSE (unexpected<int> (1) != unexpected<int> (1));
  EXPECT_TRUE (unexpected<int> (1) == unexpected<long> (1));
  EXPECT_TRUE (unexpected<long> (1) == unexpected<int> (1));
  EXPECT_TRUE (unexpected<std::string> ("x")
               == unexpected<char const *> ("x"));
  EXPECT_TRUE (unexpected<std::string> ("x")
               != unexpected<char const *> ("y"));
}

TEST (UnexpectedParityTest, Compare_ErrorsThatCannotBeCompared_AreRejected)
{
  static_assert (can_compare<unexpected<int>, unexpected<long>>::value,
                 "ints compare with longs");
  static_assert (!can_compare<unexpected<int>, unexpected<std::string>>::value,
                 "an int and a string do not compare");
  SUCCEED ();
}
