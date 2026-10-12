# string_view benchmark results

- machine="12th Gen Intel(R) Core(TM) i7-12700K" os="Linux 6.1.170-1-generic" logical_cpus=20 python=3.7.3
- passes=5 cpu=3
- variant gcc lumex_cxx11: compiler="GCC 13.2.0" library="libstdc++" cplusplus=201103
- variant gcc lumex_cxx17: compiler="GCC 13.2.0" library="libstdc++" cplusplus=201703
- variant gcc std_cxx17: compiler="GCC 13.2.0" library="libstdc++" cplusplus=201703
- variant gcc std_cxx20: compiler="GCC 13.2.0" library="libstdc++" cplusplus=202002
- variant gcc boost_cxx11: compiler="GCC 13.2.0" library="libstdc++" cplusplus=201103
- variant gcc boost_cxx17: compiler="GCC 13.2.0" library="libstdc++" cplusplus=201703
- variant clang lumex_cxx11: compiler="Clang 23.1.0" library="libc++" cplusplus=201103
- variant clang lumex_cxx17: compiler="Clang 23.1.0" library="libc++" cplusplus=201703
- variant clang std_cxx17: compiler="Clang 23.1.0" library="libc++" cplusplus=201703
- variant clang std_cxx20: compiler="Clang 23.1.0" library="libc++" cplusplus=202002
- variant clang boost_cxx11: compiler="Clang 23.1.0" library="libc++" cplusplus=201103
- variant clang boost_cxx17: compiler="Clang 23.1.0" library="libc++" cplusplus=201703

Time per operation in nanoseconds: the lowest median of 15 repetitions over all passes (the best pass), and in parentheses the ratio to `std::string_view` at C++17 (below 1.00 is faster than that; for `starts_with` and `ends_with`, which the standard has from C++20, the ratio is to `std::string_view` at C++20). The unit of an operation is the description of the scenario. A dagger marks a value whose slowest pass was more than 25 % slower than the best one: the processes of that executable do not agree (code and stack placement), so differences inside that band are noise.

## Build tree: gcc

