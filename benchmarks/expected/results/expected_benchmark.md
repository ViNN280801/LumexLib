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
- variant clang std_cxx23: compiler="Clang 23.1.0" library="libc++" cplusplus=202302
- variant clang lumex_cxx23: compiler="Clang 23.1.0" library="libc++" cplusplus=202302
- variant clang lumex_cxx20: compiler="Clang 23.1.0" library="libc++" cplusplus=202002
- variant clang lumex_cxx17: compiler="Clang 23.1.0" library="libc++" cplusplus=201703
- variant clang lumex_cxx14: compiler="Clang 23.1.0" library="libc++" cplusplus=201402
- variant clang lumex_cxx11: compiler="Clang 23.1.0" library="libc++" cplusplus=201103
- variant clang outcome_cxx17: compiler="Clang 23.1.0" library="libc++" cplusplus=201703

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

## clang: against std::expected C++23

| scenario | description | std::expected C++23 | lumex C++23 | lumex C++20 | lumex C++17 | lumex C++14 | lumex C++11 | Outcome C++17 |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| construct_value_int | expected<int, unsigned> built from a value, then destroyed | 0.61 | 0.59 (0.96) | 0.59 (0.96) | 0.60 (0.98) | 0.59 (0.96) | 0.59 (0.96) | 0.70 (1.14) |
| construct_error_int | expected<int, unsigned> built from an error, then destroyed | 0.55 | 0.49 (0.90) | 0.49 (0.90) | 0.50 (0.91) | 0.49 (0.90) | 0.49 (0.90) | 0.87 (1.58) |
| construct_value_big | expected<array<int, 16>, unsigned> built from a value | 1.23 | 1.24 (1.00) | 1.23 (1.00) | 1.23 (1.00) | 1.23 (1.00) | 1.23 (1.00) | 1.35 (1.09) |
| construct_value_str12 | expected<string, unsigned>, 12 characters (no allocation) | 10.24 | 10.37 (1.01) | 10.23 (1.00) | 10.15 (0.99) | 10.18 (0.99) | 10.13 (0.99) | 10.22 (1.00) |
| construct_value_str64 | expected<string, unsigned>, 64 characters (allocation) | 17.74 | 18.40 (1.04) | 18.48 (1.04) | 17.19 (0.97) | 17.33 (0.98) | 17.25 (0.97) | 17.71 (1.00) |
| construct_error_str12 | expected<int, string> from an error, 12 characters | 10.43 | 10.26 (0.98) | 10.24 (0.98) | 10.40 (1.00) | 10.30 (0.99) | 10.26 (0.98) | 10.12 (0.97) |
| copy_value_int | copy of an expected<int, unsigned> that holds a value | 0.53 | 0.44 (0.84) | 0.45 (0.84) | 0.49 (0.93) | 0.51 (0.96) | 0.47 (0.88)† | 0.49 (0.92) |
| copy_error_int | copy of an expected<int, unsigned> that holds an error | 0.51 | 0.45 (0.87) | 0.44 (0.87) | 0.48 (0.95) | 0.53 (1.03) | 0.55 (1.08) | 0.49 (0.96) |
| copy_value_big | copy of an expected<array<int, 16>, unsigned> with a value | 1.12 | 1.10 (0.98) | 1.11 (0.99) | 1.11 (0.99) | 1.10 (0.98) | 1.10 (0.99) | 1.10 (0.99) |
| copy_value_str12 | copy of an expected<string, unsigned>, 12 characters | 1.00 | 1.02 (1.02) | 1.01 (1.01) | 1.02 (1.02) | 1.01 (1.01) | 1.02 (1.02) | 1.27 (1.27) |
| copy_value_str64 | copy of an expected<string, unsigned>, 64 characters | 20.61 | 21.00 (1.02) | 20.97 (1.02) | 20.30 (0.99) | 20.57 (1.00) | 19.77 (0.96) | 20.17 (0.98) |
| copy_error_str12 | copy of an expected<int, string> with an error, 12 chars | 1.11 | 0.99 (0.89) | 1.00 (0.89) | 1.00 (0.90) | 1.00 (0.90) | 1.03 (0.93) | 1.36 (1.22) |
| move_value_int | construct an expected<int, unsigned> and move it | 0.58 | 0.59 (1.01) | 0.58 (1.00) | 0.59 (1.01) | 0.59 (1.01) | 0.59 (1.01) | 0.71 (1.21) |
| move_value_str12 | construct an expected<string, unsigned> (12) and move it | 10.23 | 10.17 (0.99) | 10.19 (1.00) | 10.22 (1.00) | 10.14 (0.99) | 10.51 (1.03) | 10.21 (1.00) |
| move_value_str64 | construct an expected<string, unsigned> (64) and move it | 18.43 | 17.12 (0.93) | 17.04 (0.92) | 17.92 (0.97) | 17.62 (0.96) | 17.52 (0.95) | 17.69 (0.96) |
| move_error_str12 | construct an expected<int, string> (12) and move it | 10.23 | 10.13 (0.99) | 10.29 (1.01) | 10.16 (0.99) | 10.24 (1.00) | 10.55 (1.03) | 10.12 (0.99) |
| assign_same_int | copy assignment, value over value, expected<int, unsigned> | 0.82 | 0.67 (0.82) | 0.66 (0.81) | 0.67 (0.82) | 0.66 (0.81) | 0.67 (0.82) | 0.74 (0.90) |
| assign_same_str12 | copy assignment, value over value, expected<string, unsigned> | 2.21 | 2.01 (0.91) | 2.02 (0.91) | 1.84 (0.83) | 1.85 (0.84) | 1.84 (0.83) | 2.93 (1.32) |
| assign_switch_int | copy assignment that changes the alternative, <int, unsigned> | 0.73 | 0.39 (0.53) | 0.39 (0.53) | 0.39 (0.53) | 0.39 (0.54) | 0.39 (0.53) | 0.41 (0.56) |
| assign_switch_str12 | copy assignment that changes the alternative, <string, text_error_t> | 2.08 | 2.08 (1.00) | 2.05 (0.99) | 2.03 (0.98) | 2.06 (0.99) | 2.04 (0.98) | 2.47 (1.19) |
| value_int | value () of an expected<int, unsigned> that holds a value | 0.38 | 0.38 (1.00) | 0.38 (0.99) | 0.38 (1.00) | 0.38 (0.99) | 0.38 (1.00) | 0.38 (1.00) |
| deref_int | operator* of an expected<int, unsigned> that holds a value | 0.26 | 0.38 (1.47) | 0.38 (1.48) | 0.38 (1.48) | 0.38 (1.48) | 0.38 (1.47) | 0.26 (1.00) |
| error_int | error () of an expected<int, unsigned> that holds an error | 0.12 | 0.38 (3.08) | 0.38 (3.05) | 0.38 (3.04) | 0.38 (3.06) | 0.38 (3.06) | 0.49 (3.96) |
| value_str12 | value ().size () of an expected<string, unsigned> | 0.68 | 0.69 (1.02) | 0.69 (1.02) | 0.69 (1.01) | 0.69 (1.02) | 0.69 (1.02) | 0.69 (1.02) |
| has_value_ok | has_value () over values only (predictable branch) | 0.18 | 0.19 (1.01) | 0.18 (1.01) | 0.18 (1.00) | 0.18 (1.00) | 0.18 (1.00) | 0.42 (2.27) |
| has_value_mixed | has_value () over 1/8 errors in a fixed random order | 0.18 | 0.18 (1.00) | 0.18 (1.00) | 0.18 (1.00) | 0.18 (1.00) | 0.18 (1.00) | 0.42 (2.26) |
| branch_mixed | if (has_value ()) *x else x.error (), 1/8 errors | 0.56 | 0.57 (1.01) | 0.57 (1.01) | 0.57 (1.01) | 0.56 (1.00) | 0.56 (1.01) | 0.71 (1.27) |
| value_or_mixed | value_or (-1), 1/8 errors | 0.38 | 0.38 (0.98) | 0.38 (0.99) | 0.38 (1.00) | 0.38 (0.99) | 0.38 (0.99) |  |
| transform_int | transform (v + 1) on <int, unsigned>, 1/8 errors | 0.75 | 0.78 (1.04) | 0.78 (1.03) | 0.77 (1.02) | 0.78 (1.03) | 0.78 (1.04) |  |
| and_then_int | and_then (returns an expected) on <int, unsigned>, 1/8 errors | 1.63 | 1.56 (0.95) | 1.56 (0.96) | 1.55 (0.95) | 1.56 (0.96) | 1.56 (0.96) |  |
| or_else_int | or_else (recovers the error) on <int, unsigned>, 1/8 errors | 0.74 | 0.85 (1.14) | 0.84 (1.14) | 0.84 (1.13) | 0.84 (1.13) | 0.84 (1.13) |  |
| chain_int | transform, and_then, or_else in a row, <int, unsigned> | 1.55 | 1.69 (1.09) | 1.70 (1.10) | 1.70 (1.09) | 1.72 (1.11) | 1.69 (1.09) |  |
| transform_str12 | transform (s.size ()) on <string, unsigned>, const lvalue | 1.54 | 1.88 (1.22) | 1.87 (1.21) | 1.82 (1.18) | 1.79 (1.16) | 1.55 (1.00) |  |
| return_chain_int | 4 nested calls return expected<int, unsigned> by value | 5.89 | 5.46 (0.93) | 5.32 (0.90) | 4.92 (0.83) | 4.91 (0.83) | 7.14 (1.21) | 6.05 (1.03) |
| return_chain_str12 | 4 nested calls return expected<string, unsigned> (12) | 16.95 | 17.22 (1.02) | 17.27 (1.02) | 16.80 (0.99) | 16.89 (1.00) | 45.64 (2.69) | 20.37 (1.20) |
| return_chain_str64 | 4 nested calls return expected<string, unsigned> (64) | 23.58 | 23.17 (0.98) | 23.29 (0.99) | 23.20 (0.98) | 23.38 (0.99) | 56.15 (2.38) | 23.59 (1.00) |
| return_chain_big | 4 nested calls return expected<array<int, 16>, unsigned> | 28.73† | 28.54 (0.99)† | 28.45 (0.99) | 28.73 (1.00) | 28.17 (0.98) | 28.03 (0.98) | 29.65 (1.03)† |

