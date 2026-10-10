# LumexOptional: a nullable value from C++11 {#lumex_optional}

`lumex::core::optional::opt::optional<T>` (target `lumex::optional`, header-only, no dependency on another module of the library) is the interface of C++17 `std::optional<T>` from C++11 on, on every compiler of the library. The class is the module's own on every standard and is never an alias of `std::optional`: from C++17 it converts implicitly to and from `std::optional<T>` (the same `T`) and compares with it. The umbrella adds the global names `optional`, `nullopt`, `make_optional` and `lumex_bad_optional_access`; the types header `opt/LumexOptional.hpp` declares everything in `lumex::core::optional::opt` and puts no name at global scope.

```cpp
#include <iostream>
#include <string>

#include "lumex/core/optional/LumexOptional"

namespace
{
optional<std::string>
maybe_operator (bool logged_in)
{
  if (!logged_in)
    return nullopt;
  return optional<std::string> (std::string ("analyst.a"));
}
}

int
main ()
{
  optional<std::string> const missing = maybe_operator (false);
  optional<std::string> const present = maybe_operator (true);
  std::cout << "anonymous=" << missing.value_or (std::string ("<unsigned>")) << '\n';
  std::cout << "signed=" << present.value_or (std::string ("<unsigned>")) << '\n';
}
```

From `lumex/examples/optional/example_optional_workflow.cpp`. The members in use (`has_value`, `value_or`, `reset`, `emplace`, `nullopt` comparison and assignment) are in `lumex/examples/optional/example_optional.cpp`. Both are built and run by the tests (`examples.optional.*`).

From C++17 the conversions to and from the standard type are implicit (compiled and run for this README, there is no example file for them yet):

```cpp
#include <optional>

#include "lumex/core/optional/LumexOptional"

std::optional<int> standard = 3;
optional<int> own = standard;       // implicit, copies the value or stays empty
std::optional<int> back = own;      // implicit
bool const same = (own == standard) // compares like two optionals
                  && !(own != back);
```

## Origin

The interface follows [optional] of C++17: `nullopt_t`, `in_place_t`, `bad_optional_access`, the constructors, observers, `value_or`, `swap`, `reset`, `emplace`, the comparisons and `std::hash`. It is a backport written for this library; the design is the public one of `std::optional` and Boost.Optional, no source text of them is copied. The library needs it below C++17 (`LumexMemRead::as` returns it) and keeps one class on every standard so that a program means the same thing in C++11 and in C++20.

## Headers

| Header | Contents |
| --- | --- |
| `lumex/core/optional/LumexOptional` | Umbrella: the types and the global names |
| `opt/LumexOptional.hpp` | `optional<T>`, `nullopt_t`, `nullopt`, `in_place_t`, `in_place`, `lumex_bad_optional_access`, `make_optional`, the comparisons, `std::hash`, the `std::optional` conversions and comparisons (C++17), all in `lumex::core::optional::opt`; no global name |
| `opt/LumexOptionalGlobals.hpp` | The global `optional` (an alias template), `nullopt`, `make_optional`, `lumex_bad_optional_access`. `in_place` has no global alias (`expected` has its own): write `lumex::core::optional::opt::in_place` |

A header of the library that returns the type includes `opt/LumexOptional.hpp` and spells the namespace out. Including `<optional>` together with the umbrella and writing `optional<T>` unqualified is ambiguous with `using namespace std;`; qualify one side.

## Interface

| Name | Notes |
| --- | --- |
| `optional<T>` | `T` is a non-reference, non-array, non-void, non-const object type (see the limits); `value_type`; layout is an `alignas (T)` byte buffer of `sizeof (T)` followed by a `bool`, so `sizeof` is that of `std::optional<T>` |
| construction | default, `nullopt`, copy, move, from `T const &` and `T &&` (implicit), `in_place` with arguments (explicit), `in_place` with an `initializer_list` and arguments (explicit); default and `nullopt` are `noexcept` and marked `constexpr`, but the class is not a literal type (non-trivial destructor), so no `constexpr` variable of it compiles |
| `~optional ()` | non-trivial, destroys the value |
| `operator= (nullopt_t)`, copy, move, `operator= (U &&)` | the `U &&` form is enabled only when `decay<U>` is `T` |
| `has_value ()`, `explicit operator bool`, `*`, `->` | `*` and `->` do not check |
| `value ()` | `&`, `const &`, `&&`, `const &&`; throws `lumex_bad_optional_access` (derives from `std::logic_error`, message `LumexBadOptionalAccess: Bad optional access`) when empty |
| `value_or (default)` | `const &` and `&&` forms |
| `swap`, free `swap` | `noexcept` when `T` is nothrow move constructible and move assignable |
| `reset ()`, `emplace (args...)`, `emplace (ilist, args...)` | `emplace` destroys the old value first and returns `T &` |
| comparisons | `==`, `!=`, `<`, `<=`, `>`, `>=` between `optional<T>` and `optional<U>`, with `nullopt_t` (both orders), with a value (both orders); the empty one is less than any value |
| `make_optional (value)`, `make_optional<T> (args...)`, `make_optional<T> (ilist, args...)` | |
| `std::hash<optional<T>>` | `0` for an empty one, the hash of the value otherwise |
| C++17: `optional<T>` from `std::optional<T> const &` and `&&`, conversion operator to `std::optional<T>` (`const &` and `&&`), `==`, `!=`, `<`, `<=`, `>`, `>=` with `std::optional<U>` in both orders | exactly the same `T`; not `constexpr` |