| scenario | operation | std C++17 | std C++20 | lumex C++11 | lumex C++17 | boost C++11 | boost C++17 |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| ctor_literal | construct from a string literal (length known to the compiler) | 0.55 | 0.59 (1.07) | 0.56 (1.02) | 0.55 (1.01) | 0.58 (1.07) | 0.55 (1.00) |
| ctor_cstr | construct from a char const * (length by strlen) | 2.61 | 2.68 (1.03) | 2.69 (1.03) | 3.01 (1.15) | 2.68 (1.03) | 2.72 (1.04) |
| ctor_string | construct from a std::string | 0.81 | 0.81 (1.00) | 0.81 (1.00) | 0.81 (1.00) | 0.80 (0.99) | 0.81 (1.00) |
| ctor_ptr_size | construct from pointer and size | 0.73 | 0.73 (1.00) | 0.73 (1.00) | 0.73 (1.00) | 0.73 (1.00) | 0.73 (1.00) |
| copy | copy construct a view | 0.60 | 0.59 (0.98) | 0.59 (0.99) | 0.59 (0.99) | 0.60 (1.00) | 0.60 (1.00) |
| index_sum | sum of the characters of one string of 30 to 33, by operator[] | 8.69 | 9.08 (1.05) | 9.08 (1.04) | 9.62 (1.11) | 9.02 (1.04) | 9.31 (1.07) |
| range_sum | sum of the characters of one string of 30 to 33, by range-for | 8.66 | 9.09 (1.05) | 9.03 (1.04) | 9.68 (1.12) | 9.07 (1.05) | 9.11 (1.05) |
| find_char | find('/') in one string, hit at position 7 | 1.56 | 1.57 (1.01) | 1.58 (1.01) | 1.61 (1.03) | 1.55 (0.99) | 1.54 (0.99) |
| find_char_late | find('.') in one string, hit near the end | 2.71 | 2.60 (0.96) | 2.55 (0.94) | 2.54 (0.94) | 2.58 (0.95) | 2.60 (0.96) |
| find_char_miss | find('#') in one string, no hit | 3.00 | 2.82 (0.94) | 2.82 (0.94) | 2.97 (0.99) | 2.82 (0.94) | 2.78 (0.92) |
| rfind_char | rfind('/') in one string | 7.37 | 7.41 (1.01) | 7.54 (1.02) | 7.54 (1.02) | 11.06 (1.50) | 6.76 (0.92) |
| find_str_hit | find a 9-character view in one string, hit at 17 | 4.85 | 5.52 (1.14) | 5.10 (1.05) | 5.27 (1.09) | 5.19 (1.07) | 5.05 (1.04) |
| find_str_miss | find an 8-character view in one string, no hit | 7.88 | 8.06 (1.02) | 7.15 (0.91) | 7.79 (0.99) | 7.66 (0.97) | 8.36 (1.06) |
| find_str_text | find an 8-character view in a text of 4096 characters, no hit; its first letter occurs at every 16th position | 1433 | 1427 (1.00) | 1279 (0.89) | 1363 (0.95) | 1371 (0.96) | 1437 (1.00) |
| find_str_absent | find an 8-character view in a text of 4096 characters, no hit; its first letter does not occur | 27.92 | 27.94 (1.00) | 29.34 (1.05) | 27.99 (1.00) | 30.84 (1.10) | 30.82 (1.10) |
| rfind_str | rfind a 9-character view in one string | 13.63 | 13.62 (1.00) | 14.12 (1.04) | 14.09 (1.03) | 12.95 (0.95) | 13.13 (0.96) |
| find_first_of | find_first_of a set of 3 characters in one string | 17.83 | 17.67 (0.99) | 21.71 (1.22) | 18.80 (1.05) | 16.52 (0.93) | 18.41 (1.03) |
| find_last_of | find_last_of a set of 3 characters in one string | 5.06 | 5.07 (1.00) | 4.72 (0.93) | 5.65 (1.12) | 4.02 (0.79) | 6.91 (1.37) |
| find_first_not_of | find_first_not_of a set of 10 characters in one string | 19.98 | 20.23 (1.01) | 20.45 (1.02) | 22.35 (1.12) | 23.02 (1.15) | 20.07 (1.00) |
| substr | substr(8, 12) of one string | 0.93 | 0.93 (1.00) | 0.96 (1.03) | 0.96 (1.03) | 0.94 (1.01) | 0.95 (1.03) |
| substr_tail | substr(size - 4) of one string | 0.78 | 0.79 (1.01) | 0.78 (1.01) | 0.78 (1.00) | 0.78 (1.00) | 0.81 (1.04) |
| compare_equal | compare two equal strings in different buffers | 2.60 | 2.51 (0.97) | 2.43 (0.93) | 2.43 (0.93) | 2.01 (0.77) | 2.02 (0.78) |
| compare_early | compare two strings that differ in the first character | 1.74 | 1.73 (0.99) | 1.75 (1.00) | 1.76 (1.01) | 1.66 (0.95) | 1.66 (0.95) |
| compare_late | compare two strings that differ in the last character | 3.00 | 3.07 (1.02) | 3.10 (1.03) | 3.07 (1.02) | 3.05 (1.01) | 3.03 (1.01) |
| eq_equal | operator== of two equal strings in different buffers | 1.94 | 1.94 (1.00) | 1.94 (1.00) | 1.94 (1.00) | 1.96 (1.01) | 1.96 (1.01) |
| eq_size | operator== of two strings of different sizes | 0.27 | 0.27 (1.00) | 0.27 (1.00) | 0.27 (1.00) | 0.29 (1.07) | 0.30 (1.12) |
| eq_late | operator== of two strings of one size that differ in the last character | 2.87 | 2.89 (1.01) | 2.89 (1.01) | 2.90 (1.01) | 3.06 (1.07) | 3.13 (1.09) |
| less | operator< of two strings that differ in the last character | 1.95 | 1.95 (1.00) | 1.95 (1.00) | 1.96 (1.00) | 2.69 (1.38) | 2.86 (1.46) |
| eq_literal | operator== of a view and a string literal | 0.27 | 0.27 (1.00) | 0.27 (1.00) | 0.28 (1.01) | 0.28 (1.02) | 0.28 (1.02) |
| pass_by_value | call a noinline function that takes the view by value | 0.67 | 0.68 (1.01) | 0.68 (1.01) | 0.68 (1.01) | 0.68 (1.00) | 0.68 (1.01) |
| return_value | call a noinline function that returns a view built from pointer and size | 0.82 | 0.82 (1.00) | 0.82 (1.00) | 0.82 (1.01) | 0.81 (0.99) | 0.82 (1.00) |
| sort | std::sort of 256 views (per element) | 27.71 | 25.45 (0.92) | 25.80 (0.93) | 26.39 (0.95) | 28.58 (1.03) | 29.65 (1.07) |
| map_lookup | find in a std::map of 256 views by an equal view | 25.01 | 28.63 (1.14) | 28.18 (1.13) | 27.83 (1.11) | 27.34 (1.09) | 28.32 (1.13) |
| split | split a text of 4096 characters at ',' into views (about 450 fields; one split) | 2190 | 2194 (1.00) | 2223 (1.02) | 2227 (1.02) | 2161 (0.99) | 2162 (0.99) |
| starts_with | starts_with a view in one string |  | 2.24 | 1.95 (0.87) | 1.95 (0.87) | 1.94 (0.87) | 1.96 (0.87) |
| ends_with | ends_with a view in one string |  | 1.70 | 1.70 (1.00) | 1.70 (1.00) | 1.71 (1.00) | 1.70 (1.00) |
| starts_with_lit | starts_with a string literal |  | 0.43 | 0.43 (1.00) | 0.44 (1.02) | 0.43 (1.00) | 0.44 (1.03) |
| ends_with_lit | ends_with a string literal |  | 0.54 | 0.54 (1.01) | 0.54 (1.00) | 0.53 (0.99) | 0.53 (0.99) |
| starts_with_chr | starts_with a character |  | 0.44 | 0.43 (0.98) | 0.44 (1.00) | 0.44 (1.00) | 0.44 (1.00) |
| hash | hash one string (the hash of each library: std::hash, boost::hash) | 4.27 | 4.31 (1.01) | 5.77 (1.35) | 5.79 (1.35) | 5.03 (1.18) | 5.07 (1.19) |
| to_std | construct a std::string_view from the view (the conversion) | 0.60 | 0.59 (0.99) |  | 0.65 (1.10) |  |  |
| from_std | construct the view from a std::string_view (the conversion) | 0.69 | 0.69 (1.00) |  | 0.81 (1.16) |  |  |
| eq_std | operator== of the view and a std::string_view | 1.72 | 1.73 (1.01) |  | 1.74 (1.01) |  |  |
| less_std | operator< of the view and a std::string_view | 1.93 | 1.87 (0.97) |  | 1.87 (0.97) |  |  |

