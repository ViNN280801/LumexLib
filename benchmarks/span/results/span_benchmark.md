# span benchmark results

- machine="12th Gen Intel(R) Core(TM) i7-12700K" os="Linux 6.1.170-1-generic" logical_cpus=20 python=3.7.3
- passes=5 cpu=4
- variant gcc lumex_cxx11: compiler="GCC 13.2.0" library="libstdc++" cplusplus=201103
- variant gcc lumex_cxx20: compiler="GCC 13.2.0" library="libstdc++" cplusplus=202002
- variant gcc std_cxx20: compiler="GCC 13.2.0" library="libstdc++" cplusplus=202002
- variant gcc boost_cxx11: compiler="GCC 13.2.0" library="libstdc++" cplusplus=201103
- variant gcc boost_cxx20: compiler="GCC 13.2.0" library="libstdc++" cplusplus=202002
- variant clang lumex_cxx11: compiler="Clang 23.1.0" library="libc++" cplusplus=201103
- variant clang lumex_cxx20: compiler="Clang 23.1.0" library="libc++" cplusplus=202002
- variant clang std_cxx20: compiler="Clang 23.1.0" library="libc++" cplusplus=202002
- variant clang boost_cxx11: compiler="Clang 23.1.0" library="libc++" cplusplus=201103
- variant clang boost_cxx20: compiler="Clang 23.1.0" library="libc++" cplusplus=202002

Time per operation in nanoseconds: the lowest median of 15 repetitions over all passes (the best pass), and in parentheses the ratio to the span of this library at C++11 (below 1.00 is faster than that). The unit of an operation is the description of the scenario. A dagger marks a value whose slowest pass was more than 25 % slower than the best one: the processes of that executable do not agree (code and stack placement), so differences inside that band are noise.

## gcc

