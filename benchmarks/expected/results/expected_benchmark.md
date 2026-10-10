# expected benchmark results

- machine="12th Gen Intel(R) Core(TM) i7-12700K" os="Linux 6.1.170-1-generic" logical_cpus=20 python=3.7.3
- passes=5 cpu=4
- variant gcc std_cxx23: compiler="GCC 13.2.0" library="libstdc++" cplusplus=202100
- variant gcc lumex_cxx23: compiler="GCC 13.2.0" library="libstdc++" cplusplus=202100
- variant gcc lumex_cxx20: compiler="GCC 13.2.0" library="libstdc++" cplusplus=202002
- variant gcc lumex_cxx17: compiler="GCC 13.2.0" library="libstdc++" cplusplus=201703
- variant gcc lumex_cxx14: compiler="GCC 13.2.0" library="libstdc++" cplusplus=201402
- variant gcc lumex_cxx11: compiler="GCC 13.2.0" library="libstdc++" cplusplus=201103
- variant gcc outcome_cxx17: compiler="GCC 13.2.0" library="libstdc++" cplusplus=201703

Time per operation in nanoseconds: the median of the medians of the passes (each is the median of 15 repetitions), and in parentheses the ratio to the baseline (below 1.00 is faster than the baseline). The unit of an operation is the description of the scenario. A dagger marks a value whose slowest pass median was more than 25 % above the fastest one: the processes of that executable do not agree, so differences inside that band are noise.

## gcc: against std::expected C++23

