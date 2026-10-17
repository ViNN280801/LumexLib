# span benchmark results

- machine="12th Gen Intel(R) Core(TM) i7-12700K" os="Linux 6.1.170-1-generic" logical_cpus=20 python=3.7.3
- passes=5 cpu=3
- variant gcc lumex_cxx11: compiler="GCC 13.2.0" library="libstdc++" cplusplus=201103
- variant gcc lumex_cxx20: compiler="GCC 13.2.0" library="libstdc++" cplusplus=202002
- variant gcc lumex_checks_cxx11: compiler="GCC 13.2.0" library="libstdc++" cplusplus=201103
- variant gcc lumex_checks_cxx20: compiler="GCC 13.2.0" library="libstdc++" cplusplus=202002
- variant gcc std_cxx20: compiler="GCC 13.2.0" library="libstdc++" cplusplus=202002
- variant gcc boost_cxx11: compiler="GCC 13.2.0" library="libstdc++" cplusplus=201103
- variant gcc boost_cxx20: compiler="GCC 13.2.0" library="libstdc++" cplusplus=202002
- variant clang lumex_cxx11: compiler="Clang 23.1.0" library="libc++" cplusplus=201103
- variant clang lumex_cxx20: compiler="Clang 23.1.0" library="libc++" cplusplus=202002
- variant clang lumex_checks_cxx11: compiler="Clang 23.1.0" library="libc++" cplusplus=201103
- variant clang lumex_checks_cxx20: compiler="Clang 23.1.0" library="libc++" cplusplus=202002
- variant clang std_cxx20: compiler="Clang 23.1.0" library="libc++" cplusplus=202002
- variant clang boost_cxx11: compiler="Clang 23.1.0" library="libc++" cplusplus=201103
- variant clang boost_cxx20: compiler="Clang 23.1.0" library="libc++" cplusplus=202002

Time per operation in nanoseconds: the lowest median of 15 repetitions over all passes (the best pass), and in parentheses the ratio to the span of this library at C++11 (below 1.00 is faster than that). The unit of an operation is the description of the scenario. A dagger marks a value whose slowest pass was more than 25 % slower than the best one: the processes of that executable do not agree (code and stack placement), so differences inside that band are noise.

## gcc

