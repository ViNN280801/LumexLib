# string_view benchmark results

- machine="12th Gen Intel(R) Core(TM) i7-12700K" os="Linux 6.1.170-1-generic" logical_cpus=20 python=3.7.3
- passes=5 cpu=3
- variant shared lumex_cxx11: compiler="GCC 13.2.0" library="libstdc++" cplusplus=201103
- variant shared lumex_cxx17: compiler="GCC 13.2.0" library="libstdc++" cplusplus=201703
- variant shared lumex_unity_cxx17: compiler="GCC 13.2.0" library="libstdc++" cplusplus=201703
- variant shared std_cxx17: compiler="GCC 13.2.0" library="libstdc++" cplusplus=201703
- variant shared std_cxx20: compiler="GCC 13.2.0" library="libstdc++" cplusplus=202002
- variant shared boost_cxx11: compiler="GCC 13.2.0" library="libstdc++" cplusplus=201103
- variant shared boost_cxx17: compiler="GCC 13.2.0" library="libstdc++" cplusplus=201703
- variant static lumex_cxx11: compiler="GCC 13.2.0" library="libstdc++" cplusplus=201103
- variant static lumex_cxx17: compiler="GCC 13.2.0" library="libstdc++" cplusplus=201703
- variant static lumex_unity_cxx17: compiler="GCC 13.2.0" library="libstdc++" cplusplus=201703
- variant static std_cxx17: compiler="GCC 13.2.0" library="libstdc++" cplusplus=201703
- variant static std_cxx20: compiler="GCC 13.2.0" library="libstdc++" cplusplus=202002
- variant static boost_cxx11: compiler="GCC 13.2.0" library="libstdc++" cplusplus=201103
- variant static boost_cxx17: compiler="GCC 13.2.0" library="libstdc++" cplusplus=201703

Time per operation in nanoseconds: the lowest median of 15 repetitions over all passes (the best pass), and in parentheses the ratio to `std::string_view` at C++17 (below 1.00 is faster than that; for `starts_with` and `ends_with`, which the standard has from C++20, the ratio is to `std::string_view` at C++20). The unit of an operation is the description of the scenario. A dagger marks a value whose slowest pass was more than 25 % slower than the best one: the processes of that executable do not agree (code and stack placement), so differences inside that band are noise.

## Build tree: shared

