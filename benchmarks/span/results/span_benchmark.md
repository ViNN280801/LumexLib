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
| ctor_ptr_size | 1,024 | 0.30 | 0.31 (1.04) | 0.30 (1.02) | 0.30 (1.02) | 0.30 (1.01) | 0.30 (1.00) | 0.30 (1.00) |
| ctor_c_array |  | 0.25 | 0.25 (1.01) | 0.25 (1.01) | 0.25 (1.01) | 0.25 (1.01) | 0.26 (1.04) | 0.25 (1.01) |
| ctor_std_array |  | 0.25 | 0.25 (1.00) | 0.25 (1.00) | 0.26 (1.03) | 0.26 (1.05) | 0.25 (1.00) | 0.26 (1.03) |
| ctor_vector |  | 0.39 | 0.41 (1.05) | 0.41 (1.04) | 0.41 (1.04) | 0.41 (1.04) | 0.39 (1.00) | 0.41 (1.05) |
| ctor_static_array |  | 0.14 | 0.14 (1.02) | 0.14 (1.01) | 0.14 (1.03) | 0.14 (1.01) | 0.14 (1.01) | 0.14 (1.02) |
| copy | 1,024 | 0.26 | 0.25 (0.96) | 0.25 (0.96) | 0.25 (0.95) | 0.25 (0.95) | 0.25 (0.95) | 0.25 (0.95) |
| sum_range_for | 8 | 1.95 | 2.99 (1.53) | 2.93 (1.50) | 2.56 (1.31) | 2.60 (1.33) | 2.96 (1.52) | 2.57 (1.32) |
| sum_range_for | 1,024 | 241.1 | 241.6 (1.00) | 245.6 (1.02) | 243.7 (1.01) | 242.8 (1.01) | 245.8 (1.02) | 242.3 (1.00) |
| sum_range_for | 1,000,000 | 241097 | 241339 (1.00) | 241455 (1.00) | 241128 (1.00) | 241802 (1.00) | 240710 (1.00) | 240341 (1.00) |
| sum_index | 8 | 2.98 | 1.99 (0.67) | 1.98 (0.66) | 2.72 (0.91) | 2.56 (0.86) | 2.07 (0.69) | 2.58 (0.86) |
| sum_index | 1,024 | 242.5 | 243.7 (1.00) | 242.0 (1.00) | 244.1 (1.01) | 245.0 (1.01) | 242.8 (1.00) | 241.5 (1.00) |
| sum_index | 1,000,000 | 240667 | 241970 (1.01) | 240801 (1.00) | 240858 (1.00) | 240813 (1.00) | 239490 (1.00) | 240240 (1.00) |
| sum_iterators | 8 | 2.05 | 2.84 (1.38) | 3.00 (1.46) | 1.98 (0.96) | 2.00 (0.97) | 2.55 (1.24) | 1.98 (0.96) |
| sum_iterators | 1,024 | 240.2 | 242.7 (1.01) | 243.0 (1.01) | 242.8 (1.01) | 241.3 (1.00) | 241.8 (1.01) | 241.6 (1.01) |
| sum_iterators | 1,000,000 | 240280 | 240755 (1.00) | 241725 (1.01) | 242717 (1.01) | 241791 (1.01) | 241883 (1.01) | 239167 (1.00) |
| sum_static_range_for |  | 1.96 | 2.05 (1.05) | 2.04 (1.04) | 1.99 (1.02) | 1.98 (1.01) | 1.98 (1.01) | 1.98 (1.01) |
| first | 1,024 | 0.50 | 0.50 (1.00) | 0.60 (1.21) | 0.60 (1.20) | 0.50 (0.99) | 0.49 (0.99) | 0.50 (1.00) |
| last | 1,024 | 0.59 | 0.60 (1.02) | 0.74 (1.25) | 0.74 (1.26) | 0.60 (1.03) | 0.60 (1.02) | 0.60 (1.02) |
| subspan | 1,024 | 0.50 | 0.50 (1.00) | 0.75 (1.48) | 0.73 (1.45) | 0.50 (0.99) | 0.50 (1.00) | 0.50 (0.99) |
| subspan_rest | 1,024 | 0.65 | 0.64 (0.99) | 0.75 (1.16) | 0.74 (1.15) | 0.65 (1.00) | 0.65 (1.00) | 0.64 (0.99) |
| static_first |  | 0.31 | 0.30 (0.99) | 0.31 (1.00) | 0.31 (1.01) | 0.31 (1.01) | 0.30 (0.98) | 0.31 (1.01) |
| static_subspan |  | 0.36 | 0.35 (0.97) | 0.33 (0.93) | 0.35 (0.98) | 0.33 (0.93) | 0.33 (0.93) | 0.33 (0.93) |
| static_first_of_dynamic | 1,024 | 0.31 | 0.30 (0.99) | 0.40 (1.30) | 0.40 (1.29) | 0.31 (1.02) | 0.30 (1.00) | 0.31 (1.02) |
| static_to_dynamic |  | 0.30 | 0.30 (0.98) | 0.31 (1.02) | 0.30 (0.98) | 0.30 (0.98) | 0.31 (1.01) | 0.30 (0.97) |
| dynamic_to_static |  | 0.17 | 0.17 (1.01) | 0.30 (1.81) | 0.30 (1.79) | 0.17 (1.00) | 0.17 (1.01) | 0.17 (1.01) |
| mutable_to_const | 1,024 | 0.35 | 0.31 (0.88) | 0.30 (0.86) | 0.35 (1.00) | 0.31 (0.87) | 0.30 (0.85) | 0.30 (0.86) |
| pass_noinline | 1,024 | 0.50 | 0.50 (1.01) | 0.50 (1.00) | 0.50 (1.00) | 0.50 (1.00) | 0.50 (1.00) | 0.51 (1.02) |
| pass_inline | 1,024 | 0.41 | 0.39 (0.96) | 0.39 (0.96) | 0.45 (1.09) | 0.45 (1.09) | 0.40 (0.97) | 0.45 (1.09) |
| pass_static_noinline |  | 0.49 | 0.49 (0.99) | 0.49 (1.00) | 0.49 (1.00) | 0.49 (1.00) | 0.50 (1.00) | 0.49 (0.99) |
| divide_and_conquer | 8 | 23.53 | 21.73 (0.92) | 20.41 (0.87) | 21.15 (0.90) | 26.77 (1.14) | 22.24 (0.95) | 26.22 (1.11) |
| divide_and_conquer | 1,024 | 3191 | 2991 (0.94) | 2583 (0.81) | 2776 (0.87) | 3578 (1.12) | 2851 (0.89) | 3583 (1.12) |
| divide_and_conquer | 1,000,000 | 3054060 | 2802890 (0.92) | 2543910 (0.83) | 2535930 (0.83) | 3114090 (1.02) | 2645160 (0.87) | 3114520 (1.02) |
| bytes_sum | 1,024 | 981.1 | 967.4 (0.99) | 978.4 (1.00) | 966.6 (0.99) | 969.7 (0.99) |  | 970.7 (0.99) |
| bytes_sum | 1,000,000 | 953744 | 953183 (1.00) | 962211 (1.01) | 946346 (0.99) | 953314 (1.00) |  | 958548 (1.01) |
| size_bytes | 1,024 | 0.46 | 0.46 (1.00) | 0.47 (1.03) | 0.46 (1.00) | 0.47 (1.03) | 0.46 (1.00) | 0.47 (1.03) |
| front_back | 1,024 | 0.34 | 0.33 (0.99) | 0.35 (1.04) | 0.34 (1.01) | 0.33 (0.98) | 0.34 (1.00) | 0.34 (0.99) |

