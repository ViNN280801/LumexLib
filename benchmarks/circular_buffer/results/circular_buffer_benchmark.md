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
- variant gcc lumex_wrap_cxx17: compiler="GCC 13.2.0" library="libstdc++" cplusplus=201703   (measured in a separate 5-pass run after the others)

Time per operation in nanoseconds: the lowest median of 15 repetitions over all passes (the best pass), and in parentheses the ratio to the baseline column (the first column; below 1.00 is faster than that). The unit of an operation is the description of the scenario. A dagger marks a value whose slowest pass was more than 25 % slower than the best one: the processes of that executable do not agree (code and stack placement), so differences inside that band are noise.

## gcc

| scenario | size | boost C++17 | boost C++11 | lumex C++11 | lumex C++17 | deque C++17 | vector C++17 | vector % C++17 | lumex compare-wrap C++17 |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| push_overwrite_int | 1,024 | 2.16 | 2.16 (1.00) | 5.67 (2.63) | 5.67 (2.63) | 1.61 (0.74) | 1.00 (0.46) | 5.75 (2.66) | 1.17 (0.54) |
| push_overwrite_str_short | 1,024 | 3.42 | 3.46 (1.01) | 8.99 (2.63) | 8.98 (2.62) | 5.36 (1.57) | 3.69 (1.08) | 5.86 (1.71) | 3.92 (1.15) |
| push_overwrite_str_long | 1,024 | 6.21 | 6.21 (1.00) | 13.52 (2.18) | 13.34 (2.15) | 15.17 (2.44) | 5.95 (0.96) | 6.38 (1.03) | 13.56 (2.18) |
| push_pop_steady_int | 1,024 | 1.19 | 1.18 (0.99) | 8.95 (7.51) | 8.96 (7.51) | 1.86 (1.56) | 1.11 (0.93) | 5.66 (4.74) | 1.38 (1.15) |
| push_pop_steady_str_short | 1,024 | 3.87 | 3.88 (1.00) | 9.04 (2.33) | 9.02 (2.33) | 5.51 (1.42) | 4.15 (1.07) | 6.09 (1.57) | 4.48 (1.16) |
| push_pop_steady_str_long | 1,024 | 13.63 | 13.29 (0.98) | 18.38 (1.35) | 13.96 (1.02) | 15.03 (1.10) | 6.80 (0.50) | 7.28 (0.53) | 13.57 (1.00) |
| fill_int | 1,024 | 690.5 | 702.7 (1.02) | 2418 (3.50) | 2415 (3.50) | 1598 (2.31) | 699.9 (1.01) | 2415 (3.50) | 626.2 (0.91) |
| fill_str_short | 256 | 942.2 | 882.4 (0.94) | 1471 (1.56) | 1467 (1.56) | 1515 (1.61) | 894.3 (0.95) | 935.3 (0.99) | 995.4 (1.06) |
| fill_str_long | 256 | 3842 | 3824 (1.00) | 4067 (1.06) | 4125 (1.07) | 4699 (1.22) | 1608 (0.42) | 1627 (0.42) | 3990 (1.04) |
| fill_drain_int | 1,024 | 1212 | 1221 (1.01) | 9137 (7.54) | 9142 (7.54) | 2561 (2.11) | 1286 (1.06) | 6719 (5.55) | 1389 (1.15) |
| fill_drain_str_short | 256 | 1005 | 996.8 (0.99) | 2556 (2.54) | 2580 (2.57) | 1845 (1.84) | 1121 (1.12) | 2048 (2.04) | 1146 (1.14) |
| front_back_int | 1,024 | 0.45 | 0.45 (1.00) | 4.71 (10.41) | 4.71 (10.41) | 0.61 (1.34) | 0.64 (1.42) | 2.36 (5.20) | 0.82 (1.82) |
| front_back_str_short | 1,024 | 0.84 | 0.84 (1.00) | 4.72 (5.63) | 4.72 (5.63) | 1.01 (1.21) | 1.29 (1.54) | 2.46 (2.93) | 1.43 (1.70) |
| index_sum_int | 1,024 | 280.0 | 263.9 (0.94) | 1809 (6.46) | 1809 (6.46) | 547.8 (1.96) | 260.8 (0.93) | 1809 (6.46) | 259.8 (0.93) |
| index_sum_str_short | 1,024 | 586.0 | 584.5 (1.00) | 1826 (3.12) | 1828 (3.12) | 833.8 (1.42) | 582.4 (0.99) | 1827 (3.12) | 567.3 (0.97) |
| iterate_sum_int | 1,024 | 208.1 | 207.1 (1.00) | 1809 (8.69) | 1807 (8.68) | 263.1 (1.26) | 262.6 (1.26) | 1809 (8.69) | 261.9 (1.26) |
| iterate_sum_str_short | 1,024 | 394.5 | 396.0 (1.00) | 1822 (4.62) | 1821 (4.62) | 402.3 (1.02) | 586.6 (1.49) | 1824 (4.62) | 581.9 (1.48) |
| producer_consumer_int | 1,024 | 18335 | 18031 (0.98) | 37962 (2.07) | 37972 (2.07) | 25271 (1.38) | 19385 (1.06) | 30922 (1.69) | 18328 (1.00) |
| producer_consumer_str_short | 1,024 | 38015 | 35912 (0.94) | 47936 (1.26) | 48673 (1.28) | 46945 (1.23) | 38502 (1.01) | 40582 (1.07) | 37518 (0.99) |
| ctor_dtor_int | 1,024 | 23.36 | 23.11 (0.99) | 19.88 (0.85) | 19.48 (0.83) | 16.92 (0.72) | 43.32 (1.85) | 43.92 (1.88) | 20.19 (0.86) |
| ctor_dtor_str_short | 1,024 | 20.89 | 20.88 (1.00) | 20.38 (0.98) | 20.39 (0.98) | 19.10 (0.91) | 683.8 (32.73) | 688.8 (32.97) | 20.75 (0.99) |
| copy_ctor_int | 1,024 | 229.6 | 230.8 (1.01) | 3642 (15.87) | 3638 (15.85) | 150.4 (0.66) | 49.63 (0.22) | 50.00 (0.22) | 558.3 (2.43) |
| copy_ctor_str_short | 256 | 461.9 | 465.8 (1.01) | 1431 (3.10) | 1416 (3.07) | 917.7 (1.99) | 550.7 (1.19) | 508.7 (1.10) | 751.4 (1.63) |
| copy_ctor_str_long | 128 | 2399 | 2376 (0.99) | 2300 (0.96) | 2325 (0.97) | 1693 (0.71) | 2779 (1.16) | 2840 (1.18) | 2258 (0.94) |
| copy_assign_int | 1,024 | 237.3 | 238.9 (1.01) | 3638 (15.33) | 3636 (15.32) | 93.58 (0.39) | 28.26 (0.12) | 28.45 (0.12) | 560.0 (2.36) |
| copy_assign_str_short | 256 | 447.7 | 464.9 (1.04) | 1408 (3.15) | 1403 (3.13) | 702.4 (1.57) | 650.4 (1.45) | 649.8 (1.45) | 684.1 (1.53) |
| copy_assign_str_long | 128 | 2305 | 2266 (0.98) | 2362 (1.03) | 2364 (1.03) | 404.1 (0.18) | 405.1 (0.18) | 406.2 (0.18) | 2346 (1.02) |
| move_pair_int | 1,024 | 3.32 | 3.33 (1.00) | 3.41 (1.03) | 3.38 (1.02) | 24.88 (7.50) | 3.46 (1.04) | 3.49 (1.05) | 3.25 (0.98) |
| move_pair_str_short | 256 | 3.62 | 3.58 (0.99) | 3.40 (0.94) | 3.43 (0.95) | 25.41 (7.02) | 2.98 (0.82) | 3.01 (0.83) | 3.36 (0.93) |

