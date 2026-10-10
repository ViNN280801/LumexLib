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
| construct_value_int | expected<int, unsigned> built from a value, then destroyed | 0.55 | 0.99 (1.79)† | 0.99 (1.79) | 0.96 (1.75) | 0.97 (1.75) | 0.56 (1.01) | 0.70 (1.27) |
| construct_error_int | expected<int, unsigned> built from an error, then destroyed | 0.56 | 0.99 (1.77) | 0.98 (1.75) | 0.98 (1.76) | 0.97 (1.75) | 0.56 (1.00) | 0.63 (1.13) |
| construct_value_big | expected<array<int, 16>, unsigned> built from a value | 3.48 | 3.48 (1.00) | 3.53 (1.02) | 3.53 (1.02) | 3.50 (1.01) | 3.49 (1.00) | 3.51 (1.01) |
| construct_value_str12 | expected<string, unsigned>, 12 characters (no allocation) | 3.87 | 4.14 (1.07) | 4.14 (1.07) | 5.25 (1.36) | 5.31 (1.37) | 5.25 (1.36) | 5.23 (1.35) |
| construct_value_str64 | expected<string, unsigned>, 64 characters (allocation) | 17.69 | 16.52 (0.93) | 16.45 (0.93) | 20.13 (1.14) | 20.05 (1.13) | 18.67 (1.06) | 22.30 (1.26) |
| construct_error_str12 | expected<int, string> from an error, 12 characters | 3.87 | 3.90 (1.01) | 3.88 (1.00) | 5.23 (1.35) | 5.29 (1.37) | 5.22 (1.35) | 9.41 (2.43) |
| copy_value_int | copy of an expected<int, unsigned> that holds a value | 0.67 | 0.67 (0.99) | 0.68 (1.01) | 0.68 (1.01) | 0.68 (1.01) | 0.67 (1.00) | 0.68 (1.01) |
| copy_error_int | copy of an expected<int, unsigned> that holds an error | 0.46 | 0.47 (1.02) | 0.47 (1.02) | 0.47 (1.01) | 0.46 (1.01) | 0.47 (1.01) | 0.56 (1.21) |
| copy_value_big | copy of an expected<array<int, 16>, unsigned> with a value | 1.05 | 1.04 (1.00) | 1.05 (1.00) | 1.05 (1.00) | 1.05 (1.00) | 1.05 (1.00) | 1.04 (1.00) |
| copy_value_str12 | copy of an expected<string, unsigned>, 12 characters | 2.43 | 2.44 (1.00) | 2.44 (1.00) | 3.47 (1.43) | 3.47 (1.43) | 3.49 (1.43) | 3.48 (1.43) |
| copy_value_str64 | copy of an expected<string, unsigned>, 64 characters | 20.70 | 18.53 (0.90) | 18.20 (0.88) | 21.17 (1.02) | 22.06 (1.07) | 19.85 (0.96) | 23.19 (1.12) |
| copy_error_str12 | copy of an expected<int, string> with an error, 12 chars | 2.44 | 2.44 (1.00) | 2.44 (1.00) | 3.48 (1.42) | 3.49 (1.43) | 3.48 (1.42) | 3.85 (1.58) |
| move_value_int | construct an expected<int, unsigned> and move it | 0.56 | 0.56 (1.00) | 0.56 (1.00) | 0.56 (1.00) | 0.56 (1.00) | 0.68 (1.22) | 0.62 (1.11) |
| move_value_str12 | construct an expected<string, unsigned> (12) and move it | 5.93 | 5.93 (1.00) | 5.92 (1.00) | 6.00 (1.01) | 5.94 (1.00) | 6.06 (1.02) | 7.36 (1.24) |
| move_value_str64 | construct an expected<string, unsigned> (64) and move it | 18.26 | 17.63 (0.97) | 17.72 (0.97) | 19.54 (1.07) | 19.10 (1.05) | 18.90 (1.03) | 22.32 (1.22) |
| move_error_str12 | construct an expected<int, string> (12) and move it | 5.92 | 5.95 (1.00) | 5.91 (1.00) | 6.11 (1.03) | 6.12 (1.03) | 6.15 (1.04) | 15.76 (2.66) |
| assign_same_int | copy assignment, value over value, expected<int, unsigned> | 0.93 | 0.76 (0.82) | 0.77 (0.83) | 0.77 (0.83) | 0.77 (0.83) | 0.76 (0.82) | 0.77 (0.83) |
| assign_same_str12 | copy assignment, value over value, expected<string, unsigned> | 2.78 | 3.10 (1.11) | 3.15 (1.14) | 3.83 (1.38) | 3.83 (1.38) | 3.81 (1.37) | 4.54 (1.63) |
| assign_switch_int | copy assignment that changes the alternative, <int, unsigned> | 0.70 | 0.52 (0.75) | 0.52 (0.75) | 0.52 (0.75) | 0.52 (0.75) | 0.58 (0.83) | 0.49 (0.70) |
| assign_switch_str12 | copy assignment that changes the alternative, <string, text_error_t> | 8.47 | 9.16 (1.08) | 9.01 (1.06) | 10.29 (1.22) | 10.91 (1.29) | 9.70 (1.15) | 10.74 (1.27) |
| value_int | value () of an expected<int, unsigned> that holds a value | 0.36 | 0.65 (1.78) | 0.66 (1.81) | 0.67 (1.85) | 0.36 (1.00) | 0.66 (1.82) | 0.66 (1.81) |
| deref_int | operator* of an expected<int, unsigned> that holds a value | 0.35 | 0.37 (1.04) | 0.36 (1.04) | 0.66 (1.89) | 0.37 (1.04) | 0.37 (1.04) | 0.35 (1.01) |
| error_int | error () of an expected<int, unsigned> that holds an error | 0.35 | 0.37 (1.04) | 0.36 (1.03) | 0.36 (1.04) | 0.60 (1.73) | 0.36 (1.04) | 0.36 (1.04) |
| value_str12 | value ().size () of an expected<string, unsigned> | 0.70 | 0.40 (0.58) | 0.41 (0.58) | 0.41 (0.58) | 0.42 (0.60) | 0.40 (0.58) | 0.42 (0.60) |
| has_value_ok | has_value () over values only (predictable branch) | 0.59 | 0.35 (0.60) | 0.35 (0.59) | 0.36 (0.62) | 0.36 (0.61) | 0.54 (0.91)† | 0.35 (0.60) |
| has_value_mixed | has_value () over 1/8 errors in a fixed random order | 0.36 | 0.35 (0.95) | 0.35 (0.96) | 0.35 (0.97) | 0.35 (0.95) | 0.37 (1.01) | 0.35 (0.96) |
| branch_mixed | if (has_value ()) *x else x.error (), 1/8 errors | 0.63 | 0.88 (1.40) | 0.88 (1.41) | 0.89 (1.42) | 0.89 (1.42) | 0.63 (1.00) | 0.60 (0.95) |
| value_or_mixed | value_or (-1), 1/8 errors | 0.86 | 0.56 (0.65) | 0.55 (0.65) | 0.55 (0.64) | 0.56 (0.65) | 0.62 (0.72) |  |
| transform_int | transform (v + 1) on <int, unsigned>, 1/8 errors | 0.91 | 0.97 (1.08) | 0.98 (1.08) | 1.10 (1.21) | 1.13 (1.25) | 1.02 (1.12) |  |
| and_then_int | and_then (returns an expected) on <int, unsigned>, 1/8 errors | 1.18 | 1.35 (1.15) | 1.37 (1.17) | 1.18 (1.00) | 1.11 (0.94) | 1.22 (1.04) |  |
| or_else_int | or_else (recovers the error) on <int, unsigned>, 1/8 errors | 0.64 | 0.77 (1.20) | 0.77 (1.20) | 0.78 (1.21) | 0.77 (1.19) | 0.77 (1.20) |  |
| chain_int | transform, and_then, or_else in a row, <int, unsigned> | 1.12 | 1.11 (0.99) | 1.11 (0.99) | 1.01 (0.91) | 1.04 (0.93) | 1.11 (0.99) |  |
| transform_str12 | transform (s.size ()) on <string, unsigned>, const lvalue | 0.98 | 1.49 (1.51) | 1.46 (1.49) | 1.53 (1.56) | 1.37 (1.39) | 1.89 (1.93) |  |
| return_chain_int | 4 nested calls return expected<int, unsigned> by value | 6.43 | 28.74 (4.47) | 28.89 (4.49) | 28.57 (4.44) | 29.00 (4.51) | 28.83 (4.48) | 6.30 (0.98) |
| return_chain_str12 | 4 nested calls return expected<string, unsigned> (12) | 44.56 | 44.43 (1.00) | 44.79 (1.01) | 44.78 (1.00) | 44.40 (1.00) | 44.55 (1.00) | 44.60 (1.00) |
| return_chain_str64 | 4 nested calls return expected<string, unsigned> (64) | 23.36 | 21.32 (0.91) | 21.59 (0.92) | 23.93 (1.02) | 24.46 (1.05) | 23.19 (0.99) | 23.37 (1.00) |
| return_chain_big | 4 nested calls return expected<array<int, 16>, unsigned> | 16.86 | 16.75 (0.99) | 16.77 (0.99) | 16.79 (1.00) | 16.76 (0.99) | 16.84 (1.00) | 16.75 (0.99) |