## How it works

- **Storage.** `alignas (T) unsigned char m_storage[sizeof (T)]` and `bool m_has_value`; the value is built with placement `new` and destroyed by an explicit destructor call. This is not `std::aligned_storage` (deprecated in C++23). The object has the size and alignment of the old `aligned_storage` layout, which is also `std::optional`'s for the types measured (benchmark: all ten equal).
- **Not trivial for any `T`.** The copy and move constructors, the assignments and the destructor are user-provided, so `optional<int>` is not trivially copyable, not trivially destructible and not a literal type: it cannot be a `constexpr` variable and is not passed in registers (it is returned and passed through memory). `std::optional<int>` is all three.
- **Assignment destroys and rebuilds.** Copy and move assignment of two engaged optionals destroy the old value and construct the new one, instead of assigning to the contained value (`std::optional` assigns). Consequence for exceptions: if the copy constructor of `T` throws in a copy assignment, `*this` is empty afterwards (`std::optional` keeps its old value); measured below. Value assignment (`o = value`) does assign when `o` is engaged.
- **A moved-from optional is empty.** The move constructor and the move assignment move the value and then destroy it in the source, so `source.has_value ()` is `false` afterwards. In `std::optional` the source still holds a moved-from value. Code that reads a moved-from optional (`if (src)`) behaves differently.
- **Exception safety.** Constructors and `emplace` leave the object empty when the constructor of `T` throws; `value ()` is the only function of the interface that throws by itself; `noexcept` on the special members follows `T` (`std::is_nothrow_*`). No operation of the module allocates.
- **Thread safety.** None added: an optional is a plain object; concurrent const access is as safe as for the contained value.
- **ABI and export.** Header-only, no compiled part, nothing is exported; the class layout does not depend on the standard (the C++17 conversions are members templates and add no data).
- **Hash.** `std::hash` is specialized for the library's class (with a global qualifier on purpose, inside `namespace std` a bare `optional` would be `std::optional`).

## Comparison with existing solutions

Measured with [benchmarks/optional](../../../benchmarks/optional/README.md): GCC 13.2.0, libstdc++, Release (`-O2`, `-march=x86-64`), no sanitizers, Intel Core i7-12700K pinned to one core, idle machine under the build lock, 5 passes, Boost 1.92.0 headers. Clang, libc++, MSVC and other CPUs were not measured. The ratios are the time of the library divided by the time of `std::optional` at C++17 (above 1 is slower).

| | this library | `std::optional` (C++17) | `boost::optional` |
| --- | --- | --- | --- |
| Standard floor | C++11 | C++17 | C++11 (Boost 1.92 was used) |
| Dependency | none (header-only) | the standard library | Boost headers |
| `sizeof (optional<T>)` | `int` 8, `double` 16, `std::string` 40 | the same | the same |
| Trivially copyable / destructible for `int` | no / no | yes / yes | no / yes |
| Literal type, `constexpr` use | no | yes | partly |
| Moved-from source | empty | holds a moved-from value | holds a moved-from value |
| Copy assignment of two engaged | destroy and rebuild | assign | assign |
| Monadic operations (`and_then`, `transform`, `or_else`, C++23) | no | C++23 | no (`map`, `flat_map` in Boost) |
| `optional<T&>`, `optional<const T>` | no | `T&` no, `const T` yes | `T&` yes |
| Converting constructors and assignment between `optional<U>` and `optional<T>` | no | yes | yes |
| `in_place` | `lumex::core::optional::opt::in_place` | `std::in_place` | `boost::in_place_init` |
| Interop | implicit conversion to and from `std::optional<T>` (same `T`) at C++17 | n/a | none |

Run-time results from the benchmark (46 scenarios):