| scenario | operation | std C++17 | std C++20 | lumex C++11 | lumex C++17 | lumex inlined C++17 | boost C++11 | boost C++17 |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| ctor_literal | construct from a string literal (length known to the compiler) | 0.62 | 0.59 (0.94) | 2.16 (3.49) | 2.17 (3.49) | 0.59 (0.95) | 0.58 (0.93) | 0.59 (0.95) |
| ctor_cstr | construct from a char const * (length by strlen) | 2.84 | 2.96 (1.04) | 3.78 (1.33) | 3.81 (1.34) | 2.83 (1.00) | 2.84 (1.00) | 2.92 (1.03) |
| ctor_string | construct from a std::string | 0.85 | 0.86 (1.01) | 1.12 (1.32) | 1.12 (1.32) | 0.85 (1.00) | 0.84 (0.99) | 0.87 (1.02) |
| ctor_ptr_size | construct from pointer and size | 0.76 | 0.76 (1.00) | 0.73 (0.96) | 0.73 (0.95) | 0.76 (1.00) | 0.76 (1.00) | 0.77 (1.01) |
| copy | copy construct a view | 0.63 | 0.63 (1.00) | 0.61 (0.98) | 0.60 (0.95) | 0.64 (1.02) | 0.62 (0.99) | 0.65 (1.03) |
| index_sum | sum of the characters of one string of 33 to 36, by operator[] | 9.75 | 10.20 (1.05) | 8.48 (0.87) | 9.13 (0.94) | 9.11 (0.93) | 10.17 (1.04) | 9.17 (0.94) |
| range_sum | sum of the characters of one string of 33 to 36, by range-for | 9.89 | 10.24 (1.04) | 8.61 (0.87) | 9.13 (0.92) | 9.34 (0.94) | 10.24 (1.04) | 9.28 (0.94) |
| find_char | find('/') in one string, hit at position 7 | 1.74 | 1.73 (0.99) | 3.36 (1.93) | 3.36 (1.93) | 1.74 (1.00) | 1.67 (0.96) | 1.70 (0.97) |
| find_char_late | find('.') in one string, hit near the end | 2.71 | 2.78 (1.03) | 3.70 (1.37) | 3.85 (1.42) | 2.78 (1.03) | 2.66 (0.98) | 2.53 (0.94) |
| find_char_miss | find('#') in one string, no hit | 2.92 | 2.92 (1.00) | 4.13 (1.41) | 4.16 (1.43) | 3.04 (1.04) | 3.12 (1.07) | 3.16 (1.08) |
| rfind_char | rfind('/') in one string | 7.80 | 7.84 (1.00) | 8.74 (1.12) | 9.04 (1.16) | 7.55 (0.97) | 7.14 (0.91) | 7.25 (0.93) |
| find_str_hit | find a 9-character view in one string, hit at 17 | 5.46 | 5.14 (0.94) | 9.36 (1.71) | 9.25 (1.70) | 9.61 (1.76) | 5.92 (1.08) | 5.94 (1.09) |
| find_str_miss | find an 8-character view in one string, no hit | 8.12 | 9.24 (1.14) | 13.20 (1.63) | 12.88 (1.59) | 12.58 (1.55) | 8.89 (1.10) | 8.93 (1.10) |
| find_str_text | find an 8-character view in a text of 4096 characters, no hit; its first letter occurs at every 16th position | 1448 | 1378 (0.95) | 1631 (1.13) | 1720 (1.19) | 1871 (1.29) | 1455 (1.01) | 1457 (1.01) |
| find_str_absent | find an 8-character view in a text of 4096 characters, no hit; its first letter does not occur | 28.63 | 28.68 (1.00) | 969.2 (33.86) | 969.6 (33.87) | 1002 (35.01) | 31.73 (1.11) | 31.87 (1.11) |
| rfind_str | rfind a 9-character view in one string | 14.09 | 14.13 (1.00) | 13.17 (0.93) | 13.23 (0.94) | 15.82 (1.12) | 14.36 (1.02) | 14.28 (1.01) |
| find_first_of | find_first_of a set of 3 characters in one string | 18.67 | 20.11 (1.08) | 20.18 (1.08) | 19.35 (1.04) | 17.04 (0.91) | 18.97 (1.02) | 17.29 (0.93) |
| find_last_of | find_last_of a set of 3 characters in one string | 5.53 | 5.52 (1.00) | 4.21 (0.76) | 4.21 (0.76) | 6.13 (1.11) | 7.60 (1.37) | 4.43 (0.80) |
| find_first_not_of | find_first_not_of a set of 10 characters in one string | 21.04 | 22.93 (1.09) | 28.46 (1.35) | 28.55 (1.36) | 28.47 (1.35) | 23.28 (1.11) | 23.29 (1.11) |
| substr | substr(8, 12) of one string | 0.99 | 0.98 (0.99) | 1.35 (1.36) | 1.34 (1.36) | 1.00 (1.01) | 1.00 (1.00) | 1.00 (1.01) |
| substr_tail | substr(size - 4) of one string | 0.84 | 0.84 (1.01) | 1.35 (1.62) | 1.34 (1.61) | 0.78 (0.93) | 0.86 (1.02) | 0.83 (0.99) |
| compare_equal | compare two equal strings in different buffers | 2.66 | 2.72 (1.02) | 3.18 (1.20) | 3.11 (1.17) | 2.09 (0.79) | 2.13 (0.80) | 2.15 (0.81) |
| compare_early | compare two strings that differ in the first character | 1.88 | 1.85 (0.99) | 2.44 (1.30) | 2.43 (1.29) | 1.74 (0.93) | 1.72 (0.92) | 1.72 (0.92) |
| compare_late | compare two strings that differ in the last character | 3.24 | 3.15 (0.97) | 4.56 (1.41) | 4.70 (1.45) | 3.34 (1.03) | 3.17 (0.98) | 3.15 (0.97) |
| eq_equal | operator== of two equal strings in different buffers | 2.06 | 2.06 (1.00) | 2.72 (1.32)† | 2.78 (1.35) | 1.97 (0.96) | 2.05 (0.99) | 2.05 (0.99) |
| eq_size | operator== of two strings of different sizes | 0.30 | 0.28 (0.95) | 0.44 (1.48) | 0.43 (1.44) | 0.30 (1.00) | 0.30 (1.00) | 0.31 (1.04) |
| eq_late | operator== of two strings of one size that differ in the last character | 3.12 | 3.01 (0.96) | 4.61 (1.48) | 4.42 (1.42) | 3.16 (1.01) | 3.23 (1.03) | 3.15 (1.01) |
| less | operator< of two strings that differ in the last character | 2.12 | 2.08 (0.98) | 2.49 (1.18) | 2.48 (1.17) | 2.04 (0.96) | 2.93 (1.39) | 2.93 (1.38) |
| eq_literal | operator== of a view and a string literal | 0.31 | 0.29 (0.95) | 1.72 (5.62) | 1.73 (5.66) | 0.29 (0.94) | 0.31 (1.01) | 0.29 (0.96) |
| pass_by_value | call a noinline function that takes the view by value | 0.71 | 0.71 (1.00) | 0.67 (0.95) | 0.68 (0.96) | 0.71 (1.00) | 0.71 (1.00) | 0.70 (1.00) |
| return_value | call a noinline function that returns a view built from pointer and size | 0.86 | 0.86 (1.00) | 0.83 (0.96) | 0.81 (0.94) | 0.87 (1.01) | 0.87 (1.01) | 0.87 (1.01) |
| sort | std::sort of 256 views (per element) | 29.21 | 26.39 (0.90) | 44.83 (1.53) | 42.97 (1.47) | 30.09 (1.03) | 29.93 (1.02) | 30.58 (1.05) |
| map_lookup | find in a std::map of 256 views by an equal view | 30.47 | 27.58 (0.91) | 63.11 (2.07) | 59.05 (1.94) | 24.98 (0.82) | 30.07 (0.99) | 28.35 (0.93) |
| split | split a text of 4096 characters at ',' into views (about 450 fields; one split) | 2204 | 2204 (1.00) | 2744 (1.24) | 2799 (1.27) | 2242 (1.02) | 2171 (0.98) | 2170 (0.98) |
| starts_with | starts_with a view in one string |  | 2.31 | 2.12 (0.92) | 2.20 (0.95) | 1.89 (0.82) | 2.03 (0.88) | 2.03 (0.88) |
| ends_with | ends_with a view in one string |  | 1.81 | 1.95 (1.07) | 2.29 (1.26) | 1.66 (0.92) | 1.81 (1.00) | 1.78 (0.98) |
| starts_with_lit | starts_with a string literal |  | 0.46 | 3.41 (7.47) | 3.71 (8.14) | 0.46 (1.01) | 0.46 (1.00) | 0.48 (1.04) |
| ends_with_lit | ends_with a string literal |  | 0.57 | 3.62 (6.30) | 3.97 (6.91) | 0.56 (0.98) | 0.56 (0.98) | 0.56 (0.98) |
| starts_with_chr | starts_with a character |  | 0.46 | 0.78 (1.71) | 0.84 (1.82) | 0.47 (1.01) | 0.46 (1.00) | 0.47 (1.02) |
| hash | hash one string (lumex: std::hash of the converted std::string_view) | 4.64 | 4.65 (1.00) |  | 4.68 (1.01) | 4.63 (1.00) | 5.45 (1.17) | 5.36 (1.15) |
| to_std | construct a std::string_view from the view (the conversion) | 0.62 | 0.63 (1.01) |  | 0.77 (1.23) | 0.69 (1.11) |  |  |
| from_std | construct the view from a std::string_view (the conversion) | 0.73 | 0.73 (1.00) |  | 0.85 (1.16) | 0.85 (1.17) |  |  |
| eq_std | operator== of the view and a std::string_view | 1.80 | 1.81 (1.01) |  | 2.76 (1.54) | 1.69 (0.94) |  |  |
| less_std | operator< of the view and a std::string_view | 2.04 | 1.99 (0.97) |  | 2.80 (1.37) | 1.94 (0.95) |  |  |

