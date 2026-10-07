// LumexSpanStdDifferential.cxx20.tests.cpp
//
// Differential tests against the std::span of the standard library in use
// (libstdc++, libc++, the MSVC STL): the same scenarios are compiled for both
// types and the results are compared, so the span of this library keeps the
// behavior of the standard one where the standard has one. Compared: whether
// every constructor-like conversion of a matrix of element types, extents and
// sources is constructible, whether it is implicit, the member types and
// constants, the result types of the subviews, and the observable results
// (data, size, subviews, bytes, elements) of the same inputs. noexcept
// specifications are not compared: the standard does not fix them for the
// range and iterator constructors and the libraries differ.
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <list>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeinfo>
#include <vector>
#if __has_include(<span>)
#include <span>
#endif

#include <gtest/gtest.h>

#include "LumexSpanTestSupport.hpp"
#include "lumex/core/span/LumexSpan"

#if LUMEX_HAS_STD_SPAN && LUMEX_SPAN_HAS_RANGES

namespace
{
namespace own = lumex::core::span::view;
using lumex_span_test::base_element;
using lumex_span_test::derived_element;

// A span-typed source whose span template is chosen per side of the
// comparison.
template <typename U, std::size_t N> struct span_marker
{
};

template <template <typename, std::size_t> class S, typename Descriptor>
struct resolve
{
  using type = Descriptor;
};

template <template <typename, std::size_t> class S, typename U, std::size_t N>
struct resolve<S, span_marker<U, N>>
{
  using type = S<U, N>;
};

template <template <typename, std::size_t> class S, typename Descriptor>
struct resolve<S, Descriptor &>
{
  using type = typename resolve<S, Descriptor>::type &;
};

template <template <typename, std::size_t> class S, typename Descriptor>
struct resolve<S, Descriptor &&>
{
  using type = typename resolve<S, Descriptor>::type &&;
};

template <template <typename, std::size_t> class S, typename Descriptor>
struct resolve<S, Descriptor const>
{
  using type = typename resolve<S, Descriptor>::type const;
};

template <template <typename, std::size_t> class S, typename T, std::size_t E,
          typename... Sources>
constexpr unsigned
signature ()
{
  using target = S<T, E>;
  constexpr bool constructible
      = std::is_constructible_v<target, typename resolve<S, Sources>::type...>;
  constexpr bool implicit = [] ()
    {
      if constexpr (sizeof...(Sources) == 1)
        {
          return std::is_convertible_v<typename resolve<S, Sources>::type...,
                                       target>;
        }
      else if constexpr (sizeof...(Sources) == 2)
        {
          return lumex_span_test::is_implicitly_listable<
              target, typename resolve<S, Sources>::type...>::value;
        }
      else
        {
          return false;
        }
    }();
  return (constructible ? 1u : 0u) | (implicit ? 2u : 0u);
}

template <typename T, std::size_t E, typename... Sources>
void
expect_same_constructibility (char const *label)
{
  SCOPED_TRACE (label);
  EXPECT_EQ ((signature<std::span, T, E, Sources...> ()),
             (signature<own::span, T, E, Sources...> ()))
      << "std::span and the span of this library disagree (1 = constructible, "
         "2 = implicit)";
}

#define LUMEX_SPAN_SAME(T, E, ...)                                            \
  expect_same_constructibility<T, E, __VA_ARGS__> (#T ", " #E                 \
                                                      ", " #__VA_ARGS__)

// Runs the macro for the extents the matrix covers.
#define LUMEX_SPAN_SAME_EVERY_EXTENT(T, ...)                                  \
  LUMEX_SPAN_SAME (T, std::dynamic_extent, __VA_ARGS__);                      \
  LUMEX_SPAN_SAME (T, 0, __VA_ARGS__);                                        \
  LUMEX_SPAN_SAME (T, 3, __VA_ARGS__);                                        \
  LUMEX_SPAN_SAME (T, 4, __VA_ARGS__)
} // namespace

// -- Member types and constants --

TEST (LumexSpanStdDifferentialTest, GivenSpans_WhenMemberTypes_ThenSame)
{
  auto expect_same_types = [] (auto standard, auto own_span)
    {
      using s = decltype (standard);
      using o = decltype (own_span);
      static_assert (
          std::is_same_v<typename s::element_type, typename o::element_type>);
      static_assert (
          std::is_same_v<typename s::value_type, typename o::value_type>);
      static_assert (
          std::is_same_v<typename s::size_type, typename o::size_type>);
      static_assert (std::is_same_v<typename s::difference_type,
                                    typename o::difference_type>);
      static_assert (std::is_same_v<typename s::pointer, typename o::pointer>);
      static_assert (std::is_same_v<typename s::const_pointer,
                                    typename o::const_pointer>);
      static_assert (
          std::is_same_v<typename s::reference, typename o::reference>);
      static_assert (std::is_same_v<typename s::const_reference,
                                    typename o::const_reference>);
      static_assert (s::extent == o::extent);
      static_assert (
          std::is_same_v<decltype (s::extent), decltype (o::extent)>);
      static_assert (std::is_default_constructible_v<s>
                     == std::is_default_constructible_v<o>);
      static_assert (std::is_trivially_copyable_v<s>
                     == std::is_trivially_copyable_v<o>);
      static_assert (std::is_trivially_destructible_v<s>
                     == std::is_trivially_destructible_v<o>);
      static_assert (std::is_nothrow_copy_constructible_v<s>
                     == std::is_nothrow_copy_constructible_v<o>);
      static_assert (std::is_nothrow_copy_assignable_v<s>
                     == std::is_nothrow_copy_assignable_v<o>);
      static_assert (sizeof (s) == sizeof (o));
      static_assert (std::is_standard_layout_v<s>
                     == std::is_standard_layout_v<o>);
    };
  expect_same_types (std::span<int>{}, own::span<int>{});
  expect_same_types (std::span<int const>{}, own::span<int const>{});
  expect_same_types (std::span<int volatile>{}, own::span<int volatile>{});
  expect_same_types (std::span<int const volatile>{},
                     own::span<int const volatile>{});
  expect_same_types (std::span<int, 0>{}, own::span<int, 0>{});
  expect_same_types (std::span<std::string>{}, own::span<std::string>{});
  int values[3] = { 1, 2, 3 };
  expect_same_types (std::span<int, 3> (values), own::span<int, 3> (values));
  expect_same_types (
      std::span<char const, 8> (static_cast<char const *> (""), 8),
      own::span<char const, 8> (static_cast<char const *> (""), 8));
  EXPECT_EQ (std::dynamic_extent, own::dynamic_extent);
}

// -- Construction matrix --

TEST (LumexSpanStdDifferentialTest,
      GivenPointerSources_WhenPointerAndCount_ThenSameConstructibility)
{
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, int *, std::size_t);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, int const *, std::size_t);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int const, int *, std::size_t);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int const, int const *, std::size_t);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int const, int volatile *, std::size_t);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int volatile, int *, std::size_t);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int const volatile, int *, std::size_t);
  LUMEX_SPAN_SAME_EVERY_EXTENT (base_element, base_element *, std::size_t);
  LUMEX_SPAN_SAME_EVERY_EXTENT (base_element, derived_element *, std::size_t);
  LUMEX_SPAN_SAME_EVERY_EXTENT (base_element const, derived_element *,
                                std::size_t);
  LUMEX_SPAN_SAME_EVERY_EXTENT (derived_element, base_element *, std::size_t);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, void *, std::size_t);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, char *, std::size_t);
  LUMEX_SPAN_SAME_EVERY_EXTENT (char, unsigned char *, std::size_t);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, std::nullptr_t, std::size_t);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, int *, int);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, int *, unsigned);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, int *, long long);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, int *, bool);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, int *, double);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, int *, std::nullptr_t);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, int *);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, int);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, std::nullptr_t);
}