| scenario | size | lumex C++11 | lumex C++20 | std::span C++20 | boost::span C++11 | boost::span C++20 |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| ctor_ptr_size | 1,024 | 0.42 | 0.52 (1.24) | 0.52 (1.22) | 0.42 (0.99) | 0.51 (1.21) |
| ctor_c_array |  | 0.47 | 0.35 (0.74) | 0.35 (0.76) | 0.43 (0.92) | 0.35 (0.74) |
| ctor_std_array |  | 0.35 | 0.35 (1.00) | 0.35 (1.00) | 0.35 (0.99) | 0.36 (1.00) |
| ctor_vector |  | 0.56 | 0.62 (1.12) | 0.63 (1.13) | 0.56 (1.00) | 0.63 (1.13) |
| ctor_static_array |  | 0.24 | 0.25 (1.01) | 0.25 (1.01) | 0.25 (1.00) | 0.24 (0.99) |
| copy | 1,024 | 0.34 | 0.34 (0.98) | 0.35 (1.01) | 0.35 (1.02) | 0.35 (1.01) |
| sum_range_for | 8 | 3.39 | 3.93 (1.16) | 3.93 (1.16) | 3.33 (0.98) | 4.02 (1.19) |
| sum_range_for | 1,024 | 349.6 | 356.7 (1.02) | 350.3 (1.00) | 352.1 (1.01) | 350.9 (1.00) |
| sum_range_for | 1,000,000 | 347593 | 349639 (1.01) | 347946 (1.00) | 348940 (1.00) | 352800 (1.01) |
| sum_index | 8 | 3.54 | 2.80 (0.79) | 2.79 (0.79) | 3.58 (1.01) | 2.83 (0.80) |
| sum_index | 1,024 | 355.5 | 355.3 (1.00) | 351.9 (0.99) | 356.4 (1.00) | 348.4 (0.98) |
| sum_index | 1,000,000 | 348812 | 348795 (1.00) | 350272 (1.00) | 347378 (1.00) | 355077 (1.02) |
| sum_iterators | 8 | 4.14 | 3.59 (0.87) | 3.73 (0.90) | 4.10 (0.99) | 3.79 (0.91) |
| sum_iterators | 1,024 | 353.3 | 350.1 (0.99) | 345.3 (0.98) | 355.5 (1.01) | 346.7 (0.98) |
| sum_iterators | 1,000,000 | 346121 | 348819 (1.01) | 355901 (1.03) | 346682 (1.00) | 349230 (1.01) |
| sum_static_range_for |  | 2.86 | 2.84 (0.99) | 2.81 (0.98) | 2.78 (0.97) | 2.79 (0.97) |
| first | 1,024 | 0.69 | 0.70 (1.02) | 0.70 (1.01) | 0.68 (1.00) | 0.70 (1.02) |
| last | 1,024 | 0.94 | 0.85 (0.90) | 0.84 (0.89) | 0.95 (1.01) | 0.84 (0.90) |
| subspan | 1,024 | 0.69 | 0.71 (1.03) | 0.70 (1.03) | 0.71 (1.03) | 0.69 (1.01) |
| subspan_rest | 1,024 | 0.91 | 0.91 (0.99) | 0.91 (1.00) | 0.91 (1.00) | 0.91 (1.00) |
| static_first |  | 0.42 | 0.43 (1.01) | 0.42 (0.99) | 0.44 (1.04) | 0.43 (1.01) |
| static_subspan |  | 0.46 | 0.52 (1.11) | 0.54 (1.16) | 0.46 (0.99) | 0.52 (1.13) |
| static_first_of_dynamic | 1,024 | 0.42 | 0.42 (1.01) | 0.43 (1.03) | 0.42 (1.01) | 0.42 (1.01) |
| static_to_dynamic |  | 0.47 | 0.35 (0.73) | 0.35 (0.74) | 0.47 (0.99) | 0.34 (0.72) |
| dynamic_to_static |  | 0.28 | 0.28 (1.01) | 0.27 (0.96) | 0.28 (1.01) | 0.28 (0.99) |
| mutable_to_const | 1,024 | 0.43 | 0.53 (1.21) | 0.52 (1.21) | 0.42 (0.98) | 0.49 (1.13) |
| pass_noinline | 1,024 | 0.70 | 0.70 (0.99) | 0.69 (0.99) | 0.70 (0.99) | 0.69 (0.98) |
| pass_inline | 1,024 | 0.62 | 0.56 (0.91) | 0.56 (0.91) | 0.63 (1.02) | 0.57 (0.92) |
| pass_static_noinline |  | 0.70 | 0.69 (0.99) | 0.69 (0.99) | 0.69 (0.99) | 0.69 (0.98) |
| divide_and_conquer | 8 | 31.57 | 32.06 (1.02) | 32.48 (1.03) | 31.65 (1.00) | 32.71 (1.04) |
| divide_and_conquer | 1,024 | 4115 | 4390 (1.07) | 4347 (1.06) | 4111 (1.00) | 4380 (1.06) |
| divide_and_conquer | 1,000,000 | 3938250 | 4124780 (1.05) | 4090060 (1.04) | 3907010 (0.99) | 4087770 (1.04) |
| bytes_sum | 1,024 | 1381 | 1396 (1.01) | 1390 (1.01) |  | 1401 (1.01) |
| bytes_sum | 1,000,000 | 1389540 | 1387910 (1.00) | 1374330 (0.99) |  | 1368210 (0.98) |
| size_bytes | 1,024 | 0.69 | 0.67 (0.97) | 0.67 (0.98) | 0.67 (0.98) | 0.67 (0.97) |
| front_back | 1,024 | 0.47 | 0.57 (1.21) | 0.57 (1.20) | 0.48 (1.01) | 0.57 (1.21) |

Summary against lumex C++11 (geometric mean of the ratio over all scenarios; scenarios slower or faster by more than 10 %):

| variant | scenarios | geometric mean | slower | faster |
| --- | ---: | ---: | ---: | ---: |
| lumex C++20 | 36 | 0.999 | 6 | 4 |
| std::span C++20 | 36 | 0.999 | 6 | 5 |
| boost::span C++11 | 34 | 0.998 | 0 | 0 |
| boost::span C++20 | 36 | 0.997 | 6 | 3 |

The columns of one C++ standard compared with each other (time of the first divided by the time of the second; the span of this library is faster below 1.00): geometric mean over all scenarios, the best and the worst scenario of the first:

| compared | scenarios | geometric mean | best | worst |
| --- | ---: | ---: | --- | --- |
| lumex C++20 / std::span C++20 | 36 | 0.999 | static_subspan 0.96x | dynamic_to_static 1.05x |
| lumex C++20 / boost::span C++20 | 36 | 1.002 | sum_iterators (8) 0.95x | mutable_to_const (1,024) 1.07x |
| lumex C++11 / boost::span C++11 | 34 | 1.002 | static_first 0.96x | ctor_c_array 1.09x |

