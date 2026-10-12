# LumexSpan: a non-owning view of contiguous elements from C++11 {#lumex_span}

`lumex::core::span::view::span<T, Extent>` (target `lumex::span`, header-only, no dependency on another module of the library) is the interface of C++20 `std::span<T, Extent>` from C++11 on, on every compiler of the library, with the later additions of the standard (`const_iterator`, `cbegin`, `cend`, `crbegin`, `crend` from C++23 and `at` from C++26). The class is the module's own on every standard and is never an alias of `std::span`: the type and the overload sets that take it are the same in C++11, C++14, C++17 and C++20. From C++20 it converts implicitly to and from `std::span` (through the range constructors of both classes, and for the byte type through members), and from C++17 the two byte types convert to each other. The module also has `as_bytes` and `as_writable_bytes`, an own `byte` type, `dynamic_extent` and the trait `is_contiguous_iterator`, which recognizes the iterators of the standard containers below C++20. The preconditions of the standard (an index below `size ()`, a count within the size and the like) are checked when the macro `LUMEX_SPAN_CHECKS` is 1, which is the default in a build without `NDEBUG`. Nothing is declared at global scope.

```cpp
#include <array>
#include <cstdint>
#include <iostream>
#include <vector>

#include "lumex/core/span/LumexSpan"

using namespace lumex::core::span::view;

namespace
{
// One parameter type for every contiguous source, in C++11 and later.
std::int64_t
sum (span<std::int32_t const> values)
{
  std::int64_t total = 0;
  for (std::int32_t value : values)
    total += value;
  return total;
}
} // namespace

int
main ()
{
  std::int32_t raw[4] = { 1, 2, 3, 4 };
  std::vector<std::int32_t> vector_values = { 10, 20, 30 };
  std::array<std::int32_t, 2> array_values = { { 100, 200 } };

  std::cout << sum (raw) << ' ' << sum (vector_values) << ' '
            << sum (array_values) << '\n';

  span<std::int32_t> const window (vector_values);
  std::cout << sum (window.subspan (1)) << ' ' << sum (window.first (2)) << ' '
            << as_bytes (window).size () << '\n';
  return 0;
}
```

From `lumex/examples/span/example_span_basic.cpp`; it prints `10 60 300` and `50 30 12`. A larger tour (sources of a span, writes through it, checked access, subviews, static extents, conversions, `as_bytes`) is `example_span.cpp`; `example_span_workflow.cpp` parses frames out of one receive buffer with `first` and `subspan` and no copy. All examples are built and run by the tests (`examples.span.*`).

## Origin

The interface follows `std::span` as specified by P0122R7, with P1024 (`size_bytes`, `empty` and the like), P1976 (explicit constructors for a static extent) and LWG 3255, and the later additions listed above. The implementation is written from the text of the standard with `<span>` of libc++ and libstdc++ and `boost/core/span.hpp` as references; no code of those libraries is copied. The library keeps one class on every standard for the same reason as `optional` and `string_view`: below C++20 there is no standard type, and an overload set must mean the same in C++11 and in C++20. The C++20 concepts (`std::ranges::contiguous_range`, `sized_range`, `borrowed_range`, `std::contiguous_iterator`) that constrain the standard's constructors are replaced by member detection (below C++20) and used directly from C++20.

## Headers

| Header | Contents |
| --- | --- |
| `lumex/core/span/LumexSpan` | Umbrella: includes both headers below |
| `view/LumexSpanTraits.hpp` | `dynamic_extent`, `byte` with its operators and `to_integer`, `is_contiguous_iterator`, the configuration macros `LUMEX_SPAN_HAS_CONCEPTS`, `LUMEX_SPAN_HAS_RANGES` and `LUMEX_SPAN_HAS_STD_SPAN`, and `detail::` helpers (not interface) |
| `view/LumexSpanChecks.hpp` | The macro `LUMEX_SPAN_CHECKS` (the switch of the precondition checks), its `detect_mismatch` record for MSVC and the internal macro `LUMEX_SPAN_CHECKED` that the members use |
| `view/LumexSpanStdIterators.hpp` | The specializations of `is_contiguous_iterator` for the iterator classes of libstdc++, libc++ and the standard library of MSVC (below C++20; included by `LumexSpanTraits.hpp`) |
| `view/LumexSpan.hpp` | `span<T, Extent>`, `as_bytes`, `as_writable_bytes`, the deduction guides (C++17) and the `std::ranges::enable_borrowed_range` / `enable_view` opt-ins (C++20) |

All names are in `lumex::core::span::view`. There is no `portable_*` alias and no export macro: the module has no compiled part.

## Interface

The facts are from `view/LumexSpan.hpp` and `view/LumexSpanTraits.hpp`.

