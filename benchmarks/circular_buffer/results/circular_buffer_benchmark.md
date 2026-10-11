# circular_buffer benchmark results

- machine="12th Gen Intel(R) Core(TM) i7-12700K" os="Linux 6.1.170-1-generic" logical_cpus=20 python=3.7.3
- passes=5 cpu=3
- variant gcc lumex_cxx11: compiler="GCC 13.2.0" library="libstdc++" cplusplus=201103
- variant gcc lumex_cxx17: compiler="GCC 13.2.0" library="libstdc++" cplusplus=201703
- variant gcc boost_cxx11: compiler="GCC 13.2.0" library="libstdc++" cplusplus=201103
- variant gcc boost_cxx17: compiler="GCC 13.2.0" library="libstdc++" cplusplus=201703
- variant gcc deque_cxx17: compiler="GCC 13.2.0" library="libstdc++" cplusplus=201703
- variant gcc vector_cxx17: compiler="GCC 13.2.0" library="libstdc++" cplusplus=201703
- variant gcc vector_mod_cxx17: compiler="GCC 13.2.0" library="libstdc++" cplusplus=201703

Time per operation in nanoseconds: the lowest median of 15 repetitions over all passes (the best pass), and in parentheses the ratio to the baseline column (the first column; below 1.00 is faster than that). The unit of an operation is the description of the scenario. A dagger marks a value whose slowest pass was more than 25 % slower than the best one: the processes of that executable do not agree (code and stack placement), so differences inside that band are noise.

## gcc