Summary against lumex C++11 (geometric mean of the ratio over all scenarios; scenarios slower or faster by more than 10 %):

| variant | scenarios | geometric mean | slower | faster |
| --- | ---: | ---: | ---: | ---: |
| lumex C++20 | 36 | 1.001 | 2 | 2 |
| lumex checks C++11 | 36 | 1.045 | 8 | 5 |
| lumex checks C++20 | 36 | 1.047 | 7 | 3 |
| std::span C++20 | 36 | 1.010 | 3 | 2 |
| boost::span C++11 | 34 | 0.992 | 2 | 4 |
| boost::span C++20 | 36 | 1.008 | 3 | 2 |

The columns of one C++ standard compared with each other (time of the first divided by the time of the second; the span of this library is faster below 1.00): geometric mean over all scenarios, the best and the worst scenario of the first:

| compared | scenarios | geometric mean | best | worst |
| --- | ---: | ---: | --- | --- |
| lumex C++20 / std::span C++20 | 36 | 0.991 | sum_index (8) 0.78x | sum_iterators (8) 1.42x |
| lumex C++20 / boost::span C++20 | 36 | 0.993 | sum_index (8) 0.77x | sum_iterators (8) 1.43x |
| lumex C++11 / boost::span C++11 | 34 | 1.008 | sum_range_for (8) 0.66x | sum_index (8) 1.44x |
| lumex checks C++11 / lumex C++11 | 36 | 1.045 | sum_index (8) 0.66x | dynamic_to_static 1.81x |
| lumex checks C++20 / lumex C++20 | 39 | 1.047 | sum_iterators (8) 0.70x | dynamic_to_static 1.78x |

