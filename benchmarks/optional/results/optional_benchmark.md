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
| ctor_empty_int |  | 0.30 | 0.30 (1.01) | 0.31 (1.01) | 0.30 (1.01) | 0.30 (1.00) |
| ctor_empty_str_short |  | 0.39† | 0.38 (0.97)† | 0.38 (0.98)† | 0.38 (0.97)† | 0.38 (0.98) |
| ctor_value_int |  | 0.30 | 0.30 (0.99) | 0.31 (1.02) | 0.31 (1.01) | 0.30 (1.00) |
| ctor_value_pod32 |  | 0.73 | 0.73 (1.00) | 0.73 (1.00) | 0.73 (1.00)† | 0.73 (1.00) |
| ctor_value_str_short |  | 2.83 | 2.74 (0.97) | 2.83 (1.00) | 2.74 (0.97) | 2.72 (0.96) |
| ctor_value_str_long |  | 14.72 | 13.60 (0.92) | 14.57 (0.99) | 13.57 (0.92) | 14.45 (0.98) |
| copy_ctor_int |  | 0.25 | 0.25 (1.00) | 0.25 (1.00) | 0.45 (1.79) | 0.60 (2.41) |
| copy_ctor_pod32 |  | 0.89 | 0.88 (1.00) | 0.88 (1.00) | 0.90 (1.02)† | 0.89 (1.01) |
| copy_ctor_str_short |  | 3.09 | 3.05 (0.99) | 3.10 (1.00) | 3.08 (0.99) | 2.87 (0.93) |
| copy_ctor_str_long |  | 15.14 | 13.93 (0.92) | 14.47 (0.96) | 13.77 (0.91) | 14.25 (0.94) |
| make_move_destroy_int |  | 5.38 | 5.37 (1.00) | 5.39 (1.00) | 0.63 (0.12) | 0.75 (0.14) |
| make_move_destroy_pod32 |  | 1.23 | 1.21 (0.99) | 1.23 (1.00) | 1.49 (1.22) | 1.69 (1.38) |
| make_move_destroy_str_short |  | 7.58 | 7.22 (0.95) | 7.36 (0.97) | 7.36 (0.97) | 7.36 (0.97) |
| make_move_destroy_str_long |  | 15.91 | 14.47 (0.91) | 15.24 (0.96) | 14.76 (0.93) | 14.92 (0.94) |
| assign_copy_int |  | 0.25 | 0.26 (1.02) | 0.25 (1.00) | 0.49 (1.97) | 0.49 (1.97) |
| assign_copy_pod32 |  | 0.89 | 0.89 (1.00) | 0.89 (1.00) | 0.98 (1.09) | 0.98 (1.10) |
| assign_copy_str_short |  | 3.41 | 3.34 (0.98) | 3.37 (0.99) | 3.10 (0.91) | 3.08 (0.90) |
| assign_copy_str_long |  | 3.65† | 3.59 (0.98)† | 3.64 (1.00)† | 3.59 (0.98) | 3.62 (0.99) |
| assign_value_int |  | 0.35 | 0.33 (0.96) | 0.35 (0.99) | 0.37 (1.07) | 0.37 (1.06) |
| assign_value_pod32 |  | 0.90 | 0.89 (0.99) | 0.89 (0.99) | 0.90 (1.00) | 0.89 (0.99) |
| assign_value_str_short |  | 3.08 | 3.13 (1.02) | 3.05 (0.99) | 3.09 (1.00) | 3.07 (1.00) |
| assign_value_str_long |  | 3.43† | 3.46 (1.01) | 3.48 (1.01) | 3.38 (0.98) | 3.32 (0.97)† |
| emplace_reset_int |  | 0.46 | 0.38 (0.83) | 0.38 (0.83) | 0.38 (0.83) | 0.38 (0.83) |
| emplace_reset_pod32 |  | 0.89 | 0.75 (0.84) | 0.74 (0.83) | 0.74 (0.84)† | 0.73 (0.83)† |
| emplace_reset_str_short |  | 2.89 | 2.88 (1.00) | 2.83 (0.98) | 2.83 (0.98) | 2.87 (0.99) |
| emplace_reset_str_long |  | 14.57 | 13.69 (0.94) | 14.17 (0.97) | 13.93 (0.96) | 14.50 (0.99) |
| swap_int |  | 0.56 | 0.55 (0.99) | 0.55 (0.99) | 0.50 (0.90) | 0.50 (0.90) |
| swap_str_short |  | 4.34 | 4.41 (1.02) | 4.38 (1.01) | 4.32 (1.00) | 4.34 (1.00) |
| deref_sum | 8 | 2.27 | 2.38 (1.05) | 2.12 (0.93) | 2.53 (1.11) | 1.78 (0.78) |
| deref_sum | 1,024 | 243.2 | 239.4 (0.98) | 240.8 (0.99) | 239.2 (0.98) | 242.4 (1.00) |
| value_sum | 1,024 | 356.9 | 360.2 (1.01) | 358.6 (1.00) | 349.7 (0.98) | 362.4 (1.02) |
| value_or_sum | 1,024 | 552.1 | 2698 (4.89) | 763.9 (1.38) | 535.1 (0.97) | 520.2 (0.94) |
| count_engaged | 1,024 | 246.2 | 240.1 (0.98) | 251.2 (1.02) | 240.5 (0.98) | 241.5 (0.98) |
| eq_opt_opt | 1,024 | 735.7 | 654.3 (0.89) | 1015 (1.38) | 750.3 (1.02) | 689.8 (0.94) |
| lt_opt_opt | 1,024 | 545.4 | 543.7 (1.00) | 546.9 (1.00) | 678.1 (1.24) | 636.7 (1.17) |
| eq_value | 1,024 | 588.9 | 684.2 (1.16) | 601.2 (1.02) | 536.9 (0.91) | 516.8 (0.88) |
| eq_nullopt | 1,024 | 259.5 | 258.7 (1.00) | 266.1 (1.03) | 262.3 (1.01) | 260.5 (1.00) |
| eq_opt_opt_str | 256 | 473.2 | 474.4 (1.00) | 468.3 (0.99) | 558.2 (1.18) | 596.6 (1.26) |
| return_int |  | 5.30 | 5.33 (1.01) | 5.32 (1.00) | 0.71 (0.13) | 0.68 (0.13) |
| pass_int |  | 4.37 | 4.36 (1.00) | 4.36 (1.00) | 0.66 (0.15) | 0.62 (0.14) |
| return_str |  | 4.56 | 4.21 (0.92) | 4.67 (1.02) | 4.39 (0.96) | 4.41 (0.97) |
| pass_str |  | 4.09 | 4.08 (1.00) | 3.95 (0.97) | 4.22 (1.03) | 4.25 (1.04) |
| vector_copy_int | 1,024 | 204.1 | 212.8 (1.04) | 202.8 (0.99) | 785.4 (3.85) | 573.5 (2.81) |
| vector_copy_str | 256 | 940.5 | 919.2 (0.98) | 896.6 (0.95) | 930.2 (0.99) | 868.8 (0.92) |
| vector_push_int | 1,024 | 4585 | 4595 (1.00) | 4593 (1.00) | 764.0 (0.17) | 773.2 (0.17) |
| vector_grow_str | 256 | 2311 | 2249 (0.97) | 2322 (1.00) | 2360 (1.02) | 2314 (1.00) |

