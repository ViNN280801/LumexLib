# optional benchmark results

- machine="12th Gen Intel(R) Core(TM) i7-12700K" os="Linux 6.1.170-1-generic" logical_cpus=20 python=3.7.3
- passes=5 cpu=3
- variant gcc lumex_cxx11: compiler="GCC 13.2.0" library="libstdc++" cplusplus=201103
- variant gcc lumex_cxx17: compiler="GCC 13.2.0" library="libstdc++" cplusplus=201703
- variant gcc std_cxx17: compiler="GCC 13.2.0" library="libstdc++" cplusplus=201703
- variant gcc boost_cxx11: compiler="GCC 13.2.0" library="libstdc++" cplusplus=201103
- variant gcc boost_cxx17: compiler="GCC 13.2.0" library="libstdc++" cplusplus=201703

Time per operation in nanoseconds: the lowest median of 15 repetitions over all passes (the best pass), and in parentheses the ratio to `std::optional` at C++17 (below 1.00 is faster than that). The unit of an operation is the description of the scenario. A dagger marks a value whose slowest pass was more than 25 % slower than the best one: the processes of that executable do not agree (code and stack placement), so differences inside that band are noise.

## gcc

| scenario | size | std::optional C++17 | lumex C++11 | lumex C++17 | boost::optional C++11 | boost::optional C++17 |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| ctor_empty_int |  | 0.29 | 0.29 (1.00) | 0.29 (1.00) | 0.28 (0.98) | 0.29 (1.00) |
| ctor_empty_str_short |  | 0.35† | 0.24 (0.67) | 0.24 (0.67) | 0.35 (1.00)† | 0.35 (1.00)† |
| ctor_value_int |  | 0.29 | 0.29 (1.00) | 0.28 (0.98) | 0.29 (1.00) | 0.29 (1.00) |
| ctor_value_pod32 |  | 0.70 | 0.71 (1.01) | 0.71 (1.00) | 0.70 (1.00) | 0.70 (1.00)† |
| ctor_value_str_short |  | 2.69 | 2.58 (0.96) | 2.72 (1.01) | 2.58 (0.96) | 2.57 (0.96) |
| ctor_value_str_long |  | 13.82 | 12.59 (0.91) | 12.94 (0.94) | 12.71 (0.92) | 13.56 (0.98) |
| copy_ctor_int |  | 0.23 | 0.47 (2.00) | 0.47 (2.00) | 0.42 (1.80) | 0.56 (2.40) |
| copy_ctor_pod32 |  | 0.87 | 0.86 (1.00) | 0.86 (0.99) | 0.93 (1.07) | 0.86 (0.99) |
| copy_ctor_str_short |  | 2.82 | 2.82 (1.00) | 2.82 (1.00) | 2.83 (1.00) | 2.59 (0.92) |
| copy_ctor_str_long |  | 14.10 | 12.70 (0.90) | 13.29 (0.94) | 12.92 (0.92) | 13.47 (0.96) |
| make_move_destroy_int |  | 5.37 | 0.77 (0.14) | 0.73 (0.14)† | 0.68 (0.13) | 0.71 (0.13) |
| make_move_destroy_pod32 |  | 1.18 | 1.41 (1.20) | 1.40 (1.19) | 1.40 (1.19)† | 1.40 (1.19) |
| make_move_destroy_str_short |  | 7.26 | 7.03 (0.97) | 7.01 (0.97) | 7.25 (1.00) | 7.25 (1.00) |
| make_move_destroy_str_long |  | 14.74 | 13.01 (0.88) | 13.54 (0.92) | 13.54 (0.92) | 13.57 (0.92) |
| assign_copy_int |  | 0.23 | 0.47 (2.00) | 0.47 (2.00) | 0.47 (2.00) | 0.47 (1.99) |
| assign_copy_pod32 |  | 0.86 | 1.01 (1.18) | 1.02 (1.19) | 0.94 (1.09) | 0.94 (1.09) |
| assign_copy_str_short |  | 3.04 | 2.83 (0.93) | 2.82 (0.93) | 2.82 (0.93) | 2.82 (0.93) |
| assign_copy_str_long |  | 3.29† | 12.77 (3.88) | 13.23 (4.02) | 3.30 (1.00) | 3.28 (1.00) |
| assign_value_int |  | 0.32 | 0.35 (1.10) | 0.35 (1.09) | 0.35 (1.09) | 0.35 (1.09) |
| assign_value_pod32 |  | 0.86 | 0.86 (1.00) | 0.86 (1.00) | 0.86 (1.00) | 0.86 (1.00) |
| assign_value_str_short |  | 2.82 | 2.82 (1.00) | 2.83 (1.00) | 2.81 (1.00) | 2.82 (1.00) |
| assign_value_str_long |  | 3.17 | 3.12 (0.98) | 3.27 (1.03) | 3.06 (0.96) | 3.06 (0.96) |
| emplace_reset_int |  | 0.42 | 0.42 (1.00) | 0.42 (1.00) | 0.35 (0.84) | 0.35 (0.83) |
| emplace_reset_pod32 |  | 0.87 | 0.86 (0.99) | 0.86 (0.99) | 0.71 (0.82)† | 0.71 (0.82)† |
| emplace_reset_str_short |  | 2.64 | 2.60 (0.98) | 2.63 (1.00) | 2.63 (1.00) | 2.63 (0.99) |
| emplace_reset_str_long |  | 13.63 | 12.51 (0.92) | 13.16 (0.97) | 12.91 (0.95) | 13.72 (1.01) |
| swap_int |  | 0.52 | 0.52 (1.00) | 0.52 (1.00) | 0.47 (0.91) | 0.47 (0.91) |
| swap_str_short |  | 3.99 | 4.00 (1.00) | 3.99 (1.00) | 4.00 (1.00) | 4.00 (1.00) |
| deref_sum | 8 | 2.20 | 1.64 (0.75) | 2.31 (1.05) | 2.63 (1.20) | 1.64 (0.75) |
| deref_sum | 1,024 | 235.4 | 234.0 (0.99) | 235.5 (1.00) | 235.7 (1.00) | 236.4 (1.00) |
| value_sum | 1,024 | 346.7 | 329.3 (0.95) | 329.8 (0.95) | 329.0 (0.95) | 346.7 (1.00) |
| value_or_sum | 1,024 | 534.7 | 2715 (5.08) | 2699 (5.05) | 513.9 (0.96) | 498.8 (0.93) |
| count_engaged | 1,024 | 242.7 | 243.0 (1.00) | 235.0 (0.97) | 235.2 (0.97) | 233.5 (0.96) |
| eq_opt_opt | 1,024 | 708.1 | 745.0 (1.05) | 560.1 (0.79) | 710.5 (1.00) | 688.2 (0.97) |
| lt_opt_opt | 1,024 | 509.8 | 555.3 (1.09) | 625.3 (1.23) | 649.8 (1.27) | 607.0 (1.19) |
| eq_value | 1,024 | 679.7 | 535.1 (0.79) | 624.1 (0.92) | 514.1 (0.76) | 497.8 (0.73) |
| eq_nullopt | 1,024 | 251.3 | 251.5 (1.00) | 250.4 (1.00) | 251.1 (1.00) | 251.1 (1.00) |
| eq_opt_opt_str | 256 | 450.4 | 508.5 (1.13) | 503.5 (1.12) | 525.6 (1.17) | 571.2 (1.27) |
| return_int |  | 5.31 | 0.65 (0.12) | 0.65 (0.12) | 0.65 (0.12) | 0.64 (0.12) |
| pass_int |  | 4.38 | 0.59 (0.13) | 0.59 (0.14) | 0.59 (0.13) | 0.59 (0.13) |
| return_str |  | 4.37 | 3.94 (0.90) | 4.24 (0.97) | 4.19 (0.96) | 4.06 (0.93) |
| pass_str |  | 3.89 | 3.66 (0.94) | 3.61 (0.93) | 3.99 (1.03) | 3.98 (1.02) |
| vector_copy_int | 1,024 | 194.4 | 539.1 (2.77) | 522.0 (2.69) | 751.3 (3.86) | 534.0 (2.75) |
| vector_copy_str | 256 | 894.7 | 841.7 (0.94) | 796.0 (0.89) | 870.0 (0.97) | 824.6 (0.92) |
| vector_push_int | 1,024 | 4557 | 502.7 (0.11) | 492.2 (0.11) | 732.4 (0.16) | 733.2 (0.16) |
| vector_grow_str | 256 | 2203 | 2082 (0.94) | 2190 (0.99) | 2225 (1.01) | 2179 (0.99) |