- lumex C++20 is slower by more than 10 % in: sum_range_for (8) 1.53x, sum_iterators (8) 1.38x
- lumex checks C++11 is slower by more than 10 % in: dynamic_to_static 1.81x, sum_range_for (8) 1.50x, subspan (1,024) 1.48x, sum_iterators (8) 1.46x, static_first_of_dynamic (1,024) 1.30x, last (1,024) 1.25x, first (1,024) 1.21x, subspan_rest (1,024) 1.16x
- lumex checks C++20 is slower by more than 10 % in: dynamic_to_static 1.79x, subspan (1,024) 1.45x, sum_range_for (8) 1.31x, static_first_of_dynamic (1,024) 1.29x, last (1,024) 1.26x, first (1,024) 1.20x, subspan_rest (1,024) 1.15x
- std::span C++20 is slower by more than 10 % in: sum_range_for (8) 1.33x, divide_and_conquer (8) 1.14x, divide_and_conquer (1,024) 1.12x
- boost::span C++11 is slower by more than 10 % in: sum_range_for (8) 1.52x, sum_iterators (8) 1.24x
- boost::span C++20 is slower by more than 10 % in: sum_range_for (8) 1.32x, divide_and_conquer (1,024) 1.12x, divide_and_conquer (8) 1.11x

### Conversions to and from std::span (lumex C++20 only; the ratio is to `copy` of the same variant)

| scenario | size | lumex C++20 |
| --- | --- | ---: |
| from_std_span | 1,024 | 0.30 (1.21) |
| to_std_span | 1,024 | 0.31 (1.24) |
| bytes_to_std_span | 1,024 | 0.30 (1.21) |

## clang

