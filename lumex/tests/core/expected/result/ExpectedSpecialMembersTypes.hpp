// The type zoo and the trait mask of ExpectedSpecialMembers.cxx11.tests.cpp:
// types that differ in the triviality, the noexcept and the existence of their
// special member functions, and one function that folds the properties of the
// special member functions of a type into a number.
#ifndef LUMEX_TESTS_CORE_EXPECTED_RESULT_SPECIAL_MEMBERS_TYPES_HPP
#define LUMEX_TESTS_CORE_EXPECTED_RESULT_SPECIAL_MEMBERS_TYPES_HPP

#include <memory>
#include <string>
#include <type_traits>
#include <utility>

namespace special_members
{
// Everything trivial.
struct trivial_t
{
  int number;
  double real;
};

// A copy constructor that is not trivial (and no move constructor).
struct copy_ctor_t
{
  int number;
  copy_ctor_t (int n = 0) noexcept : number (n) {}
  copy_ctor_t (copy_ctor_t const &other) noexcept : number (other.number) {}
};

// Trivial constructors, an assignment that is not trivial.
struct assign_t
{
  int number;
  assign_t (int n = 0) noexcept : number (n) {}
  assign_t &
  operator= (assign_t const &other) noexcept
  {
    number = other.number;
    return *this;
  }
};

// A destructor that is not trivial.
struct dtor_t
{
  int number;
  dtor_t (int n = 0) noexcept : number (n) {}
  dtor_t (dtor_t const &) = default;
  dtor_t &operator= (dtor_t const &) = default;
  ~dtor_t () {}
};

// Constructors and assignments that may throw.
struct throwing_t
{
  int number;
  throwing_t (int n = 0) noexcept (false) : number (n) {}
  throwing_t (throwing_t const &other) noexcept (false) : number (other.number)
  {
  }
  throwing_t (throwing_t &&other) noexcept (false) : number (other.number) {}
  throwing_t &
  operator= (throwing_t const &other) noexcept (false)
  {
    number = other.number;
    return *this;
  }
  throwing_t &
  operator= (throwing_t &&other) noexcept (false)
  {
    number = other.number;
    return *this;
  }
};

// Moved, not copied.
struct move_only_t
{
  int number;
  explicit move_only_t (int n = 0) noexcept : number (n) {}
  move_only_t (move_only_t const &) = delete;
  move_only_t (move_only_t &&) noexcept = default;
  move_only_t &operator= (move_only_t const &) = delete;
  move_only_t &operator= (move_only_t &&) noexcept = default;
};

// Copied, not moved: the move constructor and the move assignment are deleted
// (not defaulted), so a move of it does not compile and the rule that ignores
// a defaulted move that would be deleted (CWG 1402) does not apply. The copy
// is trivial.
struct copy_only_t
{
  int number;
  explicit copy_only_t (int n = 0) noexcept : number (n) {}
  copy_only_t (copy_only_t const &) = default;
  copy_only_t (copy_only_t &&) = delete;
  copy_only_t &operator= (copy_only_t const &) = default;
  copy_only_t &operator= (copy_only_t &&) = delete;
};

// The same with a copy of its own that is not trivial and may throw.
struct copy_only_user_t
{
  int number;
  explicit copy_only_user_t (int n = 0) noexcept : number (n) {}
  copy_only_user_t (copy_only_user_t const &other) noexcept (false)
      : number (other.number)
  {
  }
  copy_only_user_t (copy_only_user_t &&) = delete;
  copy_only_user_t &
  operator= (copy_only_user_t const &other) noexcept (false)
  {
    number = other.number;
    return *this;
  }
  copy_only_user_t &operator= (copy_only_user_t &&) = delete;
};

using string_t = std::string;
using owner_t = std::unique_ptr<int>;

// 0 when the member swap of T is not available, 1 when it may throw, 2 when it
// is noexcept (the member swap of expected is constrained by the library, the
// free one would fall back to std::swap on some standard libraries).
namespace swap_probe
{
template <typename T>
auto test (int)
    -> decltype (static_cast<void> (
                     std::declval<T &> ().swap (std::declval<T &> ())),
                 std::integral_constant<int,
                                        noexcept (std::declval<T &> ().swap (
                                            std::declval<T &> ()))
                                            ? 2
                                            : 1> ());

template <typename T> std::integral_constant<int, 0> test (long);
} // namespace swap_probe

template <typename T> using swap_kind = decltype (swap_probe::test<T> (0));

// The properties of the special member functions of T, one bit each.
template <typename T>
constexpr int
mask_of ()
{
  return (std::is_trivially_copy_constructible<T>::value ? 1 << 0 : 0)
         | (std::is_trivially_move_constructible<T>::value ? 1 << 1 : 0)
         | (std::is_trivially_copy_assignable<T>::value ? 1 << 2 : 0)
         | (std::is_trivially_move_assignable<T>::value ? 1 << 3 : 0)
         | (std::is_trivially_destructible<T>::value ? 1 << 4 : 0)
         | (std::is_copy_constructible<T>::value ? 1 << 5 : 0)
         | (std::is_move_constructible<T>::value ? 1 << 6 : 0)
         | (std::is_copy_assignable<T>::value ? 1 << 7 : 0)
         | (std::is_move_assignable<T>::value ? 1 << 8 : 0)
         | (std::is_nothrow_copy_constructible<T>::value ? 1 << 9 : 0)
         | (std::is_nothrow_move_constructible<T>::value ? 1 << 10 : 0)
         | (std::is_nothrow_copy_assignable<T>::value ? 1 << 11 : 0)
         | (std::is_nothrow_move_assignable<T>::value ? 1 << 12 : 0)
         | (std::is_nothrow_destructible<T>::value ? 1 << 13 : 0)
         | (swap_kind<T>::value << 14);
}
} // namespace special_members

#endif