TEST (LumexSpanStdDifferentialTest,
      GivenPointerPairs_WhenPair_ThenSameConstructibility)
{
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, int *, int *);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, int *, int const *);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, int const *, int const *);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int const, int *, int *);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int const, int *, int const *);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int const, int const *, int *);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int const, int const *, int const *);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, int *, char *);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, char *, char *);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, int *, void *);
  LUMEX_SPAN_SAME_EVERY_EXTENT (base_element, derived_element *,
                                derived_element *);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, std::vector<int>::iterator,
                                std::vector<int>::iterator);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int const, std::vector<int>::const_iterator,
                                std::vector<int>::const_iterator);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, std::vector<int>::const_iterator,
                                std::vector<int>::const_iterator);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, std::vector<int>::iterator, std::size_t);
  LUMEX_SPAN_SAME_EVERY_EXTENT (char, std::string::iterator,
                                std::string::iterator);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, std::list<int>::iterator,
                                std::list<int>::iterator);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, std::vector<bool>::iterator,
                                std::vector<bool>::iterator);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, std::reverse_iterator<int *>,
                                std::reverse_iterator<int *>);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, std::counted_iterator<int *>,
                                std::default_sentinel_t);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, std::counted_iterator<int *>,
                                std::size_t);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, std::move_iterator<int *>, std::size_t);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, lumex_span_test::pointer_iterator<int>,
                                std::size_t);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, lumex_span_test::pointer_iterator<int>,
                                lumex_span_test::pointer_iterator<int>);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, lumex_span_test::pointer_iterator<int>,
                                lumex_span_test::count_sentinel);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, lumex_span_test::guarded_iterator<int>,
                                std::size_t);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, lumex_span_test::guarded_iterator<int>,
                                lumex_span_test::guarded_iterator<int>);
  LUMEX_SPAN_SAME_EVERY_EXTENT (
      int, lumex_span_test::unregistered_iterator<int>, std::size_t);
}