| Name | Notes |
| --- | --- |
| types | `element_type`, `value_type`, `size_type` (`std::size_t`), `difference_type`, `pointer`, `const_pointer`, `reference`, `const_reference`, `iterator` and `const_iterator` (raw pointers), `reverse_iterator`, `const_reverse_iterator`, `extent` |
| construction | default (only for extent 0 or dynamic; null pointer, size 0), iterator and count, iterator and sentinel, built-in array, `std::array` (lvalue, const), any range with a pointer-returning `data ()` and a `size ()` (including `std::initializer_list<T const>`), another `span`, and the byte spans and `std::span` listed below. For a static extent the iterator, range and `span` constructors from a dynamic source are `explicit`, as in the standard |
| subviews | `first<N> ()`, `last<N> ()`, `subspan<Offset, Count> ()` with static extents; `first (n)`, `last (n)`, `subspan (offset, count)` dynamic. Static counts are checked by `static_assert`; the dynamic ones are checked when `LUMEX_SPAN_CHECKS` is 1 (see below) |
| observers | `size`, `size_bytes`, `empty` |
| access | `operator[]`, `front`, `back` (checked when `LUMEX_SPAN_CHECKS` is 1), `data`, `at` (always checked, throws `std::out_of_range`) |
| iterators | `begin`, `end`, `cbegin`, `cend`, `rbegin`, `rend`, `crbegin`, `crend` |
| free functions | `as_bytes` (a `span<byte const, ...>`), `as_writable_bytes` (a `span<byte, ...>`), for a non-volatile element type; `to_integer<I> (byte)` |
| `byte` | An own scoped enumeration `enum class byte : unsigned char` with the operators of `std::byte`, in every standard, marked `may_alias` on GCC and Clang so that a write through a `span<byte>` is seen by reads of the viewed object. It is never an alias of `std::byte` |
| conversions | Static to dynamic and `T` to `T const` are implicit; dynamic to static is `explicit`. From C++17: `span<std::byte>` and `span<byte>` convert to each other (with the qualification rules of the standard, `std::byte const` never to `byte`). From C++20 with `<span>`: `span<byte>` converts to and from `std::span<std::byte>`, and any `span<T>` and `std::span<T>` convert through the range constructors of the two classes |
| traits | `is_contiguous_iterator<It>`: `std::contiguous_iterator` from C++20; before, true for pointers to objects and for the iterators of `std::vector` (not `std::vector<bool>`), `std::array`, `std::basic_string`, `std::basic_string_view`, `std::valarray` and `std::initializer_list` in libstdc++ and libc++, and a specialization names any other type |
| deduction (C++17) | From a pointer or iterator and a count or end, a built-in array, `std::array`, a range and another `span` |
| C++20 | `std::ranges::enable_borrowed_range` and `enable_view` are `true` for every `span` |

The element type may not be a reference or `void`. The result of every observer and subview is `nodiscard`.

## How it works

