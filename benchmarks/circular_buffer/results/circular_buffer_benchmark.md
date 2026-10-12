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
- variant clang lumex_cxx11: compiler="Clang 23.1.0" library="libc++" cplusplus=201103
- variant clang lumex_cxx17: compiler="Clang 23.1.0" library="libc++" cplusplus=201703
- variant clang boost_cxx11: compiler="Clang 23.1.0" library="libc++" cplusplus=201103
- variant clang boost_cxx17: compiler="Clang 23.1.0" library="libc++" cplusplus=201703
- variant clang deque_cxx17: compiler="Clang 23.1.0" library="libc++" cplusplus=201703
- variant clang vector_cxx17: compiler="Clang 23.1.0" library="libc++" cplusplus=201703
- variant clang vector_mod_cxx17: compiler="Clang 23.1.0" library="libc++" cplusplus=201703

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

## clang

| scenario | size | boost C++17 | boost C++11 | lumex C++11 | lumex C++17 | deque C++17 | vector C++17 | vector % C++17 |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| push_overwrite_int | 1,024 | 0.92 | 0.92 (1.00) | 0.74 (0.81)† | 0.75 (0.82) | 2.00 (2.18) | 1.01 (1.10) | 5.18 (5.64) |
| push_overwrite_str_short | 1,024 | 1.74 | 1.75 (1.00) | 2.50 (1.44) | 2.49 (1.43) | 3.41 (1.96) | 2.02 (1.16) | 5.20 (2.98) |
| push_overwrite_str_long | 1,024 | 6.14 | 6.06 (0.99) | 11.71 (1.91) | 11.80 (1.92) | 12.93 (2.10) | 6.11 (1.00) | 6.45 (1.05) |
| push_pop_steady_int | 1,024 | 1.21 | 1.18 (0.98) | 1.17 (0.97) | 1.19 (0.98) | 8.58 (7.12) | 1.26 (1.05)† | 5.18 (4.30) |
| push_pop_steady_str_short | 1,024 | 2.23 | 2.24 (1.00) | 3.18 (1.43) | 3.26 (1.46) | 3.91 (1.76) | 2.81 (1.26) | 5.24 (2.35) |
| push_pop_steady_str_long | 1,024 | 12.08 | 12.21 (1.01) | 11.80 (0.98) | 11.82 (0.98) | 13.29 (1.10) | 7.03 (0.58) | 7.36 (0.61) |
| fill_int | 1,024 | 573.4 | 565.8 (0.99) | 652.5 (1.14) | 656.0 (1.14) | 1882 (3.28) | 639.0 (1.11) | 1446 (2.52) |
| fill_str_short | 256 | 447.9 | 445.6 (0.99) | 622.6 (1.39) | 626.6 (1.40) | 785.4 (1.75) | 443.5 (0.99) | 466.4 (1.04)† |
| fill_str_long | 256 | 4137 | 4123 (1.00) | 3529 (0.85) | 3471 (0.84) | 3715 (0.90) | 1560 (0.38) | 1587 (0.38) |
| fill_drain_int | 1,024 | 1044 | 1047 (1.00) | 940.2 (0.90) | 947.8 (0.91) | 2389 (2.29) | 1023 (0.98) | 3750 (3.59) |
| fill_drain_str_short | 256 | 625.4 | 622.7 (1.00) | 774.8 (1.24) | 768.1 (1.23) | 1026 (1.64) | 792.7 (1.27) | 1505 (2.41) |
| front_back_int | 1,024 | 0.50 | 0.49 (0.98) | 0.66 (1.32) | 0.65 (1.29) | 0.84 (1.66) | 0.65 (1.28) | 1.44 (2.86) |
| front_back_str_short | 1,024 | 0.93 | 0.91 (0.98) | 1.86 (2.00) | 1.86 (2.00) | 2.46 (2.65) | 1.89 (2.03) | 2.33 (2.51) |
| index_sum_int | 1,024 | 309.5 | 306.1 (0.99) | 325.9 (1.05) | 327.9 (1.06) | 297.4 (0.96) | 330.4 (1.07) | 1086 (3.51) |
| index_sum_str_short | 1,024 | 726.3 | 738.3 (1.02) | 657.5 (0.91) | 658.0 (0.91) | 870.7 (1.20) | 659.9 (0.91) | 1194 (1.64) |
| iterate_sum_int | 1,024 | 497.9 | 498.0 (1.00) | 326.4 (0.66) | 327.9 (0.66) | 265.9 (0.53) | 325.5 (0.65) | 1086 (2.18) |
| iterate_sum_str_short | 1,024 | 675.7 | 673.5 (1.00) | 669.4 (0.99) | 665.5 (0.98) | 499.4 (0.74) | 660.0 (0.98) | 1196 (1.77) |
| producer_consumer_int | 1,024 | 16596 | 16695 (1.01) | 17204 (1.04) | 17242 (1.04) | 24654 (1.49) | 17217 (1.04) | 20364 (1.23) |
| producer_consumer_str_short | 1,024 | 26088 | 26346 (1.01) | 26249 (1.01) | 26176 (1.00) | 31109 (1.19) | 28228 (1.08) | 30920 (1.19) |
| ctor_dtor_int | 1,024 | 26.51 | 26.61 (1.00) | 25.65 (0.97) | 26.22 (0.99)† | 1.24 (0.05) | 47.13 (1.78) | 46.20 (1.74) |
| ctor_dtor_str_short | 1,024 | 17.65 | 17.41 (0.99) | 16.71 (0.95) | 16.65 (0.94) | 2.01 (0.11) | 375.2 (21.26) | 377.1 (21.36) |
| copy_ctor_int | 1,024 | 326.4 | 324.2 (0.99) | 63.88 (0.20) | 63.10 (0.19) | 310.9 (0.95) | 64.51 (0.20) | 71.76 (0.22) |
| copy_ctor_str_short | 256 | 234.2 | 235.2 (1.00) | 246.9 (1.05) | 247.7 (1.06) | 329.6 (1.41) | 244.9 (1.05) | 245.8 (1.05) |
| copy_ctor_str_long | 128 | 2111 | 2096 (0.99) | 2114 (1.00) | 2082 (0.99) | 2190 (1.04) | 2809 (1.33) | 2840 (1.35) |
| copy_assign_int | 1,024 | 327.4 | 328.6 (1.00) | 58.85 (0.18) | 59.29 (0.18)† | 25.51 (0.08) | 26.44 (0.08)† | 26.41 (0.08) |
| copy_assign_str_short | 256 | 248.1 | 246.0 (0.99) | 246.0 (0.99) | 244.2 (0.98) | 215.9 (0.87) | 248.1 (1.00) | 246.0 (0.99) |
| copy_assign_str_long | 128 | 2207 | 2223 (1.01) | 2228 (1.01) | 2214 (1.00) | 347.0 (0.16) | 429.7 (0.19) | 442.1 (0.20) |
| move_pair_int | 1,024 | 9.57 | 9.51 (0.99) | 3.31 (0.35) | 3.14 (0.33) | 8.47 (0.88) | 3.21 (0.34)† | 3.01 (0.31) |
| move_pair_str_short | 256 | 3.47 | 3.48 (1.00) | 3.31 (0.96) | 3.05 (0.88) | 9.18 (2.65) | 3.11 (0.90) | 3.18 (0.92) |

