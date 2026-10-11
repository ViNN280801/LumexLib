# string_view benchmark results

- machine="12th Gen Intel(R) Core(TM) i7-12700K" os="Linux 6.1.170-1-generic" logical_cpus=20 python=3.7.3
- passes=5 cpu=3
- variant gcc lumex_cxx11: compiler="GCC 13.2.0" library="libstdc++" cplusplus=201103
- variant gcc lumex_cxx17: compiler="GCC 13.2.0" library="libstdc++" cplusplus=201703
- variant gcc std_cxx17: compiler="GCC 13.2.0" library="libstdc++" cplusplus=201703
- variant gcc std_cxx20: compiler="GCC 13.2.0" library="libstdc++" cplusplus=202002
- variant gcc boost_cxx11: compiler="GCC 13.2.0" library="libstdc++" cplusplus=201103
- variant gcc boost_cxx17: compiler="GCC 13.2.0" library="libstdc++" cplusplus=201703

Time per operation in nanoseconds: the lowest median of 15 repetitions over all passes (the best pass), and in parentheses the ratio to `std::string_view` at C++17 (below 1.00 is faster than that; for `starts_with` and `ends_with`, which the standard has from C++20, the ratio is to `std::string_view` at C++20). The unit of an operation is the description of the scenario. A dagger marks a value whose slowest pass was more than 25 % slower than the best one: the processes of that executable do not agree (code and stack placement), so differences inside that band are noise.

## Build tree: gcc