| scenario | description | std::expected C++23 | lumex C++23 | lumex C++20 | lumex C++17 | lumex C++14 | lumex C++11 | Outcome C++17 |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| construct_value_int | expected<int, unsigned> built from a value, then destroyed | 0.57 | 0.57 (1.00) | 0.57 (1.00) | 0.58 (1.01) | 0.58 (1.00) | 0.65 (1.13) | 0.66 (1.15) |
| construct_error_int | expected<int, unsigned> built from an error, then destroyed | 0.57 | 0.58 (1.00) | 0.57 (1.00) | 0.58 (1.00) | 0.57 (1.00) | 0.65 (1.13) | 0.64 (1.12) |
| construct_value_big | expected<array<int, 16>, unsigned> built from a value | 3.61 | 3.63 (1.01) | 3.61 (1.00) | 3.64 (1.01) | 3.62 (1.00) | 3.63 (1.00) | 3.59 (1.00) |
| construct_value_str12 | expected<string, unsigned>, 12 characters (no allocation) | 4.08 | 4.15 (1.02) | 4.13 (1.01) | 5.53 (1.36) | 5.47 (1.34) | 5.50 (1.35) | 5.49 (1.34) |
| construct_value_str64 | expected<string, unsigned>, 64 characters (allocation) | 17.83 | 16.93 (0.95) | 16.88 (0.95) | 20.07 (1.13) | 19.86 (1.11) | 18.95 (1.06) | 20.95 (1.17) |
| construct_error_str12 | expected<int, string> from an error, 12 characters | 4.07 | 4.15 (1.02) | 4.11 (1.01) | 5.53 (1.36) | 5.43 (1.33) | 5.54 (1.36) | 9.55 (2.34) |
| copy_value_int | copy of an expected<int, unsigned> that holds a value | 0.60 | 0.48 (0.80) | 0.49 (0.81) | 0.48 (0.80) | 0.48 (0.81) | 0.48 (0.80) | 0.62 (1.04) |
| copy_error_int | copy of an expected<int, unsigned> that holds an error | 0.49 | 0.61 (1.25) | 0.61 (1.25) | 0.51 (1.04) | 0.53 (1.09) | 0.60 (1.23) | 0.57 (1.18) |
| copy_value_big | copy of an expected<array<int, 16>, unsigned> with a value | 1.08 | 1.09 (1.00) | 1.09 (1.01) | 1.08 (1.00) | 1.08 (1.00) | 1.09 (1.01) | 1.09 (1.01) |
| copy_value_str12 | copy of an expected<string, unsigned>, 12 characters | 2.55 | 2.55 (1.00) | 2.56 (1.00) | 3.67 (1.44) | 3.69 (1.45) | 3.69 (1.45) | 3.73 (1.46) |
| copy_value_str64 | copy of an expected<string, unsigned>, 64 characters | 19.13 | 18.85 (0.99) | 18.61 (0.97) | 22.14 (1.16) | 21.67 (1.13) | 19.76 (1.03) | 21.80 (1.14) |
| copy_error_str12 | copy of an expected<int, string> with an error, 12 chars | 2.53 | 2.51 (0.99) | 2.54 (1.00) | 3.66 (1.45) | 3.75 (1.48) | 3.68 (1.45) | 4.05 (1.60) |
| move_value_int | construct an expected<int, unsigned> and move it | 0.57 | 0.58 (1.01) | 0.58 (1.01) | 0.64 (1.11) | 0.63 (1.09) | 0.57 (1.00) | 0.65 (1.13) |
| move_value_str12 | construct an expected<string, unsigned> (12) and move it | 6.23 | 6.26 (1.00) | 6.30 (1.01) | 6.45 (1.03) | 6.37 (1.02) | 6.32 (1.01) | 7.68 (1.23) |
| move_value_str64 | construct an expected<string, unsigned> (64) and move it | 18.75 | 18.83 (1.00) | 18.99 (1.01) | 20.54 (1.10) | 19.43 (1.04) | 20.26 (1.08) | 22.32 (1.19) |
| move_error_str12 | construct an expected<int, string> (12) and move it | 6.20 | 6.38 (1.03) | 6.22 (1.00) | 6.35 (1.02) | 6.33 (1.02) | 6.35 (1.02) | 15.73 (2.54) |
| assign_same_int | copy assignment, value over value, expected<int, unsigned> | 0.96 | 0.73 (0.76) | 0.73 (0.76) | 0.72 (0.75) | 0.72 (0.75) | 0.83 (0.87) | 0.80 (0.83) |
| assign_same_str12 | copy assignment, value over value, expected<string, unsigned> | 2.88 | 3.26 (1.13) | 3.26 (1.13) | 4.07 (1.41) | 4.04 (1.40) | 4.08 (1.41) | 4.73 (1.64) |
| assign_switch_int | copy assignment that changes the alternative, <int, unsigned> | 0.72 | 0.44 (0.61) | 0.44 (0.60) | 0.44 (0.60) | 0.44 (0.61) | 0.44 (0.62) | 0.51 (0.70) |
| assign_switch_str12 | copy assignment that changes the alternative, <string, text_error_t> | 8.50 | 8.84 (1.04) | 8.79 (1.03) | 10.52 (1.24) | 10.75 (1.27) | 10.37 (1.22) | 10.08 (1.19) |
| value_int | value () of an expected<int, unsigned> that holds a value | 0.37 | 0.52 (1.41) | 0.52 (1.40) | 0.50 (1.35) | 0.37 (1.00) | 0.52 (1.40) | 0.51 (1.37) |
| deref_int | operator* of an expected<int, unsigned> that holds a value | 0.35 | 0.38 (1.06) | 0.38 (1.06) | 0.38 (1.06) | 0.39 (1.09) | 0.51 (1.44) | 0.35 (1.00) |
| error_int | error () of an expected<int, unsigned> that holds an error | 0.35 | 0.49 (1.40) | 0.47 (1.35) | 0.50 (1.41) | 0.50 (1.42) | 0.38 (1.07) | 0.37 (1.06) |
| value_str12 | value ().size () of an expected<string, unsigned> | 0.59 | 0.42 (0.71) | 0.42 (0.70) | 0.42 (0.71) | 0.43 (0.73) | 0.42 (0.70) | 0.43 (0.72) |
| has_value_ok | has_value () over values only (predictable branch) | 0.41 | 0.35 (0.87) | 0.35 (0.86) | 0.36 (0.89) | 0.36 (0.88) | 0.45 (1.11) | 0.36 (0.89) |
| has_value_mixed | has_value () over 1/8 errors in a fixed random order | 0.36 | 0.36 (0.99) | 0.35 (0.97) | 0.35 (0.99) | 0.36 (0.99) | 0.36 (1.01) | 0.36 (1.01) |
| branch_mixed | if (has_value ()) *x else x.error (), 1/8 errors | 0.66 | 0.92 (1.40) | 0.91 (1.39) | 0.92 (1.40) | 0.91 (1.38) | 0.65 (0.99) | 0.62 (0.94) |
| value_or_mixed | value_or (-1), 1/8 errors | 0.87 | 0.56 (0.65) | 0.57 (0.66) | 0.57 (0.65) | 0.57 (0.65) | 0.88 (1.01) |  |
| transform_int | transform (v + 1) on <int, unsigned>, 1/8 errors | 0.93 | 1.11 (1.18) | 1.11 (1.18) | 1.10 (1.18) | 1.08 (1.16) | 0.93 (0.99) |  |
| and_then_int | and_then (returns an expected) on <int, unsigned>, 1/8 errors | 1.26 | 1.35 (1.07) | 1.36 (1.08) | 1.24 (0.99) | 1.22 (0.96) | 1.33 (1.06) |  |
| or_else_int | or_else (recovers the error) on <int, unsigned>, 1/8 errors | 0.67 | 0.79 (1.18) | 0.80 (1.20) | 0.82 (1.23) | 0.82 (1.22) | 0.79 (1.18) |  |
| chain_int | transform, and_then, or_else in a row, <int, unsigned> | 1.15 | 1.07 (0.93) | 1.08 (0.93) | 1.18 (1.02) | 1.07 (0.93) | 1.14 (0.99) |  |
| transform_str12 | transform (s.size ()) on <string, unsigned>, const lvalue | 1.03 | 1.05 (1.02) | 1.05 (1.02) | 1.05 (1.02) | 1.01 (0.98) | 0.85 (0.82) |  |
| return_chain_int | 4 nested calls return expected<int, unsigned> by value | 5.08 | 5.28 (1.04) | 5.28 (1.04) | 5.33 (1.05) | 5.21 (1.02) | 6.84 (1.35) | 5.35 (1.05) |
| return_chain_str12 | 4 nested calls return expected<string, unsigned> (12) | 45.39 | 45.30 (1.00) | 45.34 (1.00) | 45.13 (0.99) | 44.84 (0.99) | 45.26 (1.00) | 45.55 (1.00) |
| return_chain_str64 | 4 nested calls return expected<string, unsigned> (64) | 23.80 | 23.28 (0.98) | 23.10 (0.97) | 24.54 (1.03) | 23.79 (1.00) | 24.01 (1.01) | 24.31 (1.02) |
| return_chain_big | 4 nested calls return expected<array<int, 16>, unsigned> | 17.31 | 17.12 (0.99) | 17.04 (0.98) | 17.12 (0.99) | 17.07 (0.99) | 17.14 (0.99) | 17.06 (0.99) |

