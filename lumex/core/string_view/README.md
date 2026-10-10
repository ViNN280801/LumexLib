# LumexStringView: non-owning string views from C++11 {#lumex_string_view}

`lumex::core::string_view::view::lumex_string_view` and `lumex_wstring_view` (target `lumex::string_view`, a compiled library that depends on `lumex::utility`) are the library's own non-owning views over a `char` and a `wchar_t` sequence: the interface of C++17 `std::string_view` and `std::wstring_view` from C++11 on, on every compiler of the library. Each view is one class on every standard and is never an alias of the standard type: from C++17 it converts implicitly to and from `std::string_view` (`std::wstring_view`) and compares with it. The umbrella adds the global aliases `lumex_string_view` and `lumex_wstring_view`. These are the text parameter types of the library: the string overloads of base64, crc, exceptions and xml (and of fmt, json and others) take `lumex_string_view` by value in every standard, and a literal, a `std::string` or, from C++17, a `std::string_view` converts to it.

```cpp
#include <iostream>
#include <string>

#include "lumex/core/string_view/LumexStringView"

using namespace lumex::core::string_view::view;

int
main ()
{
  std::cout << "=== Workflow: classify a method path without copying ===\n\n";

  char const *raw = "methods/isocratic.ini";
  lumex_string_view path (raw);
  bool const ini = path.ends_with (lumex_string_view (".ini"));
  lumex_string_view::size_type const slash = path.rfind ('/');
  lumex_string_view const name
      = (slash == lumex_string_view::npos) ? path : path.substr (slash + 1U);
  std::cout << "ini=" << (ini ? "yes" : "no") << " name=\""
            << std::string (name.data (), name.size ()) << "\"\n";
  return 0;
}
```

From `lumex/examples/string_view/example_string_view_workflow.cpp`. The members in use elsewhere (`size`, `empty`, `starts_with`, `ends_with`, `find`, `rfind`, `compare`, `substr`) are in `lumex/examples/string_view/example_string_view.cpp`. Both are built and run by the tests (`examples.string_view.*`).

A function that takes text takes the view by value; the same call sites work in C++11 and in C++20 (compiled and run for this README, there is no example file for it yet):

```cpp
#include <iostream>
#include <string>

#include "lumex/core/string_view/LumexStringView"

namespace
{
// A text parameter: by value, one overload for every standard.
std::size_t
count_slashes (lumex_string_view path)
{
  std::size_t count = 0;
  for (char const c : path)
    {
      if (c == '/')
        ++count;
    }
  return count;
}
}

int
main ()
{
  std::string const owned = "methods/gradient/fast.ini";
  std::cout << count_slashes ("methods/a.ini") << ' '          // a literal
            << count_slashes (owned) << ' '                    // a std::string
            << count_slashes (lumex_string_view (owned.data (), 7)) << '\n';
}
```

From C++17 the conversions to and from the standard type are implicit; the view has no `std::hash` (compiled and run for this README, there is no example file for the conversions yet):

```cpp
#include <functional>
#include <iostream>
#include <string>
#include <string_view>

#include "lumex/core/string_view/LumexStringView"

int
main ()
{
  std::string_view standard = "methods/isocratic.ini";
  lumex_string_view own = standard;       // implicit, no copy of the text
  std::string_view back = own;            // implicit
  bool const same = (own == standard)     // compares like two views
                    && !(own != back) && (own < lumex_string_view ("z"));
  // The view has no std::hash: hash the standard view it converts to.
  std::size_t const hash = std::hash<std::string_view> () (own);
  std::string const copy = own.to_string ();   // an owning copy is explicit
  std::cout << same << ' ' << (hash == std::hash<std::string_view> () (back))
            << ' ' << copy.size () << '\n';
}
```

The wide view is the same class over `wchar_t` (compiled and run for this README):

```cpp
#include <iostream>

#include "lumex/core/string_view/LumexStringView"

int
main ()
{
  lumex_wstring_view const name (L"methods/gradient.ini");
  lumex_wstring_view::size_type const slash = name.rfind (L'/');
  lumex_wstring_view const leaf = name.substr (slash + 1U);
  std::wcout << leaf << L' ' << leaf.size () << L'\n';
}
```

## Origin