| scenario | size | boost C++17 | boost C++11 | lumex C++11 | lumex C++17 | deque C++17 | vector C++17 | vector % C++17 |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| push_overwrite_int | 1,024 | 2.13 | 2.12 (1.00) | 0.76 (0.36) | 0.76 (0.36) | 1.62 (0.76) | 1.00 (0.47) | 5.63 (2.65) |
| push_overwrite_str_short | 1,024 | 3.43 | 3.47 (1.01) | 6.49 (1.89) | 6.44 (1.88) | 5.36 (1.56) | 3.74 (1.09) | 5.83 (1.70) |
| push_overwrite_str_long | 1,024 | 6.18 | 6.21 (1.00) | 13.18 (2.13) | 13.20 (2.13) | 15.63 (2.53) | 6.01 (0.97) | 6.45 (1.04) |
| push_pop_steady_int | 1,024 | 1.20 | 1.18 (0.99) | 1.25 (1.04) | 1.23 (1.03) | 1.86 (1.55) | 1.13 (0.94) | 5.56 (4.65) |
| push_pop_steady_str_short | 1,024 | 4.06 | 3.91 (0.96) | 4.01 (0.99) | 3.99 (0.98) | 5.62 (1.38) | 4.17 (1.03)† | 6.02 (1.48) |
| push_pop_steady_str_long | 1,024 | 13.85 | 13.77 (0.99) | 13.66 (0.99) | 13.66 (0.99) | 15.12 (1.09) | 6.79 (0.49) | 7.38 (0.53) |
| fill_int | 1,024 | 700.6 | 708.7 (1.01) | 772.2 (1.10) | 774.4 (1.11) | 1617 (2.31) | 711.0 (1.01)† | 2364 (3.37) |
| fill_str_short | 256 | 933.8 | 900.9 (0.96) | 939.2 (1.01) | 940.1 (1.01) | 1551 (1.66) | 894.3 (0.96)† | 940.9 (1.01) |
| fill_str_long | 256 | 3943 | 3928 (1.00) | 4004 (1.02) | 4000 (1.01) | 4746 (1.20) | 1610 (0.41) | 1660 (0.42) |
| fill_drain_int | 1,024 | 1211 | 1228 (1.01) | 1297 (1.07) | 1286 (1.06) | 2521 (2.08) | 1284 (1.06) | 6564 (5.42) |
| fill_drain_str_short | 256 | 1013 | 994.6 (0.98) | 1053 (1.04) | 1065 (1.05) | 1852 (1.83) | 1113 (1.10) | 2002 (1.98) |
| front_back_int | 1,024 | 0.45 | 0.45 (0.99) | 0.65 (1.42) | 0.65 (1.42) | 0.60 (1.32) | 0.65 (1.43) | 2.30 (5.07) |
| front_back_str_short | 1,024 | 0.84 | 0.84 (1.00) | 1.29 (1.54) | 1.30 (1.54) | 1.02 (1.21) | 1.27 (1.51) | 2.42 (2.88) |
| index_sum_int | 1,024 | 281.3 | 269.6 (0.96) | 275.1 (0.98) | 275.4 (0.98) | 540.9 (1.92) | 259.6 (0.92) | 1729 (6.15) |
| index_sum_str_short | 1,024 | 587.2 | 583.3 (0.99) | 573.8 (0.98) | 579.9 (0.99) | 833.6 (1.42) | 591.6 (1.01) | 1782 (3.04) |
| iterate_sum_int | 1,024 | 206.6 | 206.2 (1.00) | 259.2 (1.25) | 265.9 (1.29) | 260.7 (1.26) | 263.4 (1.27) | 1765 (8.54) |
| iterate_sum_str_short | 1,024 | 394.3 | 395.8 (1.00) | 581.7 (1.48) | 585.0 (1.48) | 406.9 (1.03) | 575.0 (1.46) | 1783 (4.52) |
| producer_consumer_int | 1,024 | 18426 | 18087 (0.98) | 19185 (1.04) | 19225 (1.04) | 25086 (1.36) | 19371 (1.05) | 30346 (1.65) |
| producer_consumer_str_short | 1,024 | 38474 | 35953 (0.93) | 39383 (1.02) | 39400 (1.02) | 47451 (1.23) | 38711 (1.01) | 40845 (1.06) |
| ctor_dtor_int | 1,024 | 23.61 | 23.70 (1.00) | 22.89 (0.97) | 22.92 (0.97) | 17.43 (0.74) | 43.70 (1.85) | 43.73 (1.85) |
| ctor_dtor_str_short | 1,024 | 21.36 | 21.24 (0.99) | 21.91 (1.03) | 21.48 (1.01) | 19.51 (0.91) | 684.3 (32.03) | 677.3 (31.71) |
| copy_ctor_int | 1,024 | 226.8 | 232.9 (1.03) | 178.5 (0.79) | 178.6 (0.79) | 155.6 (0.69) | 56.13 (0.25) | 57.14 (0.25) |
| copy_ctor_str_short | 256 | 468.7 | 465.0 (0.99) | 472.3 (1.01) | 471.1 (1.01) | 927.8 (1.98) | 544.0 (1.16) | 505.2 (1.08) |
| copy_ctor_str_long | 128 | 2401 | 2395 (1.00) | 2170 (0.90) | 2178 (0.91) | 1713 (0.71) | 2842 (1.18) | 2871 (1.20) |
| copy_assign_int | 1,024 | 237.0 | 238.0 (1.00) | 185.5 (0.78) | 186.1 (0.79) | 92.74 (0.39) | 34.29 (0.14) | 34.71 (0.15) |
| copy_assign_str_short | 256 | 452.3 | 468.9 (1.04) | 476.6 (1.05) | 475.5 (1.05) | 711.2 (1.57) | 651.6 (1.44) | 652.7 (1.44) |
| copy_assign_str_long | 128 | 2309 | 2314 (1.00) | 2193 (0.95) | 2165 (0.94) | 412.3 (0.18) | 407.0 (0.18) | 406.3 (0.18) |
| move_pair_int | 1,024 | 3.32 | 3.34 (1.00) | 3.34 (1.00) | 3.30 (0.99) | 25.14 (7.57) | 3.36 (1.01) | 3.35 (1.01) |
| move_pair_str_short | 256 | 3.54 | 3.56 (1.01) | 3.02 (0.85) | 2.97 (0.84) | 26.33 (7.44) | 3.00 (0.85)† | 2.98 (0.84) |

Variants compared with each other (time of the first divided by the time of the second; the first is faster below 1.00): geometric mean over all scenarios, the best and the worst scenario of the first, and the number of scenarios where the first is slower or faster by more than 10 %:

| compared | scenarios | geometric mean | slower | faster | best | worst |
| --- | ---: | ---: | ---: | ---: | --- | --- |
| lumex C++17 / boost C++17 | 29 | 1.044 | 7 | 4 | push_overwrite_int (1,024) 0.36x | push_overwrite_str_long (1,024) 2.13x |
| lumex C++17 / vector C++17 | 29 | 1.092 | 7 | 6 | ctor_dtor_str_short (1,024) 0.03x | copy_assign_int (1,024) 5.43x |
| lumex C++17 / deque C++17 | 29 | 0.779 | 9 | 17 | move_pair_str_short (256) 0.11x | copy_assign_str_long (128) 5.25x |
| lumex C++17 / vector % C++17 | 29 | 0.625 | 7 | 17 | ctor_dtor_str_short (1,024) 0.03x | copy_assign_int (1,024) 5.36x |
| vector % C++17 / vector C++17 | 29 | 1.747 | 14 | 0 | copy_ctor_str_short (256) 0.93x | iterate_sum_int (1,024) 6.70x |
| lumex C++11 / lumex C++17 | 29 | 1.001 | 0 | 0 | iterate_sum_int (1,024) 0.97x | ctor_dtor_str_short (1,024) 1.02x |
| lumex C++11 / boost C++11 | 29 | 1.051 | 6 | 4 | push_overwrite_int (1,024) 0.36x | push_overwrite_str_long (1,024) 2.12x |

- lumex C++17 is slower than boost C++17 by more than 10 % in: push_overwrite_str_long (1,024) 2.13x, push_overwrite_str_short (1,024) 1.88x, front_back_str_short (1,024) 1.54x, iterate_sum_str_short (1,024) 1.48x, front_back_int (1,024) 1.42x, iterate_sum_int (1,024) 1.29x, fill_int (1,024) 1.11x
- lumex C++17 is faster than boost C++17 by more than 10 % in: push_overwrite_int (1,024) 0.36x, copy_assign_int (1,024) 0.79x, copy_ctor_int (1,024) 0.79x, move_pair_str_short (256) 0.84x
- lumex C++17 is slower than vector C++17 by more than 10 % in: copy_assign_int (1,024) 5.43x, copy_assign_str_long (128) 5.32x, copy_ctor_int (1,024) 3.18x, fill_str_long (256) 2.48x, push_overwrite_str_long (1,024) 2.19x, push_pop_steady_str_long (1,024) 2.01x, push_overwrite_str_short (1,024) 1.72x
- lumex C++17 is faster than vector C++17 by more than 10 % in: ctor_dtor_str_short (1,024) 0.03x, ctor_dtor_int (1,024) 0.52x, copy_assign_str_short (256) 0.73x, push_overwrite_int (1,024) 0.75x, copy_ctor_str_long (128) 0.77x, copy_ctor_str_short (256) 0.87x

## Footprint

A buffer of capacity 1 024 elements of `T`. `sizeof` of the container object; heap memory requested from `operator new` (bytes / calls) after construction (empty), after 10 `push_back` and after 1 024 `push_back`; NM: nothrow move constructible and nothrow move assignable. `std::string` is the empty string, so no heap memory is counted for the characters. Sizes in bytes; the values do not depend on the toolchain of this run (one 64-bit Linux x86-64 data model).

`T` = `char` (`sizeof (T)` 1)

| variant | sizeof (container) | empty | 10 elements | 1 024 elements | NM (ctor/assign) |
| --- | ---: | ---: | ---: | ---: | --- |
| boost C++17 | 40 | 1024 / 1 | 1024 / 1 | 1024 / 1 | Y/Y |
| boost C++11 | 40 | 1024 / 1 | 1024 / 1 | 1024 / 1 | Y/Y |
| lumex C++11 | 40 | 1024 / 1 | 1024 / 1 | 1024 / 1 | Y/Y |
| lumex C++17 | 40 | 1024 / 1 | 1024 / 1 | 1024 / 1 | Y/Y |
| deque C++17 | 88 | 576 / 2 | 576 / 2 | 1600 / 4 | n/Y |
| vector C++17 | 48 | 1024 / 1 | 1024 / 1 | 1024 / 1 | Y/Y |
| vector % C++17 | 48 | 1024 / 1 | 1024 / 1 | 1024 / 1 | Y/Y |