Variants compared with each other (time of the first divided by the time of the second; the first is faster below 1.00): geometric mean over the scenarios both have, the best and the worst scenario of the first, and the number of scenarios where the first is slower or faster by more than 10 %:

| compared | scenarios | geometric mean | slower | faster | best | worst |
| --- | ---: | ---: | ---: | ---: | --- | --- |
| lumex C++17 / std C++17 | 44 | 1.552 | 33 | 1 | find_last_of 0.76x | find_str_absent 33.87x |
| lumex C++11 / std C++17 | 39 | 1.578 | 28 | 3 | find_last_of 0.76x | find_str_absent 33.86x |
| lumex C++11 / lumex C++17 | 39 | 0.988 | 0 | 1 | ends_with 0.85x | map_lookup 1.07x |
| lumex C++17 / boost C++17 | 40 | 1.580 | 29 | 2 | less 0.85x | find_str_absent 30.42x |
| lumex C++11 / boost C++11 | 39 | 1.555 | 27 | 4 | find_last_of 0.55x | find_str_absent 30.55x |
| boost C++17 / std C++17 | 40 | 0.999 | 5 | 3 | find_last_of 0.80x | less 1.38x |
| lumex inlined C++17 / std C++17 | 44 | 1.104 | 9 | 3 | compare_equal 0.79x | find_str_absent 35.01x |
| lumex C++17 / lumex inlined C++17 | 44 | 1.406 | 29 | 2 | find_last_of 0.69x | starts_with_lit 8.03x |