Variants compared with each other (time of the first divided by the time of the second; the first is faster below 1.00): geometric mean over all scenarios, the best and the worst scenario of the first, and the number of scenarios where the first is slower or faster by more than 10 %:

| compared | scenarios | geometric mean | slower | faster | best | worst |
| --- | ---: | ---: | ---: | ---: | --- | --- |
| lumex C++17 / boost C++17 | 29 | 2.771 | 21 | 1 | ctor_dtor_int (1,024) 0.83x | copy_ctor_int (1,024) 15.85x |
| lumex C++17 / vector C++17 | 29 | 2.921 | 25 | 3 | ctor_dtor_str_short (1,024) 0.03x | copy_assign_int (1,024) 128.69x |
| lumex C++17 / deque C++17 | 29 | 2.078 | 21 | 4 | move_pair_str_short (256) 0.13x | copy_assign_int (1,024) 38.86x |
| lumex compare-wrap C++17 / boost C++17 | 29 | 1.182 | 14 | 2 | push_overwrite_int (1,024) 0.54x | copy_ctor_int (1,024) 2.43x |
| lumex compare-wrap C++17 / lumex C++17 | 29 | 0.426 | 0 | 20 | index_sum_int (1,024) 0.14x | ctor_dtor_int (1,024) 1.04x |
| lumex C++17 / vector % C++17 | 29 | 1.652 | 19 | 3 | ctor_dtor_str_short (1,024) 0.03x | copy_assign_int (1,024) 127.79x |
| vector % C++17 / vector C++17 | 29 | 1.768 | 14 | 0 | copy_ctor_str_short (256) 0.92x | index_sum_int (1,024) 6.94x |
| lumex C++11 / lumex C++17 | 29 | 1.010 | 1 | 0 | producer_consumer_str_short (1,024) 0.98x | push_pop_steady_str_long (1,024) 1.32x |
| lumex C++11 / boost C++11 | 29 | 2.817 | 22 | 1 | ctor_dtor_int (1,024) 0.86x | copy_ctor_int (1,024) 15.78x |