| scenario | size | lumex C++11 | lumex C++20 | lumex checks C++11 | lumex checks C++20 | std::span C++20 | boost::span C++11 | boost::span C++20 |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| ctor_ptr_size | 1,024 | 0.25 | 0.25 (1.02) | 0.25 (1.02) | 0.25 (1.02) | 0.25 (1.02)† | 0.25 (1.02) | 0.25 (1.02) |
| ctor_c_array |  | 0.21 | 0.21 (1.01) | 0.21 (1.00) | 0.21 (1.01) | 0.21 (1.00)† | 0.21 (1.00) | 0.21 (1.00) |
| ctor_std_array |  | 0.21 | 0.21 (1.00)† | 0.21 (1.00) | 0.21 (1.00) | 0.21 (1.01)† | 0.21 (1.00) | 0.22 (1.05) |
| ctor_vector |  | 0.33 | 0.34 (1.04)† | 0.36 (1.08) | 0.33 (1.00) | 0.34 (1.04)† | 0.33 (1.00) | 0.35 (1.05) |
| ctor_static_array |  | 0.12 | 0.12 (0.98)† | 0.12 (0.97) | 0.12 (0.98) | 0.12 (0.98) | 0.12 (0.98) | 0.12 (0.97) |
| copy | 1,024 | 0.21 | 0.21 (0.99)† | 0.21 (0.99) | 0.21 (0.99) | 0.21 (0.99) | 0.20 (0.98) | 0.21 (0.98) |
| sum_range_for | 8 | 1.65 | 2.04 (1.24)† | 1.67 (1.01) | 2.09 (1.27) | 1.98 (1.20) | 1.83 (1.11) | 1.78 (1.08)† |
| sum_range_for | 1,024 | 210.8 | 211.9 (1.01) | 211.4 (1.00) | 210.8 (1.00) | 213.1 (1.01) | 210.1 (1.00) | 212.6 (1.01) |
| sum_range_for | 1,000,000 | 206270 | 206419 (1.00) | 205794 (1.00) | 207551 (1.01) | 206640 (1.00) | 205774 (1.00) | 205468 (1.00) |
| sum_index | 8 | 1.83† | 1.67 (0.92)† | 1.71 (0.94) | 1.76 (0.96) | 1.88 (1.03) | 1.67 (0.91) | 1.88 (1.03) |
| sum_index | 1,024 | 210.3 | 210.8 (1.00) | 210.5 (1.00) | 213.3 (1.01) | 211.9 (1.01) | 210.3 (1.00) | 210.4 (1.00) |
| sum_index | 1,000,000 | 205464 | 206285 (1.00) | 205930 (1.00) | 206387 (1.00) | 206406 (1.00) | 206082 (1.00) | 205893 (1.00) |
| sum_iterators | 8 | 1.66† | 2.02 (1.22) | 1.64 (0.99) | 1.88 (1.13) | 1.65 (1.00) | 1.71 (1.03) | 1.67 (1.01) |
| sum_iterators | 1,024 | 210.5 | 210.4 (1.00) | 210.3 (1.00) | 211.7 (1.01) | 212.2 (1.01) | 210.3 (1.00) | 212.6 (1.01) |
| sum_iterators | 1,000,000 | 206265 | 206006 (1.00) | 206152 (1.00) | 206959 (1.00) | 206522 (1.00) | 205580 (1.00) | 205237 (1.00) |
| sum_static_range_for |  | 1.63 | 1.69 (1.04) | 1.63 (1.00) | 1.65 (1.01) | 1.64 (1.00) | 1.64 (1.00) | 1.64 (1.00) |
| first | 1,024 | 0.41 | 0.41 (1.00) | 0.49 (1.20) | 0.49 (1.20) | 0.41 (1.00) | 0.41 (1.00) | 0.41 (1.00) |
| last | 1,024 | 0.49 | 0.49 (1.00) | 0.62 (1.26) | 0.62 (1.25) | 0.49 (1.01) | 0.49 (1.00) | 0.49 (1.00) |
| subspan | 1,024 | 0.41 | 0.41 (1.00) | 0.62 (1.50) | 0.62 (1.51) | 0.40 (0.97) | 0.41 (1.00) | 0.40 (0.97) |
| subspan_rest | 1,024 | 0.54 | 0.53 (1.00) | 0.62 (1.15) | 0.62 (1.15) | 0.53 (0.98) | 0.53 (0.98) | 0.53 (0.98) |
| static_first |  | 0.25 | 0.25 (1.01) | 0.25 (1.00) | 0.25 (1.00) | 0.24 (0.97) | 0.25 (1.01) | 0.24 (0.97) |
| static_subspan |  | 0.28 | 0.29 (1.00) | 0.29 (1.01) | 0.28 (0.97) | 0.28 (0.97) | 0.27 (0.96) | 0.28 (0.97) |
| static_first_of_dynamic | 1,024 | 0.25 | 0.25 (1.00) | 0.33 (1.31) | 0.33 (1.30) | 0.25 (1.00) | 0.25 (1.00) | 0.25 (1.00) |
| static_to_dynamic |  | 0.25 | 0.25 (0.99) | 0.25 (1.00) | 0.25 (1.01) | 0.25 (1.00) | 0.26 (1.04) | 0.25 (1.00) |
| dynamic_to_static |  | 0.14 | 0.14 (1.01) | 0.25 (1.83) | 0.25 (1.83) | 0.14 (1.03) | 0.15 (1.06) | 0.15 (1.10)† |
| mutable_to_const | 1,024 | 0.29 | 0.25 (0.87) | 0.30 (1.03) | 0.26 (0.90) | 0.25 (0.87) | 0.25 (0.87) | 0.25 (0.87) |
| pass_noinline | 1,024 | 0.41 | 0.41 (1.00) | 0.41 (1.00) | 0.41 (1.00)† | 0.41 (1.00) | 0.41 (1.00) | 0.41 (1.00) |
| pass_inline | 1,024 | 0.34 | 0.33 (0.97) | 0.33 (0.97) | 0.33 (0.97)† | 0.37 (1.09) | 0.36 (1.05) | 0.37 (1.10) |
| pass_static_noinline |  | 0.41† | 0.42 (1.01) | 0.41 (1.00) | 0.41 (1.00)† | 0.41 (1.00) | 0.41 (1.00) | 0.41 (1.00) |
| divide_and_conquer | 8 | 20.07 | 18.83 (0.94) | 17.44 (0.87) | 19.48 (0.97)† | 22.55 (1.12) | 19.14 (0.95) | 22.31 (1.11) |
| divide_and_conquer | 1,024 | 2708 | 2559 (0.94) | 2173 (0.80) | 2464 (0.91)† | 2987 (1.10) | 2408 (0.89) | 3002 (1.11) |
| divide_and_conquer | 1,000,000 | 2615550 | 2410120 (0.92) | 2083980 (0.80) | 2307920 (0.88)† | 2611170 (1.00) | 2272500 (0.87) | 2608370 (1.00) |
| bytes_sum | 1,024 | 846.0 | 841.8 (1.00) | 842.8 (1.00) | 840.7 (0.99)† | 840.8 (0.99) |  | 840.4 (0.99) |
| bytes_sum | 1,000,000 | 827595 | 826949 (1.00) | 821911 (0.99) | 824121 (1.00)† | 824746 (1.00) |  | 823286 (0.99) |
| size_bytes | 1,024 | 0.41 | 0.41 (1.00) | 0.41 (1.00) | 0.41 (1.00) | 0.41 (1.00) | 0.41 (1.00) | 0.41 (0.99) |
| front_back | 1,024 | 0.29 | 0.28 (0.96) | 0.30 (1.03) | 0.28 (0.98)† | 0.28 (0.98) | 0.28 (0.96) | 0.28 (0.96) |