TEST (LumexSpanStdDifferentialTest,
      GivenArrays_WhenSpan_ThenSameConstructibility)
{
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, int (&)[3]);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, int (&)[4]);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, int (&)[0 + 1]);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, int const (&)[3]);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, int (&&)[3]);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int const, int (&)[3]);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int const, int const (&)[3]);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int const, int const (&)[4]);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int const, int (&&)[3]);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int const, int const (&&)[3]);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int volatile, int (&)[3]);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, int volatile (&)[3]);
  LUMEX_SPAN_SAME_EVERY_EXTENT (base_element, base_element (&)[3]);
  LUMEX_SPAN_SAME_EVERY_EXTENT (base_element, derived_element (&)[3]);
  LUMEX_SPAN_SAME_EVERY_EXTENT (char const, char const (&)[4]);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, long (&)[3]);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, int (&)[3][3]);
}

TEST (LumexSpanStdDifferentialTest,
      GivenStdArrays_WhenSpan_ThenSameConstructibility)
{
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, std::array<int, 3> &);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, std::array<int, 3> const &);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, std::array<int, 3> &&);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, std::array<int, 4> &);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, std::array<int, 0> &);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, std::array<int const, 3> &);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int const, std::array<int, 3> &);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int const, std::array<int, 3> const &);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int const, std::array<int, 3> &&);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int const, std::array<int const, 3> &);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int const, std::array<int const, 3> const &);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int const, std::array<int, 4> &);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int const, std::array<int, 0> &);
  LUMEX_SPAN_SAME_EVERY_EXTENT (base_element, std::array<base_element, 3> &);
  LUMEX_SPAN_SAME_EVERY_EXTENT (base_element,
                                std::array<derived_element, 3> &);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, std::array<long, 3> &);
}

TEST (LumexSpanStdDifferentialTest,
      GivenContainers_WhenSpan_ThenSameConstructibility)
{
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, std::vector<int> &);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, std::vector<int> const &);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, std::vector<int> &&);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, std::vector<int> const &&);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int const, std::vector<int> &);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int const, std::vector<int> const &);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int const, std::vector<int> &&);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int const, std::vector<int> const &&);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int const, std::vector<long> &);
  LUMEX_SPAN_SAME_EVERY_EXTENT (base_element, std::vector<derived_element> &);
  LUMEX_SPAN_SAME_EVERY_EXTENT (base_element const,
                                std::vector<derived_element> &);
  LUMEX_SPAN_SAME_EVERY_EXTENT (bool, std::vector<bool> &);
  LUMEX_SPAN_SAME_EVERY_EXTENT (bool const, std::vector<bool> &);
  LUMEX_SPAN_SAME_EVERY_EXTENT (bool const, std::vector<bool> const &);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, std::list<int> &);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int const, std::list<int> &);
  LUMEX_SPAN_SAME_EVERY_EXTENT (char, std::string &);
  LUMEX_SPAN_SAME_EVERY_EXTENT (char, std::string const &);
  LUMEX_SPAN_SAME_EVERY_EXTENT (char const, std::string &);
  LUMEX_SPAN_SAME_EVERY_EXTENT (char const, std::string const &);
  LUMEX_SPAN_SAME_EVERY_EXTENT (char const, std::string &&);
  LUMEX_SPAN_SAME_EVERY_EXTENT (char const, std::string_view);
  LUMEX_SPAN_SAME_EVERY_EXTENT (char const, std::string_view &);
  LUMEX_SPAN_SAME_EVERY_EXTENT (char, std::string_view);
  LUMEX_SPAN_SAME_EVERY_EXTENT (wchar_t const, std::wstring &);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int const, std::initializer_list<int>);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int const, std::initializer_list<int> &);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, std::initializer_list<int> &);
}

