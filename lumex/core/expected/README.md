# LumexExpected: a value or an error without exceptions, from C++11 {#lumex_expected}

`lumex::core::expected` (target `lumex::expected`, header-only, requires `lumex::utility` for `LUMEX_ASSERT`) provides `expected<T, E>` and its `void` specialization, `unexpected<E>`, `bad_expected_access<E>`, the tags `unexpect` and `in_place` and the return markers `success ()` and `failure ()`: the interface of `std::expected` of C++23 ([expected], P0323R12 with the monadic operations of P2505R5) from C++11 on, on every platform. The classes are the module's own on every standard and toolchain (no aliases of the `std` ones). The classes live in `lumex::core::expected::result` (`expected`, tags, markers) and `lumex::core::expected::error` (`unexpected`, `bad_expected_access`).

```cpp
// lumex/examples/expected/example_expected_quickstart.cpp
#include <iostream>
#include <string>

#include "lumex/core/expected/Expected"

namespace ex = lumex::core::expected::result;

using channel_t = ex::expected<int, std::string>;

channel_t
parse_channel (char const *text)
{
  if (text == nullptr || text[0] == '\0')
    return ex::failure (std::string ("empty"));
  if (text[0] < '0' || text[0] > '9')
    return ex::failure (std::string ("not a digit"));
  return ex::success (text[0] - '0');
}

int
main ()
{
  channel_t const good = parse_channel ("5");
  channel_t const bad = parse_channel ("x");

  std::cout << good.transform ([] (int v) { return v * 2; }).value_or (-1)
            << '\n';                 // 10
  std::cout << bad.error () << '\n'; // not a digit

  channel_t const recovered
      = bad.or_else ([] (std::string const &) { return channel_t (0); });
  std::cout << recovered.value () << '\n'; // 0

  try
    {
      std::cout << bad.value () << '\n'; // an error: throws
    }
  catch (lumex::core::expected::error::bad_expected_access<std::string> const
             &failed)
    {
      std::cout << "value () threw: " << failed.error ()
                << '\n'; // not a digit
    }
  return 0;
}
```

## Origin