Variants compared with each other (time of the first divided by the time of the second; the first is faster below 1.00): geometric mean over all scenarios, the best and the worst scenario of the first, and the number of scenarios where the first is slower or faster by more than 10 %:

| compared | scenarios | geometric mean | slower | faster | best | worst |
| --- | ---: | ---: | ---: | ---: | --- | --- |
| lumex C++17 / std::optional C++17 | 46 | 0.926 | 9 | 7 | vector_push_int (1,024) 0.11x | value_or_sum (1,024) 5.05x |
| lumex C++11 / std::optional C++17 | 46 | 0.916 | 8 | 8 | vector_push_int (1,024) 0.11x | value_or_sum (1,024) 5.08x |
| lumex C++11 / lumex C++17 | 46 | 0.989 | 1 | 3 | deref_sum (8) 0.71x | eq_opt_opt (1,024) 1.33x |
| lumex C++17 / boost::optional C++17 | 46 | 1.064 | 6 | 5 | ctor_empty_str_short 0.67x | value_or_sum (1,024) 5.41x |
| lumex C++11 / boost::optional C++11 | 46 | 1.038 | 7 | 5 | deref_sum (8) 0.62x | value_or_sum (1,024) 5.28x |

- lumex C++17 is slower than std::optional C++17 by more than 10 % in: value_or_sum (1,024) 5.05x, assign_copy_str_long 4.02x, vector_copy_int (1,024) 2.69x, assign_copy_int 2.00x, copy_ctor_int 2.00x, lt_opt_opt (1,024) 1.23x, make_move_destroy_pod32 1.19x, assign_copy_pod32 1.19x, eq_opt_opt_str (256) 1.12x
- lumex C++17 is faster than std::optional C++17 by more than 10 % in: vector_push_int (1,024) 0.11x, return_int 0.12x, pass_int 0.14x, make_move_destroy_int 0.14x, ctor_empty_str_short 0.67x, eq_opt_opt (1,024) 0.79x, vector_copy_str (256) 0.89x
- lumex C++11 is slower than std::optional C++17 by more than 10 % in: value_or_sum (1,024) 5.08x, assign_copy_str_long 3.88x, vector_copy_int (1,024) 2.77x, assign_copy_int 2.00x, copy_ctor_int 2.00x, make_move_destroy_pod32 1.20x, assign_copy_pod32 1.18x, eq_opt_opt_str (256) 1.13x
- lumex C++11 is faster than std::optional C++17 by more than 10 % in: vector_push_int (1,024) 0.11x, return_int 0.12x, pass_int 0.13x, make_move_destroy_int 0.14x, ctor_empty_str_short 0.67x, deref_sum (8) 0.75x, eq_value (1,024) 0.79x, make_move_destroy_str_long 0.88x

