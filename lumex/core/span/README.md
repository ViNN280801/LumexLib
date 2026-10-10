# LumexSpan: a non-owning view of contiguous elements from C++11 {#lumex_span}

`lumex::core::span::view::span<T, Extent>` (target `lumex::span`, header-only, no dependency on another module of the library) is the interface of C++20 `std::span<T, Extent>` from C++11 on, on every compiler of the library, with the later additions of the standard (`const_iterator`, `cbegin`, `cend`, `crbegin`, `crend` from C++23 and `at` from C++26). The class is the module's own on every standard and is never an alias of `std::span`: the type and the overload sets that take it are the same in C++11, C++14, C++17 and C++20. From C++20 it converts implicitly to and from `std::span` (through the range constructors of both classes, and for the byte type through members), and from C++17 the two byte types convert to each other. The module also has `as_bytes` and `as_writable_bytes`, an own `byte` type, `dynamic_extent` and the trait `is_contiguous_iterator`. Nothing is declared at global scope.

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
| `view/LumexSpan.hpp` | `span<T, Extent>`, `as_bytes`, `as_writable_bytes`, the deduction guides (C++17) and the `std::ranges::enable_borrowed_range` / `enable_view` opt-ins (C++20) |

All names are in `lumex::core::span::view`. There is no `portable_*` alias and no export macro: the module has no compiled part.

## Interface

The facts are from `view/LumexSpan.hpp` and `view/LumexSpanTraits.hpp`.

| Name | Notes |
| --- | --- |
| types | `element_type`, `value_type`, `size_type` (`std::size_t`), `difference_type`, `pointer`, `const_pointer`, `reference`, `const_reference`, `iterator` and `const_iterator` (raw pointers), `reverse_iterator`, `const_reverse_iterator`, `extent` |
| construction | default (only for extent 0 or dynamic; null pointer, size 0), iterator and count, iterator and sentinel, built-in array, `std::array` (lvalue, const), any range with a pointer-returning `data ()` and a `size ()` (including `std::initializer_list<T const>`), another `span`, and the byte spans and `std::span` listed below. For a static extent the iterator, range and `span` constructors from a dynamic source are `explicit`, as in the standard |
| subviews | `first<N> ()`, `last<N> ()`, `subspan<Offset, Count> ()` with static extents; `first (n)`, `last (n)`, `subspan (offset, count)` dynamic. Static counts are checked by `static_assert`; the dynamic ones are not checked |
| observers | `size`, `size_bytes`, `empty` |
| access | `operator[]`, `front`, `back`, `data` (unchecked), `at` (throws `std::out_of_range`) |
| iterators | `begin`, `end`, `cbegin`, `cend`, `rbegin`, `rend`, `crbegin`, `crend` |
| free functions | `as_bytes` (a `span<byte const, ...>`), `as_writable_bytes` (a `span<byte, ...>`), for a non-volatile element type; `to_integer<I> (byte)` |
| `byte` | An own scoped enumeration `enum class byte : unsigned char` with the operators of `std::byte`, in every standard, marked `may_alias` on GCC and Clang so that a write through a `span<byte>` is seen by reads of the viewed object. It is never an alias of `std::byte` |
| conversions | Static to dynamic and `T` to `T const` are implicit; dynamic to static is `explicit`. From C++17: `span<std::byte>` and `span<byte>` convert to each other (with the qualification rules of the standard, `std::byte const` never to `byte`). From C++20 with `<span>`: `span<byte>` converts to and from `std::span<std::byte>`, and any `span<T>` and `std::span<T>` convert through the range constructors of the two classes |
| traits | `is_contiguous_iterator<It>`: `std::contiguous_iterator` from C++20; before, true for pointers to objects only, and a specialization names any other type |
| deduction (C++17) | From a pointer or iterator and a count or end, a built-in array, `std::array`, a range and another `span` |
| C++20 | `std::ranges::enable_borrowed_range` and `enable_view` are `true` for every `span` |

The element type may not be a reference or `void`. The result of every observer and subview is `nodiscard`.

## How it works