Variants compared with each other (time of the first divided by the time of the second; the first is faster below 1.00): geometric mean over all scenarios, the best and the worst scenario of the first, and the number of scenarios where the first is slower or faster by more than 10 %:

| compared | scenarios | geometric mean | slower | faster | best | worst |
| --- | ---: | ---: | ---: | ---: | --- | --- |
| lumex C++17 / std::optional C++17 | 46 | 1.000 | 2 | 2 | emplace_reset_pod32 0.83x | value_or_sum (1,024) 1.38x |
| lumex C++11 / std::optional C++17 | 46 | 1.015 | 2 | 3 | emplace_reset_int 0.83x | value_or_sum (1,024) 4.89x |
| lumex C++11 / lumex C++17 | 46 | 1.015 | 3 | 1 | eq_opt_opt (1,024) 0.64x | value_or_sum (1,024) 3.53x |
| lumex C++17 / boost::optional C++17 | 46 | 1.135 | 9 | 6 | vector_copy_int (1,024) 0.35x | return_int 7.80x |
| lumex C++11 / boost::optional C++11 | 46 | 1.144 | 7 | 8 | vector_copy_int (1,024) 0.27x | make_move_destroy_int 8.49x |

- lumex C++17 is slower than std::optional C++17 by more than 10 % in: value_or_sum (1,024) 1.38x, eq_opt_opt (1,024) 1.38x
- lumex C++17 is faster than std::optional C++17 by more than 10 % in: emplace_reset_pod32 0.83x, emplace_reset_int 0.83x
- lumex C++11 is slower than std::optional C++17 by more than 10 % in: value_or_sum (1,024) 4.89x, eq_value (1,024) 1.16x
- lumex C++11 is faster than std::optional C++17 by more than 10 % in: emplace_reset_int 0.83x, emplace_reset_pod32 0.84x, eq_opt_opt (1,024) 0.89x