TEST (LumexSpanStdDifferentialTest,
      GivenSpans_WhenSpan_ThenSameConstructibility)
{
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, span_marker<int, 3>);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, span_marker<int, 3> &);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, span_marker<int, 3> const &);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, span_marker<int, 4>);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, span_marker<int, 0>);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, span_marker<int, std::dynamic_extent>);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, span_marker<int, std::dynamic_extent> &);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, span_marker<int const, 3>);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int,
                                span_marker<int const, std::dynamic_extent>);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int const, span_marker<int, 3>);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int const, span_marker<int, 4>);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int const, span_marker<int, 0>);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int const,
                                span_marker<int, std::dynamic_extent>);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int const, span_marker<int const, 3>);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int const,
                                span_marker<int const, std::dynamic_extent>);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int const volatile, span_marker<int, 3>);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, span_marker<int volatile, 3>);
  LUMEX_SPAN_SAME_EVERY_EXTENT (base_element, span_marker<derived_element, 3>);
  LUMEX_SPAN_SAME_EVERY_EXTENT (
      base_element const, span_marker<derived_element, std::dynamic_extent>);
  LUMEX_SPAN_SAME_EVERY_EXTENT (char, span_marker<int, 3>);
}

namespace
{
// A span of the other kind as the source behaves as the span of the same kind
// would: own from std like own from own, std from own like std from std. Two
// cases differ by design and are left out of the comparison of the implicit
// bit and, for std, of the constructible bit: a static extent made from a
// range is explicit (own from std::span<U, N> and std from own::span<U, N>),
// and std::span cannot see the extent of a span that is not its own, so it
// accepts a static extent that differs (a precondition violation at run time).
template <typename T, std::size_t E, typename U, std::size_t N>
void
expect_foreign_span_behaves_as_native ()
{
  SCOPED_TRACE (testing::Message ()
                << "T=" << typeid (T).name () << " E=" << E
                << " U=" << typeid (U).name () << " N=" << N);
  constexpr unsigned own_from_std
      = signature<own::span, T, E, std::span<U, N>> ();
  constexpr unsigned own_from_own
      = signature<own::span, T, E, own::span<U, N>> ();
  constexpr unsigned std_from_own
      = signature<std::span, T, E, own::span<U, N>> ();
  constexpr unsigned std_from_std
      = signature<std::span, T, E, std::span<U, N>> ();
  constexpr bool both_static
      = E != std::dynamic_extent && N != std::dynamic_extent;
  EXPECT_EQ (own_from_std & 1u, own_from_own & 1u)
      << "own target: constructible from std::span as from own span";
  if (!both_static)
    {
      EXPECT_EQ (own_from_std, own_from_own)
          << "own target: the implicit bit follows the extents";
      EXPECT_EQ (std_from_own, std_from_std)
          << "std target: constructible and implicit as from std::span";
    }
  else if (E == N)
    {
      EXPECT_EQ (std_from_own & 1u, std_from_std & 1u);
    }
}
} // namespace

#define LUMEX_SPAN_FOREIGN_ROW(T, U, E)                                       \
  expect_foreign_span_behaves_as_native<T, E, U, std::dynamic_extent> ();     \
  expect_foreign_span_behaves_as_native<T, E, U, 0> ();                       \
  expect_foreign_span_behaves_as_native<T, E, U, 3> ();                       \
  expect_foreign_span_behaves_as_native<T, E, U, 4> ()

#define LUMEX_SPAN_FOREIGN(T, U)                                              \
  LUMEX_SPAN_FOREIGN_ROW (T, U, std::dynamic_extent);                         \
  LUMEX_SPAN_FOREIGN_ROW (T, U, 0);                                           \
  LUMEX_SPAN_FOREIGN_ROW (T, U, 3);                                           \
  LUMEX_SPAN_FOREIGN_ROW (T, U, 4)