- lumex C++17 is slower than std C++17 by more than 10 % in: find_str_absent 33.87x, starts_with_lit 8.14x, ends_with_lit 6.91x, eq_literal 5.66x, ctor_literal 3.49x, map_lookup 1.94x, find_char 1.93x, starts_with_chr 1.82x, find_str_hit 1.70x, substr_tail 1.61x, find_str_miss 1.59x, eq_std 1.54x, sort 1.47x, compare_late 1.45x, eq_size 1.44x, find_char_miss 1.43x, find_char_late 1.42x, eq_late 1.42x, less_std 1.37x, find_first_not_of 1.36x, substr 1.36x, eq_equal 1.35x, ctor_cstr 1.34x, ctor_string 1.32x, compare_early 1.29x, split 1.27x, ends_with 1.26x, to_std 1.23x, find_str_text 1.19x, less 1.17x, compare_equal 1.17x, from_std 1.16x, rfind_char 1.16x
- lumex C++17 is faster than std C++17 by more than 10 % in: find_last_of 0.76x
- boost C++17 is slower than std C++17 by more than 10 % in: less 1.38x, hash 1.15x, find_str_absent 1.11x, find_first_not_of 1.11x, find_str_miss 1.10x
- boost C++17 is faster than std C++17 by more than 10 % in: find_last_of 0.80x, compare_equal 0.81x, starts_with 0.88x
- lumex inlined C++17 is slower than std C++17 by more than 10 % in: find_str_absent 35.01x, find_str_hit 1.76x, find_str_miss 1.55x, find_first_not_of 1.35x, find_str_text 1.29x, from_std 1.17x, rfind_str 1.12x, find_last_of 1.11x, to_std 1.11x
- lumex inlined C++17 is faster than std C++17 by more than 10 % in: compare_equal 0.79x, starts_with 0.82x, map_lookup 0.82x

## Build tree: static