Summary against lumex C++11 (geometric mean of the ratio over all scenarios; scenarios slower or faster by more than 10 %):

| variant | scenarios | geometric mean | slower | faster |
| --- | ---: | ---: | ---: | ---: |
| lumex C++20 | 36 | 1.000 | 2 | 1 |
| lumex checks C++11 | 36 | 1.036 | 6 | 3 |
| lumex checks C++20 | 36 | 1.049 | 8 | 1 |
| std::span C++20 | 36 | 1.009 | 3 | 1 |
| boost::span C++11 | 34 | 0.989 | 1 | 3 |
| boost::span C++20 | 36 | 1.008 | 3 | 1 |

The columns of one C++ standard compared with each other (time of the first divided by the time of the second; the span of this library is faster below 1.00): geometric mean over all scenarios, the best and the worst scenario of the first:

| compared | scenarios | geometric mean | best | worst |
| --- | ---: | ---: | --- | --- |
| lumex C++20 / std::span C++20 | 36 | 0.991 | divide_and_conquer (8) 0.84x | sum_iterators (8) 1.22x |
| lumex C++20 / boost::span C++20 | 36 | 0.993 | divide_and_conquer (8) 0.84x | sum_iterators (8) 1.21x |
| lumex C++11 / boost::span C++11 | 34 | 1.011 | sum_range_for (8) 0.90x | divide_and_conquer (1,000,000) 1.15x |
| lumex checks C++11 / lumex C++11 | 36 | 1.036 | divide_and_conquer (1,000,000) 0.80x | dynamic_to_static 1.83x |
| lumex checks C++20 / lumex C++20 | 39 | 1.046 | sum_iterators (8) 0.93x | dynamic_to_static 1.81x |

- lumex C++20 is slower by more than 10 % in: sum_range_for (8) 1.24x, sum_iterators (8) 1.22x
- lumex checks C++11 is slower by more than 10 % in: dynamic_to_static 1.83x, subspan (1,024) 1.50x, static_first_of_dynamic (1,024) 1.31x, last (1,024) 1.26x, first (1,024) 1.20x, subspan_rest (1,024) 1.15x
- lumex checks C++20 is slower by more than 10 % in: dynamic_to_static 1.83x, subspan (1,024) 1.51x, static_first_of_dynamic (1,024) 1.30x, sum_range_for (8) 1.27x, last (1,024) 1.25x, first (1,024) 1.20x, subspan_rest (1,024) 1.15x, sum_iterators (8) 1.13x
- std::span C++20 is slower by more than 10 % in: sum_range_for (8) 1.20x, divide_and_conquer (8) 1.12x, divide_and_conquer (1,024) 1.10x
- boost::span C++11 is slower by more than 10 % in: sum_range_for (8) 1.11x
- boost::span C++20 is slower by more than 10 % in: divide_and_conquer (8) 1.11x, divide_and_conquer (1,024) 1.11x, dynamic_to_static 1.10x