`T` = `int` (`sizeof (T)` 4)

| variant | sizeof (container) | empty | 10 elements | 1 024 elements | NM (ctor/assign) |
| --- | ---: | ---: | ---: | ---: | --- |
| boost C++17 | 40 | 4096 / 1 | 4096 / 1 | 4096 / 1 | Y/Y |
| boost C++11 | 40 | 4096 / 1 | 4096 / 1 | 4096 / 1 | Y/Y |
| lumex C++11 | 40 | 4096 / 1 | 4096 / 1 | 4096 / 1 | Y/Y |
| lumex C++17 | 40 | 4096 / 1 | 4096 / 1 | 4096 / 1 | Y/Y |
| deque C++17 | 88 | 576 / 2 | 576 / 2 | 4816 / 11 | n/Y |
| vector C++17 | 48 | 4096 / 1 | 4096 / 1 | 4096 / 1 | Y/Y |
| vector % C++17 | 48 | 4096 / 1 | 4096 / 1 | 4096 / 1 | Y/Y |

`T` = `double` (`sizeof (T)` 8)

| variant | sizeof (container) | empty | 10 elements | 1 024 elements | NM (ctor/assign) |
| --- | ---: | ---: | ---: | ---: | --- |
| boost C++17 | 40 | 8192 / 1 | 8192 / 1 | 8192 / 1 | Y/Y |
| boost C++11 | 40 | 8192 / 1 | 8192 / 1 | 8192 / 1 | Y/Y |
| lumex C++11 | 40 | 8192 / 1 | 8192 / 1 | 8192 / 1 | Y/Y |
| lumex C++17 | 40 | 8192 / 1 | 8192 / 1 | 8192 / 1 | Y/Y |
| deque C++17 | 88 | 576 / 2 | 576 / 2 | 9216 / 20 | n/Y |
| vector C++17 | 48 | 8192 / 1 | 8192 / 1 | 8192 / 1 | Y/Y |
| vector % C++17 | 48 | 8192 / 1 | 8192 / 1 | 8192 / 1 | Y/Y |

`T` = `pod32` (`sizeof (T)` 32)

| variant | sizeof (container) | empty | 10 elements | 1 024 elements | NM (ctor/assign) |
| --- | ---: | ---: | ---: | ---: | --- |
| boost C++17 | 40 | 32768 / 1 | 32768 / 1 | 32768 / 1 | Y/Y |
| boost C++11 | 40 | 32768 / 1 | 32768 / 1 | 32768 / 1 | Y/Y |
| lumex C++11 | 40 | 32768 / 1 | 32768 / 1 | 32768 / 1 | Y/Y |
| lumex C++17 | 40 | 32768 / 1 | 32768 / 1 | 32768 / 1 | Y/Y |
| deque C++17 | 88 | 576 / 2 | 576 / 2 | 35680 / 70 | n/Y |
| vector C++17 | 48 | 32768 / 1 | 32768 / 1 | 32768 / 1 | Y/Y |
| vector % C++17 | 48 | 32768 / 1 | 32768 / 1 | 32768 / 1 | Y/Y |

`T` = `std::string` (`sizeof (T)` 32)

| variant | sizeof (container) | empty | 10 elements | 1 024 elements | NM (ctor/assign) |
| --- | ---: | ---: | ---: | ---: | --- |
| boost C++17 | 40 | 32768 / 1 | 32768 / 1 | 32768 / 1 | Y/Y |
| boost C++11 | 40 | 32768 / 1 | 32768 / 1 | 32768 / 1 | Y/Y |
| lumex C++11 | 40 | 32768 / 1 | 32768 / 1 | 32768 / 1 | Y/Y |
| lumex C++17 | 40 | 32768 / 1 | 32768 / 1 | 32768 / 1 | Y/Y |
| deque C++17 | 88 | 576 / 2 | 576 / 2 | 35680 / 70 | n/Y |
| vector C++17 | 48 | 32768 / 1 | 32768 / 1 | 32768 / 1 | Y/Y |
| vector % C++17 | 48 | 32768 / 1 | 32768 / 1 | 32768 / 1 | Y/Y |