- lumex C++17 is slower than boost C++17 by more than 10 % in: copy_ctor_int (1,024) 15.85x, copy_assign_int (1,024) 15.32x, front_back_int (1,024) 10.41x, iterate_sum_int (1,024) 8.68x, fill_drain_int (1,024) 7.54x, push_pop_steady_int (1,024) 7.51x, index_sum_int (1,024) 6.46x, front_back_str_short (1,024) 5.63x, iterate_sum_str_short (1,024) 4.62x, fill_int (1,024) 3.50x, copy_assign_str_short (256) 3.13x, index_sum_str_short (1,024) 3.12x, copy_ctor_str_short (256) 3.07x, push_overwrite_int (1,024) 2.63x, push_overwrite_str_short (1,024) 2.62x, fill_drain_str_short (256) 2.57x, push_pop_steady_str_short (1,024) 2.33x, push_overwrite_str_long (1,024) 2.15x, producer_consumer_int (1,024) 2.07x, fill_str_short (256) 1.56x, producer_consumer_str_short (1,024) 1.28x
- lumex C++17 is faster than boost C++17 by more than 10 % in: ctor_dtor_int (1,024) 0.83x
- lumex C++17 is slower than vector C++17 by more than 10 % in: copy_assign_int (1,024) 128.69x, copy_ctor_int (1,024) 73.32x, push_pop_steady_int (1,024) 8.10x, front_back_int (1,024) 7.32x, fill_drain_int (1,024) 7.11x, index_sum_int (1,024) 6.93x, iterate_sum_int (1,024) 6.88x, copy_assign_str_long (128) 5.84x, push_overwrite_int (1,024) 5.67x, front_back_str_short (1,024) 3.67x, fill_int (1,024) 3.45x, index_sum_str_short (1,024) 3.14x, iterate_sum_str_short (1,024) 3.10x, copy_ctor_str_short (256) 2.57x, fill_str_long (256) 2.57x, push_overwrite_str_short (1,024) 2.43x, fill_drain_str_short (256) 2.30x, push_overwrite_str_long (1,024) 2.24x, push_pop_steady_str_short (1,024) 2.17x, copy_assign_str_short (256) 2.16x, push_pop_steady_str_long (1,024) 2.05x, producer_consumer_int (1,024) 1.96x, fill_str_short (256) 1.64x, producer_consumer_str_short (1,024) 1.26x, move_pair_str_short (256) 1.15x
- lumex C++17 is faster than vector C++17 by more than 10 % in: ctor_dtor_str_short (1,024) 0.03x, ctor_dtor_int (1,024) 0.45x, copy_ctor_str_long (128) 0.84x

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
| lumex compare-wrap C++17 | 40 | 1024 / 1 | 1024 / 1 | 1024 / 1 | Y/Y |

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
| lumex compare-wrap C++17 | 40 | 4096 / 1 | 4096 / 1 | 4096 / 1 | Y/Y |

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
| lumex compare-wrap C++17 | 40 | 8192 / 1 | 8192 / 1 | 8192 / 1 | Y/Y |

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
| lumex compare-wrap C++17 | 40 | 32768 / 1 | 32768 / 1 | 32768 / 1 | Y/Y |

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
| lumex compare-wrap C++17 | 40 | 32768 / 1 | 32768 / 1 | 32768 / 1 | Y/Y |