- **Layout.** A pointer, and for a dynamic extent a size: `sizeof (span<int>)` is 16 bytes and `sizeof (span<int, 4>)` is 8 on x86-64 Linux (probed at C++11 to C++20 with GCC 13.2 and GCC 8.3). The type is trivially copyable, trivially destructible and standard layout, so it is passed in registers. It is not trivially default constructible (the default constructor writes a null pointer, as `std::span`'s does).
- **Iterators are pointers.** `begin ()` returns `T *`, so range-for, `std::sort` and `std::copy` over a span are the code they are over a pointer pair. The iterator of `std::span` is a wrapper class in libstdc++ and in libc++ (probed); the benchmark's sums over it take the same time (see the comparison).
- **Precondition checks, opt in.** `LUMEX_SPAN_CHECKS` is 1 or 0. The default is 1 when `NDEBUG` is not defined and 0 when it is; a program may define it as 0 or 1 before the first include (`-DLUMEX_SPAN_CHECKS=1` keeps the checks in a release build). With 1 the preconditions of the standard are checked: `operator[]` (index below `size ()`), `front` and `back` (not empty), `first (n)`, `last (n)`, `first<N> ()`, `last<N> ()` (count within the size), `subspan (offset, count)` and `subspan<Offset, Count> ()` (offset and count within the size), and the constructors: a pointer or iterator and a count, an iterator pair, a range, a `span` or a `std::span` for a static extent need a count or size equal to the extent, and an iterator pair needs `last` not before `first`. A violation calls an inline handler of the module (`detail::check_failed` in `view/LumexSpanChecks.hpp`), which prints `LUMEX_SPAN_CHECKS: <condition> failed at <file>:<line>` to standard error and calls `std::abort`, in every build mode (`NDEBUG` included). With 0 the members are the code they were before the checks existed (the disassembly of a translation unit that uses every member is identical to that of the version without the macro, see the benchmark) and a violation is undefined behavior: a span of 4 elements over a buffer of 8 returns `s[6]`. The checks are plain expressions, so they work in constant expressions from C++11: a true condition adds nothing, a false one is not a constant expression, so a violated precondition in a `constexpr` initializer is a compile error (it is accepted when the checks are off, if the access stays inside the object). `at` is checked in both modes. Nothing else throws and nothing allocates; every observer and subview is `noexcept` except `at`, and the constructors are `noexcept`, the sentinel and range constructors conditionally on the operations they call.
- **One value per program.** The members are inline templates, so all translation units of a program must see the same `LUMEX_SPAN_CHECKS`; two that differ hand the linker two bodies of one function. MSVC reports a mismatch at link time (`#pragma detect_mismatch`), GCC and Clang do not. On the library's own compiled sources: `base64/encode/Encoder.cpp` and `crc/catalog/LumexCrcCatalog.cpp` include the span header, so with MSVC their objects carry `detect_mismatch("LUMEX_SPAN_CHECKS", "0")` in a Release build of the library, and a program built with `-DLUMEX_SPAN_CHECKS=1` that links the prebuilt `lumex::base64` or `lumex::crc` gets LNK2038; GCC and Clang do not notice (those units only call `data ()` and `size ()`, so it is harmless today). On the MSVC debug runtime `std::abort` may show a modal dialog. The handler is a header-only function of the module (not `LUMEX_ASSERT`, whose handler is compiled into `lumex::utility`, which links `lumex::span`), so a program that links only `lumex::span` builds and aborts on a violation with the checks on; it is not `constexpr`, so a violation in a constant expression is a compile error.
- **`constexpr`.** The constructors, observers, element access, `begin`/`end` and the subviews are `constexpr` from C++11 (probed with `static_assert` at C++11, 14, 17 and 20: `size`, `operator[]`, `front`, `back`, `data`, `first (n)`, `last (n)`, `subspan (o, n)`, `first<N>`, `subspan<O>` and `begin () + size () == end ()`); a loop over a span in a `constexpr` function is C++14 and later, as the language requires.
- **Which sources convert.** A range converts when it has a pointer-returning `data ()` and a `size ()`, is not an array, `std::array` or `span`, is an lvalue or is viewed with `T const` elements, and its element type differs from `T` by qualification only (probes: `std::vector<int>&` to `span<int>` yes; a `std::vector<int>` rvalue to `span<int>` no, to `span<int const>` yes, as for `std::span`; `std::vector<bool>`, `std::deque`, `std::list` no; `std::string&` to `span<char>` from C++17 only, because `data ()` of a non-const `std::string` returns `char *` from C++17; `span<Derived>` to `span<Base>` no; `span<int>` to `span<long>` no). A temporary viewed through `span<T const>` dangles when the full expression ends, as it does with `std::span`.
- **Iterators below C++20.** The language cannot tell the iterator of a `std::vector` from the iterator of a `std::deque`, so below C++20 the library names the iterator classes that wrap a pointer, one standard library at a time (`view/LumexSpanStdIterators.hpp`): `__gnu_cxx::__normal_iterator` of libstdc++ (`std::vector`, `std::basic_string`), `std::__wrap_iter`, `std::__bounded_iter` and `std::__static_bounded_iter` of libc++ (`std::vector`, `std::basic_string`, `std::array`, `std::basic_string_view`, and the hardened modes), and the vector, string and array iterators of the standard library of MSVC. A wrapper is contiguous exactly when what it wraps is, and each of them moves a pointer by pointer arithmetic, which is what the trait promises. So `span<int> (v.begin (), v.end ())` and `span<int> (v.begin (), n)` compile at C++11, 14 and 17 for the iterators of `std::vector` (not `std::vector<bool>`), `std::array`, `std::basic_string` and `std::basic_string_view`, and for the pointers that `std::valarray` and `std::initializer_list` give. The iterators of `std::vector<bool>`, `std::deque`, `std::list`, reverse and move iterators, the checked iterators of `_GLIBCXX_DEBUG` and any iterator class of a program stay not contiguous (probed with tests: a user class with the category `random_access_iterator_tag` is rejected) until the program names them with a specialization of `is_contiguous_iterator`, which is a promise that `*it` and `*(it + n)` are `n` elements apart (breaking it is undefined behavior) and also overrides the detection. From C++20 the answer is `std::contiguous_iterator`. An empty range given by iterators has a null `data ()` below C++20 and `std::to_address` of the iterator from C++20.
- **Conversions with `std::span`.** A dynamic `std::span<T>` converts to `span<T>` and back implicitly; a `std::span<T, N>` converts to `span<T>` implicitly; the static-extent pairs follow the standard's `explicit` rules (probes: `std::span<int, 3>` to `span<int, 3>` and `span<int, 3>` to `std::span<int, 3>` are explicit; `span<int, 3>` to `std::span<int>` is implicit). Because both classes are separate types with converting constructors, a call `f (v)` with `f` overloaded for `std::span<int const>` and for `span<int const>` is ambiguous for a `std::vector<int>`, for a `span<int>` and for a `std::span<int>` alike (probed); an exact `std::span<int const>` or `span<int const>` argument picks its own overload, so the caller names the type: `f (span<int const> (v))` or `f (std::span<int const> (v))` (the example below).
- **The byte type.** `as_bytes` returns `span<byte const, N * sizeof (T)>` with the library's `byte`, so the result is the same type on every standard and `std::as_bytes` of a `std::span` is not applicable to it. Elements convert between the two byte types by `static_cast` only (an enumeration has no conversion functions), spans convert implicitly: `std::span<std::byte const>` to `span<byte>` is rejected (it would drop `const`).
- **Threads.** Concurrent reads of one span are safe; the viewed elements are the caller's to synchronize. Assigning one span from two threads is a data race as for any object.
- **Contract.** The span does not own the elements, which must outlive it. `dynamic_extent` is the largest `std::size_t`, as in the standard. There is no `operator==` and no `std::hash`, as for `std::span`.

Standard-library behavior is not mirrored where C++20 gives none: `std::span` has no `at` before C++26, the library provides it from C++11 and it throws `std::out_of_range` with the message `lumex::span::at: index out of range`.

## Conversions with std::span and the iterator trait

Conversions in both directions (C++20, `lumex/examples/span/example_span_std_conversion.cpp`; the file prints a note and returns 0 where `<span>` is missing; output `sum=108 back=6`, `std::span<std::byte const> size=24 count_bytes=24`, `loose=6 exact=6`, `overloads=12`; the last line resolves a call that is ambiguous for a container argument by naming the type):

```cpp
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>
#if defined(__has_include)
#if __has_include(<span>)
#include <span>
#endif
#endif

#include "lumex/core/span/LumexSpan"

using namespace lumex::core::span::view;

#if LUMEX_SPAN_HAS_STD_SPAN
namespace
{
// An interface that takes the standard type ...
std::int64_t
total (std::span<std::int32_t const> values)
{
  std::int64_t sum = 0;
  for (std::int32_t value : values)
    sum += value;
  return sum;
}

// ... and one that takes the library's.
std::size_t
count_bytes (span<byte const> bytes)
{
  return bytes.size ();
}

// A function overloaded for both spans: a container converts to either, so
// the call describe (values) is ambiguous and does not compile.
int
describe (std::span<std::int32_t const>)
{
  return 1;
}

int
describe (span<std::int32_t const>)
{
  return 2;
}
} // namespace
#endif

int
main ()
{
#if LUMEX_SPAN_HAS_STD_SPAN
  std::vector<std::int32_t> values = { 4, 8, 15, 16, 23, 42 };
  span<std::int32_t> own (values);

  std::int64_t const sum = total (own); // span to std::span: implicit
  std::span<std::int32_t> standard (values);
  span<std::int32_t const> back = standard; // std::span to span: implicit
  std::cout << "sum=" << sum << " back=" << back.size () << '\n';

  // The byte views convert to the std::byte spans and back.
  std::span<std::byte const> raw = as_bytes (own);
  std::cout << "std::span<std::byte const> size=" << raw.size ()
            << " count_bytes=" << count_bytes (std::as_bytes (standard))
            << '\n';

  // A static extent follows the standard's rules: implicit to a dynamic span,
  // explicit from a dynamic one.
  span<std::int32_t, 6> fixed (values.data (), 6);
  std::span<std::int32_t> loose = fixed;
  std::span<std::int32_t, 6> exact (own);
  std::cout << "loose=" << loose.size () << " exact=" << exact.size () << '\n';

  // Resolving the ambiguity: name the type the call means.
  std::cout << "overloads="
            << describe (std::span<std::int32_t const> (values))
            << describe (span<std::int32_t const> (values)) << '\n';
#else
  std::cout << "no std::span in this build (C++20 with <span> is needed)\n";
#endif
  return 0;
}
```

Iterators below C++20 (`lumex/examples/span/example_span_iterators.cpp`, built at C++11; output `pointers=5 iterators=6 count=3`, `empty size=0 null=1`, `user=3 first=1`; the iterators of the standard containers need no specialization, the program's own `cursor` does):

```cpp
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <type_traits>
#include <vector>

#include "lumex/core/span/LumexSpan"

namespace example
{
// A program's own iterator over contiguous storage: a pointer with a name.
class cursor
{
public:
  using iterator_category = std::random_access_iterator_tag;
  using value_type = std::int32_t;
  using difference_type = std::ptrdiff_t;
  using pointer = std::int32_t *;
  using reference = std::int32_t &;

  cursor () : m_current (nullptr) {}
  explicit cursor (std::int32_t *current) : m_current (current) {}

  reference
  operator* () const
  {
    return *m_current;
  }
  pointer
  operator->() const
  {
    return m_current;
  }
  reference
  operator[] (difference_type n) const
  {
    return m_current[n];
  }
  cursor &
  operator++ ()
  {
    ++m_current;
    return *this;
  }
  cursor
  operator++ (int)
  {
    cursor old (*this);
    ++m_current;
    return old;
  }
  cursor &
  operator-- ()
  {
    --m_current;
    return *this;
  }
  cursor
  operator-- (int)
  {
    cursor old (*this);
    --m_current;
    return old;
  }
  cursor &
  operator+= (difference_type n)
  {
    m_current += n;
    return *this;
  }
  cursor &
  operator-= (difference_type n)
  {
    m_current -= n;
    return *this;
  }
  friend cursor
  operator+ (cursor it, difference_type n)
  {
    return it += n;
  }
  friend cursor
  operator+ (difference_type n, cursor it)
  {
    return it += n;
  }
  friend cursor
  operator- (cursor it, difference_type n)
  {
    return it -= n;
  }
  friend difference_type
  operator- (cursor a, cursor b)
  {
    return a.m_current - b.m_current;
  }
  friend bool
  operator== (cursor a, cursor b)
  {
    return a.m_current == b.m_current;
  }
  friend bool
  operator!= (cursor a, cursor b)
  {
    return a.m_current != b.m_current;
  }
  friend bool
  operator< (cursor a, cursor b)
  {
    return a.m_current < b.m_current;
  }
  friend bool
  operator> (cursor a, cursor b)
  {
    return a.m_current > b.m_current;
  }
  friend bool
  operator<= (cursor a, cursor b)
  {
    return a.m_current <= b.m_current;
  }
  friend bool
  operator>= (cursor a, cursor b)
  {
    return a.m_current >= b.m_current;
  }

private:
  std::int32_t *m_current;
};
} // namespace example

// The iterators of std::vector, std::array, std::basic_string and the like
// are recognized by the library in every standard. The iterator class of a
// program is not: before C++20 the language cannot tell a contiguous iterator
// from a deque's, so the program names it. The line below is a promise that
// *it and *(it + n) are n elements apart. From C++20 the answer is
// std::contiguous_iterator, so the line is redundant only for an iterator that
// models it (one with iterator_concept = std::contiguous_iterator_tag and an
// element_type); the class below declares neither, so it needs the line at
// every standard.
namespace lumex
{
namespace core
{
namespace span
{
namespace view
{
template <> struct is_contiguous_iterator<example::cursor> : std::true_type
{
};
} // namespace view
} // namespace span
} // namespace core
} // namespace lumex

using namespace lumex::core::span::view;
using example::cursor;

int
main ()
{
  std::vector<std::int32_t> values = { 3, 1, 4, 1, 5, 9, 2, 6 };

  span<std::int32_t> const by_pointers (values.data (), values.data () + 5);
  span<std::int32_t> const by_iterators (values.begin () + 2, values.end ());
  span<std::int32_t> const by_count (values.begin (), 3);
  std::cout << "pointers=" << by_pointers.size ()
            << " iterators=" << by_iterators.size ()
            << " count=" << by_count.size () << '\n';

  // An empty range from an iterator pair has a null pointer below C++20,
  // because the iterator of an empty range is not dereferenced.
  std::vector<std::int32_t> empty;
  span<std::int32_t> const none (empty.begin (), empty.end ());
  std::cout << "empty size=" << none.size ()
            << " null=" << (none.data () == nullptr) << '\n';

  // The program's own iterator, named above.
  span<std::int32_t> const by_cursor (cursor (values.data () + 1), 3);
  std::cout << "user=" << by_cursor.size () << " first=" << by_cursor.front ()
            << '\n';
  return 0;
}
```

## Comparison with existing solutions

Measured by [benchmarks/span](../../../benchmarks/span/README.md) against `std::span` of libstdc++ 13.2.0 (GCC 13.2.0) and of libc++ (Clang 23.1.0), and `boost::span` of Boost.Core 1.92.0 (headers only, C++11 and C++20), `-O2 -DNDEBUG`, Intel Core i7-12700K pinned to one core, 5 passes of 15 repetitions; the benchmark has 39 scenarios (36 common to all implementations). The span of this library is measured with its precondition checks off (`lumex`) and on (`lumex checks`). Other spans in wide use were looked for and are not installed on this machine, so they were not measured and nothing of them is vendored: `gsl::span` (Microsoft GSL), `absl::Span` (Abseil), `tcb::span`, `nonstd::span-lite`.

| | `lumex::...::span` | `std::span` | `boost::span` |
| --- | --- | --- | --- |
| Standard floor | C++11, one class on every standard | C++20 | C++11 |
| Header-only | yes | yes (the standard library) | yes |
| `sizeof` dynamic / static `int` | 16 / 8 | 16 / 8 | 16 / 8 (all probed with GCC 13.2 and Clang 23.1) |
| Iterator type | `T *` | a wrapper class in libstdc++ and in libc++ (`std::span<int>::iterator` is not `int *`, probed) | `T *` (probed) |
| Precondition checks | `LUMEX_SPAN_CHECKS`: on by default without `NDEBUG`, a violation prints the condition, file and line and aborts in every build mode, with a handler of the header; off, the code is the unchecked code | libstdc++: with `_GLIBCXX_ASSERTIONS` (run); libc++: not probed | `BOOST_ASSERT` (off with `NDEBUG`; read from the header) |
| `at` | yes (from C++11) | C++26 | no |
| `const_iterator`, `cbegin`, `crbegin` | yes | C++23 | yes |
| Iterator + count / iterator + end | pointers; the iterators of the standard containers (libstdc++, libc++, MSVC STL) by name, other iterators by trait (C++11 to 17); `std::contiguous_iterator` (C++20) | C++20, contiguous iterators | pointers only (`I *`) |
| `enable_borrowed_range`, `enable_view` | yes (C++20) | yes | no (no opt-in in the header) |
| `as_bytes` element | the library's `byte` (every standard) | `std::byte` | `std::byte`, only where `std::byte` exists (C++17) |
| Converts to / from `std::span` | yes (C++20, both directions) | n/a | lvalues convert in both directions: `boost::span<int>&` to and from `std::span<int>` (C++20) and to and from the span of this library (also at C++11); rvalues do not convert (probed with GCC 13.2 and Clang 23.1) |
| Deduction guides | C++17 | C++17 | where `__cpp_deduction_guides` |

Run time, GCC 13.2.0 / libstdc++, C++20 columns, ns per operation (best of 5 passes; `lumex checks` is the span of this library with `LUMEX_SPAN_CHECKS=1`; the full tables with every row and the ratios are in [results/span_benchmark.md](../../../benchmarks/span/results/span_benchmark.md)):

| Scenario | lumex | lumex checks | `std::span` | `boost::span` |
| --- | --- | --- | --- | --- |
| sum with range-for, 1 000 000 ints | 241 339 | 241 128 | 241 802 | 240 341 |
| sum with `operator[]`, 1 000 000 ints | 241 970 | 240 858 | 240 813 | 240 240 |
| sum of `as_bytes` of 1 000 000 ints | 953 183 | 946 346 | 953 314 | 958 548 |
| `first (size / 2)` | 0.50 | 0.60 | 0.50 | 0.50 |
| `subspan (size / 4, size / 2)` | 0.50 | 0.73 | 0.50 | 0.50 |
| pass by value to a `noinline` function | 0.50 | 0.50 | 0.50 | 0.51 |
| recursive sum on halves, 1 024 ints | 2 991 | 2 776 | 3 578 | 3 583 |
| construct from pointer and count | 0.31 | 0.30 | 0.30 | 0.30 |
| construct from a `std::array` | 0.25 | 0.26 | 0.26 | 0.26 |
| `span<int, 16>` to `span<int>` | 0.30 | 0.30 | 0.30 | 0.30 |
| `span<int>` to `span<int, 16>` (explicit) | 0.17 | 0.30 | 0.17 | 0.17 |
| `span<int>` to `std::span<int>` / `std::span<int>` to `span<int>` | 0.31 / 0.30 | | n/a | n/a |

Clang 23.1.0 / libc++, C++20: the same sums are 98 188, 101 640 and 495 422 for lumex against 101 248, 101 324 and 492 517 for `std::span`; `first`, `subspan` and the pass 0.50, 0.50, 0.50 against 0.49, 0.50, 0.50; the recursive sum 1 786 against 2 278; construction from pointer and count 0.25 against 0.26; `span<int, 16>` to `span<int>` 0.31 against 0.30; the conversions to and from `std::span` 0.24 and 0.25. With the checks on (`lumex checks`) Clang gives `first` 0.56, `subspan` 0.74, the explicit `span<int>` to `span<int, 16>` 0.25 against 0.23, and the same as lumex elsewhere.

What the numbers mean. The loops of the scenarios compile to the same instructions in all implementations: the object files of the benchmark's translation unit were disassembled for every variant and the `run_*` and `consume_*` functions compared one by one (30 of them in `lumex`, `std::span` and `boost::span` at C++20: 27 equal to `boost::span` with both toolchains and 23 (GCC) and 24 (Clang) equal to `std::span`; the others differ in the operands of one `cmp`, two exchanged `punpck`, register names or the order of two stores, and in one extra `lea` of Clang's `last`; the three conversions to and from `std::span` have no counterpart). Where a row differs between executables (`sum_range_for` of 8 elements with GCC takes 1.95 ns in `lumex C++11`, 2.99 ns in `lumex C++20` and 2.60 ns in `std::span`, all the same instructions) the code is the same and the difference follows code alignment and the stack address, not the span; earlier runs traced part of this with every function aligned to 64 bytes (`-falign-functions=64`) and with the size of the environment, which moves the stack (Clang's `static_to_dynamic` took between 0.43 and 0.71 ns in one executable). Why the stack address matters was not established; it is a hypothesis that the aligned reload of the stored pair depends on it. Over the 36 common scenarios the geometric mean of the time of the span of this library at C++20 over `std::span` at C++20 is 0.991 with GCC and 0.979 with Clang (single rows range from 0.78x to 1.42x with GCC and 0.78x to 1.06x with Clang, mostly sub-nanosecond loops), over `boost::span` at C++20 0.993 and 0.982, and lumex C++11 over `boost::span` C++11 1.008 and 1.015. The conversions to and from `std::span` have no ratio (the other implementations have no such pair); each is a copy of two words in the generated code. Run time is therefore a wash in these scenarios, in both directions, and a single sub-nanosecond row must not be read as a property of a span.

The precondition checks. With `LUMEX_SPAN_CHECKS=0` the generated code is the code of the span without the checks: a translation unit that calls every member and constructor behind `noinline` functions gave identical `objdump -d` output in 18 of 18 configurations (GCC 13.2 and Clang 23.1 with libc++, C++11, 17, 20, `-O0`, `-O2`, `-O3`, each with `NDEBUG` and with `-DLUMEX_SPAN_CHECKS=0`) against the header from before the checks were added. With the checks on, 25 of the 32 `run_*` and `consume_*` functions of the benchmark are unchanged; the seven that differ contain a check, which is a `cmp` and a conditional jump to a cold call of the `noreturn` handler. In time a check costs 0.0 to 0.24 ns per call that has one at C++20 (best pass, GCC 13.2 / Clang 23.1): `first` 0.50 to 0.60 / 0.50 to 0.56 ns, `last` 0.60 to 0.74 / 0.56 to 0.65, `subspan` 0.50 to 0.73 / 0.50 to 0.74, `subspan` to the end 0.64 to 0.74 / 0.60 to 0.64, `first<N>` of a dynamic span 0.30 to 0.40 / 0.33 to 0.40, the explicit `span<int>` to `span<int, 16>` 0.17 to 0.30 / 0.23 to 0.25, `front` and `back` 0.33 to 0.34 / 0.35 to 0.34; the sums, constructors and the other 25 scenarios are the same code and their ratios (the geometric mean of `lumex checks` over `lumex` of the identical rows is 1.000 with GCC and 1.022 with Clang) are placement.

Compile time (`-fsyntax-only -O0`, a unit that includes the header and uses the whole API, over the same unit with a stub instead of a span; median of 7; differs by 5 to 10 % from run to run): GCC 13.2 at C++11 +80 ms for this library and +77 ms for `boost::span`; at C++20 +236 ms for this library, +22 ms for `std::span` and +114 ms for `boost::span`. Clang 23.1.0 at C++11 +27 ms and +21 ms; at C++20 +145 ms, +58 ms for `std::span` and +53 ms for Boost (the machine was faster in this run than in the one before, so the absolute numbers are about a third lower; the ratios between the columns are the same). At C++20 the header costs +223 ms with GCC and +136 ms with Clang after `<array>` and `<vector>`, and including `<memory>` and `<ranges>` alone costs +218 ms and +126 ms: those two headers (`std::to_address`, and the `enable_borrowed_range` and `enable_view` opt-ins) are the cost, while `<span>` is +3 ms and +2 ms (a separate run of one unit per header, [header_cost.csv](../../../benchmarks/span/results/header_cost.csv); the other figures are in compile_time.csv). The precondition checks and the iterator detection added nothing measurable (measured with the variant that forces the checks off, `BENCH_SPAN_IMPL=1`; the compile time with the checks on was not measured) (+211 and +130 ms for the header before, inside the 5 to 10 % of the run-to-run spread). Code size: the benchmark's unit compiled with `-O2 -c` folds to a constant for every implementation (with GCC `size -A` shows 0 bytes of `.text` and 72 or 77 bytes of `.text.startup`, which is `main`, and with Clang 3 bytes of `.text`; the same for the span and for the stub; measured before the checks were added and not repeated), so there is no code-size row: a header-only span of this kind produces no code of its own in these scenarios.

## Strengths and weaknesses

Strengths:
- One class on every standard from C++11: the same type and overload sets in C++11, 14, 17 and 20, with `const_iterator` and `at` that the standard adds in C++23 and C++26.
- `sizeof` is 16 bytes (8 for a static extent, which stores no size), trivially copyable, iterators are pointers; run time equal to `std::span` and `boost::span` in 36 scenarios on GCC 13.2 and Clang 23.1 (identical loops; geometric mean 0.98 to 1.02 of the time), also with the checks on in the scenarios that have none (a compare and a branch, 0.1 to 0.24 ns, where one runs).
- Implicit conversions with `std::span` in both directions from C++20, and between the byte spans from C++17, at the cost of a copy of two words.
- Observers, element access and subviews are `constexpr` from C++11, with their precondition checks: a violated one in a constant expression is a compile error.
- The preconditions of the standard are checked (`LUMEX_SPAN_CHECKS`, on by default without `NDEBUG`), abort with the condition, the file and the line, and cost nothing when off.
- The iterators of `std::vector`, `std::array`, `std::basic_string` and `std::basic_string_view` make a span at C++11 to 17 without a specialization.
- A span of `std::vector<bool>`, `std::deque`, `std::list`, an rvalue `std::vector<int>` to `span<int>`, `span<long>` from `span<int>`, and `span<Base>` from `span<Derived>` are rejected at compile time (34 further rejected uses are in the compile checks).

Weaknesses:
- Compile time at C++20: +236 ms per translation unit with GCC 13.2 (`std::span` +22 ms, `boost::span` +114 ms), +145 ms with Clang 23.1 (+58 ms, +53 ms), from `<memory>` and `<ranges>`. At C++11 it is +80 ms and +27 ms, equal to Boost.
- Every translation unit of a program must agree on the value (MSVC reports a mismatch, GCC and Clang do not). A check costs a compare and a branch where it runs (0.1 to 0.24 ns in the benchmark).
- At C++11, 14 and 17 an iterator class of a program (not one of the standard containers named in `LumexSpanStdIterators.hpp`) is rejected until `is_contiguous_iterator` is specialized for its type; the specialization is an unchecked promise. The iterators of `std::vector<bool>`, `std::deque`, `std::list`, the checked iterators of libstdc++'s debug mode and (untested here) any MSVC STL iterator not named in the header are not detected.
- A separate type from `std::span`: code that is overloaded for both is ambiguous for a container argument, and a template that deduces `std::span<T, N>` does not accept it. The conversion is implicit only for dynamic extents.
- `byte` is not `std::byte`: `as_bytes` of this library returns spans of the own type, `std::as_bytes` does not take it, and element conversion needs `static_cast`.
- Not tested on MSVC (the CHANGELOG records MinGW compile checks only), and the conversions with `std::span` need `<span>`, so GCC 8 at `-std=c++2a` has none.
- A temporary container viewed through `span<T const>` dangles as with `std::span`; the library does not guard it.

## Testing

On GCC 13.2.0 Release the span suites, the 5 examples (`examples.span.*`) and the 3 `cmake.*` tests (`cmake.wiring_span`, `cmake.require_fail_utility_without_span`, `cmake.span_compile_checks`) pass: 513 tests at C++11 and 699 at C++20 (each with the main, `checks_on` and `checks_off` suites; the numbers include the examples), and the whole `cmake.*` and `lint.*` set of the tree (193 tests) passes. The suites are `span.` and `span.view.`, files `LumexSpanConstruction`, `LumexSpanObservers`, `LumexSpanSubviews`, `LumexSpanBytes`, `LumexSpanConstexpr`, `LumexSpanTraits`, `LumexSpanStdIterators` (the iterator detection: every standard container, the wrappers of libstdc++ and libc++ by name, the negative cases, the spans made from them), `LumexSpanChecks` (every precondition check at its border and one past it, as death tests, and the same calls not aborting with the checks off), `LumexSpanGlobalNames` at C++11; `LumexSpan`, `LumexSpanByteTwin`, `LumexSpanStdIterators` (`std::string_view`) and `LumexSpanChecksBytes` at C++17; `LumexSpanRanges`, `LumexSpanStdDifferential`, `LumexSpanByteTwin` and `LumexSpanChecksStdSpan` at C++20. Every suite exists three ways: with the checks of the build (`.cxx11`, `.cxx17`, `.cxx20`: off in a Release build, on in a Debug one), forced on (`.checks_on.cxx11`, `.checks_on.cxx20`) and forced off (`.checks_off.cxx11`, `.checks_off.cxx20`). The existing tests all pass with the checks on, so none of them relied on an unchecked violation, and the tests of `base64`, `crc`, `utility.mem` and `hazard_pointer.holder` built at C++11 and C++20 (1681 tests; the examples of those modules were not built) pass in a build with `-DLUMEX_SPAN_CHECKS=1`. The same `ctest -R span` passes on GCC 8.3 (513 at C++11; 192 at C++17 and 586 at C++2a before the last round, where `<span>` is missing and the `std::span` tests skip), Clang 23.1 with libc++ (699 at C++20) and with libstdc++, under Clang with UndefinedBehaviorSanitizer (513 at C++11, the checks-off suites included, which execute no undefined behavior), and under AddressSanitizer and UndefinedBehaviorSanitizer (GCC 13.2 Debug: 520 at C++11 and 706 at C++20, so the checks are on by default). The differential tests compare `span` with `std::span` at C++20 on the constructibility and implicitness of a table of combinations of element types, extents and sources, the member types, the types and values of the subviews and `as_bytes`. `cmake.span_compile_checks` compiles 34 uses that `std::span` rejects (a discarded result, a constructor the constraints remove, a failing `static_assert`, an assignment to a const element, invalid element types) at C++11, C++17 and C++20, and they must not compile; it also compiles 14 violated preconditions in constant expressions, which must not compile with the checks on and must compile with them off (so the check is the only reason), the value of `LUMEX_SPAN_CHECKS` with and without `NDEBUG` and when the program defines it, and preprocesses the checks header as MSVC would to see that `detect_mismatch` records the value. `cmake.install_header_only_without_utility` installs the library without the utility module, links a program against `lumex::span` alone with the checks forced on, and runs it twice: once normally and once with a violated check, which must abort with `LUMEX_SPAN_CHECKS: index < m_storage.size () failed at ...`. The death tests pin the message format. The CHANGELOG entry of the module (`[v2.0.0.0]`) records the matrix run when the module was added: GCC 8.3, GCC 13.2, Clang 23.1 with libc++ and libstdc++, ASan and UBSan, a warning-free build on all of them and MinGW, and 75 deliberate corruptions of the code of which 72 were caught. For the checks and the iterator detection 36 deliberate corruptions were made (an off-by-one or a wrong comparison in each check, a handler that returns instead of aborting or ends with `exit (1)`, a changed message format, the default of the macro, the override by the program, the `detect_mismatch` values, the constexpr behavior, each iterator family, the negative cases): all were caught, by the named tests. The part of `LumexSpanStdIterators.hpp` for the standard library of MSVC is written from the declarations of its headers and was not compiled (no MSVC here); for libc++ with the bounded iterators (`_LIBCPP_ABI_BOUNDED_ITERATORS*`, where `std::vector<int>::iterator` is `std::__bounded_iter<int *>`) a small program was compiled and run by hand at C++17 and C++20, outside the suites; `_LIBCPP_HARDENING_MODE` alone does not change the iterator types. The benchmark checks its own work: each scenario's result is compared with a raw-pointer reference, and a corrupted `to_std_span` (one element too many) stops it (`checksum 8200, the raw-pointer reference 8192`).

## Not done on purpose / limits / known issues

- Fixed: precondition checks. They are opt in by the macro `LUMEX_SPAN_CHECKS` (on by default without `NDEBUG`), abort through a header-only handler of the module (no dependency on `lumex::utility`, which links `lumex::span`) and cost nothing when off. There is no separate hardening mode.
- Fixed: `is_contiguous_iterator` before C++20 recognizes the iterators of the standard containers of libstdc++ and libc++ (the part for MSVC is not compiled here); an iterator class of a program still needs a specialization, on purpose. The iterators of the checked debug mode of libstdc++ (`_GLIBCXX_DEBUG`) are not detected.
- The C++20 header cost comes from `<memory>` and `<ranges>`; reported, not changed.
- The static-extent conversions with `std::span` are explicit in the directions the standard makes explicit; there is no conversion from `span<int>` to a `std::span<int, N>`.
- A function overloaded for both `std::span` and this `span` is ambiguous for a container argument; the caller names the type (see the example above).
- Only exactly the two byte types convert; `std::span<unsigned char>` from a `span<byte>` is rejected (probe).
- Windows and MSVC: not tested. The MSVC part of the iterator detection is not compiled by any test here, and the `detect_mismatch` record is checked only by preprocessing the header as MSVC would.