| scenario | operation | std C++17 | std C++20 | lumex C++11 | lumex C++17 | lumex inlined C++17 | boost C++11 | boost C++17 |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| ctor_literal | construct from a string literal (length known to the compiler) | 0.58 | 0.58 (1.00) | 2.05 (3.51) | 2.03 (3.48) | 0.58 (0.99) | 0.58 (1.00) | 0.58 (0.99) |
| ctor_cstr | construct from a char const * (length by strlen) | 2.97 | 2.99 (1.00) | 3.42 (1.15) | 3.36 (1.13) | 2.84 (0.96) | 2.89 (0.97) | 2.94 (0.99) |
| ctor_string | construct from a std::string | 0.90 | 0.83 (0.92) | 1.02 (1.14) | 1.01 (1.12) | 0.87 (0.97) | 0.85 (0.95) | 0.85 (0.95) |
| ctor_ptr_size | construct from pointer and size | 0.77 | 0.77 (1.00) | 0.76 (0.99) | 0.76 (1.00) | 0.78 (1.01) | 0.77 (1.00) | 0.77 (1.00) |
| copy | copy construct a view | 0.66 | 0.65 (0.99) | 0.67 (1.01) | 0.68 (1.03) | 0.66 (0.99) | 0.67 (1.02) | 0.67 (1.02) |
| index_sum | sum of the characters of one string of 33 to 36, by operator[] | 9.99 | 10.28 (1.03) | 10.30 (1.03) | 10.32 (1.03) | 10.29 (1.03) | 10.27 (1.03) | 9.87 (0.99) |
| range_sum | sum of the characters of one string of 33 to 36, by range-for | 9.88 | 10.24 (1.04) | 10.41 (1.05) | 10.47 (1.06) | 10.30 (1.04) | 10.34 (1.05) | 9.95 (1.01) |
| find_char | find('/') in one string, hit at position 7 | 1.73 | 1.69 (0.98) | 2.80 (1.62) | 2.80 (1.62) | 1.74 (1.00) | 1.73 (1.00) | 1.66 (0.96) |
| find_char_late | find('.') in one string, hit near the end | 2.72 | 2.55 (0.94) | 3.18 (1.17) | 3.29 (1.21) | 2.52 (0.93) | 2.56 (0.94) | 2.71 (1.00) |
| find_char_miss | find('#') in one string, no hit | 2.79 | 2.98 (1.07) | 3.73 (1.34) | 3.83 (1.37) | 3.19 (1.14) | 2.98 (1.07) | 2.90 (1.04) |
| rfind_char | rfind('/') in one string | 7.76 | 8.11 (1.04) | 9.15 (1.18) | 9.19 (1.18) | 7.60 (0.98) | 7.29 (0.94) | 7.16 (0.92) |
| find_str_hit | find a 9-character view in one string, hit at 17 | 5.09 | 5.09 (1.00) | 9.69 (1.90) | 9.42 (1.85) | 9.59 (1.88) | 5.85 (1.15) | 7.25 (1.42) |
| find_str_miss | find an 8-character view in one string, no hit | 8.33 | 8.31 (1.00) | 14.27 (1.71) | 12.81 (1.54) | 13.05 (1.57) | 7.92 (0.95) | 8.21 (0.99) |
| find_str_text | find an 8-character view in a text of 4096 characters, no hit; its first letter occurs at every 16th position | 1452 | 1461 (1.01) | 1904 (1.31) | 1842 (1.27) | 1847 (1.27) | 1482 (1.02) | 1411 (0.97) |
| find_str_absent | find an 8-character view in a text of 4096 characters, no hit; its first letter does not occur | 29.01 | 28.90 (1.00) | 1000 (34.47) | 998.8 (34.43) | 997.1 (34.37) | 28.57 (0.98) | 28.70 (0.99) |
| rfind_str | rfind a 9-character view in one string | 14.21 | 13.30 (0.94) | 15.80 (1.11) | 14.87 (1.05) | 14.67 (1.03) | 13.60 (0.96) | 13.52 (0.95) |
| find_first_of | find_first_of a set of 3 characters in one string | 18.31 | 21.13 (1.15) | 18.61 (1.02) | 17.53 (0.96) | 17.22 (0.94) | 18.88 (1.03) | 17.43 (0.95) |
| find_last_of | find_last_of a set of 3 characters in one string | 5.47 | 6.48 (1.18) | 5.15 (0.94) | 6.11 (1.12) | 7.06 (1.29) | 4.45 (0.81) | 4.47 (0.82) |
| find_first_not_of | find_first_not_of a set of 10 characters in one string | 20.80 | 26.53 (1.28) | 28.47 (1.37) | 36.44 (1.75) | 26.58 (1.28) | 22.51 (1.08) | 24.04 (1.16) |
| substr | substr(8, 12) of one string | 0.98 | 1.00 (1.02) | 1.27 (1.30) | 1.26 (1.29) | 0.99 (1.01) | 0.99 (1.01) | 1.03 (1.05) |
| substr_tail | substr(size - 4) of one string | 0.86 | 0.86 (1.00) | 1.39 (1.62) | 1.39 (1.61) | 0.79 (0.91) | 0.86 (1.00) | 0.86 (0.99) |
| compare_equal | compare two equal strings in different buffers | 2.68 | 2.67 (1.00) | 3.19 (1.19) | 3.16 (1.18) | 2.06 (0.77) | 2.14 (0.80) | 2.15 (0.80) |
| compare_early | compare two strings that differ in the first character | 1.78 | 1.78 (1.00) | 2.30 (1.29) | 2.30 (1.29) | 1.77 (0.99) | 1.78 (1.00) | 1.73 (0.97) |
| compare_late | compare two strings that differ in the last character | 3.26 | 3.16 (0.97) | 4.22 (1.30) | 4.36 (1.34) | 3.37 (1.04) | 3.32 (1.02) | 3.09 (0.95) |
| eq_equal | operator== of two equal strings in different buffers | 2.06 | 2.06 (1.00) | 2.79 (1.36) | 2.75 (1.34) | 2.00 (0.97) | 2.05 (1.00) | 2.05 (1.00) |
| eq_size | operator== of two strings of different sizes | 0.28 | 0.30 (1.08) | 0.43 (1.57) | 0.43 (1.55) | 0.31 (1.12) | 0.30 (1.07) | 0.30 (1.08) |
| eq_late | operator== of two strings of one size that differ in the last character | 2.95 | 3.08 (1.04) | 4.00 (1.36) | 4.13 (1.40) | 3.20 (1.08) | 3.12 (1.06) | 3.11 (1.05) |
| less | operator< of two strings that differ in the last character | 2.21 | 2.23 (1.01) | 2.62 (1.19) | 2.67 (1.21) | 2.16 (0.98) | 3.12 (1.41) | 2.84 (1.29) |
| eq_literal | operator== of a view and a string literal | 0.29 | 0.29 (1.00) | 1.79 (6.23) | 1.79 (6.26) | 0.29 (1.00) | 0.29 (1.01) | 0.29 (1.02) |
| pass_by_value | call a noinline function that takes the view by value | 0.66 | 0.67 (1.01) | 0.67 (1.01) | 0.67 (1.01) | 0.67 (1.01) | 0.66 (1.00) | 0.66 (0.99) |
| return_value | call a noinline function that returns a view built from pointer and size | 0.78 | 0.79 (1.01) | 0.78 (1.00) | 0.79 (1.01) | 0.79 (1.01) | 0.79 (1.01) | 0.80 (1.03) |
| sort | std::sort of 256 views (per element) | 29.52 | 27.31 (0.93) | 33.77 (1.14) | 39.00 (1.32) | 29.05 (0.98) | 30.77 (1.04) | 29.36 (0.99) |
| map_lookup | find in a std::map of 256 views by an equal view | 31.33 | 27.35 (0.87) | 42.71 (1.36) | 39.02 (1.25) | 26.47 (0.84) | 28.61 (0.91) | 30.15 (0.96) |
| split | split a text of 4096 characters at ',' into views (about 450 fields; one split) | 2179 | 2170 (1.00) | 2816 (1.29) | 2790 (1.28) | 2173 (1.00) | 2163 (0.99) | 2165 (0.99) |
| starts_with | starts_with a view in one string |  | 2.29 | 2.06 (0.90) | 2.08 (0.91) | 1.91 (0.83) | 2.02 (0.88) | 2.03 (0.89) |
| ends_with | ends_with a view in one string |  | 1.79 | 2.05 (1.14) | 2.05 (1.15) | 1.65 (0.92) | 1.80 (1.01) | 1.79 (1.00) |
| starts_with_lit | starts_with a string literal |  | 0.46 | 3.39 (7.38) | 3.37 (7.34) | 0.46 (1.00) | 0.46 (1.00) | 0.47 (1.03) |
| ends_with_lit | ends_with a string literal |  | 0.58 | 3.90 (6.77) | 3.85 (6.69) | 0.56 (0.97) | 0.57 (0.99) | 0.57 (0.99) |
| starts_with_chr | starts_with a character |  | 0.45 | 0.70 (1.55) | 0.69 (1.52) | 0.46 (1.01) | 0.47 (1.03) | 0.45 (0.99) |
| hash | hash one string (lumex: std::hash of the converted std::string_view) | 5.02 | 5.02 (1.00) |  | 5.03 (1.00) | 5.00 (0.99) | 5.80 (1.15) | 5.78 (1.15) |
| to_std | construct a std::string_view from the view (the conversion) | 0.65 | 0.66 (1.01) |  | 0.69 (1.06) | 0.77 (1.17) |  |  |
| from_std | construct the view from a std::string_view (the conversion) | 0.74 | 0.71 (0.97) |  | 0.90 (1.22) | 0.85 (1.16) |  |  |
| eq_std | operator== of the view and a std::string_view | 1.80 | 1.81 (1.00) |  | 2.78 (1.54) | 1.69 (0.94) |  |  |
| less_std | operator< of the view and a std::string_view | 2.26 | 2.14 (0.95) |  | 2.76 (1.22) | 2.09 (0.92) |  |  |

