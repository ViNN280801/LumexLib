// LumexMemReadConstraints.cxx11.tests.cpp
//
// Which calls of as<T> exist. The constraints are SFINAE in every standard, so
// a call that does not satisfy them finds no overload; the detectors below ask
// the compiler instead of compiling a rejected call. T must be trivially
// copyable, standard layout, neither a pointer nor a reference, and neither
// const nor volatile (the bytes are copied into a local object of T); the
// element of a span must be char, unsigned char or the byte of the span module
// (std::byte from C++17); a source object needs get_data () convertible to
// void const * and get_data_size () convertible to int, both on a const
// object. The result type is in LumexMemReadResultType.
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/span/LumexSpan"
#include "lumex/core/utility/mem/LumexMemRead.hpp"

namespace traits = lumex::core::utility::traits;
using lumex::core::span::view::byte;
using lumex::core::span::view::span;
using lumex::core::utility::mem::as;

namespace
{
template <typename T, typename = void>
struct reads_from_pointer : std::false_type
{
};

template <typename T>
struct reads_from_pointer<
    T, traits::meta::void_t<decltype (as<T> (std::declval<void const *> (),
                                             std::declval<std::size_t> ()))>>
    : std::true_type
{
};

template <typename T, typename Source, typename = void>
struct reads_from_source : std::false_type
{
};

template <typename T, typename Source>
struct reads_from_source<
    T, Source,
    traits::meta::void_t<decltype (as<T> (std::declval<Source const &> ()))>>
    : std::true_type
{
};

template <typename T, typename Element, typename = void>
struct reads_from_span : std::false_type
{
};

template <typename T, typename Element>
struct reads_from_span<T, Element,
                       traits::meta::void_t<decltype (as<T> (
                           std::declval<span<Element const>> ()))>>
    : std::true_type
{
};

template <typename T, typename Element, typename = void>
struct reads_from_mutable_span : std::false_type
{
};

template <typename T, typename Element>
struct reads_from_mutable_span<
    T, Element,
    traits::meta::void_t<decltype (as<T> (std::declval<span<Element>> ()))>>
    : std::true_type
{
};

struct plain_point
{
  std::int32_t x;
  std::int32_t y;
};

struct base_part
{
  std::int32_t a;
};

// Trivially copyable, but members in the base and in the derived class: not
// standard layout.
struct mixed_layout : base_part
{
  std::int32_t b;
};

struct polymorphic
{
  virtual ~polymorphic () {}
  std::int32_t value;
};

enum class small_enum : std::uint8_t
{
  first,
  second
};

struct good_source
{
  void const *
  get_data () const
  {
    return nullptr;
  }
  int
  get_data_size () const
  {
    return 0;
  }
};

struct wide_source
{
  unsigned char const *
  get_data () const
  {
    return nullptr;
  }
  std::size_t
  get_data_size () const
  {
    return 0;
  }
};

struct only_data
{
  void const *
  get_data () const
  {
    return nullptr;
  }
};

struct only_size
{
  int
  get_data_size () const
  {
    return 0;
  }
};

struct text_data
{
  std::string
  get_data () const
  {
    return std::string ();
  }
  int
  get_data_size () const
  {
    return 0;
  }
};

struct text_size
{
  void const *
  get_data () const
  {
    return nullptr;
  }
  std::string
  get_data_size () const
  {
    return std::string ();
  }
};

struct mutating_source
{
  void const *
  get_data ()
  {
    return nullptr;
  }
  int
  get_data_size ()
  {
    return 0;
  }
};
} // namespace

TEST (LumexMemReadConstraintsTest,
      GivenValueTypes_WhenReadFromPointer_ThenOnlyExtractibleOnes)
{
  EXPECT_TRUE (reads_from_pointer<int>::value);
  EXPECT_TRUE (reads_from_pointer<std::uint8_t>::value);
  EXPECT_TRUE (reads_from_pointer<double>::value);
  EXPECT_TRUE (reads_from_pointer<bool>::value);
  EXPECT_TRUE (reads_from_pointer<plain_point>::value);
  EXPECT_TRUE (reads_from_pointer<small_enum>::value);
  EXPECT_TRUE ((reads_from_pointer<std::array<char, 4>>::value));

  EXPECT_FALSE (reads_from_pointer<std::string>::value);
  EXPECT_FALSE (reads_from_pointer<std::vector<int>>::value);
  EXPECT_FALSE (reads_from_pointer<polymorphic>::value);
  EXPECT_FALSE (reads_from_pointer<mixed_layout>::value);
  EXPECT_FALSE (reads_from_pointer<int *>::value);
  EXPECT_FALSE (reads_from_pointer<char const *>::value);
  EXPECT_FALSE (reads_from_pointer<int &>::value);
  EXPECT_FALSE (reads_from_pointer<plain_point &>::value);
  EXPECT_FALSE (reads_from_pointer<void (*) ()>::value);
}