Variants compared with each other (time of the first divided by the time of the second; the first is faster below 1.00): geometric mean over the scenarios both have, the best and the worst scenario of the first, and the number of scenarios where the first is slower or faster by more than 10 %:

| compared | scenarios | geometric mean | slower | faster | best | worst |
| --- | ---: | ---: | ---: | ---: | --- | --- |
| lumex C++17 / std C++17 | 44 | 1.027 | 8 | 1 | starts_with 0.87x | hash 1.35x |
| lumex C++11 / std C++17 | 40 | 1.009 | 3 | 2 | starts_with 0.87x | hash 1.35x |
| lumex C++11 / lumex C++17 | 40 | 0.985 | 1 | 2 | find_last_of 0.83x | find_first_of 1.15x |
| lumex C++17 / boost C++17 | 40 | 0.996 | 5 | 4 | less 0.69x | compare_equal 1.20x |
| lumex C++11 / boost C++11 | 40 | 0.993 | 4 | 3 | rfind_char 0.68x | find_first_of 1.31x |
| boost C++17 / std C++17 | 40 | 1.028 | 6 | 2 | compare_equal 0.78x | less 1.46x |

- lumex C++17 is slower than std C++17 by more than 10 % in: hash 1.35x, from_std 1.16x, ctor_cstr 1.15x, find_first_not_of 1.12x, find_last_of 1.12x, range_sum 1.12x, map_lookup 1.11x, index_sum 1.11x
- lumex C++17 is faster than std C++17 by more than 10 % in: starts_with 0.87x
- boost C++17 is slower than std C++17 by more than 10 % in: less 1.46x, find_last_of 1.37x, hash 1.19x, map_lookup 1.13x, eq_size 1.12x, find_str_absent 1.10x
- boost C++17 is faster than std C++17 by more than 10 % in: compare_equal 0.78x, starts_with 0.87x

## Build tree: clang