Variants compared with each other (time of the first divided by the time of the second; the first is faster below 1.00): geometric mean over all scenarios, the best and the worst scenario of the first, and the number of scenarios where the first is slower or faster by more than 10 %:

| compared | scenarios | geometric mean | slower | faster | best | worst |
| --- | ---: | ---: | ---: | ---: | --- | --- |
| lumex C++17 / boost C++17 | 29 | 0.915 | 8 | 7 | copy_assign_int (1,024) 0.18x | front_back_str_short (1,024) 2.00x |
| lumex C++17 / vector C++17 | 29 | 1.019 | 8 | 4 | ctor_dtor_str_short (1,024) 0.04x | copy_assign_str_long (128) 5.15x |
| lumex C++17 / deque C++17 | 29 | 0.901 | 8 | 18 | push_pop_steady_int (1,024) 0.14x | ctor_dtor_int (1,024) 21.07x |
| lumex C++17 / vector % C++17 | 29 | 0.663 | 6 | 19 | ctor_dtor_str_short (1,024) 0.04x | copy_assign_str_long (128) 5.01x |
| vector % C++17 / vector C++17 | 29 | 1.538 | 15 | 0 | move_pair_int (1,024) 0.94x | push_overwrite_int (1,024) 5.14x |
| lumex C++11 / lumex C++17 | 29 | 1.004 | 0 | 0 | push_pop_steady_str_short (1,024) 0.98x | move_pair_str_short (256) 1.09x |
| lumex C++11 / boost C++11 | 29 | 0.922 | 8 | 8 | copy_assign_int (1,024) 0.18x | front_back_str_short (1,024) 2.05x |