## Layout and traits of optional<T>

`sizeof` and `alignof` of the optional and the properties of the optional type (not of `T`). TC: trivially copyable, TD: trivially destructible, TCC: trivially copy constructible, SL: standard layout, NM: nothrow move constructible. Sizes in bytes; the values do not depend on the toolchain of this run (one 64-bit Linux x86-64 data model).

| T | sizeof (T) | std::optional C++17: size, TC/TD/TCC/SL/NM | lumex C++11: size, TC/TD/TCC/SL/NM | lumex C++17: size, TC/TD/TCC/SL/NM | boost::optional C++11: size, TC/TD/TCC/SL/NM | boost::optional C++17: size, TC/TD/TCC/SL/NM |
| --- | ---: | --- | --- | --- | --- | --- |
| `bool` | 1 | 2, Y/Y/Y/Y/Y | 2, n/n/n/Y/Y | 2, n/n/n/Y/Y | 2, n/Y/n/Y/Y | 2, n/Y/n/Y/Y |
| `char` | 1 | 2, Y/Y/Y/Y/Y | 2, n/n/n/Y/Y | 2, n/n/n/Y/Y | 2, n/Y/n/Y/Y | 2, n/Y/n/Y/Y |
| `int` | 4 | 8, Y/Y/Y/Y/Y | 8, n/n/n/Y/Y | 8, n/n/n/Y/Y | 8, n/Y/n/Y/Y | 8, n/Y/n/Y/Y |
| `double` | 8 | 16, Y/Y/Y/Y/Y | 16, n/n/n/Y/Y | 16, n/n/n/Y/Y | 16, n/Y/n/Y/Y | 16, n/Y/n/Y/Y |
| `uint64_t` | 8 | 16, Y/Y/Y/Y/Y | 16, n/n/n/Y/Y | 16, n/n/n/Y/Y | 16, n/Y/n/Y/Y | 16, n/Y/n/Y/Y |
| `pod32` | 32 | 40, Y/Y/Y/Y/Y | 40, n/n/n/Y/Y | 40, n/n/n/Y/Y | 40, n/Y/n/Y/Y | 40, n/Y/n/Y/Y |
| `std::string` | 32 | 40, n/n/n/Y/Y | 40, n/n/n/Y/Y | 40, n/n/n/Y/Y | 40, n/n/n/Y/Y | 40, n/n/n/Y/Y |
| `std::vector<int>` | 24 | 32, n/n/n/Y/Y | 32, n/n/n/Y/Y | 32, n/n/n/Y/Y | 32, n/n/n/Y/Y | 32, n/n/n/Y/Y |
| `std::unique_ptr<int>` | 8 | 16, n/n/n/Y/Y | 16, n/n/n/Y/Y | 16, n/n/n/Y/Y | 16, n/n/n/Y/Y | 16, n/n/n/Y/Y |
| `std::shared_ptr<int>` | 16 | 24, n/n/n/Y/Y | 24, n/n/n/Y/Y | 24, n/n/n/Y/Y | 24, n/n/n/Y/Y | 24, n/n/n/Y/Y |

## Compile time

- compiler gcc: g++ (GCC) 13.2.0

Wall time of `-fsyntax-only -O0` of `compile_time.cpp`, median of the repeats, in milliseconds. `no optional` is the same unit with a stub instead of an optional: the cost of the shared headers (`<string>`, `<vector>`, `<unordered_set>`).

Unit `include`: the header is included and one object is declared.

| toolchain | no optional C++11 | no optional C++17 | std::optional C++17 | lumex C++11 | lumex C++17 | boost::optional C++11 | boost::optional C++17 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| gcc | 110 | 147 | 152 | 117 | 188 | 131 | 192 |

Unit `use`: the whole API is used: construction, observers, modifiers, comparisons, `swap`, `std::vector` of optionals, `std::hash`.

| toolchain | no optional C++11 | no optional C++17 | std::optional C++17 | lumex C++11 | lumex C++17 | boost::optional C++11 | boost::optional C++17 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| gcc | 110 | 146 | 189 | 142 | 212 | 149 | 212 |

