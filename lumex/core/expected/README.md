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

The object is a flag and a union of the value and the error, as [expected.object.general] describes: no allocation, no dynamic type, `sizeof (expected<int, unsigned>)` is 8 bytes (the same as `std::expected` in libstdc++; the sizes of 16 types are in the benchmark). The flag is a `bool` held in a class as large as the alignment of the union (`has_val_flag` in `ExpectedStorage.hpp`), which changes no `sizeof (expected)` (a class that derives from `expected` or holds it as a `[[no_unique_address]]` member can grow, see Limits): it makes the padding after the flag part of the flag instead of tail padding of the storage. The storage is a base class of `expected` (the layers that give each special member function its triviality), and the Itanium ABI lets a derived class use the tail padding of a base that is not a POD, so with a plain `bool` GCC 13.2 built a trivially copyable `expected<int, unsigned>` through a stack slot (a 4-byte and a 1-byte store, then one 8-byte load that waited for both) every time a function returned it; the class has no such padding now and the compiler builds the 64-bit value in a register, as it does for `std::expected`, which has no base classes. The special member functions follow the properties of `T` and `E`: the copy constructor and copy assignment are deleted unless both can be copied; the move constructor and move assignment take part in overload resolution only if both can be moved; each of the five is trivial exactly when the current working draft says so ([expected.object.cons], [expected.object.assign]/5; the trivial assignments are not C++23 itself), and the destructor is trivial when both types are trivially destructible. A trivially copyable `T` and `E` therefore give a trivially copyable `expected` (libstdc++ 13.2's `std::expected` is not trivially copyable for the same types, see the benchmark). The assignments follow `reinit-expected` ([expected.object.assign]/1): no temporary `expected`, and the strong guarantee where `T` or `E` can be moved without throwing.

**`constexpr`.** Every member that the standard declares `constexpr` is `constexpr` as far as the language of the standard in use allows. C++11: the constructors from a value, `in_place`, `unexpect` and `unexpected`, the default constructor, the trivial copy and move constructors, the observers on a `const` object, the monadic operations on a `const` object, `==`; C++14 also the observers and monadic operations on an object that is not `const`; C++17 also monadic operations with a lambda; C++20 also the operations that change the active alternative of the union (assignments, `emplace`, `swap`, the non-trivial copy and destructor), through `std::construct_at`-style placement in constant expressions. The types themselves must be usable in a constant expression.

**Exceptions.** `value ()` on an error throws `bad_expected_access<E>` (derived from `bad_expected_access<void>`, a `std::exception`; `what ()` is the same text for all, `error ()` gives the stored error). The constructors and assignments are `noexcept` when the operation they do cannot throw.

**Without exceptions.** The macro `LUMEX_EXPECTED_HAS_EXCEPTIONS` is 1 or 0 and decides only what `value ()` does on an error: throw `bad_expected_access` (1) or call `std::terminate ()` (0), in every overload (lvalue, rvalue, `const`, `void`). By default it follows the compiler (`__cpp_exceptions`, `__EXCEPTIONS` or `_CPPUNWIND`), so `-fno-exceptions` selects 0; a program may also define it as 0 before the first include of the library to get a terminating `value ()` with exceptions on. Whether `emplace`, assignment and `swap` keep the old contents when a constructor throws follows the compiler alone (the internal `LUMEX_EXPECTED_COMPILER_HAS_EXCEPTIONS`), never the macro, so the strong guarantee holds in every build that has exceptions; with `-fno-exceptions` those paths have no `try` block, because nothing can throw. All translation units of one program must see the same value of the macro (the functions are `inline`; MSVC reports a mismatch at link time through `detect_mismatch`, GCC and Clang do not). Tests build and run this mode at C++11 and C++20 with `-fno-exceptions` and at C++17 with the macro and exceptions on, with types that really throw and count their lifetimes (`cmake.consumer_expected_no_exceptions_*`).

**Preconditions.** `error ()`, `operator*` and `operator->` check their precondition with `LUMEX_ASSERT`, which prints the condition and aborts in every build, `NDEBUG` included, where `std::expected` leaves the call undefined. The check is a compare and a branch; the compiler removes it after a `has_value ()` test (in the benchmark's `branch_mixed` loop the assert is gone in every build). `lumex_assert_handler` is in the compiled library `lumex::utility`, so a program that uses `expected` links it, although the header itself is self-contained. It is declared `noreturn` (not under MSVC, where the attribute would make the code after `LUMEX_ASSERT (false && ...)` unreachable, C4702; a handler that returns is undefined behavior elsewhere), so the failing branch is cold and the check does not change how the compiler lays out the code around it.

**`std::expected`.** Where the standard library has `std::expected` (C++23, `LUMEX_HAS_STD_EXPECTED`; the macro `LUMEX_EXPECTED_HAS_STD_CONVERSION` is 1), `expected<T, E>` and `std::expected<T, E>` of exactly the same `T` and `E` (also `void`) convert to each other implicitly, copying from an lvalue or a `const` object and moving from an rvalue; a move-only `T` or `E` converts from an rvalue only. The constructors and conversion functions are templates constrained to that one type, so `std::expected<long, int>` does not convert to `expected<int, int>` (convert in two steps), and a call with both overloads `f (expected<int, int> const &)` and `f (std::expected<int, int> const &)` picks the exact one. They are `noexcept` when the copy (the move) of `T` and `E` is, and `constexpr` as far as the language allows (C++20 and later, the constructors that change the active alternative). `mine.to_std ()` (a member of both specializations, same constraints and `noexcept`) is the spelling that always converts the state. Below C++23 there is no conversion and `std::expected` is not included.

```cpp
// lumex/examples/expected/example_expected_std.cpp
// expected next to std::expected (C++23): the same T and E convert both ways
// without a cast. Below C++23 the standard library has no std::expected and
// the program says so.
#include <iostream>
#include <string>

#if __has_include(<expected>)
#include <expected>
#endif

#include "lumex/core/expected/Expected"

#if LUMEX_EXPECTED_HAS_STD_CONVERSION

namespace ex = lumex::core::expected::result;

using lumex_result_t = ex::expected<int, std::string>;
using std_result_t = std::expected<int, std::string>;

// A function of a library that uses std::expected.
std_result_t
parse_with_std (char const digit)
{
  if (digit < '0' || digit > '9')
    return std::unexpected<std::string> ("not a digit");
  return digit - '0';
}

// A function that takes this library's expected.
int
twice (lumex_result_t const &parsed)
{
  return parsed.has_value () ? parsed.value () * 2 : -1;
}

int
main ()
{
  // std::expected to expected: implicit, the value or the error is copied
  // (moved from an rvalue).
  std::cout << twice (parse_with_std ('4')) << '\n'; // 8
  std::cout << twice (parse_with_std ('x')) << '\n'; // -1

  // expected to std::expected: implicit as well.
  lumex_result_t const mine = ex::failure (std::string ("kept"));
  std_result_t const theirs = mine;
  std::cout << theirs.error () << '\n'; // kept
  return 0;
}

#else

int
main ()
{
  std::cout << "std::expected is not available (C++23 is needed)\n";
  return 0;
}

#endif
```

**Thread safety.** None beyond the standard's: concurrent reads of one object are safe if `T` and `E` allow it, any write needs synchronization.

## Comparison with existing solutions

Measured with [benchmarks/expected](../../../benchmarks/expected/README.md) on an Intel Core i7-12700K (Linux 6.1, pinned to a performance core), GCC 13.2.0 with libstdc++, Release profile of the library (`-O2 -DNDEBUG`, `-march=x86-64`), no sanitizers, 5 passes of 15 repetitions; Boost 1.92.0 headers. The tables are the GCC run; Clang with libc++ is below. The time is the median of the pass medians, the ratio is to `std::expected` at C++23. Sub-nanosecond ratios between executables of the same source are code placement, not the library (see the benchmark README); only repeatable differences are quoted here.

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
| Geometric mean of the time, 37 scenarios | 1.00 (C++20 and C++23), 1.04 to 1.07 (C++11 to C++17) | 1.00 | 1.15 (31 scenarios; measured at C++17, so compare with lumex C++17: 1.06) |
| Compile time of the benchmark unit (`-fsyntax-only -O0`) | 376 ms (C++11), 730 ms (C++23) | 369 ms (C++23) | 878 ms (C++17) |

Where this library is slower or larger than the alternatives, as measured:

- **Returning a trivially copyable `expected` through calls: on par (was 5.5 times slower, fixed).** Four nested `noinline` calls returning `expected<int, unsigned>` take 5.3 ns against 5.1 ns for `std::expected` at C++23 (1.02 to 1.05 at C++14 to C++20; 1.35 at C++11 for the same machine code, which is placement). Before the flag of the storage was moved into a class as wide as the alignment of the union and `lumex_assert_handler` was declared `noreturn`, the same chain took 28.3 ns, and the counter `LD_BLOCKS.STORE_FORWARD` showed one blocked load per chain (20.15 million for 20 million) and none now; the cause and the evidence are in the benchmark README. Objects returned through memory (strings, 64-byte values) were always on par.
- **Short strings below C++20: 35 to 43 % slower** for construction, copy and assignment of `expected<std::string, unsigned>` at C++11, 14 and 17 (3.5 against 2.4 ns for a copy), 0 to 14 % at C++20 and C++23 (at C++23 the construction, copy and move loops compile to the same machine code as `std::expected`'s); Outcome shows the same, so it is the cost of `std::string` below C++20 and not of the module.
- **Monadic operations: slower by 0 to 24 % at C++23** (`transform` 1.18, `and_then` 1.07, `or_else` 1.18, `transform` over a string 1.02; the three in a row 0.93).
- **`value ()` and `error ()` in a loop: about 1.4 times `std::expected`** (0.50 against 0.37 ns per element in most executables, 0.37 in some): every call tests the state, `std::expected`'s loop has none. `branch_mixed` (test, then `*x` or `error ()`) reads 0.92 against 0.66 ns at C++14 to C++23 and 0.65 ns at C++11; its machine code did not change with the storage fix and the cause was not established.
- **Compile time: +57 ms for `std::expected` against +417 ms for this library at C++23** over a unit without the header (`-fsyntax-only -O0`), +226 ms at C++11; with `-c -O2` 947 ms against 574 ms. Outcome is the slowest (878 ms and 1 779 ms).
- Faster than `std::expected` as measured: copy assignment 0.76 (value over value) and 0.61 (switching the alternative) times for `<int, unsigned>`; the code differs from libstdc++'s (its copy assignment is non-trivial), so these are real. The `value_or` 0.65 and `value ()` of a string 0.71 rows are identical code or placement and are not claimed.

**Clang 23.1.0 with libc++** (the same machine, flags, 5 passes and CPU; the ratios are to the `std::expected` of libc++; the rows are in the `clang` sections of [benchmarks/expected/results/expected_benchmark.md](../../../benchmarks/expected/results/expected_benchmark.md)): the geometric mean over the 37 scenarios against `std::expected` C++23 is 1.009 for this library at C++23, 1.007 at C++20, 1.005 at C++17, 1.008 at C++14 and 1.066 at C++11 (GCC: 0.997, 0.995, 1.058, 1.041, 1.071) and 1.151 for Outcome (GCC: 1.147). At C++23 the rows more than 10 % slower are `error_int` 3.08x (GCC: 1.40x), `deref_int` 1.47x (GCC: 1.06x), `transform_str12` 1.22x and `or_else_int` 1.14x; `value_int` and `branch_mixed`, 1.41x and 1.40x with GCC, are 1.00x and 1.01x. More than 10 % faster are `assign_switch_int` 0.53x, `assign_same_int` 0.82x, `copy_value_int` 0.84x, `copy_error_int` 0.87x and `copy_error_str12` 0.89x. At C++11 `return_chain_str12` and `return_chain_str64` are 2.69x and 2.38x of `std::expected` (GCC: 1.00x and 1.01x), and the string rows that GCC puts at 1.35x to 1.45x are 0.99x (`construct_value_str12`), 1.02x (`copy_value_str12`), 0.93x (`copy_error_str12`) and 0.83x (`assign_same_str12`). No cause was investigated. The size of this library's type equals that of libc++'s `std::expected` for all 16 pairs, and it is trivially copyable where `std::expected` is not, as with GCC; `sizeof (std::string)` is 24 with libc++, so the five pairs with a `std::string` are 32 bytes instead of 40. Compile time with Clang (`-fsyntax-only -O0`, over a stub of 156 ms at C++11 and 265 ms at C++23): 312 ms for this library at C++11 and 680 ms at C++23, 312 ms for `std::expected` and 667 ms for Outcome; with `-c -O2` 384 and 785 ms, 392 ms and 798 ms.

Not measured: `tl::expected` (not installed on this machine; nothing is vendored), MSVC, Clang with libstdc++, any CPU but one.

## Strengths and weaknesses

Strengths:

- Works from C++11 with the interface of C++23 `std::expected`, including `expected<void, E>`, the four monadic operations, `unexpected` and `bad_expected_access`; the same source builds and passes its tests at C++11, 14, 17, 20 and 23 (GCC 13.2 here; the release matrix of the repository covers four compilers and was not rerun for this document).
- `sizeof` equals libstdc++'s `std::expected` for all 16 type pairs measured (Outcome is larger for 3).
- Trivially copyable (assignments trivial too) where `T` and `E` are: 10 of 10 pairs measured; libstdc++ 13.2's copy assignment is non-trivial, so its `std::expected` is trivially copyable in none. Both are returned in registers, and a chain of calls returning one costs the same (the machine code of the chain equals `std::expected`'s).
- `constexpr` from C++11 for the constructors and the observers on a `const` object, and from C++20 for the assignments and `emplace`.
- A precondition violation of `error ()`, `operator*` and `operator->` aborts with a message in every build instead of being undefined.
- Builds with `-fno-exceptions`: `value ()` on an error terminates, the rest works (`LUMEX_EXPECTED_HAS_EXCEPTIONS`).
- Implicit conversions to and from `std::expected<T, E>` of the same types at C++23.
- Differential tests against the `std::expected` of the toolchain, and tests of the special member functions against [expected.object] for a type zoo.

Weaknesses:

- `value ()`, `error ()` and `operator*` in a loop cost about 1.4 times `std::expected` in most executables (the state is tested in every call).
- Compile time of a unit that uses the header is 226 ms (C++11) to 417 ms (C++23) above a unit without it; `std::expected` costs 57 ms.
- `lumex_assert_handler` needs the compiled library `lumex::utility`; the checks are always on.
- `std::in_place_t` and `std::unexpect_t` are not accepted, the module's own tags are; `std::expected` converts to and from `expected` only for the same `T` and `E` and only at C++23.
- `expected` is also a global name (`::expected`): `using namespace std;` with `<expected>` at C++23 makes `expected<int, int>` ambiguous (checked with GCC 13.2). `unexpected` is not global, which differs from the habit of `using namespace std;` code.
- Monadic operations are up to 24 % slower than `std::expected` in the benchmark (C++23), and short-string construction, copy and assignment are 35 to 43 % slower at C++11 to C++17 only (0 to 14 % at C++20 and C++23).
- No `expected<T&, E>` (the standard has none either).
- Measured with GCC 13.2 on one CPU only.

## Testing

`ctest -R expected` runs the unit tests (one suite per standard of the module, `lumex/tests/LumexTestStandards.cmake`): the build used for this document (GCC 13.2, Release, 2026-10-11) has 3 351 tests whose names contain `expected`, all passed (the suites at C++11, 14, 17, 20 and 23, the CMake cases and the examples). The same suites pass with GCC 8.3 at C++11 (684 tests), with Clang 23 at C++11 and C++20 on libc++ and on libstdc++ (1 368 each), and under AddressSanitizer and UndefinedBehaviorSanitizer with GCC 13.2 (1 306 of 1 309; the three others are example executables that the run did not build). The release matrix (`create_release.sh` at C++11 and C++20 with GCC 8.3, GCC 13.2, Clang 23 and MinGW) builds with an empty `warns/` directory. What they cover:

- every member of `expected<T, E>` and `expected<void, E>`, the monadic operations (`ExpectedMonadic`, `ExpectedMonadicParity`), comparisons, conversions, `swap`, the tags, the markers, with a matrix of value and error types per standard (`SuccessFailureTestSupport.hpp`);
- the special member functions against the standard for a zoo of types (`ExpectedMembers`: deleted, left out, trivial, `noexcept`), and the order of construction and destruction of the assignments (`ExpectedReinit`, `ExpectedProbe.hpp`);
- constant expressions per standard (`ExpectedConstexpr` at C++11, 14, 17, 20);
- parity with `std::expected` (`ExpectedStdParity` at C++17, with the `std::expected` of the toolchain as the oracle) and the differential tests `ExpectedStdDifferential` at C++23 (construction, assignment, `swap`, `emplace`, observers, monadic operations, comparisons, `value ()` throwing, move-only contents; one skipped test where the toolchain has no `std::expected`);
- `ExpectedLayout` (the sizes of 22 pairs equal to a flag and a union, the triviality of the pairs, and that the storage has no tail padding), `ExpectedExceptionsSwitch`, and `ExpectedStdConversion` at C++23 (both directions, value and error, `void`, move-only, copies against moves with a counting type, the exact constraints, `noexcept`, constant expressions, overload selection, and the converted object against `std::expected` for a range of values);
- `cmake.consumer_expected_no_exceptions_*` (a program built with `-fno-exceptions` at C++11 and C++20 and with `LUMEX_EXPECTED_HAS_EXCEPTIONS` defined as 0 and exceptions on at C++17: assignment, `emplace`, `swap` with types that throw and count their lifetimes, and a child process for each overload of `value ()` that must reach `std::terminate`);
- `ExpectedWithOptional` (both value-holding modules in one file) and `cmake.expected_compile_checks` (rejected uses, and `lumex_assert_handler` being `noreturn`).

Each new test was run against the library with the code it tests broken on purpose (21 changes of the storage flag, the no-exceptions path, the conversions and the handler); every one of them made a named test fail or the test program not compile.

## Limits and known issues

- `expected<T&, E>`, `expected<T[], E>`, `expected<void, void>` are rejected by `static_assert` (the standard forbids them).
- `-fno-exceptions`: `value ()` on an error terminates instead of throwing, and `emplace`, assignment and `swap` do not put the old contents back when a constructor fails, because exceptions cannot occur; code that relies on catching `bad_expected_access` needs exceptions on. `LUMEX_EXPECTED_HAS_EXCEPTIONS` defined as 0 with exceptions on makes only `value ()` terminate. All translation units of one program must see the same value (checked at link time with MSVC only). The macro as 1 under `-fno-exceptions` does not compile. MSVC without exceptions and the MSVC warnings of the new code (C4324 and C4702 handled by pragma and by not marking the handler `noreturn` on MSVC) were not tried.
- The conversions to and from `std::expected` exist only for the same `T` and `E` and only where the standard library has `std::expected` (C++23); `std::expected<long, int>` does not convert to `expected<int, int>`, and a call with the overloads `f (std::expected<int, int> const &)` and `f (expected<long, int> const &)` and an `expected<int, int>` argument is ambiguous (each overload needs two user-defined conversions). Convert in two steps.
- **Converting an `expected<bool, E>` (or an `expected` that `std::any` or another type can be built from) to `std::expected` by direct initialization loses the state.** `std::expected<bool, int> s (mine)`, `s {mine}` and `static_cast<std::expected<bool, int>> (mine)` select the constructor of `std::expected` that takes a value, through the explicit `operator bool` of `expected`: an error becomes the value `false`; a `std::expected<std::any, E>` holds the `expected` as its value. The library cannot prevent it. Use copy initialization (`std::expected<bool, int> s = mine;`) or `mine.to_std ()`. The conversion in the other direction is safe in every spelling.
- `std::expected == expected` compiles (the `operator==` of `std::expected` against a value of another type) but compares the lumex object as a value: two equal values give `true`, two equal errors give `false`, a value against an error `false`. Convert one side (`mine.to_std ()`) to compare the states.
- A class that derives from `expected`, or holds it as a `[[no_unique_address]]` member, can be larger than with `std::expected` (`struct D : expected<int, unsigned> { char c; }` is 12 bytes, 8 with `std::expected`): the flag class has no tail padding for the derived class to use, which is what keeps the object in a register. `sizeof (expected)` itself equals `std::expected`'s.
- Compile time of the header is the highest of the three alternatives measured except Outcome (see the table).