- lumex C++20 is slower by more than 10 % in: ctor_ptr_size (1,024) 1.24x, mutable_to_const (1,024) 1.21x, front_back (1,024) 1.21x, sum_range_for (8) 1.16x, ctor_vector 1.12x, static_subspan 1.11x
- std::span C++20 is slower by more than 10 % in: ctor_ptr_size (1,024) 1.22x, mutable_to_const (1,024) 1.21x, front_back (1,024) 1.20x, sum_range_for (8) 1.16x, static_subspan 1.16x, ctor_vector 1.13x
- boost::span C++20 is slower by more than 10 % in: front_back (1,024) 1.21x, ctor_ptr_size (1,024) 1.21x, sum_range_for (8) 1.19x, mutable_to_const (1,024) 1.13x, static_subspan 1.13x, ctor_vector 1.13x

## clang

| scenario | size | lumex C++11 | lumex C++20 | std::span C++20 | boost::span C++11 | boost::span C++20 |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| ctor_ptr_size | 1,024 | 0.37† | 0.34 (0.92)† | 0.35 (0.94)† | 0.35 (0.94)† | 0.35 (0.93)† |
| ctor_c_array |  | 0.37† | 0.34 (0.91)† | 0.35 (0.92)† | 0.35 (0.93)† | 0.35 (0.93)† |
| ctor_std_array |  | 0.37† | 0.34 (0.92)† | 0.35 (0.92)† | 0.35 (0.94)† | 0.35 (0.93)† |
| ctor_vector |  | 0.58 | 0.52 (0.89)† | 0.58 (1.00) | 0.51 (0.89)† | 0.51 (0.88)† |
| ctor_static_array |  | 0.34 | 0.34 (0.99) | 0.34 (0.99) | 0.33 (0.97) | 0.33 (0.98) |
| copy | 1,024 | 0.36† | 0.34 (0.95) | 0.35 (0.96) | 0.34 (0.94) | 0.34 (0.94) |
| sum_range_for | 8 | 3.09 | 3.05 (0.99) | 3.09 (1.00) | 3.11 (1.01) | 3.09 (1.00) |
| sum_range_for | 1,024 | 138.7 | 154.9 (1.12) | 161.0 (1.16) | 138.1 (1.00) | 153.9 (1.11) |
| sum_range_for | 1,000,000 | 140549 | 140673 (1.00) | 141932 (1.01) | 140537 (1.00) | 140744 (1.00) |
| sum_index | 8 | 3.09 | 3.10 (1.00) | 3.05 (0.99) | 3.12 (1.01) | 3.07 (0.99) |
| sum_index | 1,024 | 137.4 | 147.2 (1.07) | 146.2 (1.06) | 135.1 (0.98) | 145.0 (1.06) |
| sum_index | 1,000,000 | 140354 | 142169 (1.01) | 142113 (1.01) | 139635 (0.99) | 140592 (1.00) |
| sum_iterators | 8 | 3.06 | 3.11 (1.02) | 3.10 (1.01) | 3.12 (1.02) | 3.09 (1.01) |
| sum_iterators | 1,024 | 131.6 | 136.0 (1.03) | 137.7 (1.05) | 159.9 (1.21) | 136.4 (1.04) |
| sum_iterators | 1,000,000 | 139883 | 140480 (1.00) | 140221 (1.00) | 141632 (1.01) | 140059 (1.00) |
| sum_static_range_for |  | 2.16 | 2.18 (1.01) | 2.18 (1.01) | 2.19 (1.01) | 2.18 (1.01) |
| first | 1,024 | 0.69 | 0.70 (1.01) | 0.69 (1.00) | 0.69 (1.01) | 0.70 (1.02) |
| last | 1,024 | 0.78 | 0.77 (0.99) | 0.84 (1.07) | 0.78 (1.01) | 0.76 (0.98) |
| subspan | 1,024 | 0.69 | 0.70 (1.02) | 0.70 (1.02) | 0.70 (1.02) | 0.69 (1.01) |
| subspan_rest | 1,024 | 0.85 | 0.85 (1.00) | 0.83 (0.98) | 0.84 (0.99) | 0.83 (0.98) |
| static_first |  | 0.46 | 0.47 (1.02) | 0.46 (1.01) | 0.52 (1.12) | 0.46 (1.01) |
| static_subspan |  | 0.54 | 0.58 (1.07) | 0.58 (1.08) | 0.54 (1.00) | 0.58 (1.07) |
| static_first_of_dynamic | 1,024 | 0.48 | 0.46 (0.96) | 0.47 (0.97) | 0.47 (0.97) | 0.46 (0.96) |
| static_to_dynamic |  | 0.41† | 0.42 (1.02)† | 0.42 (1.03)† | 0.42 (1.03)† | 0.42 (1.02)† |
| dynamic_to_static |  | 0.33 | 0.34 (1.01) | 0.33 (1.00) | 0.34 (1.01) | 0.34 (1.01) |
| mutable_to_const | 1,024 | 0.35† | 0.44 (1.25)† | 0.46 (1.30)† | 0.35 (0.99)† | 0.46 (1.31)† |
| pass_noinline | 1,024 | 0.69 | 0.68 (0.99) | 0.70 (1.01) | 0.70 (1.01) | 0.70 (1.01) |
| pass_inline | 1,024 | 0.68 | 0.69 (1.01) | 0.68 (1.01) | 0.68 (1.00) | 0.69 (1.02) |
| pass_static_noinline |  | 0.50 | 0.51 (1.02) | 0.50 (1.00) | 0.48 (0.97) | 0.51 (1.02) |
| divide_and_conquer | 8 | 20.21 | 19.44 (0.96) | 19.09 (0.94) | 20.28 (1.00) | 19.55 (0.97) |
| divide_and_conquer | 1,024 | 2635 | 2573 (0.98) | 2566 (0.97) | 2708 (1.03) | 2545 (0.97) |
| divide_and_conquer | 1,000,000 | 2560030 | 2625840 (1.03) | 2654420 (1.04) | 2721520 (1.06) | 2627910 (1.03) |
| bytes_sum | 1,024 | 707.6 | 706.9 (1.00) | 706.7 (1.00) |  | 710.9 (1.00) |
| bytes_sum | 1,000,000 | 691955 | 704319 (1.02) | 699376 (1.01) |  | 694857 (1.00) |
| size_bytes | 1,024 | 0.67 | 0.66 (0.99) | 0.67 (1.00) | 0.66 (0.99) | 0.66 (0.99) |
| front_back | 1,024 | 0.46 | 0.46 (1.00) | 0.47 (1.02) | 0.47 (1.01) | 0.47 (1.02) |