The interface is that of `std::expected` and `std::unexpected` ([expected] of the C++23 working draft); the library takes the special member functions, the constraints and the `noexcept` rules from the clauses [expected.object.cons], [expected.object.assign], [expected.object.dtor] and their `void` counterparts, and checks them against the standard library on the toolchain (see Testing). The storage is a union `{ val, unex }` and a `bool`, like libstdc++ and the MSVC STL; because C++11 has no `requires` clause and no conditionally defaulted member, the special member functions come from a chain of base classes, one per member (the technique of libstdc++'s `std::optional`). The return markers `success ()` and `failure ()` use the spelling of Boost.Outcome; they are an addition. No source text of another library is copied.

## Headers

| Header | Contents |
| --- | --- |
| `lumex/core/expected/Expected` | Umbrella: everything below |
| `error/Unexpected.hpp` | `unexpected<E>`, comparison, `swap`, deduction guide (C++17) |
| `error/BadExpectedAccess.hpp` | `bad_expected_access<void>` (the base, a `std::exception`), `bad_expected_access<E>` |
| `result/Expected.hpp` | `expected<T, E>`, `==` and `!=`, `swap`, `make_expected`, `make_unexpected` |
| `result/ExpectedVoid.hpp` | `expected<void, E>` |
| `result/ExpectedTypes.hpp` | `in_place_tag` and `in_place`, `unexpect_t` and `unexpect`, `Unit` |
| `result/SuccessFailure.hpp` | `success ()`, `failure ()`, `success_t`, `failure_t` |
| `result/ExpectedStorage.hpp` | Internal: the union and the layers of special member functions |
| `result/ExpectedDetail.hpp` | Internal: traits of the monadic operations, `construct_at`, `invoke` |

`is_expected<T>`, `is_expected_v<T>` and the concept `is_expected_concept` are in `lumex/core/utility/traits/LumexTypeTraits.hpp`.

## Interface

`T` is the value type, `E` the error type; the tables list the public members of `expected<T, E>` (the `void` specialization has the same members without the value).

| Name | Notes |
| --- | --- |
| `value_type`, `error_type`, `unexpected_type`, `rebind<U>` | `rebind<U>` is `expected<U, E>` |
| `expected ()` | The value is value-initialized; `expected<void, E> ()` is success |
| `expected (U &&)` | The value, `explicit` when `T` is constructible but not convertible from `U` |
| `expected (in_place, args...)`, `expected (in_place, ilist, args...)` | The value in place |
| `expected (unexpect, args...)`, `expected (unexpect, ilist, args...)` | The error in place |
| `expected (unexpected<G>)` | The error; implicit when `E` is convertible from `G`; `return unexpected<E> (e);` works |
| `expected (expected<U, G>)` | Converting copy and move constructors, with the constraints of the standard |
| `operator=` | From another `expected`, from a value, from an `unexpected` |
| `has_value ()`, `explicit operator bool`, `has_error ()` | `has_error ()` is an addition (`! has_value ()`) |
| `value ()` | The value or throws `bad_expected_access<E>` holding a copy of the error |
| `error ()` | The error; calling it on a value is a precondition violation (`LUMEX_ASSERT`, see below) |
| `operator*`, `operator->` | Unchecked by the standard, checked here (`LUMEX_ASSERT`) |
| `value_or (u)`, `error_or (g)` | |
| `and_then (f)`, `or_else (f)`, `transform (f)`, `transform_error (f)` | For `&`, `const &`, `&&` and `const &&`; `f` is called as `std::invoke` does (pointers to member functions and `std::reference_wrapper` work). `transform` accepts any result type (`void` gives `expected<void, E>`, an `expected` gives an `expected` of that `expected`, not flattened); `and_then` and `or_else` need a function that returns an `expected` (otherwise there is no viable overload) |
| `emplace (args...)`, `emplace (ilist, args...)` | `noexcept` where the standard has the constraint |
| `emplace_error (args...)` | Addition: the same for the error |
| `swap`, free `swap` | `noexcept` when the contents swap without throwing |
| `==`, `!=` | With another `expected`, with a value, with an `unexpected`, either side |
| `make_expected (v)`, `make_unexpected<E> (args...)` | Additions |
| `success ()`, `success (v)`, `failure (e)` | Markers that convert implicitly into the `expected` the function returns, if the target value or error type can be built from the payload; a mismatch is a compile error at the `return` |
| `Unit` | The empty type that stands for `void` in the storage |

Tags: `in_place_tag` and `in_place`, `unexpect_t` and `unexpect` are this module's own types, not `std::in_place_t` and `std::unexpect_t` (C++11 has neither). `in_place_tag`, `unexpect_t`, `unexpect`, `expected`, `bad_expected_access` and `make_unexpected` are also visible at global scope; `unexpected` and `in_place` are not (the MinGW runtime declares a global function `unexpected`; `optional` has its own `in_place`): write `lumex::core::expected::error::unexpected` and `lumex::core::expected::result::in_place`, or a using-directive as the examples do. `success`, `failure`, `success_t` and `failure_t` are not global either.

A second example, `void`, markers, `unexpected` and the other monadic operations (adapted from `lumex/examples/expected/example_expected.cpp`):

```cpp
using namespace lumex::core::expected::result;
using namespace lumex::core::expected::error;

expected<void, std::string> done;                                  // success, no value
expected<void, std::string> failed (unexpect, std::string ("busy")); // error

expected<int, std::string> const refused = unexpected<std::string> (std::string ("refused"));
expected<long, std::string> const wide = expected<int, std::string> (5); // converting constructor
expected<int, int> const code = refused.transform_error (
    [] (std::string const &why) { return static_cast<int> (why.size ()); }); // 7
bool const same = (refused == unexpected<std::string> (std::string ("refused"))); // true
```

## How it works

The object is a `bool` and a union of the value and the error, as [expected.object.general] describes: no allocation, no dynamic type, `sizeof (expected<int, unsigned>)` is 8 bytes (the same as `std::expected` in libstdc++; the sizes of 16 types are in the benchmark). The special member functions follow the properties of `T` and `E`: the copy constructor and copy assignment are deleted unless both can be copied; the move constructor and move assignment take part in overload resolution only if both can be moved; each of the five is trivial exactly when the current working draft says so ([expected.object.cons], [expected.object.assign]/5; the trivial assignments are not C++23 itself), and the destructor is trivial when both types are trivially destructible. A trivially copyable `T` and `E` therefore give a trivially copyable `expected` (libstdc++ 13.2's `std::expected` is not trivially copyable for the same types, see the benchmark). The assignments follow `reinit-expected` ([expected.object.assign]/1): no temporary `expected`, and the strong guarantee where `T` or `E` can be moved without throwing.

**`constexpr`.** Every member that the standard declares `constexpr` is `constexpr` as far as the language of the standard in use allows. C++11: the constructors from a value, `in_place`, `unexpect` and `unexpected`, the default constructor, the trivial copy and move constructors, the observers on a `const` object, the monadic operations on a `const` object, `==`; C++14 also the observers and monadic operations on an object that is not `const`; C++17 also monadic operations with a lambda; C++20 also the operations that change the active alternative of the union (assignments, `emplace`, `swap`, the non-trivial copy and destructor), through `std::construct_at`-style placement in constant expressions. The types themselves must be usable in a constant expression.

**Exceptions.** `value ()` on an error throws `bad_expected_access<E>` (derived from `bad_expected_access<void>`, a `std::exception`; `what ()` is the same text for all, `error ()` gives the stored error). The constructors and assignments are `noexcept` when the operation they do cannot throw. A build with `-fno-exceptions` is not supported (see Limits).

**Preconditions.** `error ()`, `operator*` and `operator->` check their precondition with `LUMEX_ASSERT`, which prints the condition and aborts in every build, `NDEBUG` included, where `std::expected` leaves the call undefined. The check is a compare and a branch; the compiler removes it after a `has_value ()` test (in the benchmark's `branch_mixed` loop the assert is gone in every build). `lumex_assert_handler` is in the compiled library `lumex::utility`, so a program that uses `expected` links it, although the header itself is self-contained.

**Thread safety.** None beyond the standard's: concurrent reads of one object are safe if `T` and `E` allow it, any write needs synchronization.

## Comparison with existing solutions

Measured with [benchmarks/expected](../../../benchmarks/expected/README.md) on an Intel Core i7-12700K (Linux 6.1, pinned to a performance core), GCC 13.2.0 with libstdc++, Release profile of the library (`-O2 -DNDEBUG`, `-march=x86-64`), no sanitizers, 5 passes of 15 repetitions; Boost 1.92.0 headers. The time is the median of the pass medians, the ratio is to `std::expected` at C++23. Sub-nanosecond ratios between executables of the same source are code placement, not the library (see the benchmark README); only repeatable differences are quoted here.

| | `lumex expected` | `std::expected` (C++23) | `boost::outcome_v2::result` |
| --- | --- | --- | --- |
| Standard floor | C++11 | C++23 | C++14 (measured at C++17) |
| Measured | yes, C++11 to C++23 | yes, the baseline | yes, C++17 |
| `sizeof<int, unsigned>` | 8 | 8 | 8 |
| `sizeof<string, text_error_t>` | 40 | 40 | 72 |
| Trivially copyable for trivial `T`, `E` | yes | no (libstdc++ 13.2) | yes |
| Monadic operations | `and_then`, `or_else`, `transform`, `transform_error` | the same | none |
| `value_or`, `error_or` | yes | yes | no |
| Checked `operator*` and `error ()` | always (abort) | undefined | `value ()` and `error ()` by policy (`terminate` here) |
| Geometric mean of the time, 37 scenarios | 1.07 (C++20 and C++23), 1.11 to 1.15 (C++11 to C++17) | 1.00 | 1.14 (31 scenarios; measured at C++17, so compare with lumex C++17: 1.15) |
| Compile time of the benchmark unit (`-fsyntax-only -O0`) | 338 ms (C++11), 631 ms (C++23) | 330 ms (C++23) | 781 ms (C++17) |

Where this library is slower or larger than the alternatives, as measured:

- **Returning a trivially copyable `expected` through calls: 4.5 times slower than `std::expected`.** Four nested `noinline` calls returning `expected<int, unsigned>` take 28.7 ns against 6.4 ns (`std::expected`) and 6.3 ns (Outcome), at every standard. The generated code builds the 8-byte object with two partial stores and reloads it with one 8-byte load at every level, which is a store-forwarding stall: the hardware counter `LD_BLOCKS.STORE_FORWARD` (read with `perf` on the benchmark executables of this machine, GCC 13.2, i7-12700K; the benchmark harness itself does not collect it) shows 19.0 million blocked loads for this library's chain against 607 for `std::expected` and 3 575 for Outcome, about one per call. Both types are trivially copyable and returned in a register; the stall comes from how this library builds the object. It does not happen for objects returned through memory (strings, 64-byte values: 0.91 to 1.05). It is a defect of the storage that is open, not a design property.
- **Short strings below C++20: 35 to 43 % slower** for construction, copy and assignment of `expected<std::string, unsigned>` at C++11, 14 and 17 (3.5 against 2.4 ns for a copy), 0 to 14 % at C++20 and C++23 (at C++23 the construction, copy and move loops compile to the same machine code as `std::expected`'s); Outcome shows the same, so it is the cost of `std::string` below C++20 and not of the module.
- **Monadic operations: slower by 8 to 51 % at C++23** (`transform` 1.08, `and_then` 1.15, `or_else` 1.20, `transform` over a string 1.51; the three in a row 0.99). The 1.93 of `transform` over a string at C++11 is code placement (the same code as at C++23).
- `branch_mixed` (test, then `*x` or `error ()`) reads 0.88 against 0.63 ns at C++14 to C++23, but the machine code equals `std::expected`'s and the C++11 build's (the assert is removed after the `has_value ()` test in every build): placement, not a property of the library.
- **Compile time: +50 ms for `std::expected` against +351 ms for this library at C++23** over a unit without the header (`-fsyntax-only -O0`), +203 ms at C++11; with `-c -O2` 830 ms against 520 ms. Outcome is the slowest (781 ms and 1 629 ms).
- Faster than `std::expected` as measured: copy assignment 0.82 (value over value) and 0.75 (switching the alternative) times for `<int, unsigned>`; the code differs from libstdc++'s (its copy assignment is non-trivial), so these are real. The `value_or` 0.65 and `value ()` of a string 0.58 rows are identical code or placement and are not claimed.

Not measured: `tl::expected` (not installed on this machine; nothing is vendored), Clang, MSVC, libc++, any CPU but one.

## Strengths and weaknesses

Strengths:

- Works from C++11 with the interface of C++23 `std::expected`, including `expected<void, E>`, the four monadic operations, `unexpected` and `bad_expected_access`; the same source builds and passes its tests at C++11, 14, 17, 20 and 23 (GCC 13.2 here; the release matrix of the repository covers four compilers and was not rerun for this document).
- `sizeof` equals libstdc++'s `std::expected` for all 16 type pairs measured (Outcome is larger for 3).
- Trivially copyable (assignments trivial too) where `T` and `E` are: 10 of 10 pairs measured; libstdc++ 13.2's copy assignment is non-trivial, so its `std::expected` is trivially copyable in none. Both are returned in registers.
- `constexpr` from C++11 for the constructors and the observers on a `const` object, and from C++20 for the assignments and `emplace`.
- A precondition violation of `error ()`, `operator*` and `operator->` aborts with a message in every build instead of being undefined.
- Differential tests against the `std::expected` of the toolchain, and tests of the special member functions against [expected.object] for a type zoo.

Weaknesses:

- Returning a trivially copyable `expected<int, unsigned>` through four calls takes 28.7 ns against 6.4 ns for `std::expected` (GCC 13.2, every standard).
- Compile time of a unit that uses the header is 203 ms (C++11) to 351 ms (C++23) above a unit without it; `std::expected` costs 50 ms.
- `lumex_assert_handler` needs the compiled library `lumex::utility`; the checks are always on.
- No conversion to or from `std::expected` (the classes are separate types); `std::in_place_t` and `std::unexpect_t` are not accepted, the module's own tags are.
- `expected` is also a global name (`::expected`): `using namespace std;` with `<expected>` at C++23 makes `expected<int, int>` ambiguous (checked with GCC 13.2). `unexpected` is not global, which differs from the habit of `using namespace std;` code.
- Monadic operations and short-string traffic are 8 to 51 % slower than `std::expected` in the benchmark (C++23), and short-string construction, copy and assignment are 35 to 43 % slower at C++11 to C++17 only (0 to 14 % at C++20 and C++23).
- `-fno-exceptions` does not compile `value ()` (see Limits). No `expected<T&, E>` (the standard has none either).
- Measured with GCC 13.2 on one CPU only.

## Testing

`ctest -R '^expected'` runs the unit tests (one suite per standard of the module, `lumex/tests/LumexTestStandards.cmake`): the build used for this document (GCC 13.2, Release, 2026-10-10) has 3 276 tests whose names contain `expected`, all passed: `error/` (`unexpected`, `bad_expected_access`) 105 at C++11, 105 at C++14, 106 at C++17, 106 at C++20; `result/` (`expected<T, E>`, `expected<void, E>`, markers) 560 at C++11, 561 at C++14, 568 at C++17, 569 at C++20, 591 at C++23; 3 CMake cases (global names, compile checks, the resource monitor build without the module); 2 examples. What they cover:

- every member of `expected<T, E>` and `expected<void, E>`, the monadic operations (`ExpectedMonadic`, `ExpectedMonadicParity`), comparisons, conversions, `swap`, the tags, the markers, with a matrix of value and error types per standard (`SuccessFailureTestSupport.hpp`);
- the special member functions against the standard for a zoo of types (`ExpectedMembers`: deleted, left out, trivial, `noexcept`), and the order of construction and destruction of the assignments (`ExpectedReinit`, `ExpectedProbe.hpp`);
- constant expressions per standard (`ExpectedConstexpr` at C++11, 14, 17, 20);
- parity with `std::expected` (`ExpectedStdParity` at C++17, with the `std::expected` of the toolchain as the oracle) and the differential tests `ExpectedStdDifferential` at C++23 (construction, assignment, `swap`, `emplace`, observers, monadic operations, comparisons, `value ()` throwing, move-only contents; one skipped test where the toolchain has no `std::expected`);
- `ExpectedWithOptional` (both value-holding modules in one file) and `cmake.expected_compile_checks` (rejected uses).

Sanitizers and mutation checks were not run for this document.

## Limits and known issues

- `expected<T&, E>`, `expected<T[], E>`, `expected<void, void>` are rejected by `static_assert` (the standard forbids them).
- `-fno-exceptions`: a build with `-fno-exceptions` does not compile as soon as `value ()` is instantiated (GCC 13.2: `exception handling disabled, use -fexceptions to enable` at the `throw` in `ExpectedDetail.hpp`); there is no `value ()` without a `throw`. `std::expected` in libstdc++ builds in that mode.
- The return-through-calls slowdown above is open (`ExpectedStorage.hpp`, GCC 13.2 at every standard); other compilers were not measured.
- `expected` does not convert to `std::expected`; code that mixes them converts by hand.