| scenario | operation | std C++17 | std C++20 | lumex C++11 | lumex C++17 | boost C++11 | boost C++17 |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| ctor_literal | construct from a string literal (length known to the compiler) | 0.57 | 0.62 (1.10) | 0.59 (1.04) | 0.58 (1.03) | 0.62 (1.09) | 0.58 (1.03) |
| ctor_cstr | construct from a char const * (length by strlen) | 2.80 | 2.83 (1.01) | 2.88 (1.03) | 2.99 (1.07) | 2.79 (1.00) | 2.90 (1.04) |
| ctor_string | construct from a std::string | 0.84 | 0.86 (1.02) | 0.84 (1.00) | 0.86 (1.02) | 0.86 (1.03) | 0.85 (1.01) |
| ctor_ptr_size | construct from pointer and size | 0.77 | 0.77 (1.00) | 0.76 (1.00) | 0.76 (1.00) | 0.76 (0.99) | 0.76 (1.00)† |
| copy | copy construct a view | 0.64 | 0.63 (0.98) | 0.63 (0.98) | 0.63 (0.99) | 0.65 (1.02) | 0.63 (0.99) |
| index_sum | sum of the characters of one string of 30 to 33, by operator[] | 9.18 | 9.89 (1.08) | 9.99 (1.09) | 9.92 (1.08) | 9.85 (1.07) | 10.02 (1.09) |
| range_sum | sum of the characters of one string of 30 to 33, by range-for | 9.27 | 9.63 (1.04) | 10.26 (1.11) | 9.70 (1.05) | 9.76 (1.05) | 9.84 (1.06) |
| find_char | find('/') in one string, hit at position 7 | 1.72† | 1.70 (0.99) | 1.72 (1.00)† | 1.75 (1.02) | 1.70 (0.99) | 1.68 (0.98) |
| find_char_late | find('.') in one string, hit near the end | 2.80 | 2.77 (0.99) | 2.72 (0.97) | 2.74 (0.98) | 2.74 (0.98) | 2.77 (0.99) |
| find_char_miss | find('#') in one string, no hit | 3.21 | 3.06 (0.95) | 3.04 (0.95) | 3.11 (0.97) | 3.05 (0.95) | 2.93 (0.91) |
| rfind_char | rfind('/') in one string | 7.82 | 7.82 (1.00) | 7.74 (0.99) | 7.97 (1.02) | 11.07 (1.41) | 7.14 (0.91) |
| find_str_hit | find a 9-character view in one string, hit at 17 | 5.09 | 5.91 (1.16) | 5.34 (1.05) | 5.33 (1.05) | 5.40 (1.06) | 5.38 (1.06) |
| find_str_miss | find an 8-character view in one string, no hit | 8.28 | 8.34 (1.01) | 7.46 (0.90) | 8.16 (0.99) | 8.00 (0.97) | 8.46 (1.02) |
| find_str_text | find an 8-character view in a text of 4096 characters, no hit; its first letter occurs at every 16th position | 1482 | 1484 (1.00) | 1335 (0.90) | 1332 (0.90) | 1398 (0.94) | 1484 (1.00) |
| find_str_absent | find an 8-character view in a text of 4096 characters, no hit; its first letter does not occur | 29.02 | 28.93 (1.00) | 30.26 (1.04) | 29.08 (1.00) | 31.71 (1.09) | 31.60 (1.09) |
| rfind_str | rfind a 9-character view in one string | 13.91 | 14.15 (1.02) | 14.56 (1.05) | 14.67 (1.05) | 13.59 (0.98) | 13.76 (0.99) |
| find_first_of | find_first_of a set of 3 characters in one string | 19.72 | 18.28 (0.93) | 21.95 (1.11) | 19.57 (0.99) | 16.96 (0.86) | 18.92 (0.96) |
| find_last_of | find_last_of a set of 3 characters in one string | 5.60 | 5.54 (0.99) | 5.26 (0.94) | 6.23 (1.11) | 4.52 (0.81) | 7.49 (1.34) |
| find_first_not_of | find_first_not_of a set of 10 characters in one string | 20.64 | 21.17 (1.03) | 21.12 (1.02) | 23.55 (1.14) | 23.63 (1.14) | 20.86 (1.01) |
| substr | substr(8, 12) of one string | 1.02 | 0.97 (0.96) | 1.02 (1.00) | 1.00 (0.98) | 1.00 (0.98) | 1.00 (0.99) |
| substr_tail | substr(size - 4) of one string | 0.83 | 0.84 (1.02) | 0.84 (1.01) | 0.84 (1.01) | 0.84 (1.02) | 0.87 (1.05) |
| compare_equal | compare two equal strings in different buffers | 2.80 | 2.66 (0.95) | 2.55 (0.91) | 2.55 (0.91) | 2.16 (0.77) | 2.18 (0.78) |
| compare_early | compare two strings that differ in the first character | 1.86 | 1.85 (1.00) | 1.86 (1.00) | 1.87 (1.00) | 1.77 (0.95) | 1.78 (0.96) |
| compare_late | compare two strings that differ in the last character | 3.16 | 3.20 (1.01) | 3.21 (1.02) | 3.25 (1.03) | 3.17 (1.01) | 3.20 (1.01) |
| eq_equal | operator== of two equal strings in different buffers | 2.04 | 2.08 (1.02) | 2.06 (1.01) | 2.09 (1.02) | 2.07 (1.01) | 2.09 (1.02) |
| eq_size | operator== of two strings of different sizes | 0.28 | 0.28 (1.00) | 0.28 (1.00) | 0.28 (0.99) | 0.30 (1.05) | 0.31 (1.11) |
| eq_late | operator== of two strings of one size that differ in the last character | 3.02 | 3.02 (1.00) | 3.03 (1.00) | 3.02 (1.00) | 3.22 (1.07) | 3.25 (1.08) |
| less | operator< of two strings that differ in the last character | 2.11 | 2.11 (1.00) | 2.08 (0.99) | 2.12 (1.01) | 2.83 (1.34) | 2.97 (1.41) |
| eq_literal | operator== of a view and a string literal | 0.29 | 0.29 (1.00) | 0.29 (1.00) | 0.29 (1.00) | 0.29 (1.02) | 0.29 (1.00) |
| pass_by_value | call a noinline function that takes the view by value | 0.72 | 0.72 (1.00) | 0.70 (0.97) | 0.71 (0.99) | 0.71 (1.00) | 0.72 (1.00) |
| return_value | call a noinline function that returns a view built from pointer and size | 0.86 | 0.87 (1.01) | 0.89 (1.02) | 0.87 (1.00) | 0.86 (1.00) | 0.87 (1.01) |
| sort | std::sort of 256 views (per element) | 29.52 | 27.02 (0.92) | 27.47 (0.93) | 28.42 (0.96) | 30.08 (1.02) | 31.72 (1.07) |
| map_lookup | find in a std::map of 256 views by an equal view | 26.16 | 29.81 (1.14) | 29.12 (1.11) | 28.99 (1.11) | 28.71 (1.10) | 29.19 (1.12) |
| split | split a text of 4096 characters at ',' into views (about 450 fields; one split) | 2153 | 2198 (1.02) | 2191 (1.02) | 2242 (1.04) | 2157 (1.00) | 2177 (1.01) |
| starts_with | starts_with a view in one string |  | 2.30 | 2.02 (0.88) | 2.01 (0.87) | 2.02 (0.88) | 1.98 (0.86) |
| ends_with | ends_with a view in one string |  | 1.80 | 1.78 (0.99) | 1.78 (0.99) | 1.81 (1.01) | 1.80 (1.00) |
| starts_with_lit | starts_with a string literal |  | 0.46 | 0.47 (1.02) | 0.46 (0.99) | 0.45 (0.98) | 0.46 (1.00) |
| ends_with_lit | ends_with a string literal |  | 0.57 | 0.57 (0.99) | 0.57 (0.99) | 0.56 (0.97) | 0.57 (0.99) |
| starts_with_chr | starts_with a character |  | 0.46 | 0.47 (1.03) | 0.45 (0.98) | 0.46 (1.01) | 0.47 (1.01) |
| hash | hash one string (the hash of each library: std::hash, boost::hash) | 4.66 | 4.71 (1.01) | 15.07 (3.23) | 4.66 (1.00) | 5.51 (1.18) | 5.53 (1.19) |
| to_std | construct a std::string_view from the view (the conversion) | 0.64 | 0.63 (0.99) |  | 0.70 (1.09) |  |  |
| from_std | construct the view from a std::string_view (the conversion) | 0.74 | 0.74 (1.00) |  | 0.87 (1.17) |  |  |
| eq_std | operator== of the view and a std::string_view | 1.79 | 1.84 (1.03) |  | 1.84 (1.03) |  |  |
| less_std | operator< of the view and a std::string_view | 2.03 | 2.01 (0.99) |  | 2.00 (0.99) |  |  |