The interface follows `std::basic_string_view` of C++17 ([string.view]; the proposal N3921, and `boost::string_view` of Boost.Utility before it): the types, the iterators, `find` and its relatives, `compare`, `substr`, `remove_prefix`, `remove_suffix`, `starts_with` and `ends_with` (C++20), `copy`, `swap`, `operator<<`. The library needs it below C++17 (the overloads that take text could not otherwise name a view type) and keeps one class on every standard so that an overload set means the same thing in C++11 and in C++20; the decision is the project's own (the span and the optional of the library follow it).

## Headers

| Header | Contents |
| --- | --- |
| `lumex/core/string_view/LumexStringView` | Umbrella: includes both view headers |
| `view/LumexStringView.hpp` | `lumex_string_view` in `lumex::core::string_view::view`, the six comparison operators, the C++17 operators with `std::string_view`, `operator<<`, and the global alias `lumex_string_view` |
| `view/LumexWStringView.hpp` | The same for `wchar_t`: `lumex_wstring_view`, `std::wstring_view` operators, the wide `operator<<`, the global alias `lumex_wstring_view` |

The compiled part is `view/LumexStringView.cpp` and `view/LumexWStringView.cpp` (library `LumexCore_string_view`, alias `lumex::string_view`; 94 exported functions, 47 per view). The export macro of the module is `LUMEX_STRING_VIEW_API`: `dllexport` while the library itself is built on Windows, `dllimport` for everyone else, default visibility on ELF. There is no `portable_*` alias; headers of the library that take text include `view/LumexStringView.hpp` and spell the type out.

## Interface

The facts are from `view/LumexStringView.hpp`; the wide view has the same shape over `wchar_t` (`std::wstring`, `std::wostream`, `wcslen`).