- lumex C++17 is slower than boost C++17 by more than 10 % in: front_back_str_short (1,024) 2.00x, push_overwrite_str_long (1,024) 1.92x, push_pop_steady_str_short (1,024) 1.46x, push_overwrite_str_short (1,024) 1.43x, fill_str_short (256) 1.40x, front_back_int (1,024) 1.29x, fill_drain_str_short (256) 1.23x, fill_int (1,024) 1.14x
- lumex C++17 is faster than boost C++17 by more than 10 % in: copy_assign_int (1,024) 0.18x, copy_ctor_int (1,024) 0.19x, move_pair_int (1,024) 0.33x, iterate_sum_int (1,024) 0.66x, push_overwrite_int (1,024) 0.82x, fill_str_long (256) 0.84x, move_pair_str_short (256) 0.88x
- lumex C++17 is slower than vector C++17 by more than 10 % in: copy_assign_str_long (128) 5.15x, copy_assign_int (1,024) 2.24x, fill_str_long (256) 2.22x, push_overwrite_str_long (1,024) 1.93x, push_pop_steady_str_long (1,024) 1.68x, fill_str_short (256) 1.41x, push_overwrite_str_short (1,024) 1.23x, push_pop_steady_str_short (1,024) 1.16x
- lumex C++17 is faster than vector C++17 by more than 10 % in: ctor_dtor_str_short (1,024) 0.04x, ctor_dtor_int (1,024) 0.56x, copy_ctor_str_long (128) 0.74x, push_overwrite_int (1,024) 0.74x

## Footprint

A buffer of capacity 1 024 elements of `T`. `sizeof` of the container object; heap memory requested from `operator new` (bytes / calls) after construction (empty), after 10 `push_back` and after 1 024 `push_back`; NM: nothrow move constructible and nothrow move assignable. `std::string` is the empty string, so no heap memory is counted for the characters. Sizes in bytes; one set of tables per toolchain (the values of the toolchains differ).

### Toolchain gcc

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

### Toolchain clang

`T` = `char` (`sizeof (T)` 1)

| variant | sizeof (container) | empty | 10 elements | 1 024 elements | NM (ctor/assign) |
| --- | ---: | ---: | ---: | ---: | --- |
| boost C++17 | 40 | 1024 / 1 | 1024 / 1 | 1024 / 1 | Y/Y |
| boost C++11 | 40 | 1024 / 1 | 1024 / 1 | 1024 / 1 | Y/Y |
| lumex C++11 | 40 | 1024 / 1 | 1024 / 1 | 1024 / 1 | Y/Y |
| lumex C++17 | 40 | 1024 / 1 | 1024 / 1 | 1024 / 1 | Y/Y |
| deque C++17 | 56 | 0 / 0 | 4104 / 2 | 4104 / 2 | Y/Y |
| vector C++17 | 48 | 1024 / 1 | 1024 / 1 | 1024 / 1 | Y/Y |
| vector % C++17 | 48 | 1024 / 1 | 1024 / 1 | 1024 / 1 | Y/Y |

`T` = `int` (`sizeof (T)` 4)

| variant | sizeof (container) | empty | 10 elements | 1 024 elements | NM (ctor/assign) |
| --- | ---: | ---: | ---: | ---: | --- |
| boost C++17 | 40 | 4096 / 1 | 4096 / 1 | 4096 / 1 | Y/Y |
| boost C++11 | 40 | 4096 / 1 | 4096 / 1 | 4096 / 1 | Y/Y |
| lumex C++11 | 40 | 4096 / 1 | 4096 / 1 | 4096 / 1 | Y/Y |
| lumex C++17 | 40 | 4096 / 1 | 4096 / 1 | 4096 / 1 | Y/Y |
| deque C++17 | 56 | 0 / 0 | 4104 / 2 | 8216 / 4 | Y/Y |
| vector C++17 | 48 | 4096 / 1 | 4096 / 1 | 4096 / 1 | Y/Y |
| vector % C++17 | 48 | 4096 / 1 | 4096 / 1 | 4096 / 1 | Y/Y |

`T` = `double` (`sizeof (T)` 8)