TEST (LumexSpanStdDifferentialTest,
      GivenForeignSpanSources_WhenSpan_ThenBehaveAsNative)
{
  LUMEX_SPAN_FOREIGN (int, int);
  LUMEX_SPAN_FOREIGN (int, int const);
  LUMEX_SPAN_FOREIGN (int const, int);
  LUMEX_SPAN_FOREIGN (int const, int const);
  LUMEX_SPAN_FOREIGN (int, long);
  LUMEX_SPAN_FOREIGN (base_element, derived_element);
  LUMEX_SPAN_FOREIGN (base_element const, derived_element);
  LUMEX_SPAN_FOREIGN (int const volatile, int);
}

TEST (LumexSpanStdDifferentialTest,
      GivenOtherTypes_WhenSpan_ThenSameConstructibility)
{
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, double);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, std::string_view *);
  LUMEX_SPAN_SAME_EVERY_EXTENT (int, std::size_t);
  LUMEX_SPAN_SAME_EVERY_EXTENT (std::string, std::vector<std::string> &);
  LUMEX_SPAN_SAME_EVERY_EXTENT (std::string const, std::vector<std::string> &);
  LUMEX_SPAN_SAME_EVERY_EXTENT (std::string, std::vector<std::string> const &);
}

// -- Subview result types --

TEST (LumexSpanStdDifferentialTest,
      GivenSubviews_WhenResultTypes_ThenSameExtentsAndElements)
{
  auto same_result = [] (auto standard, auto own_span)
    {
      using s = decltype (standard);
      using o = decltype (own_span);
      static_assert (s::extent == o::extent);
      static_assert (
          std::is_same_v<typename s::element_type, typename o::element_type>);
    };
  int values[5] = { 1, 2, 3, 4, 5 };
  std::span<int, 5> const fixed_standard (values);
  own::span<int, 5> const fixed_own (values);
  std::span<int> const dynamic_standard (values);
  own::span<int> const dynamic_own (values);
  same_result (fixed_standard.first<2> (), fixed_own.first<2> ());
  same_result (fixed_standard.last<2> (), fixed_own.last<2> ());
  same_result (fixed_standard.subspan<1> (), fixed_own.subspan<1> ());
  same_result (fixed_standard.subspan<1, 2> (), fixed_own.subspan<1, 2> ());
  same_result (fixed_standard.subspan<5> (), fixed_own.subspan<5> ());
  same_result (fixed_standard.subspan<0, 0> (), fixed_own.subspan<0, 0> ());
  same_result (fixed_standard.first (2), fixed_own.first (2));
  same_result (fixed_standard.last (2), fixed_own.last (2));
  same_result (fixed_standard.subspan (1), fixed_own.subspan (1));
  same_result (dynamic_standard.first<2> (), dynamic_own.first<2> ());
  same_result (dynamic_standard.last<2> (), dynamic_own.last<2> ());
  same_result (dynamic_standard.subspan<1> (), dynamic_own.subspan<1> ());
  same_result (dynamic_standard.subspan<1, 2> (),
               dynamic_own.subspan<1, 2> ());
  same_result (dynamic_standard.first (2), dynamic_own.first (2));
  same_result (dynamic_standard.subspan (1, 2), dynamic_own.subspan (1, 2));
  same_result (std::as_bytes (fixed_standard), own::as_bytes (fixed_own));
  same_result (std::as_bytes (dynamic_standard), own::as_bytes (dynamic_own));
  same_result (std::as_writable_bytes (fixed_standard),
               own::as_writable_bytes (fixed_own));
  same_result (std::as_writable_bytes (dynamic_standard),
               own::as_writable_bytes (dynamic_own));
  static_assert (std::is_same_v<own::byte, std::byte>);
  SUCCEED ();
}

namespace
{
template <typename S>
concept std_has_bytes = requires (S view) { std::as_bytes (view); };
template <typename S>
concept own_has_bytes = requires (S view) { own::as_bytes (view); };
template <typename S>
concept std_has_writable_bytes
    = requires (S view) { std::as_writable_bytes (view); };
template <typename S>
concept own_has_writable_bytes
    = requires (S view) { own::as_writable_bytes (view); };
} // namespace