## Layout and traits of optional<T>

`sizeof` and `alignof` of the optional and the properties of the optional type (not of `T`). TC: trivially copyable, TD: trivially destructible, TCC: trivially copy constructible, SL: standard layout, NM: nothrow move constructible. Sizes in bytes; the values do not depend on the toolchain of this run (one 64-bit Linux x86-64 data model).

| T | sizeof (T) | std::optional C++17: size, TC/TD/TCC/SL/NM | lumex C++11: size, TC/TD/TCC/SL/NM | lumex C++17: size, TC/TD/TCC/SL/NM | boost::optional C++11: size, TC/TD/TCC/SL/NM | boost::optional C++17: size, TC/TD/TCC/SL/NM |
| --- | ---: | --- | --- | --- | --- | --- |
| `bool` | 1 | 2, Y/Y/Y/Y/Y | 2, Y/Y/Y/Y/Y | 2, Y/Y/Y/Y/Y | 2, n/Y/n/Y/Y | 2, n/Y/n/Y/Y |
| `char` | 1 | 2, Y/Y/Y/Y/Y | 2, Y/Y/Y/Y/Y | 2, Y/Y/Y/Y/Y | 2, n/Y/n/Y/Y | 2, n/Y/n/Y/Y |
| `int` | 4 | 8, Y/Y/Y/Y/Y | 8, Y/Y/Y/Y/Y | 8, Y/Y/Y/Y/Y | 8, n/Y/n/Y/Y | 8, n/Y/n/Y/Y |
| `double` | 8 | 16, Y/Y/Y/Y/Y | 16, Y/Y/Y/Y/Y | 16, Y/Y/Y/Y/Y | 16, n/Y/n/Y/Y | 16, n/Y/n/Y/Y |
| `uint64_t` | 8 | 16, Y/Y/Y/Y/Y | 16, Y/Y/Y/Y/Y | 16, Y/Y/Y/Y/Y | 16, n/Y/n/Y/Y | 16, n/Y/n/Y/Y |
| `pod32` | 32 | 40, Y/Y/Y/Y/Y | 40, Y/Y/Y/Y/Y | 40, Y/Y/Y/Y/Y | 40, n/Y/n/Y/Y | 40, n/Y/n/Y/Y |
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
| gcc | 123 | 161 | 166 | 135 | 208 | 145 | 212 |

Unit `use`: the whole API is used: construction, observers, modifiers, comparisons, `swap`, `std::vector` of optionals, `std::hash`.

| toolchain | no optional C++11 | no optional C++17 | std::optional C++17 | lumex C++11 | lumex C++17 | boost::optional C++11 | boost::optional C++17 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| gcc | 122 | 160 | 201 | 170 | 242 | 166 | 234 |

