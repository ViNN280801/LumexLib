#ifndef LUMEX_TESTS_CORE_EXPECTED_EXPECTED_PARITY_SUPPORT_HPP
#define LUMEX_TESTS_CORE_EXPECTED_EXPECTED_PARITY_SUPPORT_HPP

#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "lumex/core/expected/Expected"
#include "lumex/core/utility/traits/LumexTypeTraits.hpp"

// Types and traits shared by the tests that pin expected to std::expected
// (ExpectedTransform, ExpectedMonadicParity, ExpectedConversion,
// ExpectedComparison, UnexpectedParity, BadExpectedAccessBase). Every file
// compiles from C++11.

namespace expected_parity
{

namespace traits = lumex::core::utility::traits;

/// Remembers how it got here, so a test can tell a copy from a move. The
/// counters travel with the object: a copy of a copy has copies == 2. A
/// moved-from object has id == -1.
struct tracked_t
{
  int id;
  int copies;
  int moves;

  explicit tracked_t (int value = 0) : id (value), copies (0), moves (0) {}

  tracked_t (tracked_t const &other)
      : id (other.id), copies (other.copies + 1), moves (other.moves)
  {
  }

  tracked_t (tracked_t &&other) noexcept
      : id (other.id), copies (other.copies), moves (other.moves + 1)
  {
    other.id = -1;
  }

  tracked_t &
  operator= (tracked_t const &other)
  {
    id = other.id;
    copies = other.copies + 1;
    moves = other.moves;
    return *this;
  }

  tracked_t &
  operator= (tracked_t &&other) noexcept
  {
    id = other.id;
    copies = other.copies;
    moves = other.moves + 1;
    other.id = -1;
    return *this;
  }

  bool
  operator== (tracked_t const &other) const
  {
    return id == other.id;
  }
};

/// An `int` wrapper that only an explicit conversion reaches.
struct explicit_int_t
{
  int value;
  explicit explicit_int_t (int v) : value (v) {}

  bool
  operator== (explicit_int_t const &other) const
  {
    return value == other.value;
  }
};

/// No default constructor.
struct no_default_t
{
  int value;
  explicit no_default_t (int v) : value (v) {}
};

/// Cannot be copied.
struct move_only_t
{
  std::unique_ptr<int> pointer;
  explicit move_only_t (int v) : pointer (new int (v)) {}
  move_only_t (move_only_t &&) = default;
  move_only_t &operator= (move_only_t &&) = default;
  move_only_t (move_only_t const &) = delete;
  move_only_t &operator= (move_only_t const &) = delete;
};

/// The inner expected of the functions that return an `expected`.
using inner_t = lumex::core::expected::result::expected<std::string, int>;

// ---------------------------------------------------------------------
// Detection: is `uut.<operation> (func)` well-formed?
// ---------------------------------------------------------------------

template <typename Uut, typename Func, typename = void>
struct can_and_then : std::false_type
{
};

template <typename Uut, typename Func>
struct can_and_then<
    Uut, Func,
    traits::meta::void_t<decltype (std::declval<Uut> ().and_then (
        std::declval<Func> ()))>> : std::true_type
{
};

template <typename Uut, typename Func, typename = void>
struct can_or_else : std::false_type
{
};

template <typename Uut, typename Func>
struct can_or_else<
    Uut, Func,
    traits::meta::void_t<decltype (std::declval<Uut> ().or_else (
        std::declval<Func> ()))>> : std::true_type
{
};

template <typename Uut, typename Func, typename = void>
struct can_transform : std::false_type
{
};

template <typename Uut, typename Func>
struct can_transform<
    Uut, Func,
    traits::meta::void_t<decltype (std::declval<Uut> ().transform (
        std::declval<Func> ()))>> : std::true_type
{
};

template <typename Uut, typename Func, typename = void>
struct can_transform_error : std::false_type
{
};

template <typename Uut, typename Func>
struct can_transform_error<
    Uut, Func,
    traits::meta::void_t<decltype (std::declval<Uut> ().transform_error (
        std::declval<Func> ()))>> : std::true_type
{
};

/// `a == b` is well-formed.
template <typename A, typename B, typename = void>
struct can_compare : std::false_type
{
};

template <typename A, typename B>
struct can_compare<
    A, B,
    traits::meta::void_t<decltype (std::declval<A> () == std::declval<B> ())>>
    : std::true_type
{
};

} // namespace expected_parity

#endif // !LUMEX_TESTS_CORE_EXPECTED_EXPECTED_PARITY_SUPPORT_HPP