- **Layout.** A pointer, and for a dynamic extent a size: `sizeof (span<int>)` is 16 bytes and `sizeof (span<int, 4>)` is 8 on x86-64 Linux (probed at C++11 to C++20 with GCC 13.2 and GCC 8.3). The type is trivially copyable, trivially destructible and standard layout, so it is passed in registers. It is not trivially default constructible (the default constructor writes a null pointer, as `std::span`'s does).
- **Iterators are pointers.** `begin ()` returns `T *`, so range-for, `std::sort` and `std::copy` over a span are the code they are over a pointer pair. The iterator of `std::span` is a wrapper class in libstdc++ and in libc++ (probed); the benchmark's sums over it take the same time (see the comparison).
- **No checks.** `operator[]`, `front`, `back`, `first`, `last` and `subspan` have no precondition checks in any build mode: an out-of-range access is undefined behavior and nothing diagnoses it (probe: a span of 4 elements over a buffer of 8 returns `s[6]`; the same call on `std::span` with `-D_GLIBCXX_ASSERTIONS` aborts). Only `at` checks. Nothing else throws and nothing allocates; every observer and subview is `noexcept` except `at`, and the constructors are `noexcept`, the sentinel and range constructors conditionally on the operations they call.
- **`constexpr`.** The constructors, observers, element access, `begin`/`end` and the subviews are `constexpr` from C++11 (probed with `static_assert` at C++11, 14, 17 and 20: `size`, `operator[]`, `front`, `back`, `data`, `first (n)`, `last (n)`, `subspan (o, n)`, `first<N>`, `subspan<O>` and `begin () + size () == end ()`); a loop over a span in a `constexpr` function is C++14 and later, as the language requires.
- **Which sources convert.** A range converts when it has a pointer-returning `data ()` and a `size ()`, is not an array, `std::array` or `span`, is an lvalue or is viewed with `T const` elements, and its element type differs from `T` by qualification only (probes: `std::vector<int>&` to `span<int>` yes; a `std::vector<int>` rvalue to `span<int>` no, to `span<int const>` yes, as for `std::span`; `std::vector<bool>`, `std::deque`, `std::list` no; `std::string&` to `span<char>` from C++17 only, because `data ()` of a non-const `std::string` returns `char *` from C++17; `span<Derived>` to `span<Base>` no; `span<int>` to `span<long>` no). A temporary viewed through `span<T const>` dangles when the full expression ends, as it does with `std::span`.
- **Iterators below C++20.** The language cannot tell the iterator of a `std::vector` from the iterator of a `std::deque`, so at C++11, 14 and 17 `span<int> (v.begin (), v.end ())` and `span<int> (v.begin (), n)` do not compile for a `std::vector<int>::iterator` (probed); pointers always work, and any other contiguous iterator type is named by a specialization of `is_contiguous_iterator`, which is a promise that `*it` and `*(it + n)` are `n` elements apart (breaking it is undefined behavior). From C++20 the answer is `std::contiguous_iterator` and the specialization is not needed. An empty range given by iterators has a null `data ()` below C++20 and `std::to_address` of the iterator from C++20.
- **Conversions with `std::span`.** A dynamic `std::span<T>` converts to `span<T>` and back implicitly; a `std::span<T, N>` converts to `span<T>` implicitly; the static-extent pairs follow the standard's `explicit` rules (probes: `std::span<int, 3>` to `span<int, 3>` and `span<int, 3>` to `std::span<int, 3>` are explicit; `span<int, 3>` to `std::span<int>` is implicit). Because both classes are separate types with converting constructors, a call `f (v)` with `f` overloaded for `std::span<int const>` and for `span<int const>` is ambiguous for a `std::vector<int>`, for a `span<int>` and for a `std::span<int>` alike (probed); an exact `std::span<int const>` or `span<int const>` argument picks its own overload.
- **The byte type.** `as_bytes` returns `span<byte const, N * sizeof (T)>` with the library's `byte`, so the result is the same type on every standard and `std::as_bytes` of a `std::span` is not applicable to it. Elements convert between the two byte types by `static_cast` only (an enumeration has no conversion functions), spans convert implicitly: `std::span<std::byte const>` to `span<byte>` is rejected (it would drop `const`).
- **Threads.** Concurrent reads of one span are safe; the viewed elements are the caller's to synchronize. Assigning one span from two threads is a data race as for any object.
- **Contract.** The span does not own the elements, which must outlive it. `dynamic_extent` is the largest `std::size_t`, as in the standard. There is no `operator==` and no `std::hash`, as for `std::span`.

Standard-library behavior is not mirrored where C++20 gives none: `std::span` has no `at` before C++26, the library provides it from C++11 and it throws `std::out_of_range` with the message `lumex::span::at: index out of range`.

## Conversions with std::span and the iterator trait

Conversions in both directions (C++20, `lumex/examples/span/example_span_std_conversion.cpp`; the file prints a note and returns 0 where `<span>` is missing; output `sum=108 back=6`, `std::span<std::byte const> size=24 count_bytes=24`, `loose=6 exact=6`):

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
#else
  std::cout << "no std::span in this build (C++20 with <span> is needed)\n";
#endif
  return 0;
}
```

Iterators below C++20 (`lumex/examples/span/example_span_iterators.cpp`, built at C++11; output `pointers=5 iterators=6 count=3`, `empty size=0 null=1`):

```cpp
#include <cstdint>
#include <iostream>
#include <type_traits>
#include <vector>

