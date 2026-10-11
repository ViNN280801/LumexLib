# LumexOptional: a nullable value from C++11 {#lumex_optional}

`lumex::core::optional::opt::optional<T>` (target `lumex::optional`, header-only, no dependency on another module of the library) is the interface of C++17 `std::optional<T>` and of its C++23 monadic operations from C++11 on, on every compiler of the library. The class is the module's own on every standard and is never an alias of `std::optional`: from C++17 it converts implicitly to and from `std::optional<T>` (the same `T`) and compares with it. Its special member functions are those of `std::optional`: it is trivially copyable and trivially destructible when `T` is, a literal type with `constexpr` construction and observers from C++11, and a moved-from `optional` keeps its moved-from value. The umbrella adds the global names `optional`, `nullopt`, `make_optional` and `lumex_bad_optional_access`; the types header `opt/LumexOptional.hpp` declares everything in `lumex::core::optional::opt` and puts no name at global scope.

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

From `lumex/examples/optional/example_optional_workflow.cpp`. The members in use (`has_value`, `value_or`, `reset`, `emplace`, `nullopt` comparison and assignment) are in `lumex/examples/optional/example_optional.cpp`. All the examples are built and run by the tests (`examples.optional.*`).

A literal converts to the `optional` (`optional<std::string> s = "abc";`), and `transform`, `and_then` and `or_else` chain steps that may be empty (`lumex/examples/optional/example_optional_monadic.cpp`, abridged):

```cpp
optional<std::string> setting = "COM3";
setting = "COM4";

optional<std::string> const configured = std::string ("1502");
optional<int> const port = configured.and_then (parse_port);
optional<std::size_t> const digits = configured.transform (
    [] (std::string const &text) { return text.size (); });
optional<int> const fallback
    = optional<std::string> ().and_then (parse_port).or_else (default_port);
```

A constant `optional` and the triviality of the type (`lumex/examples/optional/example_optional_literal.cpp`, abridged):

```cpp
constexpr optional<int> default_baud = 115200;
constexpr optional<int> unset_baud;

static_assert (default_baud.value_or (9600) == 115200, "holds its value");
static_assert (unset_baud.value_or (9600) == 9600, "gives the default");
static_assert (std::is_trivially_copyable<optional<int>>::value, "memcpy");
```

From C++17 the conversions to and from the standard type are implicit (`lumex/examples/optional/example_optional_std_conversion.cpp`, built at C++17 and run by the tests):

```cpp
std::optional<int> standard = 3;
optional<int> own = standard;       // implicit, copies the value or stays empty
std::optional<int> back = own;      // implicit
bool const same = (own == standard) // compares like two optionals
                  && !(own != back);
```
## Origin

The interface follows [optional] of C++17 and [optional.monadic] of C++23: `nullopt_t`, `in_place_t`, `bad_optional_access`, the constructors including the converting ones, observers, `value_or`, `and_then`, `transform`, `or_else`, `swap`, `reset`, `emplace`, the comparisons and `std::hash`. It is a backport written for this library; the design is the public one of `std::optional` and Boost.Optional, no source text of them is copied. The special member functions come from a chain of base classes, one per member (the technique of the standard libraries, as in `core/expected`, because C++11 has no `requires` and no conditionally defaulted member). The library needs the class below C++17 (`LumexMemRead::as` returns it) and keeps one class on every standard so that a program means the same thing in C++11 and in C++20.

## Headers

| Header | Contents |
| --- | --- |
| `lumex/core/optional/LumexOptional` | Umbrella: the types and the global names |
| `opt/LumexOptional.hpp` | `optional<T>`, `make_optional`, the comparisons, `std::hash`, the `std::optional` conversions and comparisons (C++17), all in `lumex::core::optional::opt`; no global name |
| `opt/LumexOptionalTags.hpp` | `nullopt_t`, `nullopt`, `in_place_t`, `in_place`, `lumex_bad_optional_access` |
| `opt/LumexOptionalDetail.hpp` | The storage union, the layers of special member functions, an `INVOKE` for C++11 and the traits of the converting operations (`detail`, not an interface) |
| `opt/LumexOptionalGlobals.hpp` | The global `optional` (an alias template), `nullopt`, `make_optional`, `lumex_bad_optional_access`. `in_place` has no global alias (`expected` has its own): write `lumex::core::optional::opt::in_place` |