Summary against std::expected C++23 (geometric mean of the ratio over the scenarios both have; scenarios slower or faster by more than 10 %):

| variant | scenarios | geometric mean | slower | faster |
| --- | ---: | ---: | ---: | ---: |
| lumex C++23 | 37 | 0.997 | 7 | 6 |
| lumex C++20 | 37 | 0.995 | 7 | 6 |
| lumex C++17 | 37 | 1.058 | 14 | 6 |
| lumex C++14 | 37 | 1.041 | 12 | 6 |
| lumex C++11 | 37 | 1.071 | 14 | 5 |
| Outcome C++17 | 31 | 1.147 | 16 | 4 |

- lumex C++23 is slower by more than 10 % in: value_int 1.41x, branch_mixed 1.40x, error_int 1.40x, copy_error_int 1.25x, transform_int 1.18x, or_else_int 1.18x, assign_same_str12 1.13x
- lumex C++20 is slower by more than 10 % in: value_int 1.40x, branch_mixed 1.39x, error_int 1.35x, copy_error_int 1.25x, or_else_int 1.20x, transform_int 1.18x, assign_same_str12 1.13x
- lumex C++17 is slower by more than 10 % in: copy_error_str12 1.45x, copy_value_str12 1.44x, error_int 1.41x, assign_same_str12 1.41x, branch_mixed 1.40x, construct_error_str12 1.36x, construct_value_str12 1.36x, value_int 1.35x, assign_switch_str12 1.24x, or_else_int 1.23x, transform_int 1.18x, copy_value_str64 1.16x, construct_value_str64 1.13x, move_value_int 1.11x
- lumex C++14 is slower by more than 10 % in: copy_error_str12 1.48x, copy_value_str12 1.45x, error_int 1.42x, assign_same_str12 1.40x, branch_mixed 1.38x, construct_value_str12 1.34x, construct_error_str12 1.33x, assign_switch_str12 1.27x, or_else_int 1.22x, transform_int 1.16x, copy_value_str64 1.13x, construct_value_str64 1.11x
- lumex C++11 is slower by more than 10 % in: copy_error_str12 1.45x, copy_value_str12 1.45x, deref_int 1.44x, assign_same_str12 1.41x, value_int 1.40x, construct_error_str12 1.36x, construct_value_str12 1.35x, return_chain_int 1.35x, copy_error_int 1.23x, assign_switch_str12 1.22x, or_else_int 1.18x, construct_error_int 1.13x, construct_value_int 1.13x, has_value_ok 1.11x
- Outcome C++17 is slower by more than 10 % in: move_error_str12 2.54x, construct_error_str12 2.34x, assign_same_str12 1.64x, copy_error_str12 1.60x, copy_value_str12 1.46x, value_int 1.37x, construct_value_str12 1.34x, move_value_str12 1.23x, move_value_str64 1.19x, assign_switch_str12 1.19x, copy_error_int 1.18x, construct_value_str64 1.17x, construct_value_int 1.15x, copy_value_str64 1.14x, move_value_int 1.13x, construct_error_int 1.12x