### Conversions to and from std::span (lumex C++20 only; the ratio is to `copy` of the same variant)

| scenario | size | lumex C++20 |
| --- | --- | ---: |
| from_std_span | 1,024 | 0.25 (1.22) |
| to_std_span | 1,024 | 0.25 (1.23) |
| bytes_to_std_span | 1,024 | 0.25 (1.21) |

## clang

| scenario | size | lumex C++11 | lumex C++20 | lumex checks C++11 | lumex checks C++20 | std::span C++20 | boost::span C++11 | boost::span C++20 |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| ctor_ptr_size | 1,024 | 0.20† | 0.21 (1.00)† | 0.21 (1.00)† | 0.21 (1.00) | 0.20 (1.00)† | 0.21 (1.00)† | 0.20 (1.00)† |
| ctor_c_array |  | 0.21† | 0.21 (1.00)† | 0.21 (1.01)† | 0.21 (1.00) | 0.21 (1.00)† | 0.21 (1.00)† | 0.21 (1.00)† |
| ctor_std_array |  | 0.21† | 0.21 (1.00)† | 0.21 (1.03)† | 0.21 (1.01) | 0.21 (1.00)† | 0.21 (1.00)† | 0.21 (1.00)† |
| ctor_vector |  | 0.30† | 0.30 (0.99)† | 0.35 (1.17)† | 0.35 (1.17) | 0.30 (1.01)† | 0.29 (0.99)† | 0.30 (1.01)† |
| ctor_static_array |  | 0.21† | 0.21 (1.00)† | 0.21 (1.00) | 0.21 (1.00) | 0.21 (1.00) | 0.21 (1.00) | 0.20 (1.00) |
| copy | 1,024 | 0.20† | 0.20 (1.00)† | 0.21 (1.00) | 0.21 (1.00) | 0.20 (1.00) | 0.20 (1.00) | 0.20 (1.00) |
| sum_range_for | 8 | 1.86† | 1.85 (1.00)† | 1.85 (1.00) | 1.85 (1.00) | 1.85 (1.00) | 1.85 (1.00) | 1.85 (1.00) |
| sum_range_for | 1,024 | 77.58 | 77.64 (1.00)† | 80.24 (1.03) | 80.27 (1.03) | 100.6 (1.30) | 81.30 (1.05) | 94.94 (1.22) |
| sum_range_for | 1,000,000 | 82450 | 84178 (1.02)† | 80489 (0.98) | 80378 (0.97) | 90222 (1.09) | 82940 (1.01) | 82225 (1.00) |
| sum_index | 8 | 1.87† | 1.86 (1.00)† | 1.85 (0.99) | 1.84 (0.99) | 1.86 (0.99) | 1.85 (0.99) | 1.85 (0.99) |
| sum_index | 1,024 | 84.81 | 87.67 (1.03)† | 81.24 (0.96) | 81.17 (0.96) | 79.96 (0.94) | 79.65 (0.94) | 79.43 (0.94) |
| sum_index | 1,000,000 | 85368 | 85263 (1.00)† | 80289 (0.94) | 78821 (0.92) | 81330 (0.95) | 82557 (0.97) | 84211 (0.99) |
| sum_iterators | 8 | 1.85† | 1.85 (1.00)† | 1.85 (1.00) | 1.85 (1.00) | 1.85 (1.00) | 1.85 (1.00) | 1.85 (1.00) |
| sum_iterators | 1,024 | 80.05 | 80.22 (1.00)† | 77.32 (0.97) | 77.49 (0.97) | 81.26 (1.02) | 96.82 (1.21) | 81.13 (1.01) |
| sum_iterators | 1,000,000 | 83919 | 85287 (1.02)† | 79846 (0.95) | 81101 (0.97) | 80041 (0.95) | 85350 (1.02) | 80790 (0.96) |
| sum_static_range_for |  | 1.30 | 1.30 (1.00)† | 1.30 (1.00) | 1.30 (1.00) | 1.30 (1.00) | 1.30 (1.00) | 1.30 (1.00) |
| first | 1,024 | 0.41 | 0.41 (1.00)† | 0.45 (1.10) | 0.45 (1.10) | 0.41 (1.00) | 0.41 (1.00) | 0.41 (1.00) |
| last | 1,024 | 0.46 | 0.45 (0.99)† | 0.53 (1.16) | 0.53 (1.16) | 0.50 (1.09) | 0.46 (1.00) | 0.46 (1.01) |
| subspan | 1,024 | 0.41 | 0.41 (1.00)† | 0.62 (1.50) | 0.62 (1.50) | 0.41 (1.00) | 0.41 (1.00) | 0.41 (1.00) |
| subspan_rest | 1,024 | 0.49 | 0.51 (1.03)† | 0.53 (1.08) | 0.53 (1.08) | 0.49 (1.00) | 0.49 (1.00) | 0.49 (1.00) |
| static_first |  | 0.28 | 0.28 (1.00)† | 0.29 (1.04) | 0.28 (1.02) | 0.28 (1.00) | 0.29 (1.05) | 0.28 (1.00) |
| static_subspan |  | 0.32 | 0.32 (0.99)† | 0.32 (1.00) | 0.32 (1.00) | 0.36 (1.14) | 0.32 (1.00) | 0.35 (1.10) |
| static_first_of_dynamic | 1,024 | 0.28 | 0.28 (1.00)† | 0.33 (1.19) | 0.33 (1.19) | 0.28 (1.00) | 0.28 (1.00) | 0.28 (1.00) |
| static_to_dynamic |  | 0.25† | 0.25 (1.01)† | 0.25 (1.03)† | 0.25 (1.01) | 0.25 (1.01)† | 0.25 (1.01)† | 0.25 (1.00)† |
| dynamic_to_static |  | 0.21 | 0.21 (1.00) | 0.21 (1.00) | 0.21 (1.00) | 0.21 (1.00) | 0.21 (1.00) | 0.21 (1.00) |
| mutable_to_const | 1,024 | 0.20† | 0.21 (1.01)† | 0.20 (1.00)† | 0.20 (1.00)† | 0.21 (1.00)† | 0.20 (1.00)† | 0.21 (1.00)† |
| pass_noinline | 1,024 | 0.41 | 0.41 (1.00) | 0.41 (1.00) | 0.41 (1.00) | 0.41 (1.00) | 0.41 (1.00) | 0.41 (1.00) |
| pass_inline | 1,024 | 0.41 | 0.41 (1.00) | 0.41 (1.00) | 0.41 (1.00) | 0.41 (1.00) | 0.41 (1.00) | 0.41 (1.00) |
| pass_static_noinline |  | 0.28 | 0.28 (1.00) | 0.28 (1.00) | 0.28 (1.00) | 0.28 (0.98) | 0.28 (1.01) | 0.28 (0.99) |
| divide_and_conquer | 8 | 14.37 | 11.49 (0.80) | 10.72 (0.75) | 13.77 (0.96) | 14.59 (1.02) | 10.79 (0.75) | 14.50 (1.01) |
| divide_and_conquer | 1,024 | 1845 | 1454 (0.79) | 1499 (0.81) | 1903 (1.03) | 1875 (1.02) | 1476 (0.80) | 1867 (1.01) |
| divide_and_conquer | 1,000,000 | 1675200 | 1466250 (0.88) | 1491020 (0.89) | 1582930 (0.94) | 1589700 (0.95) | 1488310 (0.89) | 1586010 (0.95) |
| bytes_sum | 1,024 | 422.0 | 422.1 (1.00) | 423.2 (1.00) | 423.0 (1.00) | 423.3 (1.00) |  | 421.8 (1.00) |
| bytes_sum | 1,000,000 | 412268 | 410935 (1.00) | 412259 (1.00) | 413092 (1.00) | 412165 (1.00) |  | 411780 (1.00) |
| size_bytes | 1,024 | 0.41 | 0.41 (1.00) | 0.41 (1.00) | 0.41 (1.00) | 0.41 (1.00) | 0.41 (1.00) | 0.41 (1.00) |
| front_back | 1,024 | 0.28† | 0.28 (1.00) | 0.29 (1.02) | 0.29 (1.02) | 0.28 (0.99) | 0.28 (0.99) | 0.28 (0.99) |