| scenario | operation | std C++17 | std C++20 | lumex C++11 | lumex C++17 | boost C++11 | boost C++17 |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| ctor_literal | construct from a string literal (length known to the compiler) | 0.67 | 0.66 (0.98) | 0.67 (0.99) | 0.66 (0.98) | 0.66 (0.98) | 0.67 (0.99) |
| ctor_cstr | construct from a char const * (length by strlen) | 2.66 | 2.81 (1.06) | 3.27 (1.23) | 3.09 (1.16) | 2.81 (1.06) | 2.81 (1.05) |
| ctor_string | construct from a std::string | 1.03 | 1.03 (1.00) | 1.07 (1.03) | 1.06 (1.02) | 0.96 (0.93) | 1.05 (1.02) |
| ctor_ptr_size | construct from pointer and size | 0.59 | 0.59 (1.00) | 0.59 (1.00) | 0.58 (1.00) | 0.59 (1.00) | 0.58 (1.00) |
| copy | copy construct a view | 0.70 | 0.70 (1.00) | 0.70 (1.00) | 0.70 (1.00) | 0.70 (1.00) | 0.70 (1.00) |
| index_sum | sum of the characters of one string of 30 to 33, by operator[] | 5.48 | 5.62 (1.02) | 5.61 (1.02) | 5.51 (1.00) | 5.49 (1.00) | 5.60 (1.02) |
| range_sum | sum of the characters of one string of 30 to 33, by range-for | 5.05 | 5.05 (1.00) | 5.04 (1.00) | 5.03 (1.00) | 5.04 (1.00) | 5.03 (1.00) |
| find_char | find('/') in one string, hit at position 7 | 1.72 | 1.71 (0.99) | 1.81 (1.06) | 1.83 (1.06) | 1.71 (1.00) | 1.71 (0.99) |
| find_char_late | find('.') in one string, hit near the end | 2.49 | 2.49 (1.00) | 2.55 (1.02) | 2.71 (1.09) | 2.49 (1.00) | 2.49 (1.00) |
| find_char_miss | find('#') in one string, no hit | 2.82 | 2.87 (1.02) | 2.90 (1.03) | 3.06 (1.09) | 2.81 (1.00) | 2.88 (1.02)† |
| rfind_char | rfind('/') in one string | 10.78 | 10.55 (0.98) | 10.88 (1.01) | 10.79 (1.00) | 11.48 (1.06) | 8.82 (0.82) |
| find_str_hit | find a 9-character view in one string, hit at 17 | 4.77 | 4.88 (1.02) | 5.12 (1.07) | 5.13 (1.07) | 4.83 (1.01)† | 4.77 (1.00) |
| find_str_miss | find an 8-character view in one string, no hit | 8.61 | 7.95 (0.92) | 7.47 (0.87) | 7.47 (0.87) | 8.26 (0.96) | 8.14 (0.95) |
| find_str_text | find an 8-character view in a text of 4096 characters, no hit; its first letter occurs at every 16th position | 1411 | 1355 (0.96) | 1274 (0.90) | 1288 (0.91) | 1408 (1.00) | 1356 (0.96) |
| find_str_absent | find an 8-character view in a text of 4096 characters, no hit; its first letter does not occur | 27.63 | 27.62 (1.00) | 27.45 (0.99) | 27.48 (0.99) | 27.42 (0.99) | 27.40 (0.99) |
| rfind_str | rfind a 9-character view in one string | 5.39 | 5.33 (0.99) | 12.66 (2.35) | 12.66 (2.35) | 13.00 (2.41) | 13.67 (2.54) |
| find_first_of | find_first_of a set of 3 characters in one string | 9.18 | 8.21 (0.89) | 17.59 (1.92) | 17.61 (1.92) | 8.13 (0.89) | 8.14 (0.89) |
| find_last_of | find_last_of a set of 3 characters in one string | 5.05 | 6.13 (1.21) | 5.04 (1.00) | 5.04 (1.00) | 4.77 (0.94) | 4.89 (0.97) |
| find_first_not_of | find_first_not_of a set of 10 characters in one string | 20.52 | 21.82 (1.06) | 21.81 (1.06) | 21.86 (1.07) | 19.95 (0.97) | 20.18 (0.98) |
| substr | substr(8, 12) of one string | 0.81 | 0.80 (0.99) | 0.80 (0.99) | 0.80 (0.99) | 0.81 (1.00) | 0.80 (0.99) |
| substr_tail | substr(size - 4) of one string | 0.73 | 0.73 (1.00) | 0.73 (1.00) | 0.73 (1.00) | 0.73 (1.00) | 0.73 (0.99) |
| compare_equal | compare two equal strings in different buffers | 1.95 | 1.94 (1.00) | 2.01 (1.04) | 1.99 (1.02) | 1.96 (1.01) | 1.94 (1.00) |
| compare_early | compare two strings that differ in the first character | 1.56 | 1.52 (0.98) | 1.66 (1.06) | 1.65 (1.06) | 1.65 (1.06) | 1.53 (0.98) |
| compare_late | compare two strings that differ in the last character | 3.02 | 2.91 (0.96) | 3.04 (1.01) | 3.06 (1.02) | 3.08 (1.02) | 2.90 (0.96) |
| eq_equal | operator== of two equal strings in different buffers | 1.89 | 1.92 (1.02) | 1.97 (1.05) | 1.93 (1.03) | 1.89 (1.00) | 1.90 (1.01) |
| eq_size | operator== of two strings of different sizes | 0.32 | 0.32 (1.00) | 0.27 (0.85) | 0.27 (0.85) | 0.33 (1.04) | 0.32 (1.00) |
| eq_late | operator== of two strings of one size that differ in the last character | 2.85 | 3.06 (1.07) | 3.07 (1.08) | 3.04 (1.07) | 3.09 (1.08) | 3.05 (1.07) |
| less | operator< of two strings that differ in the last character | 1.86 | 1.86 (1.00) | 1.97 (1.06) | 1.95 (1.05) | 1.81 (0.97) | 1.85 (0.99) |
| eq_literal | operator== of a view and a string literal | 0.42 | 0.42 (1.00) | 0.41 (0.99) | 0.42 (1.01) | 0.42 (1.00) | 0.42 (1.01) |
| pass_by_value | call a noinline function that takes the view by value | 0.51 | 0.51 (1.00) | 0.51 (1.00) | 0.51 (1.00) | 0.51 (1.00) | 0.51 (1.00) |
| return_value | call a noinline function that returns a view built from pointer and size | 0.67 | 0.67 (1.00) | 0.67 (1.00) | 0.66 (0.98) | 0.66 (0.98) | 0.67 (1.00) |
| sort | std::sort of 256 views (per element) | 23.21 | 22.34 (0.96) | 24.01 (1.03) | 24.07 (1.04) | 22.33 (0.96) | 22.33 (0.96) |
| map_lookup | find in a std::map of 256 views by an equal view | 34.66 | 19.27 (0.56) | 33.62 (0.97) | 33.90 (0.98) | 33.05 (0.95) | 31.55 (0.91) |
| split | split a text of 4096 characters at ',' into views (about 450 fields; one split) | 2679 | 2678 (1.00) | 2210 (0.82) | 2211 (0.83) | 2677 (1.00) | 2678 (1.00) |
| starts_with | starts_with a view in one string |  | 1.84 | 1.94 (1.06) | 1.95 (1.06) | 1.84 (1.00) | 1.83 (1.00) |
| ends_with | ends_with a view in one string |  | 1.65 | 1.72 (1.04) | 1.72 (1.05) | 1.61 (0.98) | 1.62 (0.99) |
| starts_with_lit | starts_with a string literal |  | 0.49 | 0.50 (1.01) | 0.50 (1.00) | 0.49 (1.00) | 0.50 (1.01) |
| ends_with_lit | ends_with a string literal |  | 0.50 | 0.49 (1.00) | 0.50 (1.00) | 0.49 (0.99) | 0.49 (1.00) |
| starts_with_chr | starts_with a character |  | 0.36 | 0.40 (1.13) | 0.36 (1.00) | 0.40 (1.12) | 0.40 (1.13) |
| hash | hash one string (the hash of each library: std::hash, boost::hash) | 4.20 | 4.20 (1.00) | 7.30 (1.74) | 7.29 (1.73) | 5.74 (1.37) | 5.71 (1.36) |
| to_std | construct a std::string_view from the view (the conversion) | 0.70 | 0.70 (1.00) |  | 0.69 (0.99) |  |  |
| from_std | construct the view from a std::string_view (the conversion) | 0.73 | 0.75 (1.03) |  | 0.72 (0.98) |  |  |
| eq_std | operator== of the view and a std::string_view | 1.65 | 1.72 (1.05) |  | 1.74 (1.06) |  |  |
| less_std | operator< of the view and a std::string_view | 1.72 | 1.72 (1.00) |  | 1.83 (1.06) |  |  |

