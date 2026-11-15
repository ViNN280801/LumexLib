// The special member functions of expected<T, E> and expected<void, E>, pinned
// as numbers over a matrix of eleven types (ExpectedSpecialMembersTypes.hpp):
// the triviality of the copy and move constructors, the copy and move
// assignments and the destructor, which of them exist, which are noexcept, and
// whether the member swap exists and is noexcept; the types include two with a
// deleted move, which reach the layers that delete the move and the rule that
// ignores a defaulted move that would be deleted (CWG 1402). The numbers are
// those of the library before the special members of expected were declared
// (explicitly defaulted, so that they can be always inlined, see
// ExpectedDetail.hpp), measured with GCC 13.3, g++-9, Clang 18, cl.exe and
// clang-cl at C++11, C++14 and C++20: the same on every one of them. A
// declaration of a special member that changes one of them is a compile error
// here, not a surprise in a consumer. The size and the alignment (sizeof and
// alignof) are those of the storage layer: the class adds no data. The
// constexpr of the special members and of the operations that put the old
// contents back is pinned by evaluating them in constant expressions, each
// under the standard that allows it (C++11 copy and move, C++14 trivial
// assignments, C++20 the guarded paths). The tests compile from C++11, so
// every expected suite runs them.

#include <cstddef>

#include <gtest/gtest.h>

#include "lumex/core/expected/Expected"
#include "lumex/core/expected/result/ExpectedStorage.hpp"
#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"
#include "lumex/tests/core/expected/result/ExpectedSpecialMembersTypes.hpp"