Summary against std::expected C++23 (geometric mean of the ratio over the scenarios both have; scenarios slower or faster by more than 10 %):

| variant | scenarios | geometric mean | slower | faster |
| --- | ---: | ---: | ---: | ---: |
| lumex C++23 | 37 | 1.009 | 4 | 5 |
| lumex C++20 | 37 | 1.007 | 4 | 6 |
| lumex C++17 | 37 | 1.005 | 4 | 5 |
| lumex C++14 | 37 | 1.008 | 5 | 6 |
| lumex C++11 | 37 | 1.066 | 6 | 5 |
| Outcome C++17 | 31 | 1.151 | 12 | 1 |

- lumex C++23 is slower by more than 10 % in: error_int 3.08x, deref_int 1.47x, transform_str12 1.22x, or_else_int 1.14x
- lumex C++20 is slower by more than 10 % in: error_int 3.05x, deref_int 1.48x, transform_str12 1.21x, or_else_int 1.14x
- lumex C++17 is slower by more than 10 % in: error_int 3.04x, deref_int 1.48x, transform_str12 1.18x, or_else_int 1.13x
- lumex C++14 is slower by more than 10 % in: error_int 3.06x, deref_int 1.48x, transform_str12 1.16x, or_else_int 1.13x, chain_int 1.11x
- lumex C++11 is slower by more than 10 % in: error_int 3.06x, return_chain_str12 2.69x, return_chain_str64 2.38x, deref_int 1.47x, return_chain_int 1.21x, or_else_int 1.13x
- Outcome C++17 is slower by more than 10 % in: error_int 3.96x, has_value_ok 2.27x, has_value_mixed 2.26x, construct_error_int 1.58x, assign_same_str12 1.32x, branch_mixed 1.27x, copy_value_str12 1.27x, copy_error_str12 1.22x, move_value_int 1.21x, return_chain_str12 1.20x, assign_switch_str12 1.19x, construct_value_int 1.14x