| What | lumex / `std::optional` | Reason |
| --- | --- | --- |
| geometric mean, all scenarios | 0.93 (C++17), 0.92 (C++11) | the extremes below dominate in both directions |
| construct, emplace, reset, swap, `==` with `nullopt`, `*o`, `value ()`, strings | within 10 %, mostly 0.9x to 1.0x | same operations |
| `value_or` in a loop over a half-engaged array of `int` | 5.1x slower (2 715 ns against 535 ns per 1 024) | compiled to a branch that mispredicts; the standard one uses a conditional move |
| copy assignment of an engaged `optional<std::string>` (80 chars) | 3.9x to 4.0x slower (12.8 ns against 3.3 ns) | destroy and reallocate instead of reusing the buffer |
| copy of a `std::vector<optional<int>>` of 1 024 | 2.8x slower (539 ns against 194 ns) | element by element instead of `memmove` |
| copy construction, copy assignment of `optional<int>` | 2.0x slower (0.47 ns against 0.23 ns) | a branch instead of an 8-byte copy |
| `==` and `<` of `optional<int>` arrays, `==` of `optional<std::string>` arrays | 0.79x to 1.23x | placement dependent, mixed |
| a `noinline` function returning or taking an `optional<int>` by value, `push_back` of `optional<int>` | 0.11x to 0.14x (7x to 9x faster) | a store forwarding stall in GCC 13.2's code for the trivially copyable `std::optional<int>`; not a merit of the library, may not occur with another compiler or in a program that keeps the value in registers |
| compile time, `-fsyntax-only` of one unit, over the same unit without an optional | include: +7 ms (C++11), +41 ms (C++17) against +5 ms for `std::optional`; use of the whole API: +32 ms and +66 ms against +43 ms | at C++17 the header includes `<optional>` and `<functional>` |

`boost::optional` is within a few percent of this library in most scenarios (geometric mean of lumex against Boost at C++17: 1.06).

## Strengths and weaknesses

Strengths:

- One class and one behavior from C++11 to C++23; `sizeof` equals that of `std::optional` and `boost::optional` for the ten types measured.
- Implicit conversions to and from `std::optional<T>` and comparisons with it at C++17, so a lumex-returning API can feed a `std::optional` consumer.
- Header-only, no dependency on another module, 256 test entries of the module pass (below).
- `optional<int>` as a function result or argument by value is 7x to 9x faster than `std::optional<int>` in this GCC build (an artifact of that codegen, see the table).
- Compile time at C++11 is at the level of `std::optional` at C++17 (+7 ms against +5 ms for the `include` unit).

Weaknesses:

- Not trivially copyable, not trivially destructible, not a literal type for any `T`: no `constexpr` variables, no `memmove` copies in containers (a vector of 1 024 `optional<int>` copies 2.8x slower), passed through memory.
- A moved-from optional is empty (the standard's holds a moved-from value); copy assignment destroys and rebuilds, so a throwing copy constructor leaves the target empty and an engaged long string is reallocated (4x slower than the standard in the benchmark).
- `value_or` compiled to a branch: 5x slower than `std::optional` in a loop over unpredictable data in GCC 13.2.
- Not the whole C++17/23 interface: no monadic operations, no `optional<T&>`, no `optional<const T>`, no conversions between `optional<U>` and `optional<T>`, no constructor from `U &&` (so `optional<std::string> s = "abc";` does not compile, `optional<std::string> s ("abc");` does), no `operator= (U &&)` for a `U` that is not `T` (`o = "text"` for `optional<std::string>` does not compile).
- `is_copy_constructible<optional<std::unique_ptr<int>>>` is `true` although a copy fails to compile when used (the copy constructor is not constrained on `T`).
- `lumex_bad_optional_access` derives from `std::logic_error`, not from `std::bad_optional_access`: code that catches the standard exception does not catch it.
- The `std::optional` conversions exist only for the same `T`, only from C++17 and are not `constexpr`.
- Measured on one compiler (GCC 13.2.0) and one CPU.

## Testing

`ctest -R ptional` in a Release build with GCC 13.2.0 runs 256 entries, all passing: 51 unit tests at C++11, 67 at each of C++17, C++20 and C++23 (the 16 additional ones are `LumexOptionalStdTwin`, the `std::optional` conversions and comparisons, which exist from C++17), the two examples (`examples.optional.LumexOptionalExample`, `...Workflow`), `cmake.wiring_optional` (the types header declares no global name, the umbrella includes the globals header, `utility` requires the module) and `cmake.require_fail_utility_without_optional`. The unit tests cover every constructor, the assignments, observers, `value_or`, `emplace`, `swap`, the comparisons with optionals, `nullopt` and values, `make_optional`, the storage (size and alignment of odd-sized and over-aligned types), lifetimes (copies, moves and destructions counted), nested optionals, self assignment, `std::hash` and the global names. There are no differential tests against the standard other than the `StdTwin` conversion and comparison tests, no sanitizer run and no mutation check in the run behind these numbers. The benchmark (`benchmarks/optional`) also checks its checksums against plain-value references.

## Not done on purpose / limits / known issues

- No `constexpr` access: the value lives in a byte buffer, which a constant expression cannot hold (the source says so for the conversions and the comparisons with `if`).
- No monadic operations, no references, no `const T`, no converting constructors: the target is the C++17 interface.
- `in_place` has no global alias (two modules would collide).
- The header uses `std::string` (the constructors of `lumex_bad_optional_access`) and includes only `<stdexcept>`, relying on it to bring `<string>`.
- The state after a throwing copy assignment is empty, and a moved-from optional is empty: both differ from `std::optional` and are not documented as deviations in the header.
- Known gaps found while writing this file, not fixed here: `value_or` code generation, `is_copy_constructible` of a move-only `T`, no example for the `std::optional` conversions.
