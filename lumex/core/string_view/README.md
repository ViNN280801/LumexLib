# LumexStringView: non-owning string views from C++11 {#lumex_string_view}

`lumex::core::string_view::view::lumex_basic_string_view<CharT, Traits>` and its aliases `lumex_string_view` (`char`), `lumex_wstring_view` (`wchar_t`), `lumex_u16string_view`, `lumex_u32string_view` and, with `char8_t`, `lumex_u8string_view` (target `lumex::string_view`, an interface target: header-only, nothing to build or to link) are the library's own non-owning views over a character sequence: the interface of `std::basic_string_view` of the newest standard, from C++11 on, on every compiler of the library. The view is one class template on every standard and is never an alias of the standard type: from C++17 it converts implicitly to and from `std::string_view` (`std::wstring_view`, ...) and compares with it. The umbrella adds the global aliases `lumex_string_view` and `lumex_wstring_view`. These are the text parameter types of the library: the string overloads of base64, crc, exceptions and xml (and of fmt, json and others) take `lumex_string_view` by value in every standard, and a literal, a `std::string` or, from C++17, a `std::string_view` converts to it.

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

From `lumex/examples/string_view/example_string_view_workflow.cpp`; it prints `ini=yes name="isocratic.ini"`. The members in use elsewhere (`size`, `empty`, `starts_with`, `ends_with`, `find`, `rfind`, `compare`, `substr`) are in `lumex/examples/string_view/example_string_view.cpp`. All examples are built and run by the tests (`examples.string_view.*`).

A function that takes text takes the view by value; the same call sites work in C++11 and in C++20 (`lumex/examples/string_view/example_string_view_text_api.cpp`, prints `1 2 0`):

```cpp
#include <cstddef>
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

`std::hash` (one value in every standard), the `_sv` literals, `contains`, constant evaluation in C++11 and `operator<<` that honors `setw` and `setfill` (`lumex/examples/string_view/example_string_view_literals_hash.cpp`, built at C++11, prints `2 1 [******ab]`):

```cpp
#include <iomanip>
#include <iostream>
#include <string>
#include <unordered_map>

#include "lumex/core/string_view/LumexStringView"

using namespace lumex::core::string_view::view::literals;

namespace
{
// Evaluated by the compiler, in C++11 as well.
constexpr lumex_string_view method = "isocratic.ini"_sv;
static_assert (method.find ('.') == 9 && method.ends_with (".ini")
                   && method.contains ("crat"),
               "constant evaluation");
}

int
main ()
{
  // std::hash: a view is a key, and an equal text found elsewhere finds it.
  std::unordered_map<lumex_string_view, int> priority;
  priority["isocratic.ini"_sv] = 1;
  priority["gradient.ini"_sv] = 2;
  std::string const name = "gradient.ini";
  int const found = priority.at (lumex_string_view (name));

  std::cout << found << ' ' << method.contains ("crat") << ' ';
  // operator<< honors the width and the fill, and resets the width.
  std::cout << '[' << std::setw (8) << std::setfill ('*') << "ab"_sv << "]\n";
}
```

From C++17 the conversions to and from the standard type are implicit (`lumex/examples/string_view/example_string_view_std_conversion.cpp`, built at C++17, prints `1 1 21`):

```cpp
#include <cstddef>
#include <functional>
#include <iostream>
#include <string>
#include <string_view>

#include "lumex/core/string_view/LumexStringView"