Summary against lumex C++11 (geometric mean of the ratio over all scenarios; scenarios slower or faster by more than 10 %):

| variant | scenarios | geometric mean | slower | faster |
| --- | ---: | ---: | ---: | ---: |
| lumex C++20 | 36 | 0.986 | 0 | 3 |
| lumex checks C++11 | 36 | 1.011 | 5 | 3 |
| lumex checks C++20 | 36 | 1.024 | 4 | 0 |
| std::span C++20 | 36 | 1.011 | 2 | 0 |
| boost::span C++11 | 34 | 0.987 | 1 | 3 |
| boost::span C++20 | 36 | 1.004 | 1 | 0 |

The columns of one C++ standard compared with each other (time of the first divided by the time of the second; the span of this library is faster below 1.00): geometric mean over all scenarios, the best and the worst scenario of the first:

| compared | scenarios | geometric mean | best | worst |
| --- | ---: | ---: | --- | --- |
| lumex C++20 / std::span C++20 | 36 | 0.975 | sum_range_for (1,024) 0.77x | sum_index (1,024) 1.10x |
| lumex C++20 / boost::span C++20 | 36 | 0.982 | divide_and_conquer (1,024) 0.78x | sum_index (1,024) 1.10x |
| lumex C++11 / boost::span C++11 | 34 | 1.013 | sum_iterators (1,024) 0.83x | divide_and_conquer (8) 1.33x |
| lumex checks C++11 / lumex C++11 | 36 | 1.011 | divide_and_conquer (8) 0.75x | subspan (1,024) 1.50x |
| lumex checks C++20 / lumex C++20 | 39 | 1.035 | sum_index (1,000,000) 0.92x | subspan (1,024) 1.50x |