### clang: the layers of this library against lumex C++11

| scenario | lumex C++11 | lumex C++14 | lumex C++17 | lumex C++20 | lumex C++23 |
| --- | ---: | ---: | ---: | ---: | ---: |
| construct_value_int | 0.59 | 0.59 (1.00) | 0.60 (1.01) | 0.59 (1.00) | 0.59 (1.00) |
| construct_error_int | 0.49 | 0.49 (1.00) | 0.50 (1.01) | 0.49 (1.00) | 0.49 (1.01) |
| construct_value_big | 1.23 | 1.23 (1.00) | 1.23 (1.00) | 1.23 (1.00) | 1.24 (1.01) |
| construct_value_str12 | 10.13 | 10.18 (1.00) | 10.15 (1.00) | 10.23 (1.01) | 10.37 (1.02) |
| construct_value_str64 | 17.25 | 17.33 (1.00) | 17.19 (1.00) | 18.48 (1.07) | 18.40 (1.07) |
| construct_error_str12 | 10.26 | 10.30 (1.00) | 10.40 (1.01) | 10.24 (1.00) | 10.26 (1.00) |
| copy_value_int | 0.47† | 0.51 (1.09) | 0.49 (1.06) | 0.45 (0.96) | 0.44 (0.95) |
| copy_error_int | 0.55 | 0.53 (0.95) | 0.48 (0.87) | 0.44 (0.80) | 0.45 (0.81) |
| copy_value_big | 1.10 | 1.10 (1.00) | 1.11 (1.01) | 1.11 (1.01) | 1.10 (1.00) |
| copy_value_str12 | 1.02 | 1.01 (0.99) | 1.02 (1.00) | 1.01 (0.99) | 1.02 (0.99) |
| copy_value_str64 | 19.77 | 20.57 (1.04) | 20.30 (1.03) | 20.97 (1.06) | 21.00 (1.06) |
| copy_error_str12 | 1.03 | 1.00 (0.97) | 1.00 (0.97) | 1.00 (0.96) | 0.99 (0.96) |
| move_value_int | 0.59 | 0.59 (0.99) | 0.59 (0.99) | 0.58 (0.99) | 0.59 (1.00) |
| move_value_str12 | 10.51 | 10.14 (0.97) | 10.22 (0.97) | 10.19 (0.97) | 10.17 (0.97) |
| move_value_str64 | 17.52 | 17.62 (1.01) | 17.92 (1.02) | 17.04 (0.97) | 17.12 (0.98) |
| move_error_str12 | 10.55 | 10.24 (0.97) | 10.16 (0.96) | 10.29 (0.98) | 10.13 (0.96) |
| assign_same_int | 0.67 | 0.66 (1.00) | 0.67 (1.00) | 0.66 (1.00) | 0.67 (1.00) |
| assign_same_str12 | 1.84 | 1.85 (1.01) | 1.84 (1.00) | 2.02 (1.10) | 2.01 (1.10) |
| assign_switch_int | 0.39 | 0.39 (1.01) | 0.39 (1.00) | 0.39 (0.99) | 0.39 (1.00) |
| assign_switch_str12 | 2.04 | 2.06 (1.01) | 2.03 (1.00) | 2.05 (1.01) | 2.08 (1.02) |
| value_int | 0.38 | 0.38 (1.00) | 0.38 (1.00) | 0.38 (1.00) | 0.38 (1.00) |
| deref_int | 0.38 | 0.38 (1.00) | 0.38 (1.01) | 0.38 (1.00) | 0.38 (1.00) |
| error_int | 0.38 | 0.38 (1.00) | 0.38 (0.99) | 0.38 (1.00) | 0.38 (1.01) |
| value_str12 | 0.69 | 0.69 (0.99) | 0.69 (0.99) | 0.69 (0.99) | 0.69 (0.99) |
| has_value_ok | 0.18 | 0.18 (1.00) | 0.18 (1.00) | 0.18 (1.01) | 0.19 (1.01) |
| has_value_mixed | 0.18 | 0.18 (1.00) | 0.18 (1.00) | 0.18 (1.00) | 0.18 (1.00) |
| branch_mixed | 0.56 | 0.56 (0.99) | 0.57 (1.00) | 0.57 (1.00) | 0.57 (1.01) |
| value_or_mixed | 0.38 | 0.38 (1.00) | 0.38 (1.01) | 0.38 (1.00) | 0.38 (1.00) |
| transform_int | 0.78 | 0.78 (1.00) | 0.77 (0.99) | 0.78 (1.00) | 0.78 (1.00) |
| and_then_int | 1.56 | 1.56 (1.00) | 1.55 (0.99) | 1.56 (1.00) | 1.56 (0.99) |
| or_else_int | 0.84 | 0.84 (1.00) | 0.84 (1.00) | 0.84 (1.00) | 0.85 (1.01) |
| chain_int | 1.69 | 1.72 (1.01) | 1.70 (1.00) | 1.70 (1.00) | 1.69 (1.00) |
| transform_str12 | 1.55 | 1.79 (1.16) | 1.82 (1.18) | 1.87 (1.21) | 1.88 (1.21) |
| return_chain_int | 7.14 | 4.91 (0.69) | 4.92 (0.69) | 5.32 (0.75) | 5.46 (0.76) |
| return_chain_str12 | 45.64 | 16.89 (0.37) | 16.80 (0.37) | 17.27 (0.38) | 17.22 (0.38) |
| return_chain_str64 | 56.15 | 23.38 (0.42) | 23.20 (0.41) | 23.29 (0.41) | 23.17 (0.41) |
| return_chain_big | 28.03 | 28.17 (1.00) | 28.73 (1.02) | 28.45 (1.01) | 28.54 (1.02)† |