TEST (LumexSpanStdDifferentialTest,
      GivenByteViewConstraints_WhenConstOrVolatile_ThenSameAvailability)
{
  static_assert (std_has_bytes<std::span<int>>
                 == own_has_bytes<own::span<int>>);
  static_assert (std_has_bytes<std::span<int const>>
                 == own_has_bytes<own::span<int const>>);
  static_assert (std_has_bytes<std::span<int, 3>>
                 == own_has_bytes<own::span<int, 3>>);
  static_assert (std_has_writable_bytes<std::span<int>>
                 == own_has_writable_bytes<own::span<int>>);
  static_assert (std_has_writable_bytes<std::span<int const>>
                 == own_has_writable_bytes<own::span<int const>>);
  // A volatile element has no byte view: libc++ removes the function from
  // the overload set, libstdc++ fails when it is instantiated; this library
  // follows libc++.
  static_assert (!own_has_bytes<own::span<int volatile>>);
  static_assert (!own_has_writable_bytes<own::span<int volatile>>);
  static_assert (std_has_writable_bytes<std::span<int, 3>>
                 == own_has_writable_bytes<own::span<int, 3>>);
  static_assert (!own_has_writable_bytes<own::span<int const>>);
  static_assert (own_has_bytes<own::span<int>>);
  SUCCEED ();
}

// -- Results of the same inputs --

namespace
{
template <typename S, typename V>
std::vector<std::ptrdiff_t>
describe (S const &view, V const *origin)
{
  std::vector<std::ptrdiff_t> facts;
  facts.push_back (static_cast<std::ptrdiff_t> (view.size ()));
  facts.push_back (static_cast<std::ptrdiff_t> (view.size_bytes ()));
  facts.push_back (view.empty () ? 1 : 0);
  facts.push_back (view.data () == nullptr ? -1 : view.data () - origin);
  facts.push_back (view.end () - view.begin ());
  facts.push_back (view.rend () - view.rbegin ());
  for (std::size_t index = 0; index < view.size (); ++index)
    {
      facts.push_back (static_cast<std::ptrdiff_t> (view[index]));
    }
  if (!view.empty ())
    {
      facts.push_back (static_cast<std::ptrdiff_t> (view.front ()));
      facts.push_back (static_cast<std::ptrdiff_t> (view.back ()));
    }
  return facts;
}

} // namespace

TEST (LumexSpanStdDifferentialTest,
      GivenSameStorage_WhenObserved_ThenSameFacts)
{
  int values[8] = { 10, 11, 12, 13, 14, 15, 16, 17 };
  EXPECT_EQ (describe (std::span<int> (values), values),
             describe (own::span<int> (values), values));
  EXPECT_EQ (describe (std::span<int, 8> (values), values),
             describe (own::span<int, 8> (values), values));
  EXPECT_EQ (describe (std::span<int> (values + 2, 3), values),
             describe (own::span<int> (values + 2, 3), values));
  EXPECT_EQ (describe (std::span<int> (values + 1, values + 6), values),
             describe (own::span<int> (values + 1, values + 6), values));
  EXPECT_EQ (describe (std::span<int> (values, 0), values),
             describe (own::span<int> (values, 0), values));
  EXPECT_EQ (describe (std::span<int> (), values),
             describe (own::span<int> (), values));
  EXPECT_EQ (describe (std::span<int, 0> (), values),
             describe (own::span<int, 0> (), values));
  std::vector<int> vector_values = { 1, 2, 3, 4 };
  EXPECT_EQ (describe (std::span<int> (vector_values), vector_values.data ()),
             describe (own::span<int> (vector_values), vector_values.data ()));
  std::array<int, 5> array_values = { { 5, 4, 3, 2, 1 } };
  EXPECT_EQ (
      describe (std::span<int const> (array_values), array_values.data ()),
      describe (own::span<int const> (array_values), array_values.data ()));
  std::string text = "differential";
  EXPECT_EQ (describe (std::span<char const> (text), text.data ()),
             describe (own::span<char const> (text), text.data ()));
}

