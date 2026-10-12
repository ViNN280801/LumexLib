// The layout of expected<T, E>: the flag has_val and the union of the two
// alternatives of [expected.object.general]. The flag is a class as large as
// the alignment of the union (ExpectedStorage.hpp, `has_val_flag`), so that
// the storage, a base class of expected, has no tail padding that a derived
// class could use: a trivially
// copyable expected<int, unsigned> that a function returns in one register is
// then built in the register and not through a stack slot with a 4-byte and a
// 1-byte store and an 8-byte load (a store-forwarding stall on GCC 13.2). The
// tests pin what that change must keep (the size, the alignment and the
// triviality of the pairs, the same as a `bool` and a union give) and what it
// adds (the storage has no tail padding). They compile from C++11, so every
// expected suite runs them.

#include <cstddef>
#include <cstring>
#include <new>
#include <string>
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/core/expected/Expected"

namespace
{
using lumex::core::expected::result::expected;
using lumex::core::expected::result::in_place;
using lumex::core::expected::result::unexpect;

/// A `bool` and a union of `T` and `E`: what [expected.object.general]
/// describes.
template <typename T, typename E> struct reference_layout
{
  union
  {
    T value;
    E error;
  } storage;
  bool has_value;
};

/// The same for `expected<void, E>`: the value alternative is empty.
template <typename E> struct reference_layout_void
{
  union
  {
    char value;
    E error;
  } storage;
  bool has_value;
};

struct three_bytes
{
  char data[3];
};

struct five_bytes
{
  char data[5];
};

struct twelve_bytes
{
  int a;
  int b;
  int c;
};

struct sixteen_bytes
{
  long long a;
  long long b;
};

struct alignas (16) aligned16
{
  char c;
};

struct alignas (32) aligned32
{
  char c;
};

struct alignas (64) aligned64
{
  char c;
};

template <typename T, typename E>
void
check_same_layout ()
{
  using x_t = expected<T, E>;
  using reference_t = reference_layout<T, E>;
  EXPECT_EQ (sizeof (x_t), sizeof (reference_t));
  EXPECT_EQ (alignof (x_t), alignof (reference_t));
}

template <typename E>
void
check_same_layout_void ()
{
  using x_t = expected<void, E>;
  using reference_t = reference_layout_void<E>;
  EXPECT_EQ (sizeof (x_t), sizeof (reference_t));
  EXPECT_EQ (alignof (x_t), alignof (reference_t));
}

/// A class derived from the storage of `expected<T, E>` with one more member.
/// Where the storage ends with padding that is not a POD's (the Itanium C++
/// ABI), the member goes into that padding and the size stays; where the
/// storage has no tail padding the size grows.
template <typename T, typename E>
struct derived_from_storage
    : lumex::core::expected::result::detail::storage::expected_base<T, E>
{
  char extra;
};

template <typename T, typename E>
void
check_storage_has_no_tail_padding ()
{
  using base_t
      = lumex::core::expected::result::detail::storage::expected_base<T, E>;
  EXPECT_GT (sizeof (derived_from_storage<T, E>), sizeof (base_t))
      << "the storage ends with padding a derived class can use";
}
} // namespace

TEST (ExpectedLayoutTest, Size_IsThatOfABoolAndAUnion)
{
  check_same_layout<int, unsigned> ();
  check_same_layout<char, char> ();
  check_same_layout<char, unsigned char> ();
  check_same_layout<short, int> ();
  check_same_layout<int, short> ();
  check_same_layout<short, short> ();
  check_same_layout<long long, int> ();
  check_same_layout<int, long long> ();
  check_same_layout<long long, long long> ();
  check_same_layout<double, int> ();
  check_same_layout<float, char> ();
  check_same_layout<three_bytes, char> ();
  check_same_layout<five_bytes, char> ();
  check_same_layout<twelve_bytes, int> ();
  check_same_layout<sixteen_bytes, int> ();
  check_same_layout<bool, bool> ();
  check_same_layout<void *, int> ();
  check_same_layout<aligned16, int> ();
  check_same_layout<int, aligned32> ();
  check_same_layout<aligned64, aligned16> ();
  check_same_layout_void<int> ();
  check_same_layout_void<char> ();
  check_same_layout_void<short> ();
  check_same_layout_void<long long> ();
  check_same_layout_void<three_bytes> ();
}

TEST (ExpectedLayoutTest, Size_OfTheCommonPairsIsFixed)
{
  EXPECT_EQ (8u, (sizeof (expected<int, unsigned>)));
  EXPECT_EQ (2u, (sizeof (expected<char, char>)));
  EXPECT_EQ (2u, (sizeof (expected<char, unsigned char>)));
  EXPECT_EQ (4u, (sizeof (expected<short, short>)));
  EXPECT_EQ (16u, (sizeof (expected<long long, int>)));
  EXPECT_EQ (8u, (sizeof (expected<void, int>)));
  EXPECT_EQ (2u, (sizeof (expected<void, char>)));
  EXPECT_EQ (6u, (sizeof (expected<five_bytes, char>)));
  EXPECT_EQ (16u, (sizeof (expected<twelve_bytes, int>)));
}