| layer | geometric mean | slower | faster |
| --- | ---: | ---: | ---: |
| lumex C++14 | 0.945 | 1 | 3 |
| lumex C++17 | 0.943 | 1 | 4 |
| lumex C++20 | 0.945 | 1 | 4 |
| lumex C++23 | 0.946 | 1 | 4 |

## Sizes and properties

`sizeof` in bytes, then the properties that decide how the type is passed and copied: TC trivially copyable (passed in registers, copied with `memcpy`), TD trivially destructible, SL standard layout, NM nothrow move constructible. `array16_t` is `std::array<int, 16>`; `text_error_t` is a struct that holds a `std::string`. Outcome refuses a result whose value type and error type are the same, so the pairs differ from `<T, T>`.

Toolchain gcc:

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

Toolchain clang:

| `expected<T, E>` | std::expected C++23 | lumex (all of C++11 to C++23) | Outcome C++17 |
| --- | --- | --- | --- |
| `<char, unsigned char>` | 2 B; TC no TD yes SL yes NM yes | 2 B; TC yes TD yes SL yes NM yes | 6 B; TC yes TD yes SL yes NM yes |
| `<int, unsigned>` | 8 B; TC no TD yes SL yes NM yes | 8 B; TC yes TD yes SL yes NM yes | 8 B; TC yes TD yes SL yes NM yes |
| `<long long, unsigned long long>` | 16 B; TC no TD yes SL yes NM yes | 16 B; TC yes TD yes SL yes NM yes | 16 B; TC yes TD yes SL yes NM yes |
| `<double, int>` | 16 B; TC no TD yes SL yes NM yes | 16 B; TC yes TD yes SL yes NM yes | 16 B; TC yes TD yes SL yes NM yes |
| `<int *, int>` | 16 B; TC no TD yes SL yes NM yes | 16 B; TC yes TD yes SL yes NM yes | 16 B; TC yes TD yes SL yes NM yes |
| `<empty_t, int>` | 8 B; TC no TD yes SL yes NM yes | 8 B; TC yes TD yes SL yes NM yes | 8 B; TC yes TD yes SL yes NM yes |
| `<int, std::error_code>` | 24 B; TC no TD yes SL yes NM yes | 24 B; TC yes TD yes SL yes NM yes | 24 B; TC yes TD yes SL yes NM yes |
| `<std::string, int>` | 32 B; TC no TD no SL yes NM yes | 32 B; TC no TD no SL yes NM yes | 32 B; TC no TD no SL yes NM yes |
| `<int, std::string>` | 32 B; TC no TD no SL yes NM yes | 32 B; TC no TD no SL yes NM yes | 32 B; TC no TD no SL yes NM yes |
| `<std::string, text_error_t>` | 32 B; TC no TD no SL yes NM yes | 32 B; TC no TD no SL yes NM yes | 56 B; TC no TD no SL yes NM yes |
| `<std::string, std::error_code>` | 32 B; TC no TD no SL yes NM yes | 32 B; TC no TD no SL yes NM yes | 48 B; TC no TD no SL yes NM yes |
| `<array16_t, int>` | 68 B; TC no TD yes SL yes NM yes | 68 B; TC yes TD yes SL yes NM yes | 68 B; TC yes TD yes SL yes NM yes |
| `<std::unique_ptr<int>, int>` | 16 B; TC no TD no SL yes NM yes | 16 B; TC no TD no SL yes NM yes | 16 B; TC no TD no SL yes NM yes |
| `<void, int>` | 8 B; TC no TD yes SL yes NM yes | 8 B; TC yes TD yes SL yes NM yes | 8 B; TC yes TD yes SL yes NM yes |
| `<void, std::error_code>` | 24 B; TC no TD yes SL yes NM yes | 24 B; TC yes TD yes SL yes NM yes | 24 B; TC yes TD yes SL yes NM yes |
| `<void, std::string>` | 32 B; TC no TD no SL yes NM yes | 32 B; TC no TD no SL yes NM yes | 32 B; TC no TD no SL yes NM yes |