Variants compared with each other (time of the first divided by the time of the second; the first is faster below 1.00): geometric mean over the scenarios both have, the best and the worst scenario of the first, and the number of scenarios where the first is slower or faster by more than 10 %:

| compared | scenarios | geometric mean | slower | faster | best | worst |
| --- | ---: | ---: | ---: | ---: | --- | --- |
| lumex C++17 / std C++17 | 44 | 1.533 | 33 | 0 | starts_with 0.91x | find_str_absent 34.43x |
| lumex C++11 / std C++17 | 39 | 1.573 | 30 | 0 | starts_with 0.90x | find_str_absent 34.47x |
| lumex C++11 / lumex C++17 | 39 | 0.994 | 1 | 3 | find_first_not_of 0.78x | find_str_miss 1.11x |
| lumex C++17 / boost C++17 | 40 | 1.559 | 29 | 1 | hash 0.87x | find_str_absent 34.80x |
| lumex C++11 / boost C++11 | 39 | 1.571 | 29 | 1 | less 0.84x | find_str_absent 35.01x |
| boost C++17 / std C++17 | 40 | 1.004 | 4 | 3 | compare_equal 0.80x | find_str_hit 1.42x |
| lumex inlined C++17 / std C++17 | 44 | 1.118 | 10 | 3 | compare_equal 0.77x | find_str_absent 34.37x |
| lumex C++17 / lumex inlined C++17 | 44 | 1.372 | 27 | 1 | find_last_of 0.87x | starts_with_lit 7.33x |