| variant | sizeof (container) | empty | 10 elements | 1 024 elements | NM (ctor/assign) |
| --- | ---: | ---: | ---: | ---: | --- |
| boost C++17 | 40 | 8192 / 1 | 8192 / 1 | 8192 / 1 | Y/Y |
| boost C++11 | 40 | 8192 / 1 | 8192 / 1 | 8192 / 1 | Y/Y |
| lumex C++11 | 40 | 8192 / 1 | 8192 / 1 | 8192 / 1 | Y/Y |
| lumex C++17 | 40 | 8192 / 1 | 8192 / 1 | 8192 / 1 | Y/Y |
| deque C++17 | 56 | 0 / 0 | 4104 / 2 | 12344 / 6 | Y/Y |
| vector C++17 | 48 | 8192 / 1 | 8192 / 1 | 8192 / 1 | Y/Y |
| vector % C++17 | 48 | 8192 / 1 | 8192 / 1 | 8192 / 1 | Y/Y |

`T` = `pod32` (`sizeof (T)` 32)

| variant | sizeof (container) | empty | 10 elements | 1 024 elements | NM (ctor/assign) |
| --- | ---: | ---: | ---: | ---: | --- |
| boost C++17 | 40 | 32768 / 1 | 32768 / 1 | 32768 / 1 | Y/Y |
| boost C++11 | 40 | 32768 / 1 | 32768 / 1 | 32768 / 1 | Y/Y |
| lumex C++11 | 40 | 32768 / 1 | 32768 / 1 | 32768 / 1 | Y/Y |
| lumex C++17 | 40 | 32768 / 1 | 32768 / 1 | 32768 / 1 | Y/Y |
| deque C++17 | 56 | 0 / 0 | 4104 / 2 | 37112 / 14 | Y/Y |
| vector C++17 | 48 | 32768 / 1 | 32768 / 1 | 32768 / 1 | Y/Y |
| vector % C++17 | 48 | 32768 / 1 | 32768 / 1 | 32768 / 1 | Y/Y |

`T` = `std::string` (`sizeof (T)` 24)

| variant | sizeof (container) | empty | 10 elements | 1 024 elements | NM (ctor/assign) |
| --- | ---: | ---: | ---: | ---: | --- |
| boost C++17 | 40 | 24576 / 1 | 24576 / 1 | 24576 / 1 | Y/Y |
| boost C++11 | 40 | 24576 / 1 | 24576 / 1 | 24576 / 1 | Y/Y |
| lumex C++11 | 40 | 24576 / 1 | 24576 / 1 | 24576 / 1 | Y/Y |
| lumex C++17 | 40 | 24576 / 1 | 24576 / 1 | 24576 / 1 | Y/Y |
| deque C++17 | 56 | 0 / 0 | 4088 / 2 | 28680 / 11 | Y/Y |
| vector C++17 | 48 | 24576 / 1 | 24576 / 1 | 24576 / 1 | Y/Y |
| vector % C++17 | 48 | 24576 / 1 | 24576 / 1 | 24576 / 1 | Y/Y |

## Compile time

- compiler gcc: g++ (GCC) 13.2.0
- compiler clang: clang version 23.1.0

Wall time of `-fsyntax-only -O0` of `compile_time.cpp`, median of the repeats, in milliseconds. `no ring` is the same unit with a stub instead of a ring: the cost of the shared headers (`<deque>`, `<string>`, `<vector>`). The units of `deque` and `vector` include the ring classes of `bench_circular_buffer_impl.hpp`, which are part of what they cost.

Unit `include`: the header is included and one object is declared.

| toolchain | no ring C++11 | no ring C++17 | boost C++17 | boost C++11 | lumex C++11 | lumex C++17 | deque C++17 | vector C++17 | vector % C++17 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| gcc | 133 | 177 | 343 | 300 | 193 | 268 | 174 | 170 | 172 |
| clang | 182 | 218 | 384 | 347 | 248 | 328 | 219 | 217 | 220 |

Unit `use`: the members of the scenarios are used: construction, `size`, `capacity`, `push_back`, `pop_front`, `front`, `back`, `operator[]`, iteration, `clear`, copy and move, `std::vector` of rings.

| toolchain | no ring C++11 | no ring C++17 | boost C++17 | boost C++11 | lumex C++11 | lumex C++17 | deque C++17 | vector C++17 | vector % C++17 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| gcc | 134 | 170 | 362 | 326 | 213 | 289 | 200 | 193 | 194 |
| clang | 181 | 216 | 412 | 374 | 279 | 355 | 268 | 252 | 248 |