A header of the library that returns the type includes `opt/LumexOptional.hpp` and spells the namespace out. Including `<optional>` together with the umbrella and writing `optional<T>` unqualified is ambiguous with `using namespace std;`; qualify one side.

## Interface

| Name | Notes |
| --- | --- |
| `optional<T>` | `T` is a non-reference, non-array, non-void object type that is not `in_place_t` or `nullopt_t` (a `static_assert` says so); `value_type`; layout is a union `{ dummy, value }` of `sizeof (T)` followed by a `bool`, so `sizeof` is that of `std::optional<T>` |
| construction | default and `nullopt` (`constexpr`, `noexcept`); copy and move (trivial when `T`'s are; the copy is deleted when `T` cannot be copied, the move is ignored when `T` cannot be moved); from a value `U &&` (implicit when `U` converts to `T` implicitly, `explicit` otherwise); from `optional<U> const &` and `optional<U> &&` (the same rule; `T` is not constructed from the whole `optional<U>` when it could be); `in_place` with arguments and with an `initializer_list` and arguments (explicit). All `constexpr` except the ones from `optional<U>` (C++20) |
| `~optional ()` | trivial when `T` is trivially destructible, `constexpr` from C++20 otherwise |
| assignment | `operator= (nullopt_t)`, copy, move, `operator= (U &&)`, `operator= (optional<U> const &)`, `operator= (optional<U> &&)`; two engaged optionals assign the value |
| `has_value ()`, `explicit operator bool`, `*`, `->` | `constexpr`; `*` and `->` do not check |
| `value ()` | `&`, `const &`, `&&`, `const &&`; throws `lumex_bad_optional_access` (derives from `std::logic_error`, message `LumexBadOptionalAccess: Bad optional access`) when empty |
| `value_or (default)` | `const &` and `&&` forms, `constexpr` |
| `and_then (f)`, `transform (f)` | the four forms for the value category of `*this` (`&`, `const &`, `&&`, `const &&`); `constexpr`; `and_then` returns `remove_cvref_t` of the call result (an `optional`), `transform` returns `optional<remove_cv_t<call result>>` and does not flatten an `optional` result |
| `or_else (f)` | `const &` and `&&`; returns `*this` or the result of `f ()` (an `optional<T>`) |
| `swap`, free `swap` | `noexcept` when `T` is nothrow move constructible and nothrow swappable; the free one needs a move constructible, swappable `T` |
| `reset ()`, `emplace (args...)`, `emplace (ilist, args...)` | `emplace` destroys the old value first, returns `T &` and leaves the object empty when the constructor of `T` throws; `emplace` is constrained on `is_constructible` |
| comparisons | `==`, `!=`, `<`, `<=`, `>`, `>=` between `optional<T>` and `optional<U>`, with `nullopt_t` (both orders), with a value (both orders); the empty one is less than any value; `constexpr` |
| `make_optional (value)`, `make_optional<T> (args...)`, `make_optional<T> (ilist, args...)` | `constexpr` |
| deduction guide (C++17) | `optional (value)` is `optional<T>` |
| `std::hash<optional<T>>` | `0` for an empty one, the hash of the value otherwise |
| C++17: `optional<T>` from `std::optional<T> const &` and `&&`, conversion operator to `std::optional<T>` (`const &` and `&&`), `==`, `!=`, `<`, `<=`, `>`, `>=` with `std::optional<U>` in both orders | exactly the same `T` for the conversions; `constexpr` for a trivially copyable `T` (the conversion from `std::optional` for any `T` from C++20) |

## How it works

- **Storage.** `union { empty_member dummy; T value; }` and `bool`; the union is `optional_union` (trivially destructible for a trivially destructible `T`), the flag follows it. `sizeof` and `alignof` are those of the former `alignas (T)` byte buffer followed by a `bool`, which are also `std::optional`'s for the types measured (benchmark: all ten equal). No `std::aligned_storage` (deprecated in C++23).
- **Special member functions by layers.** `optional<T>` has one member, the last of five class templates, one per special member function: destructor (trivial when `T`'s is), copy constructor, move constructor, copy assignment, move assignment. Each is defaulted, hence trivial, when `T`'s operation is trivial, written out otherwise, and deleted when `T` lacks it (for the copy operations the standard says deleted, for the moves it says the operation does not take part, which is a defaulted move that is defined as deleted and therefore ignored by overload resolution, so an rvalue is copied). The data are a member and not a base class on purpose: a compiler copies a base class subobject without its tail padding, so `optional<int>` was copied by a 4-byte and a 1-byte move; as a member it is one 8-byte move, as for `std::optional`.
- **Trivial and constexpr.** With a trivially copyable `T` the type is trivially copyable and a literal type: constants, `constexpr` functions and a copy as plain bytes (a `std::vector` of `optional<int>` is copied by a loop of 8-byte moves without a branch per element, as for `std::optional<int>`). Constructors and the observers are `constexpr` from C++11, `reset` and the assignment of `nullopt` from C++14 for a trivially destructible `T`, and the operations that change which member of the union is alive (`emplace`, assignment of a value or of another `optional`, `swap`, the non-trivial `reset`, the non-trivial copy and move) from C++20 with `std::construct_at` (the compilers before C++20 cannot start the lifetime of a union member in a constant expression).
- **Assignment between two engaged optionals assigns.** The value is assigned (`std::optional` does the same), so a long `std::string` reuses its buffer. If `T`'s copy assignment throws, both stay engaged and the target holds what the assignment of `T` leaves. If only the source is engaged, the value is copy constructed, and a throwing constructor leaves the target empty. If only the target is engaged, it is destroyed.
- **A moved-from optional keeps its value.** The move constructor and the move assignment move the value and leave the source engaged (its value is moved-from), as `std::optional` does; `source.has_value ()` is `true` afterwards.
- **Converting operations.** The constraints and the `explicit` rules are those of [optional.ctor] and [optional.assign], including the rule for `T` = `bool` (an `optional<int>` is converted by state, not through its `explicit operator bool`) and the rule that `T` is not built from the whole `optional<U>` when it could be. `transform` builds its result from the call through a private constructor, so a call result that is an `optional` is held and not converted into the state of the outer one.
- **Monadic operations in C++11.** Written with trailing return types and SFINAE, with an own `INVOKE` (function objects, pointers to member functions and data members, `std::reference_wrapper`). Where the standard makes a violation a hard error (a Mandates element: an `and_then` result that is not an `optional`, a `transform` result that cannot be held), here the function does not take part in overload resolution, so the violation is a "no matching function" and can be tested for.
- **Exception safety.** Constructors and `emplace` leave the object empty when the constructor of `T` throws; assignment follows [optional.assign] (the engaged state is unchanged by an exception); `value ()` is the only function of the interface that throws by itself; `noexcept` on the special members follows `T` (`std::is_nothrow_*`, which for a non-trivial `T` is more than libstdc++ promises for the copy operations). No operation of the module allocates.
- **Thread safety.** None added: an optional is a plain object; concurrent const access is as safe as for the contained value.
- **ABI and export.** Header-only, no compiled part, nothing is exported; the class layout does not depend on the standard (the C++17 conversions are member templates and add no data).
- **Hash.** `std::hash` is specialized for the library's class (with a global qualifier on purpose, inside `namespace std` a bare `optional` would be `std::optional`).
- **Include cost.** `<memory>` is included only at C++20 (for `std::construct_at`) and for a compiler without `__builtin_addressof`.

## Comparison with existing solutions

Measured with [benchmarks/optional](../../../benchmarks/optional/README.md): GCC 13.2.0, libstdc++, Release (`-O2`, `-march=x86-64`), no sanitizers, Intel Core i7-12700K pinned to one core, idle machine under the build lock, 5 passes, Boost 1.92.0 headers. Clang, libc++, MSVC and other CPUs were not measured. The ratios are the time of the library divided by the time of `std::optional` at C++17 (above 1 is slower); each time is the best of 5 pass medians (the worst pass is in the results table, a dagger marks a spread above 25 %).

| | this library | `std::optional` (C++17) | `boost::optional` |
| --- | --- | --- | --- |
| Standard floor | C++11 | C++17 | C++11 (Boost 1.92 was used) |
| Dependency | none (header-only) | the standard library | Boost headers |
| `sizeof (optional<T>)` | `int` 8, `double` 16, `std::string` 40 | the same | the same |
| Trivially copyable / destructible for `int` | yes / yes | yes / yes | no / yes |
| Literal type, `constexpr` use | yes (construction and observers from C++11, the operations that change the value from C++20) | yes (the same split) | partly |
| Moved-from source | holds a moved-from value | the same | the same |
| Copy assignment of two engaged | assign | assign | assign |
| Monadic operations (`and_then`, `transform`, `or_else`, C++23) | yes, from C++11 | C++23 | no (`map`, `flat_map` in Boost) |
| `optional<T&>`, `optional<const T>` | `T&` no, `const T` yes | `T&` no, `const T` yes | `T&` yes |
| Converting constructors and assignment between `optional<U>` and `optional<T>` | yes | yes | yes |
| `in_place` | `lumex::core::optional::opt::in_place` | `std::in_place` | `boost::in_place_init` |
| Interop | implicit conversion to and from `std::optional<T>` (same `T`) at C++17 | n/a | none |

Run-time results from the benchmark (46 scenarios):

| What | lumex / `std::optional` | Reason |
| --- | --- | --- |
| geometric mean, all scenarios | 1.00 (C++17), 1.02 (C++11) | the extremes below are placement or small instruction-count effects |
| construct, copy, copy assignment, value assignment, `swap`, `==` with `nullopt`, `*o`, `value ()`, counting, `vector` copy and growth, strings and the 32-byte struct | 0.91x to 1.05x, mostly 0.97x to 1.02x | the same operations; the copy of `optional<int>`, the copy assignment of an engaged `optional<std::string>` (80 chars: 3.59 ns against 3.65 ns) and the copy of a `std::vector<optional<int>>` of 1 024 (213 ns against 204 ns at C++11, 203 ns at C++17) are the same cost as the standard's, as they are trivial copies (one 8-byte move) |
| `emplace` and `reset` of `optional<int>` and of the 32-byte struct | 0.83x to 0.84x | real, small (0.38 ns against 0.46 ns): the loop is 6 instructions shorter, `std::optional::reset` tests the flag in every iteration and this library stores `false` for a trivially destructible `T` |
| `value_or` in a loop over a half-engaged array of `int` | 4.89x (C++11) and 1.38x (C++17) | the loop is the same machine code as the standard's (checked in the disassembly, only the operand order of one `lea` and one `cmp` differs); the branch predictor learns the fixed pattern in some executables and not in others: code placement, not a property of the library |
| `==` between `optional<int>` arrays, `==` with a value | 0.89x and 1.38x (`==`), 1.16x and 1.02x (with a value), C++11 and C++17 | the loops are the same machine code as the standard's: placement |
| a `noinline` function returning or taking an `optional<int>` by value, `push_back` of `optional<int>`, construct-move-destroy of `optional<int>` | 1.00x (5.3 ns, 4.4 ns, 4.6 us per 1 024, 5.4 ns), where `boost::optional` is 0.12x to 0.17x | a store-forwarding stall: GCC 13.2 at -O2 returns the trivially copyable `optional<int>` in a register, the callee writes the 4-byte value and the 1-byte flag to the stack and the caller reloads both with one 8-byte load that cannot be forwarded. The time equals `std::optional`'s, the path differs: with `perf_event_open` (`LD_BLOCKS.STORE_FORWARD`) `std::optional<int>` has 1.006 blocked loads per call and this library 0.251, both at 24.5 cycles per call (the earlier, not trivially copyable class: 0.000 and 3.2); the machine code of the callee is not the same (this library stores the flag before the value and, on the empty path, reads a partly uninitialized slot), and why the engaged path is not counted here is not explained. A cost of being trivially copyable on this compiler in a non-inlined call |
| compile time, `-fsyntax-only` of one unit, over the same unit without an optional | include: +12 ms (C++11), +47 ms (C++17) against +5 ms for `std::optional`; use of the whole API: +49 ms and +82 ms against +41 ms | the header is large (monadic operations, layers, `INVOKE`) and at C++17 includes `<optional>` and `<functional>` |

`boost::optional` is within a few percent of this library in most scenarios (geometric mean of lumex against Boost at C++17: 1.14); it is much faster in the non-inlined return and pass of `optional<int>` and in `push_back` (it is not trivially copyable: 0.12x to 0.17x of the time of `std::optional`) and slower in the copy of `optional<int>` (1.8x to 2.4x) and of a vector of them (3.85x at C++11, 2.81x at C++17), where this library is as fast as `std::optional`.

## Strengths and weaknesses

Strengths:

- One class and one behavior from C++11 to C++23; `sizeof` equals that of `std::optional` and `boost::optional` for the ten types measured.
- The special member functions of `std::optional`: trivial when `T`'s are, so a literal type, `constexpr` constants, copies as plain bytes and register passing for trivially copyable `T`; a copy constructor and assignment that are not available when `T` cannot be copied (`std::is_copy_constructible<optional<std::unique_ptr<int>>>` is false); assignment that assigns; a moved-from `optional` that keeps its value.
- Converting constructors and assignments (`optional<std::string> s = "abc";`, `o = "text";`, `optional<long>` from `optional<int>`) and the C++23 monadic operations from C++11, with the explicit rules and constraints of the standard.
- Implicit conversions to and from `std::optional<T>` and comparisons with it at C++17, `constexpr` for a trivially copyable `T`, so a lumex-returning API can feed a `std::optional` consumer.
- Header-only, no dependency on another module; 654 unit tests of the module pass at five standards on GCC 13, Clang (both libraries) and, for C++11 to C++20, GCC 8 (below), with differential tests against `std::optional`.
- Run time equal to `std::optional` (geometric mean 1.00), with the one real difference in this library's favor (`emplace` and `reset`).

Weaknesses:

- Compile time: +12 ms (C++11) and +47 ms (C++17) for the `include` unit against +5 ms for `std::optional`, and +49 ms and +82 ms for the whole API against +41 ms.
- Being trivially copyable on GCC 13.2 at -O2, a non-inlined call that returns or takes an `optional<int>` by value, and `push_back` of one, take as long as for `std::optional<int>` (5.3 ns against 0.7 ns for `boost::optional`), through a store-forwarding stall of a different shape (fewer blocked loads per call than `std::optional`, the same cycles; see the benchmark).
- The operations that change which member of the union is alive (`emplace`, assignment of a value, `swap`, a non-trivial `reset`) are `constexpr` only from C++20, as in the standard.
- `lumex_bad_optional_access` derives from `std::logic_error`, not from `std::bad_optional_access` (that class exists only from C++17, the module works from C++11): code that catches the standard exception does not catch it.
- No `optional<T&>`; the C++17 conversions to and from `std::optional` exist only for the same `T`; there is no converting constructor from `std::optional<U>` of another `U`.
- A violation of a Mandates element of the monadic operations (an `and_then` result that is not an `optional`) is "no matching function" instead of a `static_assert`.
- Below C++17 an aggregate that has an `optional` member cannot be reflected by the automatic field types of `core/reflection` (the C++14 loophole is confused by a converting constructor, as it is for `std::optional`); a registered aggregate (`LUMEX_DEFINE_FIELD_NAMES`) and C++17 and later are not affected.
- Measured on one compiler (GCC 13.2.0) and one CPU.

## Testing

`ctest -R ptional` in a Release build with GCC 13.2.0 runs the module's 654 unit tests (112 at C++11, 112 at C++14, 143 at C++17 and at C++20, 144 at C++23), the five examples (`examples.optional.*`), `cmake.wiring_optional` (the types header declares no global name, the umbrella includes the globals header, `utility` requires the module) and `cmake.require_fail_utility_without_optional`, all passing. The same 654 pass with GCC 13.2.0 in a Debug build with AddressSanitizer and UndefinedBehaviorSanitizer, with Clang 23.1.0 on libstdc++ and on libc++ (Release, C++11 to C++23), and 510 with GCC 8.3 (C++11 to C++20; its C++2a). The suites of the modules that include the header (`utility` with `dump` and `mem`, `reflection`, `expected`, `resource_monitor`, `settings`, `logging`, `json`) pass as well, and so do the `cmake.*` and `lint.*` entries.

What the tests cover: every constructor, the assignments, observers, `value_or`, `emplace`, `swap`, the comparisons, `make_optional`, the storage (size and alignment of odd-sized and over-aligned types), lifetimes (copies, moves and destructions counted), nested optionals, self assignment, `std::hash`, the global names; the special members (the triviality traits and the deleted or ignored copy and move for move-only `T`, the state after a throwing copy constructor or copy assignment, the moved-from state, `noexcept`); the converting constructors and assignments (the `explicit` rules, the constraints, `bool`, a `T` that is constructible from the whole `optional<U>`); `constexpr` use at C++11, C++14, C++17 and C++20; the monadic operations (the four value categories, member pointers, `std::reference_wrapper`, the constraints, `transform` not flattening); and, from C++17, differential tests against `std::optional`: the same scripted operations on both with every construction, assignment and destruction of `T` compared event by event (also with a throwing `T`), the triviality, `noexcept`, constructibility and assignability traits, `sizeof` and `alignof`, the `std::hash`, and the monadic operations where the standard library has them (GCC 13 and libc++ at C++23). Each test was checked to fail against a deliberately broken header (50 defects; one is an equivalent change on GCC and Clang, which call a copy constructor of a type with a user destructor not trivial).

The differential tests found three places where the library differs from libstdc++ and not from the standard text: `is_nothrow_copy_constructible` and `is_nothrow_copy_assignable` of `optional<T>` follow `T` (libstdc++ and libc++ leave them false for a non-trivial `T`), `optional<T>` is trivially copyable where every operation that exists is trivial even if `T` has a deleted copy operation (libstdc++ and libc++ say no), and `optional<bool>` from `optional<int>` follows the C++23 rule for `bool` (GCC 13's library does not have it). The benchmark checks its checksums against plain-value references.

## Not done on purpose / limits / known issues

- The operations that start the lifetime of a union member are `constexpr` from C++20 only (a constant expression cannot do it before): `emplace`, the assignments from a value or an `optional<U>`, `swap`, the non-trivial `reset`, and the constructors from `optional<U>` and, for a `T` that is not trivially copyable, from `std::optional<T>`.
- No references (`optional<T&>`), and no converting constructor from `std::optional<U>` with `U` other than `T`.
- `in_place` has no global alias (two modules would collide).
- The violation of the Mandates of `and_then` and `transform` is not a `static_assert` (see the weaknesses).
- `optional<const T>` can be constructed, copied, reset and emplaced (also in a constant expression from C++20, tested with GCC and Clang; the value is stored without `const`), not assigned.
- `lumex::optional<bool>` has no constructor from a `std::optional<int>` (the state would be read through its `explicit operator bool`); build it from a `std::optional<bool>` or from a `lumex::optional<int>`. In the other direction `std::optional<bool> s (empty_lumex_bool);` (direct initialization) is engaged: the explicit value constructor of `std::optional<bool>` wins through the `explicit operator bool` of the library's class and stores `false`; write `std::optional<bool> s = empty_lumex_bool;` (copy initialization, the conversion operator) instead.
- A `T` whose copy constructor or copy assignment takes a non-const reference (`std::auto_ptr`) works in `optional<T>` (the copy it cannot make is deleted); a layout check of the C++14 loophole of `core/reflection` is the only place that cares (see the weaknesses).