Summary against std::expected C++23 (geometric mean of the ratio over the scenarios both have; scenarios slower or faster by more than 10 %):

| variant | scenarios | geometric mean | slower | faster |
| --- | ---: | ---: | ---: | ---: |
| lumex C++23 | 37 | 1.066 | 9 | 6 |
| lumex C++20 | 37 | 1.067 | 9 | 6 |
| lumex C++17 | 37 | 1.150 | 16 | 5 |
| lumex C++14 | 37 | 1.128 | 15 | 5 |
| lumex C++11 | 37 | 1.112 | 12 | 4 |
| Outcome C++17 | 31 | 1.142 | 16 | 4 |

- lumex C++23 is slower by more than 10 % in: return_chain_int 4.47x, construct_value_int 1.79x, value_int 1.78x, construct_error_int 1.77x, transform_str12 1.51x, branch_mixed 1.40x, or_else_int 1.20x, and_then_int 1.15x, assign_same_str12 1.11x
- lumex C++20 is slower by more than 10 % in: return_chain_int 4.49x, value_int 1.81x, construct_value_int 1.79x, construct_error_int 1.75x, transform_str12 1.49x, branch_mixed 1.41x, or_else_int 1.20x, and_then_int 1.17x, assign_same_str12 1.14x
- lumex C++17 is slower by more than 10 % in: return_chain_int 4.44x, deref_int 1.89x, value_int 1.85x, construct_error_int 1.76x, construct_value_int 1.75x, transform_str12 1.56x, copy_value_str12 1.43x, branch_mixed 1.42x, copy_error_str12 1.42x, assign_same_str12 1.38x, construct_value_str12 1.36x, construct_error_str12 1.35x, assign_switch_str12 1.22x, transform_int 1.21x, or_else_int 1.21x, construct_value_str64 1.14x
- lumex C++14 is slower by more than 10 % in: return_chain_int 4.51x, construct_value_int 1.75x, construct_error_int 1.75x, error_int 1.73x, copy_value_str12 1.43x, copy_error_str12 1.43x, branch_mixed 1.42x, transform_str12 1.39x, assign_same_str12 1.38x, construct_value_str12 1.37x, construct_error_str12 1.37x, assign_switch_str12 1.29x, transform_int 1.25x, or_else_int 1.19x, construct_value_str64 1.13x
- lumex C++11 is slower by more than 10 % in: return_chain_int 4.48x, transform_str12 1.93x, value_int 1.82x, copy_value_str12 1.43x, copy_error_str12 1.42x, assign_same_str12 1.37x, construct_value_str12 1.36x, construct_error_str12 1.35x, move_value_int 1.22x, or_else_int 1.20x, assign_switch_str12 1.15x, transform_int 1.12x
- Outcome C++17 is slower by more than 10 % in: move_error_str12 2.66x, construct_error_str12 2.43x, value_int 1.81x, assign_same_str12 1.63x, copy_error_str12 1.58x, copy_value_str12 1.43x, construct_value_str12 1.35x, assign_switch_str12 1.27x, construct_value_int 1.27x, construct_value_str64 1.26x, move_value_str12 1.24x, move_value_str64 1.22x, copy_error_int 1.21x, construct_error_int 1.13x, copy_value_str64 1.12x, move_value_int 1.11x