TEST (LumexMemReadConstraintsTest,
      GivenCvQualifiedValueTypes_WhenReadFromPointer_ThenRejected)
{
  // A const or volatile T is trivially copyable, so the constraint used to
  // accept it and the body failed to compile (memcpy into a const object).
  EXPECT_FALSE (reads_from_pointer<int const>::value);
  EXPECT_FALSE (reads_from_pointer<int volatile>::value);
  EXPECT_FALSE (reads_from_pointer<int const volatile>::value);
  EXPECT_FALSE (reads_from_pointer<std::uint8_t const>::value);
  EXPECT_FALSE (reads_from_pointer<double const>::value);
  EXPECT_FALSE (reads_from_pointer<plain_point const>::value);
  EXPECT_FALSE (reads_from_pointer<plain_point volatile>::value);
  EXPECT_FALSE (reads_from_pointer<small_enum const>::value);
  EXPECT_FALSE ((reads_from_pointer<std::array<char, 4> const>::value));
  // The unqualified types stay accepted.
  EXPECT_TRUE (reads_from_pointer<int>::value);
  EXPECT_TRUE (reads_from_pointer<plain_point>::value);
}

TEST (LumexMemReadConstraintsTest,
      GivenSourceTypes_WhenRead_ThenOnlyDataSources)
{
  EXPECT_TRUE ((reads_from_source<int, good_source>::value));
  EXPECT_TRUE ((reads_from_source<int, wide_source>::value));
  EXPECT_TRUE ((reads_from_source<plain_point, good_source>::value));

  EXPECT_FALSE ((reads_from_source<int, only_data>::value));
  EXPECT_FALSE ((reads_from_source<int, only_size>::value));
  EXPECT_FALSE ((reads_from_source<int, text_data>::value));
  EXPECT_FALSE ((reads_from_source<int, text_size>::value));
  EXPECT_FALSE ((reads_from_source<int, mutating_source>::value));
  EXPECT_FALSE ((reads_from_source<int, int>::value));
  EXPECT_FALSE ((reads_from_source<int, std::vector<unsigned char>>::value));
  // A span is read by its own overload, it is not a source object.
  EXPECT_FALSE (lumex::core::utility::mem::Detail::is_data_source<
                span<unsigned char const>>::value);
  EXPECT_TRUE (
      lumex::core::utility::mem::Detail::is_data_source<good_source>::value);
}

TEST (LumexMemReadConstraintsTest,
      GivenSourceWithNonExtractibleValue_WhenRead_ThenRejected)
{
  EXPECT_FALSE ((reads_from_source<std::string, good_source>::value));
  EXPECT_FALSE ((reads_from_source<int *, good_source>::value));
  EXPECT_FALSE ((reads_from_source<int &, good_source>::value));
}

TEST (LumexMemReadConstraintsTest,
      GivenCvQualifiedValueTypes_WhenReadFromSourceOrSpan_ThenRejected)
{
  EXPECT_FALSE ((reads_from_source<int const, good_source>::value));
  EXPECT_FALSE ((reads_from_source<int volatile, good_source>::value));
  EXPECT_FALSE ((reads_from_source<plain_point const, wide_source>::value));
  EXPECT_FALSE ((reads_from_span<int const, unsigned char>::value));
  EXPECT_FALSE ((reads_from_span<int volatile, char>::value));
  EXPECT_FALSE ((reads_from_span<plain_point const, byte>::value));
  EXPECT_TRUE ((reads_from_source<int, good_source>::value));
  EXPECT_TRUE ((reads_from_span<int, unsigned char>::value));
}

TEST (LumexMemReadConstraintsTest, GivenSpanElements_WhenRead_ThenOnlyByteLike)
{
  EXPECT_TRUE ((reads_from_span<int, char>::value));
  EXPECT_TRUE ((reads_from_span<int, unsigned char>::value));
  EXPECT_TRUE ((reads_from_span<int, std::uint8_t>::value));
  EXPECT_TRUE ((reads_from_span<int, byte>::value));

  EXPECT_FALSE ((reads_from_span<int, signed char>::value));
  EXPECT_FALSE ((reads_from_span<int, std::int8_t>::value));
  EXPECT_FALSE ((reads_from_span<int, std::uint16_t>::value));
  EXPECT_FALSE ((reads_from_span<int, int>::value));
  EXPECT_FALSE ((reads_from_span<int, bool>::value));
  EXPECT_FALSE ((reads_from_span<int, plain_point>::value));
}

TEST (LumexMemReadConstraintsTest,
      GivenSpanOfBytes_WhenValueNotExtractible_ThenRejected)
{
  EXPECT_FALSE ((reads_from_span<std::string, unsigned char>::value));
  EXPECT_FALSE ((reads_from_span<int *, unsigned char>::value));
  EXPECT_FALSE ((reads_from_span<int &, unsigned char>::value));
  EXPECT_FALSE ((reads_from_span<mixed_layout, byte>::value));
}

TEST (LumexMemReadConstraintsTest,
      GivenMutableSpan_WhenRead_ThenNeedsConversionToConstSpan)
{
  // A template deduces ByteType from span<ByteType const>: a span<char> does
  // not match, the caller converts it first (LumexMemReadSpan calls that).
  EXPECT_FALSE ((reads_from_mutable_span<int, char>::value));
  EXPECT_TRUE ((std::is_convertible<span<char>, span<char const>>::value));
  EXPECT_TRUE ((reads_from_span<int, char>::value));
}