## Compile time

- compiler gcc: g++ (GCC) 13.2.0
- compiler clang: clang version 23.1.0

Wall time of `compile_time.cpp` (includes the header and uses construction, copy, move, assignment, `value ()`, `error ()`, `swap` and, where the type has them, `value_or` and the monadic operations, for `<int, unsigned>`, `<std::string, text_error>` and `<void, int>`), median of the repeats, in milliseconds. `syntax_O0` is `-fsyntax-only -O0`, `compile_O2` is `-c -O2`. `no header` is the same unit without the header under test: the cost of `<string>`, `<utility>` and `<vector>`.

| toolchain, syntax_O0 | no header C++11 | no header C++23 | std::expected C++23 | lumex C++23 | lumex C++20 | lumex C++17 | lumex C++14 | lumex C++11 | Outcome C++17 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| gcc | 149 | 312 | 369 | 730 | 682 | 534 | 404 | 376 | 878 |
| clang | 156 | 265 | 312 | 680 | 471 | 394 | 336 | 312 | 667 |

| toolchain, compile_O2 | no header C++11 | no header C++23 | std::expected C++23 | lumex C++23 | lumex C++20 | lumex C++17 | lumex C++14 | lumex C++11 | Outcome C++17 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| gcc | 164 | 334 | 574 | 947 | 911 | 684 | 554 | 523 | 1779 |
| clang | 162 | 281 | 392 | 785 | 564 | 463 | 412 | 384 | 798 |