| scenario | size | lumex C++11 | lumex C++20 | lumex checks C++11 | lumex checks C++20 | std::span C++20 | boost::span C++11 | boost::span C++20 |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| ctor_ptr_size | 1,024 | 0.25† | 0.25 (0.99)† | 0.25 (0.99)† | 0.25 (1.00)† | 0.26 (1.01)† | 0.25 (0.99)† | 0.25 (0.98)† |
| ctor_c_array |  | 0.25† | 0.25 (1.00)† | 0.25 (1.01)† | 0.25 (1.00)† | 0.25 (1.01)† | 0.25 (1.01)† | 0.25 (1.01)† |
| ctor_std_array |  | 0.25† | 0.25 (0.99)† | 0.25 (1.00)† | 0.25 (1.00)† | 0.25 (0.99)† | 0.25 (0.99)† | 0.25 (0.99)† |
| ctor_vector |  | 0.36† | 0.37 (1.02)† | 0.37 (1.02)† | 0.37 (1.03)† | 0.35 (0.99)† | 0.37 (1.02)† | 0.35 (0.99)† |
| ctor_static_array |  | 0.23 | 0.23 (1.00) | 0.23 (1.00) | 0.23 (1.00) | 0.23 (1.00) | 0.23 (1.00) | 0.23 (1.00) |
| copy | 1,024 | 0.24 | 0.24 (1.01) | 0.24 (1.01)† | 0.25 (1.01) | 0.25 (1.01) | 0.25 (1.02) | 0.24 (1.01) |
| sum_range_for | 8 | 2.19 | 2.19 (1.00) | 2.19 (1.00) | 2.19 (1.00) | 2.19 (1.00) | 2.20 (1.00) | 2.15 (0.98) |
| sum_range_for | 1,024 | 93.78 | 92.62 (0.99) | 98.86 (1.05) | 96.85 (1.03) | 110.3 (1.18) | 98.47 (1.05) | 108.3 (1.15) |
| sum_range_for | 1,000,000 | 99033 | 98188 (0.99) | 100206 (1.01) | 99374 (1.00) | 101248 (1.02) | 100438 (1.01) | 100613 (1.02) |
| sum_index | 8 | 2.20 | 2.19 (1.00) | 2.19 (1.00) | 2.18 (0.99) | 2.17 (0.99) | 2.18 (0.99) | 2.16 (0.98) |
| sum_index | 1,024 | 102.3 | 102.7 (1.00) | 96.04 (0.94) | 96.20 (0.94) | 97.27 (0.95) | 95.18 (0.93) | 97.87 (0.96) |
| sum_index | 1,000,000 | 101975 | 101640 (1.00) | 99728 (0.98) | 100434 (0.98) | 101324 (0.99) | 99198 (0.97) | 102280 (1.00) |
| sum_iterators | 8 | 2.20 | 2.18 (0.99) | 2.18 (0.99) | 2.20 (1.00) | 2.19 (1.00) | 2.19 (1.00) | 2.15 (0.97) |
| sum_iterators | 1,024 | 98.66 | 98.07 (0.99) | 110.1 (1.12) | 109.0 (1.10) | 97.33 (0.99) | 109.4 (1.11) | 97.83 (0.99) |
| sum_iterators | 1,000,000 | 101880 | 101017 (0.99) | 102358 (1.00) | 103546 (1.02) | 99206 (0.97) | 101446 (1.00) | 100185 (0.98) |
| sum_static_range_for |  | 1.54 | 1.54 (1.00) | 1.54 (1.00) | 1.54 (1.00) | 1.53 (0.99) | 1.54 (1.00) | 1.54 (1.00) |
| first | 1,024 | 0.51 | 0.50 (0.99) | 0.56 (1.11) | 0.56 (1.11) | 0.49 (0.98) | 0.50 (0.98) | 0.50 (0.99) |
| last | 1,024 | 0.55 | 0.56 (1.01) | 0.64 (1.17) | 0.65 (1.18) | 0.60 (1.09) | 0.56 (1.01) | 0.56 (1.01) |
| subspan | 1,024 | 0.50 | 0.50 (1.00) | 0.74 (1.48) | 0.74 (1.49) | 0.50 (1.00) | 0.51 (1.01) | 0.50 (1.00) |
| subspan_rest | 1,024 | 0.60 | 0.60 (1.00)† | 0.64 (1.07) | 0.64 (1.07) | 0.60 (1.01) | 0.61 (1.02) | 0.61 (1.01) |
| static_first |  | 0.33 | 0.33 (0.99) | 0.33 (1.00) | 0.33 (1.00) | 0.33 (1.00) | 0.34 (1.02) | 0.33 (1.00) |
| static_subspan |  | 0.37 | 0.37 (1.00)† | 0.38 (1.01) | 0.38 (1.01) | 0.41 (1.09) | 0.38 (1.01) | 0.41 (1.10) |
| static_first_of_dynamic | 1,024 | 0.33 | 0.33 (0.99)† | 0.40 (1.21) | 0.40 (1.20) | 0.33 (1.00) | 0.33 (1.00) | 0.33 (1.00) |
| static_to_dynamic |  | 0.31† | 0.31 (0.99)† | 0.30 (0.97)† | 0.30 (0.97)† | 0.30 (0.97)† | 0.30 (0.99)† | 0.30 (0.96)† |
| dynamic_to_static |  | 0.23 | 0.23 (1.00) | 0.25 (1.06) | 0.25 (1.06) | 0.23 (1.00) | 0.23 (1.00) | 0.23 (1.00) |
| mutable_to_const | 1,024 | 0.24† | 0.25 (1.02)† | 0.24 (1.00)† | 0.25 (1.01)† | 0.24 (1.00)† | 0.25 (1.01)† | 0.24 (1.00)† |
| pass_noinline | 1,024 | 0.50 | 0.50 (1.00) | 0.50 (1.00) | 0.51 (1.01) | 0.50 (1.00) | 0.50 (1.00) | 0.51 (1.01) |
| pass_inline | 1,024 | 0.47 | 0.47 (1.00) | 0.47 (0.99) | 0.47 (1.00) | 0.47 (1.00) | 0.47 (1.00) | 0.47 (1.00) |
| pass_static_noinline |  | 0.36 | 0.36 (1.00) | 0.37 (1.02) | 0.37 (1.02) | 0.36 (1.01) | 0.37 (1.03) | 0.37 (1.03) |
| divide_and_conquer | 8 | 16.80 | 13.74 (0.82) | 13.32 (0.79) | 16.27 (0.97) | 17.27 (1.03) | 13.25 (0.79) | 17.20 (1.02) |
| divide_and_conquer | 1,024 | 2237 | 1786 (0.80) | 1770 (0.79) | 2216 (0.99) | 2278 (1.02) | 1774 (0.79) | 2275 (1.02) |
| divide_and_conquer | 1,000,000 | 1999270 | 1766990 (0.88) | 1839300 (0.92) | 2041980 (1.02) | 1916540 (0.96) | 1765720 (0.88) | 1909130 (0.95) |
| bytes_sum | 1,024 | 504.7 | 507.7 (1.01) | 501.4 (0.99)† | 500.6 (0.99) | 504.5 (1.00) |  | 500.1 (0.99) |
| bytes_sum | 1,000,000 | 498998 | 495422 (0.99) | 491239 (0.98)† | 495937 (0.99) | 492517 (0.99) |  | 491768 (0.99) |
| size_bytes | 1,024 | 0.46 | 0.46 (1.00) | 0.46 (1.00) | 0.46 (1.00) | 0.46 (1.00) | 0.46 (1.00) | 0.46 (1.00) |
| front_back | 1,024 | 0.35 | 0.35 (1.00) | 0.35 (1.00)† | 0.34 (1.00) | 0.33 (0.96) | 0.33 (0.95) | 0.33 (0.97) |

