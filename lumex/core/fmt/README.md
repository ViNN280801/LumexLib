# LumexFormat: std::format-style formatting from C++11 {#lumex_fmt}

`lumex::core::fmt` (target `lumex::fmt`, header-only) builds text from a
format string with replacement fields and the standard specification
mini-language `[[fill]align][sign][#][0][width][.precision][L][type]`. It is
written for LumexLib from scratch, follows `std::format` exactly where the
standard defines the output, works from C++11 on and checks literal format
strings at compile time from C++20 on.

```cpp
#include "lumex/core/fmt/LumexFormat"

namespace fmt = lumex::core::fmt;

std::string const row = fmt::format ("{:<14}|{:>10.3f}|{:#06x}", name, rt, flags);
std::string const line = fmt::format ("run {run}: {elapsed:%M:%S}",
                                      fmt::arg ("run", 17),
                                      fmt::arg ("elapsed", std::chrono::seconds (754)));
std::string const list = fmt::format ("{}", std::vector<int>{ 1, 2, 3 }); // [1, 2, 3]
```

## Headers

| Header | Contents |
| --- | --- |
| `lumex/core/fmt/LumexFormat.hpp` | Core: every built-in type, `wchar_t`, named arguments, `L`, reflected enums, `Formatter<T>`, `OstreamFormatter`, `streamed`, the output API |
| `lumex/core/fmt/LumexFormatRanges.hpp` | Ranges `[1, 2]`, sets `{1, 2}`, maps `{"a": 1}`, `std::pair` / `std::tuple` `(1, "a")` with the C++23 range specification |
| `lumex/core/fmt/LumexFormatChrono.hpp` | `std::chrono::duration` and `system_clock` time points with the chrono specification (`%F %T`, `%H:%M:%S`, ...) |
| `lumex/core/fmt/LumexFormat` | Umbrella: all three |

## Output API

| Function | Result |
| --- | --- |
| `format (fmt, args...)` | `std::string` (`std::wstring` for `L"..."`); an overload takes a `std::locale` first |
| `format_to (out, fmt, args...)` | writes to an output iterator, returns the iterator past the output |
| `format_to_n (out, n, fmt, args...)` | at most `n` characters, `{out, size}` with the full size |
| `formatted_size (fmt, args...)` | the length, nothing is written |
| `vformat`, `vformat_to` | type-erased arguments from `make_format_args` |
| `runtime (text)` | a format string known only at run time |
| `try_format (fmt, args...)` | `noexcept`: `{text, success, error}` |
| `print`, `println` | to a `std::ostream` / `std::wostream` or `std::cout` |

Errors throw `lumex::core::fmt::FormatError` (a `std::runtime_error` with
the offset of the failing field in `position ()`). From C++20 a literal
format string that does not match its arguments does not compile.

## Behaviour

- **Standard output.** Where `std::format` defines the text, LumexFormat
  produces the same bytes: shortest round-trip floats for `{}`
  (`1e-04`, `123456792` for `123456789.0f`), `a` without `0x`, a sign on
  unsigned integers, zero padding of pointers after `0x`, width and
  precision measured in extended grapheme clusters (combining marks, emoji
  sequences and flags count as one cluster). fmt differs in some of these
  points; the standard wins.
- **No locale leaks.** Without `L` the output never depends on the C or C++
  locale. `L` groups the digits of every integer presentation and uses the
  decimal point and bool names of the locale.
- **Chrono.** Negative durations print one leading `-`, `duration::min ()`
  included; a floating-point duration too large for whole seconds throws
  instead of printing garbage; far dates use the proleptic Gregorian
  calendar (year 146138514283 and year -2937836 print correctly).
- **Customization.** Specialize `lumex::core::fmt::Formatter<T>`; inherit a
  built-in formatter to reuse its specification. Types with `operator<<`
  opt in through `OstreamFormatter` or `streamed (value)`.

## Examples

Six programs in `lumex/examples/fmt/` show every public function, type and
specification option (a CTest check, `LumexFormatExamplesCoverage`, keeps
it that way):

| Example | Shows |
| --- | --- |
| `example_format.cpp` | Replacement fields, the whole specification mini-language, every built-in type, grapheme width, `L` |
| `example_format_api.cpp` | Every output function in its `char` and `wchar_t` form, type-erased arguments, locales, run-time strings, `FormatError`, `try_format`, `print` / `println`, user functions taking `FormatString<Args...>` |
| `example_format_ranges.cpp` | Sequences, sets, maps, nesting, `n` / `m` / `s` / `?s`, element specifications, `pair` / `tuple`, customizing a range formatter (`set_brackets`, `set_separator`, `underlying ()`) |
| `example_format_chrono.cpp` | Durations and time points, every conversion and modifier, `L` |
| `example_format_custom.cpp` | `Formatter<T>` reusing a built-in specification or parsing its own, a formatter for every character type, `OstreamFormatter`, `streamed`, reflected enums |
| `example_format_workflow.cpp` | A measurement report and safe log lines |

## How it is verified

- Three suites run the same sources at C++11, C++17 and C++20
  (`LumexFormatTests`, `LumexFormatCxx17Tests`, `LumexFormatCxx20Tests`),
  with the input / output cases of fmt's `format-test.cc` ported and the
  expectations checked against `std::format`.
- Differential fuzzing (`LumexFormatDifferential.tests.cpp`, C++20): about
  120 000 random specifications, valid and invalid, applied to edge-case
  integers, floating-point values, strings, characters, `bool` and pointers;
  LumexFormat must produce exactly `std::format`'s text or fail exactly when
  it fails.
- Compile-fail checks (`LumexCMake.format_compile_checks`): invalid literal
  format strings must not compile at C++20.

## Benchmarks

Median time per call of the same formatting work with LumexFormat,
`std::format`, `std::ostringstream`, `std::snprintf` and `std::to_string`
(MSVC 19.51, x64, Release; whiskers show the minimum and maximum of 15
repetitions of 200 000 calls). Every method prints identical text, which
the benchmark checks before timing.

![Time per call, log scale](../../../benchmarks/fmt/results/format_benchmark.svg)

![Time relative to LumexFormat](../../../benchmarks/fmt/results/format_benchmark_relative.svg)

- 2 to 6 times faster than `std::ostringstream` in every scenario it
  takes part in.
- Faster than MSVC's `std::format` for hexadecimal numbers and padded
  strings; 1.2 to 1.5 times slower for decimal integers, floating point and
  lines with many fields.
- `snprintf` stays the fastest for plain numbers, without type safety or
  compile-time checks.

The numbers, the method and how to regenerate the charts:
`benchmarks/fmt/README.md` and `benchmarks/fmt/results/format_benchmark.md`
in the repository.