TEST (ExpectedLayoutTest, Triviality_OfTheTriviallyCopyablePairsIsKept)
{
  static_assert (std::is_trivially_copyable<expected<int, unsigned>>::value,
                 "trivially copyable");
  static_assert (
      std::is_trivially_copy_constructible<expected<int, unsigned>>::value,
      "trivially copy constructible");
  static_assert (
      std::is_trivially_move_constructible<expected<int, unsigned>>::value,
      "trivially move constructible");
  static_assert (
      std::is_trivially_copy_assignable<expected<int, unsigned>>::value,
      "trivially copy assignable");
  static_assert (
      std::is_trivially_move_assignable<expected<int, unsigned>>::value,
      "trivially move assignable");
  static_assert (
      std::is_trivially_destructible<expected<int, unsigned>>::value,
      "trivially destructible");
  static_assert (std::is_trivially_copyable<expected<void, int>>::value,
                 "void: trivially copyable");
  static_assert (std::is_trivially_destructible<expected<void, int>>::value,
                 "void: trivially destructible");
  static_assert (std::is_trivially_copyable<expected<long long, char>>::value,
                 "trivially copyable");
  static_assert (
      std::is_trivially_copyable<expected<three_bytes, char>>::value,
      "trivially copyable");
  static_assert (
      !std::is_trivially_copyable<expected<std::string, int>>::value,
      "a string value is not trivial");
  static_assert (
      !std::is_trivially_destructible<expected<int, std::string>>::value,
      "a string error is not trivially destructible");
  static_assert (std::is_nothrow_move_constructible<expected<int, int>>::value,
                 "nothrow move");
  SUCCEED ();
}

TEST (ExpectedLayoutTest, Storage_HasNoTailPadding)
{
  check_storage_has_no_tail_padding<int, unsigned> ();
  check_storage_has_no_tail_padding<char, unsigned char> ();
  check_storage_has_no_tail_padding<short, short> ();
  check_storage_has_no_tail_padding<short, int> ();
  check_storage_has_no_tail_padding<long long, int> ();
  check_storage_has_no_tail_padding<double, int> ();
  check_storage_has_no_tail_padding<five_bytes, char> ();
  check_storage_has_no_tail_padding<twelve_bytes, int> ();
  check_storage_has_no_tail_padding<sixteen_bytes, int> ();
  check_storage_has_no_tail_padding<void *, void *> ();
  check_storage_has_no_tail_padding<bool, bool> ();
  check_storage_has_no_tail_padding<aligned16, int> ();
  check_storage_has_no_tail_padding<int, aligned32> ();
  check_storage_has_no_tail_padding<aligned64, aligned16> ();
}

TEST (ExpectedLayoutTest, StorageOfVoid_HasNoTailPadding)
{
  using lumex::core::expected::result::Unit;
  check_storage_has_no_tail_padding<Unit, int> ();
  check_storage_has_no_tail_padding<Unit, char> ();
  check_storage_has_no_tail_padding<Unit, long long> ();
}

TEST (ExpectedLayoutTest, Flag_FollowsConstructionAssignmentEmplaceAndSwap)
{
  expected<int, unsigned> a (in_place, 1);
  expected<int, unsigned> b (unexpect, 2u);
  EXPECT_TRUE (a.has_value ());
  EXPECT_FALSE (b.has_value ());
  EXPECT_TRUE (static_cast<bool> (a));
  EXPECT_FALSE (static_cast<bool> (b));

  a = b;
  EXPECT_FALSE (a.has_value ());
  EXPECT_EQ (2u, a.error ());
  a.emplace (5);
  EXPECT_TRUE (a.has_value ());
  EXPECT_EQ (5, a.value ());
  a.emplace_error (6u);
  EXPECT_FALSE (a.has_value ());
  EXPECT_EQ (6u, a.error ());
  a = expected<int, unsigned> (in_place, 7);
  EXPECT_TRUE (a.has_value ());
  EXPECT_EQ (7, a.value ());

  a.swap (b);
  EXPECT_FALSE (a.has_value ());
  EXPECT_EQ (2u, a.error ());
  EXPECT_TRUE (b.has_value ());
  EXPECT_EQ (7, b.value ());

  expected<void, int> v;
  expected<void, int> e (unexpect, 3);
  EXPECT_TRUE (v.has_value ());
  EXPECT_FALSE (e.has_value ());
  v = e;
  EXPECT_FALSE (v.has_value ());
  v.emplace ();
  EXPECT_TRUE (v.has_value ());
}

TEST (ExpectedLayoutTest, CopiedByBytes_KeepsTheState)
{
  expected<int, unsigned> value (in_place, 7);
  expected<int, unsigned> error (unexpect, 9u);
  expected<int, unsigned> copy_of_value (unexpect, 1u);
  expected<int, unsigned> copy_of_error (in_place, 1);
  std::memcpy (static_cast<void *> (&copy_of_value),
               static_cast<void const *> (&value), sizeof (value));
  std::memcpy (static_cast<void *> (&copy_of_error),
               static_cast<void const *> (&error), sizeof (error));
  ASSERT_TRUE (copy_of_value.has_value ());
  EXPECT_EQ (7, copy_of_value.value ());
  ASSERT_FALSE (copy_of_error.has_value ());
  EXPECT_EQ (9u, copy_of_error.error ());
}