Variants compared with each other (time of the first divided by the time of the second; the first is faster below 1.00): geometric mean over the scenarios both have, the best and the worst scenario of the first, and the number of scenarios where the first is slower or faster by more than 10 %:

| compared | scenarios | geometric mean | slower | faster | best | worst |
| --- | ---: | ---: | ---: | ---: | --- | --- |
| lumex C++17 / std C++17 | 44 | 1.056 | 4 | 3 | split 0.83x | rfind_str 2.35x |
| lumex C++11 / std C++17 | 40 | 1.062 | 5 | 3 | split 0.82x | rfind_str 2.35x |
| lumex C++11 / lumex C++17 | 40 | 1.003 | 1 | 0 | find_char_late 0.94x | starts_with_chr 1.13x |
| lumex C++17 / boost C++17 | 40 | 1.037 | 3 | 3 | split 0.83x | find_first_of 2.16x |
| lumex C++11 / boost C++11 | 40 | 1.033 | 4 | 2 | eq_size 0.82x | find_first_of 2.16x |
| boost C++17 / std C++17 | 40 | 1.021 | 3 | 2 | rfind_char 0.82x | rfind_str 2.54x |

- lumex C++17 is slower than std C++17 by more than 10 % in: rfind_str 2.35x, find_first_of 1.92x, hash 1.73x, ctor_cstr 1.16x
- lumex C++17 is faster than std C++17 by more than 10 % in: split 0.83x, eq_size 0.85x, find_str_miss 0.87x
- boost C++17 is slower than std C++17 by more than 10 % in: rfind_str 2.54x, hash 1.36x, starts_with_chr 1.13x
- boost C++17 is faster than std C++17 by more than 10 % in: rfind_char 0.82x, find_first_of 0.89x

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
- compiler clang: clang version 23.1.0