- lumex C++17 is slower than std C++17 by more than 10 % in: find_str_absent 34.43x, starts_with_lit 7.34x, ends_with_lit 6.69x, eq_literal 6.26x, ctor_literal 3.48x, find_str_hit 1.85x, find_first_not_of 1.75x, find_char 1.62x, substr_tail 1.61x, eq_size 1.55x, eq_std 1.54x, find_str_miss 1.54x, starts_with_chr 1.52x, eq_late 1.40x, find_char_miss 1.37x, compare_late 1.34x, eq_equal 1.34x, sort 1.32x, substr 1.29x, compare_early 1.29x, split 1.28x, find_str_text 1.27x, map_lookup 1.25x, from_std 1.22x, less_std 1.22x, less 1.21x, find_char_late 1.21x, rfind_char 1.18x, compare_equal 1.18x, ends_with 1.15x, ctor_cstr 1.13x, ctor_string 1.12x, find_last_of 1.12x
- lumex C++17 is faster than std C++17 by more than 10 % in: no scenario
- boost C++17 is slower than std C++17 by more than 10 % in: find_str_hit 1.42x, less 1.29x, find_first_not_of 1.16x, hash 1.15x
- boost C++17 is faster than std C++17 by more than 10 % in: compare_equal 0.80x, find_last_of 0.82x, starts_with 0.89x
- lumex inlined C++17 is slower than std C++17 by more than 10 % in: find_str_absent 34.37x, find_str_hit 1.88x, find_str_miss 1.57x, find_last_of 1.29x, find_first_not_of 1.28x, find_str_text 1.27x, to_std 1.17x, from_std 1.16x, find_char_miss 1.14x, eq_size 1.12x
- lumex inlined C++17 is faster than std C++17 by more than 10 % in: compare_equal 0.77x, starts_with 0.83x, map_lookup 0.84x

## Layout and traits

`sizeof` in bytes and the properties of the view type. TC: trivially copyable, TD: trivially destructible, SL: standard layout, NC: nothrow copy constructible, S: implicit from `std::string const &`, C: implicit from `char const *`, STR: explicit conversion to `std::string` possible (`std::string (v)`), IMP: implicit conversion to `std::string`, TO/FROM: implicit conversion to/from `std::string_view` (the C++17 builds). The values do not depend on the build tree.