TEST (LumexSpanStdDifferentialTest,
      GivenSameStorage_WhenSubviews_ThenSameFacts)
{
  int values[8] = { 10, 11, 12, 13, 14, 15, 16, 17 };
  std::span<int> const standard (values);
  own::span<int> const mine (values);
  for (std::size_t count = 0; count <= 8; ++count)
    {
      EXPECT_EQ (describe (standard.first (count), values),
                 describe (mine.first (count), values))
          << "first (" << count << ")";
      EXPECT_EQ (describe (standard.last (count), values),
                 describe (mine.last (count), values))
          << "last (" << count << ")";
      for (std::size_t offset = 0; offset + count <= 8; ++offset)
        {
          EXPECT_EQ (describe (standard.subspan (offset, count), values),
                     describe (mine.subspan (offset, count), values))
              << "subspan (" << offset << ", " << count << ")";
        }
    }
  for (std::size_t offset = 0; offset <= 8; ++offset)
    {
      EXPECT_EQ (describe (standard.subspan (offset), values),
                 describe (mine.subspan (offset), values))
          << "subspan (" << offset << ")";
    }
  EXPECT_EQ (describe (standard.first<3> (), values),
             describe (mine.first<3> (), values));
  EXPECT_EQ (describe (standard.last<3> (), values),
             describe (mine.last<3> (), values));
  EXPECT_EQ ((describe (standard.subspan<2, 4> (), values)),
             (describe (mine.subspan<2, 4> (), values)));
  EXPECT_EQ (describe (standard.subspan<5> (), values),
             describe (mine.subspan<5> (), values));
  std::span<int, 8> const fixed_standard (values);
  own::span<int, 8> const fixed_mine (values);
  EXPECT_EQ (describe (fixed_standard.subspan<3> (), values),
             describe (fixed_mine.subspan<3> (), values));
  EXPECT_EQ ((describe (fixed_standard.subspan<3, 2> (), values)),
             (describe (fixed_mine.subspan<3, 2> (), values)));
}

TEST (LumexSpanStdDifferentialTest, GivenSameStorage_WhenAsBytes_ThenSameBytes)
{
  std::int32_t values[3] = { 0x01020304, -1, 0x7F000080 };
  auto standard = std::as_bytes (std::span<std::int32_t> (values));
  auto mine = own::as_bytes (own::span<std::int32_t> (values));
  ASSERT_EQ (standard.size (), mine.size ());
  EXPECT_EQ (standard.data (), mine.data ());
  for (std::size_t index = 0; index < standard.size (); ++index)
    {
      EXPECT_EQ (standard[index], mine[index]);
    }
  auto standard_writable
      = std::as_writable_bytes (std::span<std::int32_t, 3> (values));
  auto mine_writable
      = own::as_writable_bytes (own::span<std::int32_t, 3> (values));
  static_assert (decltype (standard_writable)::extent
                 == decltype (mine_writable)::extent);
  EXPECT_EQ (standard_writable.data (), mine_writable.data ());
}

TEST (LumexSpanStdDifferentialTest, GivenCtadInputs_WhenDeduced_ThenSameType)
{
  int values[4] = { 1, 2, 3, 4 };
  std::array<int, 4> array_values = { { 1, 2, 3, 4 } };
  std::array<int, 4> const const_array_values = { { 1, 2, 3, 4 } };
  std::vector<int> vector_values = { 1, 2, 3, 4 };
  std::vector<int> const const_vector_values = { 1, 2, 3, 4 };
  auto same_shape = [] (auto standard, auto mine)
    {
      static_assert (decltype (standard)::extent == decltype (mine)::extent);
      static_assert (std::is_same_v<typename decltype (standard)::element_type,
                                    typename decltype (mine)::element_type>);
    };
  same_shape (std::span (values), own::span (values));
  same_shape (std::span (array_values), own::span (array_values));
  same_shape (std::span (const_array_values), own::span (const_array_values));
  same_shape (std::span (vector_values), own::span (vector_values));
  same_shape (std::span (const_vector_values),
              own::span (const_vector_values));
  same_shape (std::span (values, 2), own::span (values, 2));
  same_shape (std::span (values, values + 2), own::span (values, values + 2));
  same_shape (std::span (vector_values.begin (), vector_values.end ()),
              own::span (vector_values.begin (), vector_values.end ()));
  same_shape (std::span (std::span<int, 4> (values)),
              own::span (own::span<int, 4> (values)));
  same_shape (std::span (std::span<int> (values)),
              own::span (own::span<int> (values)));
  SUCCEED ();
}

#else // LUMEX_HAS_STD_SPAN && LUMEX_SPAN_HAS_RANGES

TEST (LumexSpanStdDifferentialTest,
      GivenLibraryWithoutStdSpan_WhenCxx20_ThenSkipped)
{
  GTEST_SKIP () << "the standard library has no std::span";
}

#endif // LUMEX_HAS_STD_SPAN && LUMEX_SPAN_HAS_RANGES