Summary against lumex C++11 (geometric mean of the ratio over all scenarios; scenarios slower or faster by more than 10 %):

| variant | scenarios | geometric mean | slower | faster |
| --- | ---: | ---: | ---: | ---: |
| lumex C++20 | 36 | 1.004 | 2 | 1 |
| std::span C++20 | 36 | 1.012 | 2 | 0 |
| boost::span C++11 | 34 | 1.001 | 2 | 1 |
| boost::span C++20 | 36 | 1.004 | 2 | 1 |

The columns of one C++ standard compared with each other (time of the first divided by the time of the second; the span of this library is faster below 1.00): geometric mean over all scenarios, the best and the worst scenario of the first:

| compared | scenarios | geometric mean | best | worst |
| --- | ---: | ---: | --- | --- |
| lumex C++20 / std::span C++20 | 36 | 0.992 | ctor_vector 0.90x | subspan_rest (1,024) 1.02x |
| lumex C++20 / boost::span C++20 | 36 | 1.000 | mutable_to_const (1,024) 0.95x | subspan_rest (1,024) 1.02x |
| lumex C++11 / boost::span C++11 | 34 | 0.999 | sum_iterators (1,024) 0.82x | ctor_vector 1.13x |

- lumex C++20 is slower by more than 10 % in: mutable_to_const (1,024) 1.25x, sum_range_for (1,024) 1.12x
- std::span C++20 is slower by more than 10 % in: mutable_to_const (1,024) 1.30x, sum_range_for (1,024) 1.16x
- boost::span C++11 is slower by more than 10 % in: sum_iterators (1,024) 1.21x, static_first 1.12x
- boost::span C++20 is slower by more than 10 % in: mutable_to_const (1,024) 1.31x, sum_range_for (1,024) 1.11x

## Compile time

- compiler gcc: g++ (GCC) 13.2.0
- compiler clang: clang version 23.1.0

Wall time of `-fsyntax-only -O0` of `compile_time.cpp` (includes the span header, builds spans of an array, a `std::array` and a `std::vector`, the subviews and `as_bytes`), median of the repeats, in milliseconds. `no span` is the same unit with a stub instead of a span: the cost of the shared headers.

| toolchain | no span C++11 | no span C++20 | lumex C++11 | lumex C++20 | std::span C++20 | boost::span C++11 | boost::span C++20 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| gcc | 87 | 186 | 208 | 559 | 283 | 222 | 435 |
| clang | 236 | 306 | 260 | 525 | 382 | 240 | 379 |