| view | variant | sizeof | TC | TD | SL | NC | S | C | STR | IMP | TO | FROM |
| --- | --- | ---: | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `string_view` | lumex C++11 | 16 | Y | Y | Y | Y | Y | Y | Y | n | n | n |
| `wstring_view` | lumex C++11 | 16 | Y | Y | Y | Y | n | n | n | n | n | n |
| `string_view` | lumex C++17 | 16 | Y | Y | Y | Y | Y | Y | Y | n | Y | Y |
| `wstring_view` | lumex C++17 | 16 | Y | Y | Y | Y | n | n | n | n | n | n |
| `string_view` | lumex inlined C++17 | 16 | Y | Y | Y | Y | Y | Y | Y | n | Y | Y |
| `wstring_view` | lumex inlined C++17 | 16 | Y | Y | Y | Y | n | n | n | n | n | n |
| `string_view` | std C++17 | 16 | Y | Y | Y | Y | Y | Y | Y | n | Y | Y |
| `wstring_view` | std C++17 | 16 | Y | Y | Y | Y | n | n | n | n | n | n |
| `string_view` | std C++20 | 16 | Y | Y | Y | Y | Y | Y | Y | n | Y | Y |
| `wstring_view` | std C++20 | 16 | Y | Y | Y | Y | n | n | n | n | n | n |
| `string_view` | boost C++11 | 16 | Y | Y | Y | Y | Y | Y | Y | n | n | n |
| `wstring_view` | boost C++11 | 16 | Y | Y | Y | Y | n | n | n | n | n | n |
| `string_view` | boost C++17 | 16 | Y | Y | Y | Y | Y | Y | Y | n | n | n |
| `wstring_view` | boost C++17 | 16 | Y | Y | Y | Y | n | n | n | n | n | n |

## The compiled library

`libLumexCore_string_view` of each build tree (the narrow and the wide view): file size, size after `strip --strip-unneeded`, the `.text` of its code, and the defined exported function symbols (`nm`).

| build tree | kind | file | bytes | stripped | .text | function symbols | narrow | wide |
| --- | --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| shared | shared | libLumexCore_string_view.so.2.0.0.0 | 35288 | 35288 | 23076 | 94 | 47 | 47 |
| static | static | libLumexCore_string_view.a | 344396 | 57524 | 8738 | 94 | 47 | 47 |

## Compile time

- compiler gcc: g++ (GCC) 13.2.0

Wall time of `compile_time.cpp`, median of the repeats, in milliseconds. `no view` is the same unit with a stub instead of a view: the cost of the shared headers (`<string>`, `<vector>`, `<map>`, `<algorithm>`). The unit of `std C++17` leaves `starts_with` and `ends_with` out (they are C++20), so it does a little less than the others.

Unit `include`: the header is included and one object is declared (`-fsyntax-only -O0`).

| toolchain | no view C++11 | no view C++17 | no view C++20 | std C++17 | std C++20 | lumex C++11 | lumex C++17 | boost C++11 | boost C++17 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| gcc | 124 | 162 | 241 | 208 | 290 | 183 | 266 | 285 | 432 |

Unit `use`: the whole API is used: construction, observers, searches, comparisons, slicing, `std::sort`, `std::map`, hashing (`-fsyntax-only -O0`).

| toolchain | no view C++11 | no view C++17 | no view C++20 | std C++17 | std C++20 | lumex C++11 | lumex C++17 | boost C++11 | boost C++17 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| gcc | 124 | 162 | 236 | 235 | 310 | 203 | 292 | 318 | 459 |

Unit `use_o2`: the same use, compiled (`-c -O2`); the last column group is the `.text` of the object in bytes.

| toolchain | no view C++11 | no view C++17 | no view C++20 | std C++17 | std C++20 | lumex C++11 | lumex C++17 | boost C++11 | boost C++17 | no view C++11 .text | no view C++17 .text | no view C++20 .text | std C++17 .text | std C++20 .text | lumex C++11 .text | lumex C++17 .text | boost C++11 .text | boost C++17 .text |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| gcc |  |  |  | 428 | 543 | 344 | 432 | 583 | 736 |  |  |  | 6723 | 7828 | 5398 | 5499 | 10703 | 10781 |