### gcc: the layers of this library against lumex C++11

| scenario | lumex C++11 | lumex C++14 | lumex C++17 | lumex C++20 | lumex C++23 |
| --- | ---: | ---: | ---: | ---: | ---: |
| construct_value_int | 0.65 | 0.58 (0.88) | 0.58 (0.89) | 0.57 (0.88) | 0.57 (0.88) |
| construct_error_int | 0.65 | 0.57 (0.88) | 0.58 (0.88) | 0.57 (0.88) | 0.58 (0.89) |
| construct_value_big | 3.63 | 3.62 (1.00) | 3.64 (1.00) | 3.61 (1.00) | 3.63 (1.00) |
| construct_value_str12 | 5.50 | 5.47 (0.99) | 5.53 (1.01) | 4.13 (0.75) | 4.15 (0.75) |
| construct_value_str64 | 18.95 | 19.86 (1.05) | 20.07 (1.06) | 16.88 (0.89) | 16.93 (0.89) |
| construct_error_str12 | 5.54 | 5.43 (0.98) | 5.53 (1.00) | 4.11 (0.74) | 4.15 (0.75) |
| copy_value_int | 0.48 | 0.48 (1.00) | 0.48 (1.00) | 0.49 (1.01) | 0.48 (1.00) |
| copy_error_int | 0.60 | 0.53 (0.88) | 0.51 (0.84) | 0.61 (1.01) | 0.61 (1.01) |
| copy_value_big | 1.09 | 1.08 (0.99) | 1.08 (1.00) | 1.09 (1.00) | 1.09 (1.00) |
| copy_value_str12 | 3.69 | 3.69 (1.00) | 3.67 (0.99) | 2.56 (0.69) | 2.55 (0.69) |
| copy_value_str64 | 19.76 | 21.67 (1.10) | 22.14 (1.12) | 18.61 (0.94) | 18.85 (0.95) |
| copy_error_str12 | 3.68 | 3.75 (1.02) | 3.66 (1.00) | 2.54 (0.69) | 2.51 (0.68) |
| move_value_int | 0.57 | 0.63 (1.10) | 0.64 (1.12) | 0.58 (1.01) | 0.58 (1.01) |
| move_value_str12 | 6.32 | 6.37 (1.01) | 6.45 (1.02) | 6.30 (1.00) | 6.26 (0.99) |
| move_value_str64 | 20.26 | 19.43 (0.96) | 20.54 (1.01) | 18.99 (0.94) | 18.83 (0.93) |
| move_error_str12 | 6.35 | 6.33 (1.00) | 6.35 (1.00) | 6.22 (0.98) | 6.38 (1.00) |
| assign_same_int | 0.83 | 0.72 (0.87) | 0.72 (0.87) | 0.73 (0.87) | 0.73 (0.87) |
| assign_same_str12 | 4.08 | 4.04 (0.99) | 4.07 (1.00) | 3.26 (0.80) | 3.26 (0.80) |
| assign_switch_int | 0.44 | 0.44 (0.99) | 0.44 (0.98) | 0.44 (0.98) | 0.44 (0.98) |
| assign_switch_str12 | 10.37 | 10.75 (1.04) | 10.52 (1.01) | 8.79 (0.85) | 8.84 (0.85) |
| value_int | 0.52 | 0.37 (0.71) | 0.50 (0.96) | 0.52 (1.00) | 0.52 (1.01) |
| deref_int | 0.51 | 0.39 (0.76) | 0.38 (0.74) | 0.38 (0.74) | 0.38 (0.74) |
| error_int | 0.38 | 0.50 (1.33) | 0.50 (1.32) | 0.47 (1.26) | 0.49 (1.30) |
| value_str12 | 0.42 | 0.43 (1.04) | 0.42 (1.01) | 0.42 (1.00) | 0.42 (1.01) |
| has_value_ok | 0.45 | 0.36 (0.79) | 0.36 (0.80) | 0.35 (0.78) | 0.35 (0.78) |
| has_value_mixed | 0.36 | 0.36 (0.98) | 0.35 (0.98) | 0.35 (0.97) | 0.36 (0.99) |
| branch_mixed | 0.65 | 0.91 (1.40) | 0.92 (1.42) | 0.91 (1.41) | 0.92 (1.42) |
| value_or_mixed | 0.88 | 0.57 (0.65) | 0.57 (0.65) | 0.57 (0.65) | 0.56 (0.64) |
| transform_int | 0.93 | 1.08 (1.17) | 1.10 (1.19) | 1.11 (1.19) | 1.11 (1.19) |
| and_then_int | 1.33 | 1.22 (0.91) | 1.24 (0.93) | 1.36 (1.02) | 1.35 (1.02) |
| or_else_int | 0.79 | 0.82 (1.04) | 0.82 (1.04) | 0.80 (1.02) | 0.79 (1.00) |
| chain_int | 1.14 | 1.07 (0.94) | 1.18 (1.03) | 1.08 (0.95) | 1.07 (0.94) |
| transform_str12 | 0.85 | 1.01 (1.19) | 1.05 (1.24) | 1.05 (1.24) | 1.05 (1.24) |
| return_chain_int | 6.84 | 5.21 (0.76) | 5.33 (0.78) | 5.28 (0.77) | 5.28 (0.77) |
| return_chain_str12 | 45.26 | 44.84 (0.99) | 45.13 (1.00) | 45.34 (1.00) | 45.30 (1.00) |
| return_chain_str64 | 24.01 | 23.79 (0.99) | 24.54 (1.02) | 23.10 (0.96) | 23.28 (0.97) |
| return_chain_big | 17.14 | 17.07 (1.00) | 17.12 (1.00) | 17.04 (0.99) | 17.12 (1.00) |