namespace
{
using lumex::core::expected::error::unexpected;
using lumex::core::expected::result::expected;
using lumex::core::expected::result::unexpect;
using lumex::core::expected::result::Unit;
using special_members::assign_t;
using special_members::copy_ctor_t;
using special_members::copy_only_t;
using special_members::copy_only_user_t;
using special_members::dtor_t;
using special_members::move_only_t;
using special_members::owner_t;
using special_members::string_t;
using special_members::throwing_t;
using special_members::trivial_t;
namespace storage = lumex::core::expected::result::detail::storage;

#define PIN(T, E, mask)                                                       \
  static_assert (special_members::mask_of<expected<T, E>> () == (mask),       \
                 "special members of expected<" #T ", " #E ">");              \
  static_assert (sizeof (expected<T, E>)                                      \
                         == sizeof (storage::expected_storage<T, E>)          \
                     && alignof (expected<T, E>)                              \
                            == alignof (storage::expected_storage<T, E>),     \
                 "size and alignment of expected<" #T ", " #E ">")

#define PIN_VOID(E, mask)                                                     \
  static_assert (special_members::mask_of<expected<void, E>> () == (mask),    \
                 "special members of expected<void, " #E ">");                \
  static_assert (sizeof (expected<void, E>)                                   \
                         == sizeof (storage::expected_storage<Unit, E>)       \
                     && alignof (expected<void, E>)                           \
                            == alignof (storage::expected_storage<Unit, E>),  \
                 "size and alignment of expected<void, " #E ">")

PIN (int, int, 0xBFFF);
PIN (int, trivial_t, 0xBFFF);
PIN (int, copy_ctor_t, 0xBFF0);
PIN (int, assign_t, 0xBFF3);
PIN (int, dtor_t, 0xBFE0);
PIN (int, throwing_t, 0x61F0);
PIN (int, move_only_t, 0xB55A);
PIN (int, copy_only_t, 0x3FFF);
PIN (int, copy_only_user_t, 0x21F0);
PIN (int, string_t, 0xB5E0);
PIN (int, owner_t, 0xB540);

PIN (trivial_t, int, 0xBFFF);
PIN (trivial_t, trivial_t, 0xBFFF);
PIN (trivial_t, copy_ctor_t, 0xBFF0);
PIN (trivial_t, assign_t, 0xBFF3);
PIN (trivial_t, dtor_t, 0xBFE0);
PIN (trivial_t, throwing_t, 0x61F0);
PIN (trivial_t, move_only_t, 0xB55A);
PIN (trivial_t, copy_only_t, 0x3FFF);
PIN (trivial_t, copy_only_user_t, 0x21F0);
PIN (trivial_t, string_t, 0xB5E0);
PIN (trivial_t, owner_t, 0xB540);

PIN (copy_ctor_t, int, 0xBFF0);
PIN (copy_ctor_t, trivial_t, 0xBFF0);
PIN (copy_ctor_t, copy_ctor_t, 0xBFF0);
PIN (copy_ctor_t, assign_t, 0xBFF0);
PIN (copy_ctor_t, dtor_t, 0xBFE0);
PIN (copy_ctor_t, throwing_t, 0x61F0);
PIN (copy_ctor_t, move_only_t, 0xB550);
PIN (copy_ctor_t, copy_only_t, 0x3FF0);
PIN (copy_ctor_t, copy_only_user_t, 0x21F0);
PIN (copy_ctor_t, string_t, 0xB5E0);
PIN (copy_ctor_t, owner_t, 0xB540);

PIN (assign_t, int, 0xBFF3);
PIN (assign_t, trivial_t, 0xBFF3);
PIN (assign_t, copy_ctor_t, 0xBFF0);
PIN (assign_t, assign_t, 0xBFF3);
PIN (assign_t, dtor_t, 0xBFE0);
PIN (assign_t, throwing_t, 0x61F0);
PIN (assign_t, move_only_t, 0xB552);
PIN (assign_t, copy_only_t, 0x3FF3);
PIN (assign_t, copy_only_user_t, 0x21F0);
PIN (assign_t, string_t, 0xB5E0);
PIN (assign_t, owner_t, 0xB540);

PIN (dtor_t, int, 0xBFE0);
PIN (dtor_t, trivial_t, 0xBFE0);
PIN (dtor_t, copy_ctor_t, 0xBFE0);
PIN (dtor_t, assign_t, 0xBFE0);
PIN (dtor_t, dtor_t, 0xBFE0);
PIN (dtor_t, throwing_t, 0x61E0);
PIN (dtor_t, move_only_t, 0xB540);
PIN (dtor_t, copy_only_t, 0x3FE0);
PIN (dtor_t, copy_only_user_t, 0x21E0);
PIN (dtor_t, string_t, 0xB5E0);
PIN (dtor_t, owner_t, 0xB540);

PIN (throwing_t, int, 0x61F0);
PIN (throwing_t, trivial_t, 0x61F0);
PIN (throwing_t, copy_ctor_t, 0x61F0);
PIN (throwing_t, assign_t, 0x61F0);
PIN (throwing_t, dtor_t, 0x61E0);
PIN (throwing_t, throwing_t, 0x2070);
PIN (throwing_t, move_only_t, 0x6150);
PIN (throwing_t, copy_only_t, 0x2070);
PIN (throwing_t, copy_only_user_t, 0x2070);
PIN (throwing_t, string_t, 0x61E0);
PIN (throwing_t, owner_t, 0x6140);

PIN (move_only_t, int, 0xB55A);
PIN (move_only_t, trivial_t, 0xB55A);
PIN (move_only_t, copy_ctor_t, 0xB550);
PIN (move_only_t, assign_t, 0xB552);
PIN (move_only_t, dtor_t, 0xB540);
PIN (move_only_t, throwing_t, 0x6150);
PIN (move_only_t, move_only_t, 0xB55A);
PIN (move_only_t, copy_only_t, 0x2010);
PIN (move_only_t, copy_only_user_t, 0x2010);
PIN (move_only_t, string_t, 0xB540);
PIN (move_only_t, owner_t, 0xB540);

PIN (copy_only_t, int, 0x3FFF);
PIN (copy_only_t, trivial_t, 0x3FFF);
PIN (copy_only_t, copy_ctor_t, 0x3FF0);
PIN (copy_only_t, assign_t, 0x3FF3);
PIN (copy_only_t, dtor_t, 0x3FE0);
PIN (copy_only_t, throwing_t, 0x2070);
PIN (copy_only_t, move_only_t, 0x2010);
PIN (copy_only_t, copy_only_t, 0x2673);
PIN (copy_only_t, copy_only_user_t, 0x2070);
PIN (copy_only_t, string_t, 0x21E0);
PIN (copy_only_t, owner_t, 0x2000);

PIN (copy_only_user_t, int, 0x21F0);
PIN (copy_only_user_t, trivial_t, 0x21F0);
PIN (copy_only_user_t, copy_ctor_t, 0x21F0);
PIN (copy_only_user_t, assign_t, 0x21F0);
PIN (copy_only_user_t, dtor_t, 0x21E0);
PIN (copy_only_user_t, throwing_t, 0x2070);
PIN (copy_only_user_t, move_only_t, 0x2010);
PIN (copy_only_user_t, copy_only_t, 0x2070);
PIN (copy_only_user_t, copy_only_user_t, 0x2070);
PIN (copy_only_user_t, string_t, 0x21E0);
PIN (copy_only_user_t, owner_t, 0x2000);

PIN (string_t, int, 0xB5E0);
PIN (string_t, trivial_t, 0xB5E0);
PIN (string_t, copy_ctor_t, 0xB5E0);
PIN (string_t, assign_t, 0xB5E0);
PIN (string_t, dtor_t, 0xB5E0);
PIN (string_t, throwing_t, 0x61E0);
PIN (string_t, move_only_t, 0xB540);
PIN (string_t, copy_only_t, 0x21E0);
PIN (string_t, copy_only_user_t, 0x21E0);
PIN (string_t, string_t, 0xB5E0);
PIN (string_t, owner_t, 0xB540);

PIN (owner_t, int, 0xB540);
PIN (owner_t, trivial_t, 0xB540);
PIN (owner_t, copy_ctor_t, 0xB540);
PIN (owner_t, assign_t, 0xB540);
PIN (owner_t, dtor_t, 0xB540);
PIN (owner_t, throwing_t, 0x6140);
PIN (owner_t, move_only_t, 0xB540);
PIN (owner_t, copy_only_t, 0x2000);
PIN (owner_t, copy_only_user_t, 0x2000);
PIN (owner_t, string_t, 0xB540);
PIN (owner_t, owner_t, 0xB540);

PIN_VOID (int, 0xBFFF);
PIN_VOID (trivial_t, 0xBFFF);
PIN_VOID (copy_ctor_t, 0xBFF0);
PIN_VOID (assign_t, 0xBFF3);
PIN_VOID (dtor_t, 0xBFE0);
PIN_VOID (throwing_t, 0x61F0);
PIN_VOID (move_only_t, 0xB55A);
PIN_VOID (copy_only_t, 0x3FFF);
PIN_VOID (copy_only_user_t, 0x21F0);
PIN_VOID (string_t, 0xB5E0);
PIN_VOID (owner_t, 0xB540);

#undef PIN
#undef PIN_VOID

// === constant expressions ==================================================
// The special members are declared defaulted and always inlined; the guarded
// paths hold a restore_guard. Neither may cost a `constexpr`.

using plain_t = expected<int, int>;

// C++11: the trivial copy and move constructors.
constexpr plain_t kFirst (1);
constexpr plain_t kCopied = kFirst;
constexpr plain_t kMoved = plain_t (plain_t (3));
static_assert (*kCopied == 1 && *kMoved == 3,
               "constexpr copy and move constructors of expected<int, int>");
constexpr expected<void, int> kVoidError (unexpect, 4);
constexpr expected<void, int> kVoidCopied = kVoidError;
static_assert (kVoidCopied.error () == 4,
               "constexpr copy constructor of expected<void, int>");

#if __cplusplus >= 201402L
// C++14: the trivial copy and move assignments.
constexpr int
trivial_assignments ()
{
  plain_t a (1);
  plain_t const b (unexpect, 2);
  a = b;
  plain_t c (5);
  c = plain_t (6);
  expected<void, int> v;
  expected<void, int> const w (unexpect, 3);
  v = w;
  return a.error () * 100 + *c * 10 + v.error ();
}
static_assert (trivial_assignments () == 263,
               "constexpr trivial assignments of expected");
#endif

#if __cplusplus >= 202002L && LUMEX_FEATURE_CONSTEXPR >= 201907L              \
    && LUMEX_HAS_STD_CONSTEXPR_DYNAMIC_ALLOC
// C++20: the paths that hold a restore_guard, with an error type that is
// neither nothrow constructible nor nothrow movable.
struct guarded_t
{
  int number;
  constexpr guarded_t (int n) noexcept (false) : number (n) {}
  constexpr guarded_t (guarded_t const &other) noexcept (false)
      : number (other.number)
  {
  }
  constexpr guarded_t (guarded_t &&other) noexcept (false)
      : number (other.number)
  {
  }
  constexpr guarded_t &operator= (guarded_t const &) = default;
  constexpr guarded_t &operator= (guarded_t &&) = default;
  constexpr ~guarded_t () {}
};

constexpr int
guarded_paths ()
{
  using guarded_value_t = expected<int, guarded_t>;
  guarded_value_t a (1);
  guarded_value_t const b (unexpect, 2);
  a = b;               // error over value: a guard over the int
  a.emplace_error (5); // error over error: a guard over guarded_t
  guarded_value_t c (3);
  c.swap (a); // a value with an error, mode 2 (guarded_t is not nothrow
              // movable): a guard over the parked int
  a.swap (c);
  guarded_value_t d (7);
  d = unexpected<guarded_t> (guarded_t (8)); // assign_error
  expected<void, guarded_t> v (unexpect, 9);
  v.emplace_error (4); // void, error over error: a guard over guarded_t
  return a.error ().number * 1000 + *c * 100 + d.error ().number * 10
         + v.error ().number;
}
static_assert (guarded_paths () == 5384,
               "constexpr guarded paths of expected (C++20)");
#endif
} // namespace

TEST (ExpectedSpecialMembersTest,
      GivenTheMatrix_WhenCompiled_ThenTheNumbersAreThoseOfTheLibrary)
{
  // The pins are the static_assert above; this keeps the file in the suite.
  constexpr int mask = special_members::mask_of<expected<int, int>> ();
  EXPECT_EQ (mask & 1, 1);
}

TEST (ExpectedSpecialMembersTest,
      GivenConstantExpressions_WhenRunAtRunTime_ThenTheResultsAreTheSame)
{
  EXPECT_EQ (*kCopied, 1);
  EXPECT_EQ (*kMoved, 3);
  EXPECT_EQ (kVoidCopied.error (), 4);
#if __cplusplus >= 201402L
  EXPECT_EQ (trivial_assignments (), 263);
#endif
#if __cplusplus >= 202002L && LUMEX_FEATURE_CONSTEXPR >= 201907L              \
    && LUMEX_HAS_STD_CONSTEXPR_DYNAMIC_ALLOC
  EXPECT_EQ (guarded_paths (), 5384);
#endif
}