Summary against lumex C++11 (geometric mean of the ratio over all scenarios; scenarios slower or faster by more than 10 %):

| variant | scenarios | geometric mean | slower | faster |
| --- | ---: | ---: | ---: | ---: |
| lumex C++20 | 36 | 0.983 | 0 | 3 |
| lumex checks C++11 | 36 | 1.014 | 5 | 2 |
| lumex checks C++20 | 36 | 1.030 | 5 | 0 |
| std::span C++20 | 36 | 1.005 | 1 | 0 |
| boost::span C++11 | 34 | 0.986 | 1 | 3 |
| boost::span C++20 | 36 | 1.001 | 1 | 0 |

The columns of one C++ standard compared with each other (time of the first divided by the time of the second; the span of this library is faster below 1.00): geometric mean over all scenarios, the best and the worst scenario of the first:

| compared | scenarios | geometric mean | best | worst |
| --- | ---: | ---: | --- | --- |
| lumex C++20 / std::span C++20 | 36 | 0.979 | divide_and_conquer (1,024) 0.78x | sum_index (1,024) 1.06x |
| lumex C++20 / boost::span C++20 | 36 | 0.982 | divide_and_conquer (1,024) 0.78x | sum_index (1,024) 1.05x |
| lumex C++11 / boost::span C++11 | 34 | 1.015 | sum_iterators (1,024) 0.90x | divide_and_conquer (8) 1.27x |
| lumex checks C++11 / lumex C++11 | 36 | 1.014 | divide_and_conquer (1,024) 0.79x | subspan (1,024) 1.48x |
| lumex checks C++20 / lumex C++20 | 39 | 1.044 | sum_index (1,024) 0.94x | subspan (1,024) 1.49x |

- lumex checks C++11 is slower by more than 10 % in: subspan (1,024) 1.48x, static_first_of_dynamic (1,024) 1.21x, last (1,024) 1.17x, sum_iterators (1,024) 1.12x, first (1,024) 1.11x
- lumex checks C++20 is slower by more than 10 % in: subspan (1,024) 1.49x, static_first_of_dynamic (1,024) 1.20x, last (1,024) 1.18x, first (1,024) 1.11x, sum_iterators (1,024) 1.10x
- std::span C++20 is slower by more than 10 % in: sum_range_for (1,024) 1.18x
- boost::span C++11 is slower by more than 10 % in: sum_iterators (1,024) 1.11x
- boost::span C++20 is slower by more than 10 % in: sum_range_for (1,024) 1.15x

### Conversions to and from std::span (lumex C++20 only; the ratio is to `copy` of the same variant)

| scenario | size | lumex C++20 |
| --- | --- | ---: |
| from_std_span | 1,024 | 0.24 (1.00) |
| to_std_span | 1,024 | 0.25 (1.01) |
| bytes_to_std_span | 1,024 | 0.24 (1.00) |

## Compile time

- compiler gcc: g++ (GCC) 13.2.0
- compiler clang: clang version 23.1.0

Wall time of `-fsyntax-only -O0` of `compile_time.cpp` (includes the span header, builds spans of an array, a `std::array` and a `std::vector`, the subviews and `as_bytes`), median of the repeats, in milliseconds. `no span` is the same unit with a stub instead of a span: the cost of the shared headers.

| toolchain | no span C++11 | no span C++20 | lumex C++11 | lumex C++20 | std::span C++20 | boost::span C++11 | boost::span C++20 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| gcc | 56 | 119 | 136 | 355 | 141 | 133 | 233 |
| clang | 130 | 181 | 157 | 326 | 239 | 151 | 234 |