Variants compared with each other (time of the first divided by the time of the second; the first is faster below 1.00): geometric mean over the scenarios both have, the best and the worst scenario of the first, and the number of scenarios where the first is slower or faster by more than 10 %:

| compared | scenarios | geometric mean | slower | faster | best | worst |
| --- | ---: | ---: | ---: | ---: | --- | --- |
| lumex C++17 / std C++17 | 44 | 1.012 | 4 | 2 | starts_with 0.87x | from_std 1.17x |
| lumex C++11 / std C++17 | 40 | 1.031 | 4 | 1 | starts_with 0.88x | hash 3.23x |
| lumex C++11 / lumex C++17 | 40 | 1.023 | 2 | 2 | find_last_of 0.84x | hash 3.23x |
| lumex C++17 / boost C++17 | 40 | 0.983 | 3 | 6 | less 0.71x | compare_equal 1.17x |
| lumex C++11 / boost C++11 | 40 | 1.016 | 4 | 3 | rfind_char 0.70x | hash 2.74x |
| boost C++17 / std C++17 | 40 | 1.024 | 5 | 2 | compare_equal 0.78x | less 1.41x |

- lumex C++17 is slower than std C++17 by more than 10 % in: from_std 1.17x, find_first_not_of 1.14x, find_last_of 1.11x, map_lookup 1.11x
- lumex C++17 is faster than std C++17 by more than 10 % in: starts_with 0.87x, find_str_text 0.90x
- boost C++17 is slower than std C++17 by more than 10 % in: less 1.41x, find_last_of 1.34x, hash 1.19x, map_lookup 1.12x, eq_size 1.11x
- boost C++17 is faster than std C++17 by more than 10 % in: compare_equal 0.78x, starts_with 0.86x