Wall time of `compile_time.cpp`, median of the repeats, in milliseconds. `no view` is the same unit with a stub instead of a view: the cost of the shared headers (`<string>`, `<vector>`, `<map>`, `<algorithm>`). The unit of `std C++17` leaves `starts_with` and `ends_with` out (they are C++20), so it does a little less than the others.

Unit `include`: the header is included and one object is declared (`-fsyntax-only -O0`).

| toolchain | no view C++11 | no view C++17 | no view C++20 | std C++17 | std C++20 | lumex C++11 | lumex C++17 | boost C++11 | boost C++17 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| gcc | 126 | 166 | 241 | 216 | 292 | 213 | 303 | 293 | 438 |
| clang | 213 | 274 | 365 | 326 | 423 | 321 | 411 | 410 | 501 |

Unit `use`: the whole API is used: construction, observers, searches, comparisons, slicing, `std::sort`, `std::map`, hashing (`-fsyntax-only -O0`).

| toolchain | no view C++11 | no view C++17 | no view C++20 | std C++17 | std C++20 | lumex C++11 | lumex C++17 | boost C++11 | boost C++17 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| gcc | 127 | 162 | 246 | 239 | 313 | 246 | 337 | 317 | 474 |
| clang | 216 | 269 | 362 | 364 | 473 | 356 | 447 | 442 | 548 |

Unit `use_o2`: the same use, compiled (`-c -O2`); the last column group is the `.text` of the object in bytes.

| toolchain | no view C++11 | no view C++17 | no view C++20 | std C++17 | std C++20 | lumex C++11 | lumex C++17 | boost C++11 | boost C++17 | no view C++11 .text | no view C++17 .text | no view C++20 .text | std C++17 .text | std C++20 .text | lumex C++11 .text | lumex C++17 .text | boost C++11 .text | boost C++17 .text |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| gcc |  |  |  | 436 | 564 | 451 | 542 | 601 | 754 |  |  |  | 5779 | 7019 | 5666 | 5757 | 8845 | 8939 |
| clang |  |  |  | 543 | 656 | 557 | 650 | 666 | 766 |  |  |  | 8015 | 8027 | 9034 | 9067 | 9998 | 10051 |

