// LumexOptionalStorage.cxx11.tests.cpp
// The value buffer of the optional: an alignas (T) array of unsigned char
// that replaced std::aligned_storage (deprecated in C++23). The size and the
// alignment of optional<T> are those of the former member: the buffer comes
// first and the engaged flag follows it. The checks are static_asserts (the
// layout is a compile-time fact) plus run-time checks that the contained
// object really sits at an address aligned for its type.
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/optional/opt/LumexOptional.hpp"

namespace
{

using lumex::core::optional::opt::optional;

struct alignas (16) Aligned16
{
  char data[3];
};

struct alignas (32) Aligned32
{
  char data[40];
};

struct OddSize3
{
  char data[3];
};

struct OddSize5
{
  char data[5];
};

struct CharAndDouble
{
  char tag;
  double number;
};

struct Empty
{
};

template <std::size_t Value, std::size_t Alignment> struct round_up
{
  static constexpr std::size_t value
      = (Value + Alignment - 1) / Alignment * Alignment;
};

template <typename T> struct max_alignment
{
  static constexpr std::size_t value
      = alignof (T) > alignof (bool) ? alignof (T) : alignof (bool);
};

#if __cplusplus <= 202002L
// What the optional held before: the aligned_storage member, then the flag.
// Not used from C++23 (a value above 202002L: GCC reports 202100L), where
// std::aligned_storage is deprecated.
template <typename T> struct FormerLayout
{
  typename std::aligned_storage<sizeof (T), alignof (T)>::type storage;
  bool engaged;
};
#endif

template <typename T>
void
expect_layout ()
{
  static_assert (alignof (optional<T>) == max_alignment<T>::value,
                 "the optional is aligned like T (or like its flag)");
  static_assert (sizeof (optional<T>)
                     == round_up<sizeof (T) + sizeof (bool),
                                 max_alignment<T>::value>::value,
                 "the optional is the buffer of T followed by the flag");
  static_assert (sizeof (optional<T>) % alignof (optional<T>) == 0,
                 "the size is a multiple of the alignment");
  static_assert (sizeof (optional<T>) > sizeof (T),
                 "the flag takes room next to the buffer");
#if __cplusplus <= 202002L
  static_assert (sizeof (optional<T>) == sizeof (FormerLayout<T>),
                 "the size is that of the former aligned_storage layout");
  static_assert (alignof (optional<T>) == alignof (FormerLayout<T>),
                 "the alignment is that of the former aligned_storage layout");
#endif
}

template <typename T>
void
expect_object_is_aligned (T const &value)
{
  optional<T> engaged (value);
  ASSERT_TRUE (engaged.has_value ());
  EXPECT_EQ (reinterpret_cast<std::uintptr_t> (std::addressof (*engaged))
                 % alignof (T),
             static_cast<std::uintptr_t> (0));
  // The pointer of the optional is the address of the buffer: the first
  // member, so the value is at the start of the object.
  EXPECT_EQ (static_cast<void const *> (std::addressof (*engaged)),
             static_cast<void const *> (&engaged));
  // A copy placed elsewhere is aligned as well.
  optional<T> copy (engaged);
  EXPECT_EQ (reinterpret_cast<std::uintptr_t> (std::addressof (*copy))
                 % alignof (T),
             static_cast<std::uintptr_t> (0));
}

} // namespace

TEST (LumexOptionalStorageTest,
      GivenScalarTypes_WhenOptional_ThenLayoutMatches)
{
  expect_layout<char> ();
  expect_layout<bool> ();
  expect_layout<short> ();
  expect_layout<int> ();
  expect_layout<long long> ();
  expect_layout<float> ();
  expect_layout<double> ();
  expect_layout<long double> ();
  expect_layout<void *> ();
  expect_layout<std::uintptr_t> ();
}

TEST (LumexOptionalStorageTest, GivenClassTypes_WhenOptional_ThenLayoutMatches)
{
  expect_layout<std::string> ();
  expect_layout<std::vector<int>> ();
  expect_layout<std::unique_ptr<int>> ();
  expect_layout<std::array<char, 7>> ();
  expect_layout<OddSize3> ();
  expect_layout<OddSize5> ();
  expect_layout<CharAndDouble> ();
  expect_layout<Empty> ();
}

TEST (LumexOptionalStorageTest,
      GivenOverAlignedTypes_WhenOptional_ThenLayoutMatches)
{
  static_assert (alignof (Aligned16) == 16, "the test type is over-aligned");
  static_assert (alignof (Aligned32) == 32, "the test type is over-aligned");
  expect_layout<Aligned16> ();
  expect_layout<Aligned32> ();
  static_assert (alignof (optional<Aligned16>) == 16,
                 "the optional of an over-aligned type is over-aligned");
  static_assert (alignof (optional<Aligned32>) == 32,
                 "the optional of an over-aligned type is over-aligned");
}

TEST (LumexOptionalStorageTest,
      GivenOddSizedTypes_WhenOptional_ThenBufferHoldsExactlyTheType)
{
  // The flag follows the buffer without a gap that would hide a short buffer.
  static_assert (sizeof (optional<OddSize3>) == 4, "3 bytes plus the flag");
  static_assert (sizeof (optional<OddSize5>) == 6, "5 bytes plus the flag");
  static_assert (sizeof (optional<char>) == 2, "1 byte plus the flag");
  static_assert (sizeof (optional<std::array<char, 7>>) == 8,
                 "7 bytes plus the flag");
}

TEST (LumexOptionalStorageTest,
      GivenObjects_WhenEngaged_ThenTheValueIsAlignedForItsType)
{
  expect_object_is_aligned (static_cast<char> (1));
  expect_object_is_aligned (static_cast<short> (2));
  expect_object_is_aligned (3);
  expect_object_is_aligned (4LL);
  expect_object_is_aligned (5.0);
  expect_object_is_aligned (6.0L);
  expect_object_is_aligned (std::string ("a string long enough to leave the "
                                         "small string buffer behind"));
  Aligned16 aligned16 = { { 1, 2, 3 } };
  expect_object_is_aligned (aligned16);
  Aligned32 aligned32 = {};
  aligned32.data[39] = 7;
  expect_object_is_aligned (aligned32);
}

TEST (LumexOptionalStorageTest,
      GivenOverAlignedValue_WhenCopiedAndMoved_ThenContentsSurvive)
{
  Aligned32 source = {};
  for (int index = 0; index < 40; ++index)
    source.data[index] = static_cast<char> (index + 1);

  optional<Aligned32> original (source);
  optional<Aligned32> copy (original);
  optional<Aligned32> moved (std::move (copy));
  ASSERT_TRUE (original.has_value ());
  ASSERT_TRUE (moved.has_value ());
  for (int index = 0; index < 40; ++index)
    {
      EXPECT_EQ (original->data[index], static_cast<char> (index + 1));
      EXPECT_EQ (moved->data[index], static_cast<char> (index + 1));
    }
  moved.reset ();
  EXPECT_FALSE (moved.has_value ());
  EXPECT_TRUE (original.has_value ());
}

TEST (LumexOptionalStorageTest,
      GivenConstOptional_WhenAccess_ThenValueIsReadThroughTheBuffer)
{
  optional<std::string> const text (std::string ("const view of the buffer"));
  ASSERT_TRUE (text.has_value ());
  EXPECT_EQ (*text, "const view of the buffer");
  EXPECT_EQ (text->size (), static_cast<std::size_t> (24));
  EXPECT_EQ (static_cast<void const *> (std::addressof (*text)),
             static_cast<void const *> (&text));
}