- lumex checks C++11 is slower by more than 10 % in: subspan (1,024) 1.50x, static_first_of_dynamic (1,024) 1.19x, ctor_vector 1.17x, last (1,024) 1.16x, first (1,024) 1.10x
- lumex checks C++20 is slower by more than 10 % in: subspan (1,024) 1.50x, static_first_of_dynamic (1,024) 1.19x, ctor_vector 1.17x, last (1,024) 1.16x
- std::span C++20 is slower by more than 10 % in: sum_range_for (1,024) 1.30x, static_subspan 1.14x
- boost::span C++11 is slower by more than 10 % in: sum_iterators (1,024) 1.21x
- boost::span C++20 is slower by more than 10 % in: sum_range_for (1,024) 1.22x

### Conversions to and from std::span (lumex C++20 only; the ratio is to `copy` of the same variant)

| scenario | size | lumex C++20 |
| --- | --- | ---: |
| from_std_span | 1,024 | 0.21 (1.00) |
| to_std_span | 1,024 | 0.21 (1.03) |
| bytes_to_std_span | 1,024 | 0.21 (1.01) |

## Compile time

- compiler gcc: g++ (GCC) 13.2.0
- compiler clang: clang version 23.1.0

Wall time of `-fsyntax-only -O0` of `compile_time.cpp` (includes the span header, builds spans of an array, a `std::array` and a `std::vector`, the subviews and `as_bytes`), median of the repeats, in milliseconds. `no span` is the same unit with a stub instead of a span: the cost of the shared headers.

| toolchain | no span C++11 | no span C++20 | lumex C++11 | lumex C++20 | std::span C++20 | boost::span C++11 | boost::span C++20 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| gcc | 44 | 91 | 103 | 266 | 110 | 100 | 178 |
| clang | 96 | 136 | 122 | 253 | 180 | 116 | 177 |