## Layout and traits

`sizeof` in bytes and the properties of the view type. TC: trivially copyable, TD: trivially destructible, SL: standard layout, NC: nothrow copy constructible, S: implicit from `std::string const &`, C: implicit from `char const *`, STR: explicit conversion to `std::string` possible (`std::string (v)`), IMP: implicit conversion to `std::string`, TO/FROM: implicit conversion to/from `std::string_view` (the C++17 builds; 'n' in the C++11 builds only says there is no standard view there). The values do not depend on the build tree.

| view | variant | sizeof | TC | TD | SL | NC | S | C | STR | IMP | TO | FROM |
| --- | --- | ---: | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `string_view` | lumex C++11 | 16 | Y | Y | Y | Y | Y | Y | Y | n | n | n |
| `wstring_view` | lumex C++11 | 16 | Y | Y | Y | Y | Y | Y | Y | n | n | n |
| `string_view` | lumex C++17 | 16 | Y | Y | Y | Y | Y | Y | Y | n | Y | Y |
| `wstring_view` | lumex C++17 | 16 | Y | Y | Y | Y | Y | Y | Y | n | Y | Y |
| `string_view` | std C++17 | 16 | Y | Y | Y | Y | Y | Y | Y | n | Y | Y |
| `wstring_view` | std C++17 | 16 | Y | Y | Y | Y | Y | Y | Y | n | Y | Y |
| `string_view` | std C++20 | 16 | Y | Y | Y | Y | Y | Y | Y | n | Y | Y |
| `wstring_view` | std C++20 | 16 | Y | Y | Y | Y | Y | Y | Y | n | Y | Y |
| `string_view` | boost C++11 | 16 | Y | Y | Y | Y | Y | Y | Y | n | n | n |
| `wstring_view` | boost C++11 | 16 | Y | Y | Y | Y | Y | Y | Y | n | n | n |
| `string_view` | boost C++17 | 16 | Y | Y | Y | Y | Y | Y | Y | n | n | n |
| `wstring_view` | boost C++17 | 16 | Y | Y | Y | Y | Y | Y | Y | n | n | n |

## Compile time

- compiler gcc: g++ (GCC) 13.2.0

Wall time of `compile_time.cpp`, median of the repeats, in milliseconds. `no view` is the same unit with a stub instead of a view: the cost of the shared headers (`<string>`, `<vector>`, `<map>`, `<algorithm>`). The unit of `std C++17` leaves `starts_with` and `ends_with` out (they are C++20), so it does a little less than the others.

Unit `include`: the header is included and one object is declared (`-fsyntax-only -O0`).

| toolchain | no view C++11 | no view C++17 | no view C++20 | std C++17 | std C++20 | lumex C++11 | lumex C++17 | boost C++11 | boost C++17 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| gcc | 126 | 162 | 241 | 216 | 294 | 211 | 296 | 285 | 433 |

Unit `use`: the whole API is used: construction, observers, searches, comparisons, slicing, `std::sort`, `std::map`, hashing (`-fsyntax-only -O0`).

| toolchain | no view C++11 | no view C++17 | no view C++20 | std C++17 | std C++20 | lumex C++11 | lumex C++17 | boost C++11 | boost C++17 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| gcc | 127 | 166 | 239 | 244 | 311 | 237 | 336 | 315 | 474 |

Unit `use_o2`: the same use, compiled (`-c -O2`); the last column group is the `.text` of the object in bytes.

| toolchain | no view C++11 | no view C++17 | no view C++20 | std C++17 | std C++20 | lumex C++11 | lumex C++17 | boost C++11 | boost C++17 | no view C++11 .text | no view C++17 .text | no view C++20 .text | std C++17 .text | std C++20 .text | lumex C++11 .text | lumex C++17 .text | boost C++11 .text | boost C++17 .text |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| gcc |  |  |  | 464 | 545 | 433 | 562 | 585 | 744 |  |  |  | 5779 | 7019 | 5381 | 5539 | 8845 | 8939 |