#include "lumex/core/span/LumexSpan"

// Before C++20 the language cannot tell the iterator of a std::vector from the
// iterator of a std::deque, so a span accepts a pointer pair always and any
// other iterator only after the type is named as contiguous. From C++20 the
// answer is std::contiguous_iterator and the line below is redundant.
namespace lumex
{
namespace core
{
namespace span
{
namespace view
{
template <>
struct is_contiguous_iterator<std::vector<std::int32_t>::iterator>
    : std::true_type
{
};
} // namespace view
} // namespace span
} // namespace core
} // namespace lumex

using namespace lumex::core::span::view;

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
  return 0;
}
```

## Comparison with existing solutions

Measured by [benchmarks/span](../../../benchmarks/span/README.md) against `std::span` of libstdc++ 13.2.0 (GCC 13.2.0) and of libc++ (Clang 23.1.0), and `boost::span` of Boost.Core 1.92.0 (headers only, C++11 and C++20), `-O2 -DNDEBUG`, Intel Core i7-12700K pinned to one core, 5 passes of 15 repetitions; the benchmark has 39 scenarios (36 common to all implementations). Other spans in wide use were looked for and are not installed on this machine, so they were not measured and nothing of them is vendored: `gsl::span` (Microsoft GSL), `absl::Span` (Abseil), `tcb::span`, `nonstd::span-lite`.

| | `lumex::...::span` | `std::span` | `boost::span` |
| --- | --- | --- | --- |
| Standard floor | C++11, one class on every standard | C++20 | C++11 |
| Header-only | yes | yes (the standard library) | yes |
| `sizeof` dynamic / static `int` | 16 / 8 | 16 / 8 | 16 / 8 (all probed with GCC 13.2 and Clang 23.1) |
| Iterator type | `T *` | a wrapper class in libstdc++ and in libc++ (`std::span<int>::iterator` is not `int *`, probed) | `T *` (probed) |
| Precondition checks | none in any build | libstdc++: with `_GLIBCXX_ASSERTIONS` (run); libc++: not probed | `BOOST_ASSERT` (off with `NDEBUG`; read from the header) |
| `at` | yes (from C++11) | C++26 | no |
| `const_iterator`, `cbegin`, `crbegin` | yes | C++23 | yes |
| Iterator + count / iterator + end | pointers always; other iterators by trait (C++11 to 17) or `std::contiguous_iterator` (C++20) | C++20, contiguous iterators | pointers only (`I *`) |
| `enable_borrowed_range`, `enable_view` | yes (C++20) | yes | no (no opt-in in the header) |
| `as_bytes` element | the library's `byte` (every standard) | `std::byte` | `std::byte`, only where `std::byte` exists (C++17) |
| Converts to / from `std::span` | yes (C++20, both directions) | n/a | no: neither `boost::span<int>` to `std::span<int>` nor back converts (probed with rvalues); it also does not convert to or from the span of this library |
| Deduction guides | C++17 | C++17 | where `__cpp_deduction_guides` |

Run time, GCC 13.2.0 / libstdc++, C++20 columns, ns per operation (best of 5 passes; the full tables with every row and the ratios are in [results/span_benchmark.md](../../../benchmarks/span/results/span_benchmark.md)):

| Scenario | lumex | `std::span` | `boost::span` |
| --- | --- | --- | --- |
| sum with range-for, 1 000 000 ints | 350 892 | 351 428 | 348 444 |
| sum with `operator[]`, 1 000 000 ints | 351 286 | 350 665 | 350 462 |
| sum of `as_bytes` of 1 000 000 ints | 1 395 580 | 1 401 390 | 1 414 400 |
| `first (size / 2)` | 0.70 | 0.69 | 0.70 |
| `subspan (size / 4, size / 2)` | 0.70 | 0.69 | 0.69 |
| pass by value to a `noinline` function | 0.69 | 0.68 | 0.69 |
| recursive sum on halves, 1 024 ints | 4 426 | 5 220 | 5 226 |
| construct from pointer and count | 0.62 | 0.42 | 0.42 |
| construct from a `std::array` | 0.35 | 0.64 | 0.49 |
| `span<int, 16>` to `span<int>` | 0.35 | 0.35 | 0.34 |
| `span<int>` to `std::span<int>` / `std::span<int>` to `span<int>` | 0.65 / 0.43 | n/a | n/a |

Clang 23.1.0 / libc++, C++20: the same sums are 132 312, 134 280 and 711 573 for lumex against 132 291, 132 451 and 704 777 for `std::span`; `first`, `subspan` and the pass 0.69, 0.70, 0.70 against 0.69, 0.69, 0.68; the recursive sum 2 536 against 3 427; construction from pointer and count 0.34 against 0.35; `span<int, 16>` to `span<int>` 0.64 against 0.35; the conversions to and from `std::span` 0.35 and 0.35.

What the numbers mean. The loops of the scenarios compile to the same instructions in all implementations: the object files of the five GCC and the five Clang executables were disassembled and compared loop by loop, and the only differences are register names, two operands swapped in a `cmp` and two exchanged `punpck` in `std::span`'s sum loops. Where a row differs between executables (0.62 against 0.42, 0.35 against 0.64) it is the code placement, not the span: the 30-byte `ctor_ptr_size` loop of GCC starts at offset 40 and 56 of a 64-byte line in the two slow executables (lumex C++11 and C++20) and at offsets 8, 24 and 8 in the three fast ones, and the `static_to_dynamic` loop of Clang is 29 bytes from offset 48 in the two executables of this library and from offsets 32 and 0 in the three others; this is read from the linked addresses, not from a hardware counter. Over the 36 common scenarios the geometric mean of the time of the span of this library at C++20 over `std::span` at C++20 is 0.971 with GCC and 0.975 with Clang (the extremes of single rows are 0.55x and 1.47x with GCC, 0.61x and 1.83x with Clang, all sub-nanosecond loops), over `boost::span` at C++20 0.979 and 0.985, and lumex C++11 over `boost::span` C++11 1.030 and 1.038. The conversions to and from `std::span` have no ratio (the other implementations have no such pair); each is a copy of two words in the generated code. Run time is therefore a wash in these scenarios, in both directions, and a single sub-nanosecond row must not be read as a property of a span.

Compile time (`-fsyntax-only -O0`, a unit that includes the header and uses the whole API, over the same unit with a stub instead of a span; median of 7; differs by 5 to 10 % from run to run): GCC 13.2 at C++11 +128 ms for this library and +125 ms for `boost::span`; at C++20 +385 ms for this library, +44 ms for `std::span` and +190 ms for `boost::span`. Clang 23.1.0 at C++11 +49 ms and +42 ms; at C++20 +249 ms, +100 ms for `std::span` and +88 ms for Boost. At C++20 the header costs +211 ms with GCC and +130 ms with Clang after `<array>` and `<vector>`, and including `<memory>` and `<ranges>` alone costs +208 ms and +121 ms: those two headers (`std::to_address`, and the `enable_borrowed_range` and `enable_view` opt-ins) are the cost, while `<span>` is +4 ms. Code size: the benchmark's unit compiled with `-O2 -c` folds to a constant for every implementation (72 or 77 bytes of `.text` with GCC and 3 with Clang, for the span and for the stub alike), so there is no code-size row: a header-only span of this kind produces no code of its own in these scenarios.

## Strengths and weaknesses

Strengths:
- One class on every standard from C++11: the same type and overload sets in C++11, 14, 17 and 20, with `const_iterator` and `at` that the standard adds in C++23 and C++26.
- `sizeof` is 16 bytes (8 for a static extent, which stores no size), trivially copyable, iterators are pointers; run time equal to `std::span` and `boost::span` in 36 scenarios on GCC 13.2 and Clang 23.1 (identical loops; geometric mean 0.97 to 1.04 of the time).
- Implicit conversions with `std::span` in both directions from C++20, and between the byte spans from C++17, at the cost of a copy of two words.
- Observers, element access and subviews are `constexpr` from C++11.
- A span of `std::vector<bool>`, `std::deque`, `std::list`, an rvalue `std::vector<int>` to `span<int>`, `span<long>` from `span<int>`, and `span<Base>` from `span<Derived>` are rejected at compile time (34 further rejected uses are in the compile checks).

Weaknesses:
- Compile time at C++20: +385 ms per translation unit with GCC 13.2 (`std::span` +44 ms, `boost::span` +190 ms), +249 ms with Clang 23.1 (+100 ms, +88 ms), from `<memory>` and `<ranges>`. At C++11 it is +128 ms and +49 ms, equal to Boost.
- No precondition checks in any build mode: `s[n]`, `front`, `back`, `first`, `last` and `subspan` with a bad argument are silent undefined behavior, where `std::span` of libstdc++ with `_GLIBCXX_ASSERTIONS` and `boost::span` without `NDEBUG` diagnose them. Only `at` checks.
- At C++11, 14 and 17 an iterator that is not a pointer is rejected until `is_contiguous_iterator` is specialized for its type (`std::vector<int>::iterator` is the common case); the specialization is an unchecked promise.
- A separate type from `std::span`: code that is overloaded for both is ambiguous for a container argument, and a template that deduces `std::span<T, N>` does not accept it. The conversion is implicit only for dynamic extents.
- `byte` is not `std::byte`: `as_bytes` of this library returns spans of the own type, `std::as_bytes` does not take it, and element conversion needs `static_cast`.
- Not tested on MSVC (the CHANGELOG records MinGW compile checks only), and the conversions with `std::span` need `<span>`, so GCC 8 at `-std=c++2a` has none.
- A temporary container viewed through `span<T const>` dangles as with `std::span`; the library does not guard it.

## Testing

On GCC 13.2.0 Release (the build used for these numbers) `ctest -R span` runs 540 tests and all pass: 151 at C++11, 170 at C++17, 211 at C++20 (suites `span.` and `span.view.`, files `LumexSpanConstruction`, `LumexSpanObservers`, `LumexSpanSubviews`, `LumexSpanBytes`, `LumexSpanConstexpr`, `LumexSpanTraits`, `LumexSpanGlobalNames` at C++11; `LumexSpan` and `LumexSpanByteTwin` at C++17 (the latter again at C++20); `LumexSpanRanges` and `LumexSpanStdDifferential` at C++20), 5 examples (`examples.span.*`) and 3 `cmake.*` tests (`cmake.wiring_span`, `cmake.require_fail_utility_without_span`, `cmake.span_compile_checks`); the whole `cmake.*` and `lint.*` set of the tree (190 tests) passes. The differential tests compare `span` with `std::span` at C++20 on the constructibility and implicitness of a table of combinations of element types, extents and sources, the member types, the types and values of the subviews and `as_bytes`. `cmake.span_compile_checks` compiles 34 uses that `std::span` rejects (a discarded result, a constructor the constraints remove, a failing `static_assert`, an assignment to a const element, invalid element types) at C++11, C++17 and C++20, and they must not compile. The CHANGELOG entry of the module (`[v2.0.0.0]`) records the matrix run when the module was added: GCC 8.3, GCC 13.2, Clang 23.1 with libc++ and libstdc++, ASan and UBSan, a warning-free build on all of them and MinGW, and 75 deliberate corruptions of the code of which 72 were caught; these were not repeated for this README. The benchmark checks its own work: each scenario's result is compared with a raw-pointer reference, and a corrupted `to_std_span` (one element too many) stops it (`checksum 8200, the raw-pointer reference 8192`).

## Not done on purpose / limits / known issues

- No precondition checks (above), and no hardening mode. Whether a debug-only check should be added is open.
- The C++20 header cost comes from `<memory>` and `<ranges>`; reported, not changed.
- The static-extent conversions with `std::span` are explicit in the directions the standard makes explicit; there is no conversion from `span<int>` to a `std::span<int, N>`.
- `is_contiguous_iterator` before C++20 is a user declaration, not a detection.
- Only exactly the two byte types convert; `std::span<unsigned char>` from a `span<byte>` is rejected (probe).
- Windows and MSVC: untested for this README.