### gcc: the layers of this library against lumex C++11

| scenario | lumex C++11 | lumex C++14 | lumex C++17 | lumex C++20 | lumex C++23 |
| --- | ---: | ---: | ---: | ---: | ---: |
| construct_value_int | 0.56 | 0.97 (1.74) | 0.96 (1.73) | 0.99 (1.78) | 0.99 (1.78)† |
| construct_error_int | 0.56 | 0.97 (1.74) | 0.98 (1.75) | 0.98 (1.75) | 0.99 (1.76) |
| construct_value_big | 3.49 | 3.50 (1.00) | 3.53 (1.01) | 3.53 (1.01) | 3.48 (1.00) |
| construct_value_str12 | 5.25 | 5.31 (1.01) | 5.25 (1.00) | 4.14 (0.79) | 4.14 (0.79) |
| construct_value_str64 | 18.67 | 20.05 (1.07) | 20.13 (1.08) | 16.45 (0.88) | 16.52 (0.88) |
| construct_error_str12 | 5.22 | 5.29 (1.01) | 5.23 (1.00) | 3.88 (0.74) | 3.90 (0.75) |
| copy_value_int | 0.67 | 0.68 (1.01) | 0.68 (1.01) | 0.68 (1.01) | 0.67 (0.99) |
| copy_error_int | 0.47 | 0.46 (0.99) | 0.47 (1.00) | 0.47 (1.00) | 0.47 (1.00) |
| copy_value_big | 1.05 | 1.05 (1.00) | 1.05 (1.00) | 1.05 (1.00) | 1.04 (1.00) |
| copy_value_str12 | 3.49 | 3.47 (1.00) | 3.47 (1.00) | 2.44 (0.70) | 2.44 (0.70) |
| copy_value_str64 | 19.85 | 22.06 (1.11) | 21.17 (1.07) | 18.20 (0.92) | 18.53 (0.93) |
| copy_error_str12 | 3.48 | 3.49 (1.00) | 3.48 (1.00) | 2.44 (0.70) | 2.44 (0.70) |
| move_value_int | 0.68 | 0.56 (0.82) | 0.56 (0.82) | 0.56 (0.82) | 0.56 (0.82) |
| move_value_str12 | 6.06 | 5.94 (0.98) | 6.00 (0.99) | 5.92 (0.98) | 5.93 (0.98) |
| move_value_str64 | 18.90 | 19.10 (1.01) | 19.54 (1.03) | 17.72 (0.94) | 17.63 (0.93) |
| move_error_str12 | 6.15 | 6.12 (1.00) | 6.11 (0.99) | 5.91 (0.96) | 5.95 (0.97) |
| assign_same_int | 0.76 | 0.77 (1.01) | 0.77 (1.01) | 0.77 (1.01) | 0.76 (1.00) |
| assign_same_str12 | 3.81 | 3.83 (1.01) | 3.83 (1.01) | 3.15 (0.83) | 3.10 (0.81) |
| assign_switch_int | 0.58 | 0.52 (0.90) | 0.52 (0.90) | 0.52 (0.90) | 0.52 (0.90) |
| assign_switch_str12 | 9.70 | 10.91 (1.12) | 10.29 (1.06) | 9.01 (0.93) | 9.16 (0.94) |
| value_int | 0.66 | 0.36 (0.55) | 0.67 (1.02) | 0.66 (1.00) | 0.65 (0.98) |
| deref_int | 0.37 | 0.37 (1.00) | 0.66 (1.81) | 0.36 (0.99) | 0.37 (1.00) |
| error_int | 0.36 | 0.60 (1.66) | 0.36 (1.00) | 0.36 (0.98) | 0.37 (1.00) |
| value_str12 | 0.40 | 0.42 (1.04) | 0.41 (1.02) | 0.41 (1.01) | 0.40 (1.00) |
| has_value_ok | 0.54† | 0.36 (0.67) | 0.36 (0.68) | 0.35 (0.65) | 0.35 (0.65) |
| has_value_mixed | 0.37 | 0.35 (0.95) | 0.35 (0.96) | 0.35 (0.95) | 0.35 (0.95) |
| branch_mixed | 0.63 | 0.89 (1.41) | 0.89 (1.42) | 0.88 (1.40) | 0.88 (1.40) |
| value_or_mixed | 0.62 | 0.56 (0.90) | 0.55 (0.89) | 0.55 (0.90) | 0.56 (0.90) |
| transform_int | 1.02 | 1.13 (1.11) | 1.10 (1.08) | 0.98 (0.97) | 0.97 (0.96) |
| and_then_int | 1.22 | 1.11 (0.91) | 1.18 (0.97) | 1.37 (1.13) | 1.35 (1.11) |
| or_else_int | 0.77 | 0.77 (1.00) | 0.78 (1.01) | 0.77 (1.00) | 0.77 (1.00) |
| chain_int | 1.11 | 1.04 (0.94) | 1.01 (0.92) | 1.11 (1.00) | 1.11 (1.01) |
| transform_str12 | 1.89 | 1.37 (0.72) | 1.53 (0.81) | 1.46 (0.77) | 1.49 (0.79) |
| return_chain_int | 28.83 | 29.00 (1.01) | 28.57 (0.99) | 28.89 (1.00) | 28.74 (1.00) |
| return_chain_str12 | 44.55 | 44.40 (1.00) | 44.78 (1.01) | 44.79 (1.01) | 44.43 (1.00) |
| return_chain_str64 | 23.19 | 24.46 (1.06) | 23.93 (1.03) | 21.59 (0.93) | 21.32 (0.92) |
| return_chain_big | 16.84 | 16.76 (1.00) | 16.79 (1.00) | 16.77 (1.00) | 16.75 (1.00) |

| layer | geometric mean | slower | faster |
| --- | ---: | ---: | ---: |
| lumex C++14 | 1.015 | 7 | 5 |
| lumex C++17 | 1.035 | 4 | 5 |
| lumex C++20 | 0.960 | 4 | 11 |
| lumex C++23 | 0.959 | 4 | 11 |

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
| gcc | 135 | 280 | 330 | 631 | 605 | 468 | 356 | 338 | 781 |

| toolchain, compile_O2 | no header C++11 | no header C++23 | std::expected C++23 | lumex C++23 | lumex C++20 | lumex C++17 | lumex C++14 | lumex C++11 | Outcome C++17 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| gcc | 148 | 303 | 520 | 830 | 808 | 602 | 490 | 468 | 1629 |