int
main ()
{
  std::string_view standard = "methods/isocratic.ini";
  lumex_string_view own = standard;   // implicit, no copy of the text
  std::string_view back = own;        // implicit
  bool const same = (own == standard) // compares like two views
                    && !(own != back) && (own < lumex_string_view ("z"));
  // The hash of a view is the same in every standard (it is not the hash of
  // std::string_view, which depends on the standard library).
  std::size_t const hash = std::hash<lumex_string_view> () (own);
  std::string const copy = own.to_string (); // an owning copy is explicit
  std::cout << same << ' '
            << (hash == std::hash<lumex_string_view> () (lumex_string_view (back)))
            << ' ' << copy.size () << '\n';
}
```

The wide view is the same class over `wchar_t` (`lumex/examples/string_view/example_wstring_view.cpp`, prints `gradient.ini 12`):

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

The interface follows `std::basic_string_view` ([string.view]; the proposal N3921) as it stands in C++23: the types and `traits_type`, the iterators, `find` and its relatives, the six `compare` overloads, `substr`, `remove_prefix`, `remove_suffix`, `starts_with` and `ends_with` (C++20), `contains` (C++23), `copy`, `swap`, `operator<<`, `operator<=>` (C++20), `std::hash` (one value in every standard), the `_sv` literals, `std::ranges::enable_view` and `enable_borrowed_range` (C++20), the iterator-pair constructor (C++20) and the range constructor (C++23). The library needs it below C++17 (the overloads that take text could not otherwise name a view type) and keeps one class on every standard so that an overload set means the same thing in C++11 and in C++20; the decision is the project's own (the span and the optional of the library follow it), and so is giving the later standards' members from C++11.

## Headers

| Header | Contents |
| --- | --- |
| `lumex/core/string_view/LumexStringView` | Umbrella: includes the three headers below |
| `view/LumexBasicStringView.hpp` | The class template `lumex_basic_string_view`, the comparison operators, the operators with the standard view (C++17), `operator<<`, the aliases of every character type, the `_sv` literals (namespace `lumex::core::string_view::view::literals`, not inline), `std::hash`, the ranges opt-ins |
| `view/LumexStringView.hpp` | The global alias `lumex_string_view` |
| `view/LumexWStringView.hpp` | The global alias `lumex_wstring_view` |

There is no compiled part and no export macro (`LUMEX_STRING_VIEW_API` is gone from `LumexExport.hpp`; the six comparison operators are hidden friends of the class). The CMake target `lumex::string_view` (`LumexCore_string_view`) is an `INTERFACE` library that carries the include directory; it does not link `lumex::utility`, although the headers include the macro and attribute headers of the utility module. The Conan component `core_string_view` has no library and no requirements. There is no `portable_*` alias; headers of the library that take text include `view/LumexStringView.hpp` and spell the type out.

## Interface

| Name | Notes |
| --- | --- |
| types | `traits_type`, `value_type`, `pointer`, `const_pointer`, `reference`, `const_reference`, `iterator`, `const_iterator` (both pointers), `reverse_iterator`, `const_reverse_iterator`, `size_type`, `difference_type`, `npos` |
| construction | default (null pointer, size 0); `CharT const *` (implicit; a null pointer gives an empty view); `CharT const *` and size; `std::basic_string` with any allocator (implicit); exactly `std::basic_string_view<CharT, Traits>` (C++17, implicit); two pointers (below C++20) or two contiguous iterators (C++20, constrained as the standard's); a contiguous range with `data ()` and `size ()` of the same character type and traits, such as `std::vector<char>`, `std::array<char, N>` or `std::span<char const>` (explicit, in every standard) |
| iterators | `begin`, `end`, `cbegin`, `cend`, `rbegin`, `rend`, `crbegin`, `crend` |
| capacity | `size`, `length`, `empty`, `max_size` (static) |
| access | `operator[]`, `front`, `back`, `data` (unchecked), `at` (throws `std::out_of_range`) |
| modifiers | `remove_prefix`, `remove_suffix` (clamp `n` to the size), `swap`, `clear` (null pointer and size 0) |
| operations | `copy`, `substr` (both throw `std::out_of_range` when `pos > size`) |
| `compare` | with a view, with `pos, n, view`, with `pos, n, view, pos2, n2`, with a C string, with `pos, n, cstr` and with `pos, n, cstr, n2` |
| searches | `find`, `rfind`, `find_first_of`, `find_last_of`, `find_first_not_of`, `find_last_not_of`, each for a view, a character, a C string with a count and a C string |
| `starts_with`, `ends_with`, `contains` | for a view, a character and a C string (`starts_with`, `ends_with` are C++20 and `contains` C++23 in the standard) |
| to `std::basic_string` | `explicit operator std::basic_string<CharT, Traits, Allocator> ()` and `to_string (alloc)`; never implicit |
| C++17 | implicit conversion to and from `std::basic_string_view<CharT, Traits>`; `==`, `!=`, `<`, `>`, `<=`, `>=` with it in both orders (constrained templates that win over the standard's operators) |
| comparisons | `==`, `!=`, `<`, `<=`, `>`, `>=` between two views; a C string or a `std::basic_string` argument goes through the implicit constructor; `operator<=>` (C++20) gives `Traits::comparison_category` if the traits name one (`std::strong_ordering` for `std::char_traits`) and `std::weak_ordering` otherwise, as [string.view.comparison] says, also with the standard view |
| `std::hash` | for views with `std::char_traits` only (a view with other traits has none): a 64-bit block hash of the library, the same value in every standard, compiler and standard library (not the value of `std::hash<std::string_view>`); usable as the key hash of the unordered containers |
| `operator<<` | a formatted output function as the standard's: `width`, `fill` and `adjustfield`, the width reset to 0, the sentry, `badbit` on a failing buffer |
| ranges (C++20) | `std::ranges::enable_view` and `enable_borrowed_range` are true; `std::ranges::contiguous_range`, `sized_range` hold |
| literals | `"text"_sv`, `L"text"_sv`, `u"text"_sv`, `U"text"_sv` (and `u8` with `char8_t`) |

Most members are marked `nodiscard`. There is no conversion between views of different character types. Differences from the standard view: a null `CharT const *` is an empty view (undefined in the standard); `remove_prefix` and `remove_suffix` clamp (undefined in the standard when `n > size`); `clear`, `to_string` and the conversion to `std::basic_string` are additions; `std::basic_string_view`'s deleted `nullptr_t` constructor is not there; the iterator-pair constructor below C++20 accepts pointers only.

## How it works

- **Layout.** A pointer and a size: `sizeof` is 16 bytes for every view on x86-64 Linux, the type is trivially copyable, trivially destructible and standard layout, and is passed in registers. It is not trivially default constructible: the default constructor sets the pointer to null. The standard view of libstdc++ keeps `{length, pointer}`, this one `{pointer, length}`.
- **Complexity and the searches.** Every operation is linear in the size of the data it looks at. `find` of a character looks it up with `char_traits::find` (`memchr`); `find` of a view looks up the first character of the needle with `char_traits::find` and compares the rest with `char_traits::compare` (`memcmp`) only where it was found, as libstdc++ does, so a text that lacks the first character costs one call and the worst case is O(n*m); `compare`, `starts_with` and `ends_with` use `char_traits::compare`; `rfind` and the "last" searches are backward loops (there is no portable `memrchr`); the set searches look every character up in the set with `char_traits::find`, as libstdc++ does.
- **Constant evaluation.** C++17 and later: every query is `constexpr` and calls the `constexpr` `char_traits` functions (on GCC 8, whose `char_traits<char>::compare` orders a byte 0x80 below `'a'` in a constant expression, the library's own loops are used, as in C++14). C++14: every query and the modifiers (`remove_prefix`, `remove_suffix`, `swap`, `clear`, `copy`) are `constexpr`; the library's own loops run in a constant expression and `char_traits` at run time, chosen with `__builtin_is_constant_evaluated` (GCC 9, Clang 9, MSVC 19.25) or, on GCC 8, `__builtin_constant_p`. C++11: the queries are `constexpr` (one `return` statement each); a search is a recursion over halves of the range, so its depth is the logarithm of the length (a text of 1000 characters is searched in a constant expression, probed with GCC's default depth of 512), the length of a narrow C string is `__builtin_strlen` and of a wide one a recursion of 4 characters per step (403 wide characters are tested); the modifiers are not `constexpr`. `rbegin`, `rend`, `crbegin` and `crend` are `constexpr` from C++17 (`std::reverse_iterator`). The compiler without the builtins (not GCC, Clang or MSVC) takes the constant-expression path at run time as well: correct, with the recursion over halves.
- **Contract.** The view does not own the data: the characters must outlive it. `operator[]`, `front`, `back` and `data` do not check; `remove_prefix` and `remove_suffix` clamp instead of leaving it undefined; `at`, `substr` and `copy` throw `std::out_of_range`. A null pointer with size 0 is a valid empty view and no function dereferences it. The characters are compared as `Traits::lt` orders them: for `char` as unsigned bytes, whatever the signedness of `char`, and a NUL is a character.
- **Exception safety and threads.** The members that allocate nothing and throw nothing are `noexcept` (the searches over a view or a character, `compare` with a view, `starts_with`, `ends_with`, `contains`, the constructors from a view, a pointer and a `std::string`); the overloads that take a C string are not (the standard leaves them without `noexcept`). Without exceptions (`-fno-exceptions`) the header compiles and the calls that would throw `std::out_of_range` (`at`, `substr`, `copy`, `compare` with a position) call `std::terminate`; `operator<<` sets the badbit and catches nothing (fixtures `consumer_string_view_no_exceptions_cxx11`, `_cxx17` and `consumer_string_view_exceptions_cxx11`). Concurrent read access to one view is safe; modifying one view from two threads is a data race, as for any object.
- **Conversions at C++17.** The conversion operator and the constructor accept exactly `std::basic_string_view<CharT, Traits>`. The comparisons with the standard view do not become ambiguous with the standard's own operators (a test covers the overload sets).
- **Hash.** `std::hash<lumex_string_view>` is one function in every standard: a 64-bit block hash (the rounds of xxHash64 over 8-byte blocks, the finalizer of MurmurHash3, the length mixed in). One-byte characters (`char`, `char8_t`) are hashed as bytes, eight at a time read as little-endian integers (a big-endian machine byte-swaps, so the value is the same); wider characters (`wchar_t`, `char16_t`, `char32_t`) as code unit values zero-extended to 32 bits, two per round, so equal code UNIT sequences hash alike: `wchar_t` of 2 or 4 bytes, and `char16_t` and `char32_t` for text inside the Basic Multilingual Plane (a character outside it is two units in UTF-16 and one in UTF-32: `u"\U0001F600"` and `U"\U0001F600"` hash differently), while a `char` string and a `wchar_t` string of the same text do not. `std::hash` exists only for views with `std::char_traits` (`char`, `wchar_t`, `char16_t`, `char32_t`, `char8_t`), as the standard has it: a view with other traits compares through its traits (a case-insensitive order, say) and a hash of the raw characters would give equal views different hashes, so it has no `std::hash` (a test pins that). It is deliberately not `std::hash<std::string_view>`: the standard's value is the standard library's choice, so a library built at C++11 and a program built at C++17 would hash a key differently and a set filled by one would not be found by the other (the fixture `consumer_string_view_hash_standard_mismatch` builds one translation unit at C++11 and the program at C++17 and C++20 and checks the values and the lookup). The cost is that a view and a `std::string` of equal text hash differently; hash the view, not the string, in a container keyed by views. On a 32-bit platform `size_t` takes the low 32 bits. The function is not `constexpr`.
- **`operator<<`.** Pads on the left, or on the right when `adjustfield` is `left`; `internal` pads like right, as libstdc++ does. The output is compared with `std::string_view`'s, `std::string`'s and a failing buffer's in the tests. With libstdc++ the unwind that glibc uses to cancel a thread (`abi::__forced_unwind`) is not swallowed: the badbit is set and the exception goes on, as in libstdc++'s own inserters (a test cancels a thread blocked in the buffer).

## Comparison with existing solutions

Measured by [benchmarks/string_view](../../../benchmarks/string_view/README.md) against `std::string_view` of libstdc++ 13.2.0 (C++17, and C++20 for `starts_with` and `ends_with`) and `boost::string_view` of Boost 1.92.0 (headers only, C++11 and C++17), GCC 13.2.0 `-O2`, Intel Core i7-12700K pinned to one core, 5 passes of 15 repetitions. Ratios are to `std::string_view`; below 1.00 is faster than it. The tables are the GCC run; Clang 23.1.0 with libc++ follows them. Other string views in wide use (`llvm::StringRef`, `folly::StringPiece`, `nonstd::string_view`) are not installed on this machine and were not measured.

| | `lumex_string_view` | `std::string_view` | `boost::string_view` |
| --- | --- | --- | --- |
| Standard floor | C++11, one class on every standard | C++17 | C++11 |
| Header-only | yes | yes | yes |
| `sizeof`, trivially copyable | 16 bytes, yes | 16 bytes, yes | 16 bytes, yes |
| `constexpr` | every query from C++11 (single `return` form), the modifiers from C++14, `rbegin`/`rend` from C++17 | nearly all members (C++17 and later) | most members |
| `starts_with`, `ends_with` | yes, from C++11 | C++20 | yes, from C++11 |
| `contains` | yes, from C++11 | C++23 | yes, from C++11 |
| `std::hash` | yes, one value in every standard (not the standard's) | yes (C++17), the library's value | `boost::hash` through `hash_value` |
| `operator<=>`, `std::ranges::view`, `borrowed_range` | yes (C++20) | yes (C++20) | no `<=>`, not a `std::ranges::view` or `borrowed_range` at C++20 (probe of the earlier README) |
| `std::string_view` conversions | implicit both ways at C++17 | n/a | none |
| `operator<<` with `setw` | honors width, fill and adjust | honors them | honors them |
| Wide view | `lumex_wstring_view` | `std::wstring_view` | `boost::wstring_view` |

Numbers (GCC 13.2.0, libstdc++; absolute times in ns per operation, ratio to `std::string_view` in parentheses; best of 5 passes):

| Scenario | lumex C++17 | `std::string_view` | `boost::string_view` C++17 |
| --- | --- | --- | --- |
| construct from a literal | 0.55 (1.01) | 0.55 | 0.55 (1.00) |
| construct from a `char const *` | 3.01 (1.15) | 2.61 | 2.72 (1.04) |
| construct from a `std::string` | 0.81 (1.00) | 0.81 | 0.81 (1.00) |
| construct from pointer and size | 0.73 (1.00) | 0.73 | 0.73 (1.00) |
| `find ('/')` | 1.61 (1.03) | 1.56 | 1.54 (0.99) |
| `find` of a 9-character view in a string of 35 | 5.27 (1.09) | 4.85 | 5.05 (1.04) |
| `find` of an 8-character view in a text of 4 096, first letter common | 1363 (0.95) | 1433 | 1437 (1.00) |
| `find` of an 8-character view in a text of 4 096, first letter absent | 27.99 (1.00) | 27.92 | 30.82 (1.10) |
| `substr (8, 12)` | 0.96 (1.03) | 0.93 | 0.95 (1.03) |
| `compare` of equal strings | 2.43 (0.93) | 2.60 | 2.02 (0.78) |
| `==` of equal strings | 1.94 (1.00) | 1.94 | 1.96 (1.01) |
| `==` with a literal | 0.28 (1.01) | 0.27 | 0.28 (1.02) |
| `starts_with` a literal (std at C++20) | 0.44 (1.02) | 0.43 | 0.44 (1.03) |
| `std::sort` of 256 views, per element | 26.39 (0.95) | 27.71 | 29.65 (1.07) |
| `std::map` lookup of a view | 27.83 (1.11) | 25.01 | 28.32 (1.13) |
| split a text of 4 096 at `,` | 2227 (1.02) | 2190 | 2162 (0.99) |
| hash of a string (C++11 / C++17) | 5.77 (1.35) / 5.79 (1.35) | 4.27 | 5.07 (1.19) |
| to / from `std::string_view` | 0.65 / 0.81 | a copy: 0.60 / 0.69 | not available |

Summary of the whole set (44 scenarios): the geometric mean of the time of `lumex_string_view` over `std::string_view` is 1.027 (C++17) and 1.009 (C++11, 40 scenarios); it is slower by more than 10 % in 8 scenarios at C++17 and 3 at C++11, and faster by more than 10 % in 1 (`starts_with`, 0.87x; boost shows the same); `boost::string_view` over `std::string_view` is 1.028. Of the C++17 build's deviations only `hash` (1.35x, 5.8 ns against 4.3 ns) is a property of the library: it is the library's own hash, the same value in every standard (before this change the C++11 hash was 3.23x). The others are most likely code placement: `index_sum` and `range_sum` are the same inline code in the C++11 and the C++17 build of the view and are 1.04x in one and 1.11x/1.12x in the other; `map_lookup` is 25.01 ns for `std::string_view` at C++17 and 28.63 ns at C++20 for the same code; `find_first_not_of` (1.12x) and `find_last_of` (1.12x) are placement and noise as well (see the benchmark README), and `from_std` (1.16x) is the 16-byte copy that GCC does with two 8-byte moves for this layout. Before the rework, when the view had a compiled library, the same figure was 1.55 (shared) and 1.53 (static), `find` of a view whose first letter is absent was 33.9x and a string literal 3.5x to 8.1x; the old numbers are kept in `benchmarks/string_view/results/*_before_rework.csv`. Compile time (`-fsyntax-only -O0`, machine busy): including the header costs +87 ms at C++11 and +137 ms at C++17, against +50 ms for `<string_view>` and +167 ms (C++11) and +272 ms (C++17) for Boost; a unit that uses the whole API compiled at `-O2` has 5666 bytes of `.text` (C++11) and 5757 (C++17) against 5779 for `std::string_view` and 8845 and 8939 for Boost.

**Clang 23.1.0 with libc++** (the same machine, flags, CPU and 5 passes; the ratios are to the `std::string_view` of libc++; the rows are in the `Build tree: clang` section of [benchmarks/string_view/results/string_view_benchmark.md](../../../benchmarks/string_view/results/string_view_benchmark.md)): the geometric mean of `lumex_string_view` over `std::string_view` is 1.056 at C++17 (44 scenarios) and 1.062 at C++11 (40 scenarios) (GCC: 1.027 and 1.009), slower by more than 10 % in 4 and 5 scenarios and faster in 3 and 3 (GCC: 8 and 3, 1 and 2); `boost::string_view` over `std::string_view` is 1.021 (GCC: 1.028). At C++17 the rows more than 10 % slower are `rfind_str` 2.35x (`boost::string_view` 2.54x), `find_first_of` 1.92x (Boost 0.89x), `hash` 1.73x (GCC: 1.35x; Boost 1.36x) and `ctor_cstr` 1.16x; faster are `split` 0.83x, `eq_size` 0.85x and `find_str_miss` 0.87x. The GCC rows `from_std`, `index_sum`, `range_sum`, `find_first_not_of`, `find_last_of` and `map_lookup` (1.11x to 1.16x) are within 7 % of `std::string_view` with Clang; no cause of the Clang rows was investigated. `sizeof` is 16 bytes and every trait is the same as with GCC. Compile time of the `include` unit (`-fsyntax-only -O0`): 321 ms (C++11) and 411 ms (C++17) for this library, 326 ms for `std::string_view` C++17 and 410 and 501 ms for Boost, over a stub of 213 and 274 ms; `use_o2` has 9034 and 9067 bytes of `.text` against 8015 for `std::string_view` C++17 and 9998 and 10051 for Boost.

## Strengths and weaknesses

Strengths:
- One class on every standard: the type and the overload sets of a text API are the same in C++11, C++14, C++17 and C++20, where `std::string_view` does not exist below C++17. The interface is the newest standard's (`starts_with`, `ends_with`, `contains`, `<=>`, `std::hash`, literals, ranges opt-ins) from C++11.
- Header-only: no library to build, link or deploy, no export macro, no dllimport/dllexport on Windows; the interface target only carries the include directory.
- On par with `std::string_view` in speed on this machine (geometric mean 1.01 to 1.03), including a string literal (the length is folded) and `find` of a view (the same `memchr` jump as libstdc++).
- `constexpr` for every query in C++11, and for the modifiers from C++14.
- Tested against `std::string_view` and `std::string` exhaustively on all short texts of an alphabet with a NUL and a high-bit byte, and on random texts (see Testing).
- A null pointer, `remove_prefix (n > size)` and `remove_suffix` are defined (an empty view, a clamp) where the standard leaves them undefined.

Weaknesses:
- Compiles slower than `<string_view>`: a class template with its algorithms (+137 ms against +50 ms at C++17 for the include unit, on a busy machine).
- The hash is the library's own, not `std::hash<std::string_view>`'s: a view and a `std::string` of equal text hash differently.
- `from_std` costs 0.81 ns against 0.69 for a copy of the standard view (two 8-byte moves where GCC uses one 16-byte move for the standard type).
- The `_sv` suffix is in a namespace that has to be opened (`using namespace lumex::core::string_view::view::literals;`); there is no global shortcut.
- The iterator-pair constructor accepts only pointers below C++20 (contiguity cannot be tested before concepts); the range constructor is duck-typed (`data ()` and `size ()`) below C++20.
- The aliases for `char16_t`, `char32_t` and `char8_t` are in the namespace only; no global alias, and no conversion to and from `std::u16string_view` and the others is tested beyond the hash.
- A `std::basic_string_view` with other traits is not accepted by the C++17 conversions; the narrow and the wide view do not convert to each other.
- Clang with libc++ and MSVC were not measured; the `find` of a view relies on how `char_traits::find` is implemented there.

## Testing

On GCC 13.2.0 Release `ctest -R string_view` runs the module's tests and all pass: 217 view tests at C++11, 218 at C++14, 237 at C++17 and 244 at C++20 (each suite also runs the files of the lower standards), 6 examples (`examples.string_view.*`), 7 `cmake.*` tests and the consumer fixtures `consumer_string_view_hash_standard_mismatch` (built with -O2; a hash that depends on the standard is caught at -O2 and missed at -O0 and -O1, where the linker keeps one weak copy of the function; the fixture also proves with `__cplusplus` that its two units differ), `consumer_string_view_no_exceptions_cxx11`, `_cxx17` and `consumer_string_view_exceptions_cxx11` (a copy of the header with the `throw` or the `try` unguarded fails to compile with `-fno-exceptions`), and `install_header_only_without_utility` (which now also builds `lumex::string_view` from a prefix without the utility module) (`cmake.wiring_string_view`, `cmake.wiring_string_view_export` and the `require_fail_*_without_string_view` cases of base64, crc, xml, exceptions and json). Before the rework there were 131, 142 and 142 view tests at C++11, C++17 and C++20. The same suites pass on GCC 8.3 (C++11, 17), on Clang 23 with libc++ and with libstdc++ (C++11, 20) and, built with AddressSanitizer and UndefinedBehaviorSanitizer on GCC 13 (C++11, 20), without a report.

The test files, in `lumex/tests/core/string_view/view/`:
- `LumexStringView.cxx11.tests.cpp`, `LumexWStringView.cxx11.tests.cpp`, `LumexStringViewImplicit`, `LumexStringViewParameter`: the class, the implicit conversions and the view as a text parameter (unchanged).
- `LumexStringViewSearch.cxx11.tests.cpp`: all six search families (view, character, C string with and without a count) against the `std::basic_string` members, for every text of an alphabet with `a`, `b`, a NUL and a high-bit byte up to 5 characters (4 for the wide view) against every needle up to 3 characters at every position from 0 to size + 2 and at `npos`, then 1500 random texts of each width; plus the fixed values of the edge cases (empty needle, needle longer than the text, `npos`, a position past the end, the first letter absent, a null view). The texts are exact-size heap copies, so a read past the end is an AddressSanitizer error.
- `LumexStringViewOrdering.cxx11.tests.cpp`: the six operators and every `compare` overload (including the exceptions) against `std::basic_string::compare` over all short texts; the unsigned order, the embedded NUL and the size tie-break as fixed values. `LumexStringViewOrdering.cxx20.tests.cpp`: `operator<=>` against `std::string_view`'s, its type, `std::sort`.
- `LumexStringViewApi.cxx11.tests.cpp`: `contains`, the C string overloads, the null-pointer views through every member, the range and the iterator-pair constructors with the types they accept and refuse (static asserts), the literals, the other character types.
- `LumexStringViewHash.cxx11.tests.cpp`: pinned hash values, no collision among all strings of up to 3 characters over 16, an avalanche check (one flipped bit changes 30 to 34 of 64 bits on average), the wide types alike for equal code points, the null view, use as an `unordered_map` key; the consumer fixture `consumer_string_view_hash_standard_mismatch` compares the values of a C++11 translation unit with a C++17 and a C++20 program.
- `LumexStringViewStream.cxx11.tests.cpp`: `operator<<` against `std::string`'s over widths, fills and adjust fields, the width reset, a failed stream, a buffer that fails, a buffer that throws (with and without `exceptions (badbit)`); `LumexStringViewStdDifferential.cxx17.tests.cpp`: the same probe against `std::string_view`'s and `std::wstring_view`'s, and every query against the standard view on every short text.
- `LumexStringViewConstexpr.cxx11/14/17.tests.cpp`: static asserts of every query under the rules of C++11 (including 1000-character and 400-wide-character texts), of the modifiers in C++14, of the reverse iterators and the standard-view conversions in C++17. `LumexStringViewRanges.cxx20.tests.cpp`: `std::ranges::view`, `borrowed_range`, the contiguous iterators accepted and refused by the constructor.

Every new test was falsified: 114 deliberate corruptions of a copy of `LumexBasicStringView.hpp` (each new member, the hash rounds, constants, tail, length and wide handling, the order and the six operators, the default category of `<=>`, the search algorithms at their edges, `operator<<` including the thread-cancellation rethrow, the conversions, the null-pointer cases, `constexpr` per standard including the wide length remainders, the literals, the constructors and the implicit range constructor) were compiled against the tests placed first on the include path; all 114 are caught (a failing test, a crash, a hang or a failing `static_assert`).


## Not done on purpose / limits / known issues

- **Formal ODR deviation of the class.** Some members of `lumex_basic_string_view` have a different text in different standards, so a program that links a translation unit of C++11 with one of C++20 has weak (inline) definitions of the same mangled name with different bodies: the constructor from and the conversion to `std::string_view` (C++17 only), `operator<=>` (C++20 only), the two iterator-pair constructors (pointers below C++20, concepts from C++20), which members are `constexpr` (C++14 and C++17 add it), the out-of-class definition of `npos` (below C++17) and the `argument_type` typedefs of `std::hash` (below C++17). The class cannot get a per-standard inline namespace: that would change the mangled name of `lumex_string_view` with the standard and break the stable interface of the compiled modules (base64, crc, exceptions, xml, ...) that take a view by value. The deviation is accepted because every pair of bodies is observably equivalent (the same results; the extra members exist only in the standards that have them and a translation unit that does not see one does not call it). A link of a C++11 and a C++20 unit with GCC 13 `-flto -Wodr` is silent. The helpers whose bodies do differ in behavior (the scans, the character functions, the search) are NOT shared: they live in an inline namespace named after the language tier (`detail::tier_recursion` in C++11, `detail::tier_loops` in C++14 and C++17 on a standard library with unreliable constexpr `char_traits`, `detail::tier_traits` otherwise), so two standards never share a symbol with different bodies (checked with `nm` on C++11, 14, 17 and 20 objects).
- No `[[gsl::Pointer]]` on the class: Clang's `-Wdangling-gsl` does not warn for a view of a temporary string.
- The range constructor probes `data ()` and `size ()` on a `const` range only: a range type whose `data ()` exists only on a non-const object is not accepted.
- `max_size ()` is `npos / 2` for every character type, which is larger than the bytes available for a wide view (libstdc++'s `std::string_view::max_size ()` takes `sizeof (CharT)` into account).
- `cond ? lumex_view : std_view` (C++17) is ambiguous: each converts to the other; cast one side.
- The class has `std::char_traits` as a template argument, so argument-dependent lookup also searches namespace `std` for an unqualified call with a view (`begin (view)`, `swap (a, b)` and the like find the standard's templates as well).
- There are no deduction guides: `lumex_basic_string_view v (first, last)` does not deduce `CharT` (spell the alias).
- There is no `std::formatter` specialization: with libc++ at C++23 `std::format ("{}", lumex_view)` formats the view as a range of characters (a list), not as text. The library's own `fmt` (core/fmt) formats views as text.
- A forward declaration `class lumex_string_view;` no longer compiles: the name is an alias of a class template (include the header, or forward-declare `lumex_basic_string_view`).
- The `_sv` literals are not exported to the global namespace; no `operator""_sv` for `char8_t` before C++20.
- `std::hash` of a view is not the value of `std::hash<std::string>` or `std::hash<std::string_view>`: one value in every standard needs a hash of the library's own.
- The `nullptr_t` constructor of `std::basic_string_view` (deleted in C++23) is not deleted here: `lumex_string_view (nullptr)` is an empty view, and existing code passes `nullptr` for a missing text.
- Contiguity of an iterator pair is checked only from C++20 (pointers only before).
- Constant evaluation in C++11 of a wide C string (`wchar_t`, `char16_t`, `char32_t`) recurses four characters per step, so the compiler's constexpr depth (512 by default) limits it: a literal of 2000 characters works and one of 2040 or more does not, on GCC 13, GCC 8 and Clang 23 alike. In a plain `constexpr` context that is a hard error; where the expression is a SFINAE context it is a silent substitution failure. Raise `-fconstexpr-depth` (`=1024` is enough for 2100 characters, `=4096` was checked up to 5000 on all three compilers; Clang's `-fconstexpr-steps` does not help, the limit is the depth) or construct the view from a pointer and a size. A narrow string uses `__builtin_strlen` and has no such limit, and so do the searches (they recurse over halves, a depth of the logarithm of the length).
- Compilers: the choice between the constant-expression path and `char_traits` uses `__builtin_is_constant_evaluated` (GCC 9, Clang 9, MSVC 19.25). GCC 8 uses `__builtin_constant_p`, which is right in the tests; a call whose size the optimizer proves constant takes the recursion at run time too (correct, slower). Clang before 9 has neither and the library falls back to "always the constant-expression path" (not `__builtin_constant_p` as GCC 8 does), so in C++11 and C++14 every call there runs the recursion (C++11) or the plain loop (C++14) at run time; it is correct, with the searches at logarithmic depth and the wide length at one frame per four characters.
- The decision 97a (the Windows export of the six inline comparison operators through a per-module macro) is obsolete: the operators are inline members of a header-only class, and the macro `LUMEX_STRING_VIEW_API` is removed.
- MinGW and MSVC: the headers are compiled by MinGW in the release matrix only; MSVC was not tested.