| Name | Notes |
| --- | --- |
| types | `value_type`, `pointer`, `const_pointer`, `reference`, `const_reference`, `iterator`, `const_iterator` (both `char const *`), `reverse_iterator`, `const_reverse_iterator`, `size_type` (`std::size_t`), `difference_type`, `npos` |
| construction | default (null pointer, size 0), `char const *` (implicit; `strlen` in the library; a null pointer gives an empty view), `char const *` and size (inline, `constexpr`), `std::string const &` and any `std::basic_string<char, ..., Allocator> const &` (implicit), copy and move (defaulted) |
| C++17 | implicit construction from and implicit conversion to exactly `std::string_view` (member templates, inline, `constexpr`) |
| iterators | `begin`, `end`, `cbegin`, `cend` (inline raw pointers, `constexpr`), `rbegin`, `rend`, `crbegin`, `crend` (in the library) |
| capacity | `size`, `length`, `empty` (inline, `constexpr`), `max_size` (static, `constexpr`) |
| access | `operator[]`, `front`, `back`, `data` (inline, `constexpr`, unchecked), `at` (checked, throws `std::out_of_range`) |
| modifiers | `remove_prefix`, `remove_suffix` (clamp `n` to the size), `swap`, `clear` (null pointer and size 0) |
| operations | `copy` (throws `std::out_of_range` when `pos > size`), `substr` (same), `compare` with a view, a `char const *`, and `pos, len, view` |
| searches | `find`, `rfind`, `find_first_of`, `find_last_of`, `find_first_not_of`, `find_last_not_of`, each for a view, a character, a `char const *` with a count and a `char const *`; all `noexcept` |
| `starts_with`, `ends_with` | for a view and for a character (C++20 in the standard, available from C++11 here) |
| to `std::string` | `explicit operator std::basic_string<char, std::char_traits<char>, Allocator> ()` and `to_string (alloc)`; never implicit |
| comparisons | `==`, `!=`, `<`, `<=`, `>`, `>=` between two views (inline, calling the exported `compare`), and from C++17 with `std::string_view` in both orders (constrained templates that win over the standard's operators); a `char const *` or a `std::string` argument goes through the implicit constructor |
| `operator<<` | writes the characters with `ostream::write`; ignores the width and the fill of the stream |

Most members are marked `nodiscard`. `lumex_string_view` is the view of `char`; there is no conversion between the narrow and the wide view.

## How it works

- **Layout.** A pointer and a size: `sizeof` is 16 bytes for both views on x86-64 Linux, the type is trivially copyable, trivially destructible and standard layout, and is passed in registers (the benchmark's `pass_by_value` and `return_value` are the same speed as `std::string_view`). It is not trivially default constructible: the default constructor sets the pointer to null.
- **Complexity.** Every operation is linear in the size of the data it looks at, like the standard's. `find` of a view tests each position (a character comparison, then `memcmp`), O(n*m) in the worst case like the standard's, but it does not use `memchr` to skip to the next occurrence of the first character, which is what makes it slow on a long text (see the comparison). `find` of a character uses `memchr`, `compare` uses `memcmp`, the set searches are nested loops.
- **The compiled part.** The class is declared `class LUMEX_STRING_VIEW_API lumex_string_view`, so every member is exported, and every member that is not written in the class body is defined in `LumexStringView.cpp` (and `LumexWStringView.cpp`). The inline members are the accessors listed above, the copy and move and the constructor from pointer and size, the six comparison operators and the C++17 conversions. Everything else, including `substr`, `clear`, `remove_prefix` and the constructors from `char const *` and `std::string`, is a call into the library, so the optimizer sees a call and not the code, and a literal's length is not folded (see the comparison for the cost). The C++17 member templates and the operators with the standard view are inline and never exported, so a library built at C++11 serves a consumer at C++17.
- **Contract.** The view does not own the data: the characters must outlive it. `operator[]`, `front`, `back` and `data` do not check; `remove_prefix` and `remove_suffix` clamp instead of leaving it undefined; `at`, `substr` and `copy` throw `std::out_of_range`. A null pointer is a valid empty view. The characters are compared with `memcmp` (`wmemcmp` for the wide view), so for `char` the order is that of the unsigned bytes.
- **Exception safety and threads.** The members that allocate nothing and throw nothing are `noexcept` (the searches, `compare` with a view, `starts_with`, `ends_with`, the constructors from a view, a `char const *` and a `std::string`); `compare (char const *)` is not. Concurrent read access to one view is safe; modifying (`remove_prefix`, ...) one view from two threads is a data race, as for any object.
- **Conversions at C++17.** The conversion operator and the constructor accept exactly `std::string_view` (and `std::wstring_view`); a `std::basic_string_view` with other traits is not accepted. The comparisons with the standard view do not become ambiguous with the standard's own operators (a test covers the overload sets). Hashing of the view is not provided (see the limits); the conversion to `std::string_view` gives access to `std::hash`.

## Comparison with existing solutions

Measured by [benchmarks/string_view](../../../benchmarks/string_view/README.md) against `std::string_view` of libstdc++ 13.2.0 (C++17, and C++20 for `starts_with` and `ends_with`) and `boost::string_view` of Boost 1.92.0 (headers only, C++11 and C++17), GCC 13.2.0 `-O2`, Intel Core i7-12700K pinned to one core, 5 passes of 15 repetitions, the compiled part linked as a shared library (the default) and, where noted, as a static one. Ratios are to `std::string_view`; below 1.00 is faster than it. Other string views in wide use (`absl::string_view` is an alias of `std::string_view` where the standard has it, `llvm::StringRef`, `folly::StringPiece`, `nonstd::string_view`) are not installed on this machine and were not measured.

| | `lumex_string_view` | `std::string_view` | `boost::string_view` |
| --- | --- | --- | --- |
| Standard floor | C++11, one class on every standard | C++17 | C++11 |
| Header-only | no, a compiled library (35288 bytes shared) | yes | yes |
| `sizeof`, trivially copyable | 16 bytes, yes | 16 bytes, yes | 16 bytes, yes |
| `constexpr` | construction from pointer and size, accessors, iterators | nearly all members (C++17 and later) | most members |
| `starts_with`, `ends_with` | yes, from C++11 | C++20 | yes, from C++11 |
| `contains` | no | C++23 | yes |
| `std::hash`, `operator<=>`, `std::ranges::view`, `borrowed_range` | none of the four | all four (hash C++17, others C++20) | `boost::hash` through `hash_value`; the others not checked |
| `std::string_view` conversions | implicit both ways at C++17 | n/a | none |
| `operator<<` with `setw` | ignores width and fill | honors them | not checked |
| Wide view | `lumex_wstring_view` | `std::wstring_view` | `boost::wstring_view` |

Numbers (GCC 13.2.0, libstdc++; absolute times in ns per operation, ratio to `std::string_view` in parentheses; the first pair is the shared library, the second the static one):

| Scenario | lumex C++17, shared | lumex C++17, static | `std::string_view` | `boost::string_view` C++17 |
| --- | --- | --- | --- | --- |
| construct from a literal | 2.17 (3.49) | 2.03 (3.48) | 0.62 | 0.59 (0.95) |
| construct from a `char const *` | 3.81 (1.34) | 3.36 (1.13) | 2.84 | 2.92 (1.03) |
| construct from a `std::string` | 1.12 (1.32) | 1.01 (1.12) | 0.85 | 0.87 (1.02) |
| construct from pointer and size | 0.73 (0.95) | 0.76 (1.00) | 0.76 | 0.77 (1.01) |
| `find ('/')` | 3.36 (1.93) | 2.80 (1.62) | 1.74 | 1.70 (0.97) |
| `find` of a 9-character view in a string of 35 | 9.25 (1.70) | 9.42 (1.85) | 5.46 | 5.94 (1.09) |
| `find` of an 8-character view in a text of 4 096, first letter common | 1720 (1.19) | 1842 (1.27) | 1448 | 1457 (1.01) |
| `find` of an 8-character view in a text of 4 096, first letter absent | 969.6 (33.9) | 998.8 (34.4) | 28.63 | 31.87 (1.11) |
| `substr (8, 12)` | 1.34 (1.36) | 1.26 (1.29) | 0.99 | 1.00 (1.01) |
| `compare` of equal strings | 3.11 (1.17) | 3.16 (1.18) | 2.66 | 2.15 (0.81) |
| `==` of equal strings | 2.78 (1.35) | 2.75 (1.34) | 2.06 | 2.05 (0.99) |
| `==` with a literal | 1.73 (5.66) | 1.79 (6.26) | 0.31 | 0.29 (0.96) |
| `starts_with` a literal (std at C++20) | 3.71 (8.14) | 3.37 (7.34) | 0.46 | 0.48 (1.04) |
| `std::sort` of 256 views, per element | 42.97 (1.47) | 39.00 (1.32) | 29.21 | 30.58 (1.05) |
| `std::map` lookup of a view | 59.05 (1.94) | 39.02 (1.25) | 30.47 | 28.35 (0.93) |
| split a text of 4 096 at `,` | 2799 (1.27) | 2790 (1.28) | 2204 | 2170 (0.98) |
| to / from `std::string_view` | 0.77 / 0.85 | 0.69 / 0.90 | a copy: 0.62 / 0.73 | not available |

Summary of the whole set (44 scenarios): the geometric mean of the time of `lumex_string_view` over `std::string_view` is 1.55 (shared) and 1.53 (static); it is slower by more than 10 % in 33 scenarios (shared) and faster by more than 10 % in 1; `boost::string_view` over `std::string_view` is 1.00. The honest sentence per row: the view of this library is slower than both on every row above except construction from pointer and size, where all three are equal, and the conversions, which cost a fraction of a nanosecond. That is a property of the compiled part (a call for every operation and no folding of a literal's length) and, for `find` of a view, of the algorithm. It is faster only in `find_last_of` over a short character set (0.76x; `find_first_of` is 1.04x). The same code built into the benchmark's translation unit, with no library boundary, is 1.10 of `std::string_view` over all scenarios and within 10 % of it (or faster) in every row above except the three `find` of a view rows and the two conversions (about a tenth of a nanosecond). Compile time (`-fsyntax-only -O0`, over a unit with a stub): including the header costs +59 ms at C++11 and +103 ms at C++17, against +46 ms for `<string_view>` and +269 ms for Boost at C++17 (+162 ms at C++11); a unit that uses the whole API compiled at `-O2` has 5499 bytes of `.text` against 6723 for `std::string_view` and 10781 for Boost. Details, the method and the caveats: [benchmarks/string_view/README.md](../../../benchmarks/string_view/README.md).

## Strengths and weaknesses

Strengths:
- One class on every standard: the type and the overload sets of a text API are the same in C++11, C++14, C++17 and C++20, where `std::string_view` does not exist below C++17. The interface includes `starts_with` and `ends_with` from C++11.
- `sizeof` is 16 bytes, trivially copyable, passed in registers; construction from pointer and size, copy, `pass_by_value`, iteration and the 16-byte layout cost the same as `std::string_view` (within 10 %).
- From C++17 it converts implicitly to and from `std::string_view`, at a cost of a fraction of a nanosecond (`to_std` 0.77 ns against 0.62 ns for a copy of the standard type), and compares with it without ambiguity; `boost::string_view` has no such conversion.
- A null pointer, `remove_prefix (n > size)` and `remove_suffix` are defined (an empty view, a clamp) where the standard leaves them undefined.
- Cheaper than Boost to compile at C++11 (+59 ms to include, Boost +162 ms) and less object code per translation unit than `std::string_view` or Boost for the same use (5499 bytes of `.text` against 6723 and 10781 in the benchmark's `use` unit).
- `find_last_of` over a short set is faster than libstdc++'s (0.76x; `find_first_of` is 1.04x).

Weaknesses:
- Slower than `std::string_view` and `boost::string_view` in most operations because they are calls into a library: geometric mean 1.55x (shared) and 1.53x (static) over 44 scenarios.
- A string literal is the worst case: construction 3.49x, `==` with a literal 5.66x, `starts_with ("methods/")` 8.14x, because the constructor from `char const *` is a `strlen` in a call.
- `find` of a view tests every position (no `memchr` jump): 1.19x the time of `std::string_view` on a text of 4 096 characters whose first letter is common, 33.9x when the first letter does not occur at all, 1.70x on a short string.
- A compiled library: it must be built, linked and, if shared, deployed; the shared library needs `libLumexCore_utility` too.
- Not `constexpr` beyond construction from pointer and size and the accessors (no `constexpr` search, comparison, `substr`, or construction from a literal), where the standard's are.
- Missing next to `std::string_view`: `std::hash` (so no `unordered_map` key as it stands), `operator<=>` (C++20), `contains` (C++23, Boost has it), `std::ranges::enable_view` and `enable_borrowed_range`, a user-defined literal, the five-argument `compare` overloads, deduction guides.
- `operator<<` ignores `setw` and the fill character.
- A `std::basic_string_view` with other traits is not accepted by the C++17 conversions.
- The narrow and the wide view do not convert to each other.

## Testing

On GCC 13.2.0 Release (the build tree of this README work) `ctest -R string_view` runs 425 tests and all pass: 131 view tests at C++11, 142 at C++17 and 142 at C++20 (the C++17 and C++20 suites add `LumexStringViewStd.cxx17.tests.cpp`: the conversions to and from the standard views, the comparisons, the overload sets and the stream), 2 examples (`examples.string_view.*`) and 8 `cmake.*` tests (`cmake.wiring_string_view`, `cmake.wiring_string_view_export`, `cmake.require_fail_string_view_without_utility` and the `require_fail_*_without_string_view` cases of base64, crc, xml, exceptions and json). The test files cover the narrow view (`LumexStringView.cxx11.tests.cpp`), the wide view (`LumexWStringView.cxx11.tests.cpp`), the implicit conversions (`LumexStringViewImplicit`) and the view as a by-value text parameter (`LumexStringViewParameter`). The tests are unit tests with expected values; there is no randomized test against `std::string_view` in the module, but the benchmark compares the result of 44 operations of the view with the same operations on `std::string` on a corpus of 256 strings and a text of 4 096 characters, and stops on a difference. Sanitizer runs, GCC 8.3, Clang and MinGW builds are recorded in the CHANGELOG entry of the conversions between the views (`[v2.0.0.0]`) and were not repeated for this README; mutation checks were not part of this work either.

## Not done on purpose / limits / known issues

- No `constexpr` search, comparison, `substr` or construction from `char const *`; no `std::hash`, `operator<=>`, `contains`, range traits (`enable_view`, `enable_borrowed_range`) or user-defined literal. Whether they will come is open; the benchmark's numbers say where the compiled part costs.
- The whole class is exported from the library and the small members (`clear`, `remove_prefix`, `remove_suffix`, `swap`, `substr`, the constructors from `char const *` and `std::string`) are not inline; making them inline would remove most of the measured cost without changing the interface (the benchmark's "inlined" build shows the size of it), and a `find` that skips with `memchr` would remove the rest on long texts. Reported, not changed here.
- The shared library `LumexCore_string_view` lists `libLumexCore_utility` as a needed library, although the view uses only macros and attributes of the utility headers (checked with `readelf -d`).
- Configuring the whole project with `-DLUMEX_BUILD_SHARED_LIBS=OFF` fails at the generate step while the install rules are on (`LumexApplied_settings` requires `nlohmann_json`, which is in no export set); `-DLUMEX_INSTALL=OFF` avoids it. Unrelated to this module, found while building the static benchmark tree.
- Only exactly `std::string_view` and `std::wstring_view` convert; `std::u16string_view`, `std::u32string_view` and `std::u8string_view` have no counterpart.
- The wide view is not measured and has the width of `wchar_t` (4 bytes on Linux, 2 on Windows); its `operator<<` is `wostream::write`, so it ignores the width and the fill as well (read from the code, not run).
- The comparison operators are `inline` but call the exported `compare`; on Windows they are therefore part of the DLL boundary.