| layer | geometric mean | slower | faster |
| --- | ---: | ---: | ---: |
| lumex C++14 | 0.972 | 4 | 9 |
| lumex C++17 | 0.988 | 6 | 8 |
| lumex C++20 | 0.929 | 4 | 14 |
| lumex C++23 | 0.931 | 4 | 14 |

## Sizes and properties

`sizeof` in bytes, then the properties that decide how the type is passed and copied: TC trivially copyable (passed in registers, copied with `memcpy`), TD trivially destructible, SL standard layout, NM nothrow move constructible. `array16_t` is `std::array<int, 16>`; `text_error_t` is a struct that holds a `std::string`. Outcome refuses a result whose value type and error type are the same, so the pairs differ from `<T, T>`.

| `expected<T, E>` | std::expected C++23 | lumex (all of C++11 to C++23) | Outcome C++17 |
| --- | --- | --- | --- |
| `<char, unsigned char>` | 2 B; TC no TD yes SL yes NM yes | 2 B; TC yes TD yes SL yes NM yes | 6 B; TC yes TD yes SL yes NM yes |
| `<int, unsigned>` | 8 B; TC no TD yes SL yes NM yes | 8 B; TC yes TD yes SL yes NM yes | 8 B; TC yes TD yes SL yes NM yes |
| `<long long, unsigned long long>` | 16 B; TC no TD yes SL yes NM yes | 16 B; TC yes TD yes SL yes NM yes | 16 B; TC yes TD yes SL yes NM yes |
| `<double, int>` | 16 B; TC no TD yes SL yes NM yes | 16 B; TC yes TD yes SL yes NM yes | 16 B; TC yes TD yes SL yes NM yes |
| `<int *, int>` | 16 B; TC no TD yes SL yes NM yes | 16 B; TC yes TD yes SL yes NM yes | 16 B; TC yes TD yes SL yes NM yes |
| `<empty_t, int>` | 8 B; TC no TD yes SL yes NM yes | 8 B; TC yes TD yes SL yes NM yes | 8 B; TC yes TD yes SL yes NM yes |
| `<int, std::error_code>` | 24 B; TC no TD yes SL yes NM yes | 24 B; TC yes TD yes SL yes NM yes | 24 B; TC yes TD yes SL yes NM yes |
| `<std::string, int>` | 40 B; TC no TD no SL yes NM yes | 40 B; TC no TD no SL yes NM yes | 40 B; TC no TD no SL yes NM yes |
| `<int, std::string>` | 40 B; TC no TD no SL yes NM yes | 40 B; TC no TD no SL yes NM yes | 40 B; TC no TD no SL yes NM yes |
| `<std::string, text_error_t>` | 40 B; TC no TD no SL yes NM yes | 40 B; TC no TD no SL yes NM yes | 72 B; TC no TD no SL yes NM yes |
| `<std::string, std::error_code>` | 40 B; TC no TD no SL yes NM yes | 40 B; TC no TD no SL yes NM yes | 56 B; TC no TD no SL yes NM yes |
| `<array16_t, int>` | 68 B; TC no TD yes SL yes NM yes | 68 B; TC yes TD yes SL yes NM yes | 68 B; TC yes TD yes SL yes NM yes |
| `<std::unique_ptr<int>, int>` | 16 B; TC no TD no SL yes NM yes | 16 B; TC no TD no SL yes NM yes | 16 B; TC no TD no SL yes NM yes |
| `<void, int>` | 8 B; TC no TD yes SL yes NM yes | 8 B; TC yes TD yes SL yes NM yes | 8 B; TC yes TD yes SL yes NM yes |
| `<void, std::error_code>` | 24 B; TC no TD yes SL yes NM yes | 24 B; TC yes TD yes SL yes NM yes | 24 B; TC yes TD yes SL yes NM yes |
| `<void, std::string>` | 40 B; TC no TD no SL yes NM yes | 40 B; TC no TD no SL yes NM yes | 40 B; TC no TD no SL yes NM yes |

## Compile time

- compiler gcc: g++ (GCC) 13.2.0

Wall time of `compile_time.cpp` (includes the header and uses construction, copy, move, assignment, `value ()`, `error ()`, `swap` and, where the type has them, `value_or` and the monadic operations, for `<int, unsigned>`, `<std::string, text_error>` and `<void, int>`), median of the repeats, in milliseconds. `syntax_O0` is `-fsyntax-only -O0`, `compile_O2` is `-c -O2`. `no header` is the same unit without the header under test: the cost of `<string>`, `<utility>` and `<vector>`.

| toolchain, syntax_O0 | no header C++11 | no header C++23 | std::expected C++23 | lumex C++23 | lumex C++20 | lumex C++17 | lumex C++14 | lumex C++11 | Outcome C++17 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| gcc | 149 | 312 | 369 | 730 | 682 | 534 | 404 | 376 | 878 |

| toolchain, compile_O2 | no header C++11 | no header C++23 | std::expected C++23 | lumex C++23 | lumex C++20 | lumex C++17 | lumex C++14 | lumex C++11 | Outcome C++17 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| gcc | 164 | 334 | 574 | 947 | 911 | 684 | 554 | 523 | 1779 |

