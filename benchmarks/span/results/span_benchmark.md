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
| ctor_ptr_size | 1,024 | 0.58 | 0.62 (1.06) | 0.42 (0.72) | 0.42 (0.72) | 0.42 (0.72) |
| ctor_c_array |  | 0.34 | 0.35 (1.02) | 0.35 (1.02) | 0.62 (1.81) | 0.34 (1.00) |
| ctor_std_array |  | 0.34 | 0.35 (1.01) | 0.64 (1.85) | 0.35 (1.01) | 0.49 (1.42)† |
| ctor_vector |  | 0.55 | 0.55 (0.99) | 0.59 (1.07) | 0.55 (0.99) | 0.62 (1.12) |
| ctor_static_array |  | 0.30 | 0.21 (0.69)† | 0.20 (0.67)† | 0.31 (1.03) | 0.27 (0.89) |
| copy | 1,024 | 0.36† | 0.35 (0.96) | 0.34 (0.96) | 0.35 (0.96) | 0.34 (0.95) |
| sum_range_for | 8 | 3.36 | 3.79 (1.13) | 4.44 (1.32) | 2.94 (0.88) | 3.85 (1.15) |
| sum_range_for | 1,024 | 368.3 | 356.9 (0.97) | 356.0 (0.97) | 355.5 (0.97) | 360.2 (0.98) |
| sum_range_for | 1,000,000 | 351581 | 350892 (1.00) | 351428 (1.00) | 349696 (0.99) | 348444 (0.99) |
| sum_index | 8 | 2.93 | 3.63 (1.24) | 3.73 (1.27) | 3.60 (1.23) | 3.75 (1.28) |
| sum_index | 1,024 | 355.7 | 354.1 (1.00) | 366.7 (1.03) | 351.7 (0.99) | 368.9 (1.04) |
| sum_index | 1,000,000 | 353302 | 351286 (0.99) | 350665 (0.99) | 351688 (1.00) | 350462 (0.99) |
| sum_iterators | 8 | 3.81 | 3.21 (0.84) | 2.98 (0.78) | 3.86 (1.01) | 3.02 (0.79) |
| sum_iterators | 1,024 | 356.9 | 352.3 (0.99) | 366.4 (1.03) | 367.3 (1.03) | 367.6 (1.03) |
| sum_iterators | 1,000,000 | 352883 | 349653 (0.99) | 341273 (0.97) | 350860 (0.99) | 352477 (1.00) |
| sum_static_range_for |  | 3.31 | 2.76 (0.83) | 2.68 (0.81) | 2.89 (0.87) | 2.64 (0.80) |
| first | 1,024 | 0.70 | 0.70 (1.00) | 0.69 (0.99) | 0.68 (0.98) | 0.70 (1.00) |
| last | 1,024 | 0.83 | 0.82 (0.99) | 0.82 (0.99) | 1.01 (1.21) | 0.82 (0.98) |
| subspan | 1,024 | 0.70 | 0.70 (1.00) | 0.69 (0.99) | 0.70 (1.00) | 0.69 (1.00) |
| subspan_rest | 1,024 | 0.90 | 0.90 (1.00) | 0.89 (0.99) | 0.90 (0.99) | 0.90 (1.00) |
| static_first |  | 0.42 | 0.42 (1.02) | 0.62 (1.49) | 0.56 (1.36) | 0.56 (1.35) |
| static_subspan |  | 0.65 | 0.65 (0.99) | 0.46 (0.71) | 0.47 (0.72) | 0.47 (0.72) |
| static_first_of_dynamic | 1,024 | 0.42 | 0.42 (1.00) | 0.61 (1.46) | 0.41 (0.98) | 0.57 (1.36) |
| static_to_dynamic |  | 0.62 | 0.35 (0.56) | 0.35 (0.57) | 0.36 (0.58)† | 0.34 (0.55) |
| dynamic_to_static |  | 0.23 | 0.32 (1.41) | 0.44 (1.93)† | 0.24 (1.04)† | 0.39 (1.72)† |
| mutable_to_const | 1,024 | 0.63 | 0.62 (0.98) | 0.43 (0.68) | 0.43 (0.68) | 0.43 (0.68) |
| pass_noinline | 1,024 | 0.69 | 0.69 (1.01) | 0.68 (1.00) | 0.69 (1.00) | 0.69 (1.01) |
| pass_inline | 1,024 | 0.55 | 0.66 (1.19) | 0.62 (1.12) | 0.65 (1.17) | 0.62 (1.12) |
| pass_static_noinline |  | 0.70 | 0.69 (1.00) | 0.68 (0.98) | 0.69 (1.00) | 0.69 (1.00) |
| divide_and_conquer | 8 | 36.60 | 32.96 (0.90) | 38.27 (1.05) | 34.19 (0.93) | 38.27 (1.05) |
| divide_and_conquer | 1,024 | 4756 | 4426 (0.93) | 5220 (1.10) | 4090 (0.86) | 5226 (1.10) |
| divide_and_conquer | 1,000,000 | 4390820 | 4079050 (0.93) | 4553930 (1.04) | 4050170 (0.92) | 4470300 (1.02) |
| bytes_sum | 1,024 | 1414 | 1416 (1.00) | 1401 (0.99) |  | 1420 (1.00) |
| bytes_sum | 1,000,000 | 1402120 | 1395580 (1.00) | 1401390 (1.00) |  | 1414400 (1.01) |
| size_bytes | 1,024 | 0.68 | 0.69 (1.01) | 0.68 (1.00) | 0.69 (1.02) | 0.70 (1.02) |
| front_back | 1,024 | 0.64 | 0.48 (0.75) | 0.47 (0.73) | 0.47 (0.74) | 0.46 (0.73) |

Summary against lumex C++11 (geometric mean of the ratio over all scenarios; scenarios slower or faster by more than 10 %):

| variant | scenarios | geometric mean | slower | faster |
| --- | ---: | ---: | ---: | ---: |
| lumex C++20 | 36 | 0.972 | 4 | 5 |
| std::span C++20 | 36 | 1.001 | 7 | 8 |
| boost::span C++11 | 34 | 0.971 | 5 | 8 |
| boost::span C++20 | 36 | 0.993 | 8 | 8 |

The columns of one C++ standard compared with each other (time of the first divided by the time of the second; the span of this library is faster below 1.00): geometric mean over all scenarios, the best and the worst scenario of the first:

| compared | scenarios | geometric mean | best | worst |
| --- | ---: | ---: | --- | --- |
| lumex C++20 / std::span C++20 | 36 | 0.971 | ctor_std_array 0.55x | ctor_ptr_size (1,024) 1.47x |
| lumex C++20 / boost::span C++20 | 36 | 0.979 | ctor_std_array 0.71x | ctor_ptr_size (1,024) 1.47x |
| lumex C++11 / boost::span C++11 | 34 | 1.030 | ctor_c_array 0.55x | static_to_dynamic 1.72x |

- lumex C++20 is slower by more than 10 % in: dynamic_to_static 1.41x, sum_index (8) 1.24x, pass_inline (1,024) 1.19x, sum_range_for (8) 1.13x
- std::span C++20 is slower by more than 10 % in: dynamic_to_static 1.93x, ctor_std_array 1.85x, static_first 1.49x, static_first_of_dynamic (1,024) 1.46x, sum_range_for (8) 1.32x, sum_index (8) 1.27x, pass_inline (1,024) 1.12x
- boost::span C++11 is slower by more than 10 % in: ctor_c_array 1.81x, static_first 1.36x, sum_index (8) 1.23x, last (1,024) 1.21x, pass_inline (1,024) 1.17x
- boost::span C++20 is slower by more than 10 % in: dynamic_to_static 1.72x, ctor_std_array 1.42x, static_first_of_dynamic (1,024) 1.36x, static_first 1.35x, sum_index (8) 1.28x, sum_range_for (8) 1.15x, pass_inline (1,024) 1.12x, ctor_vector 1.12x

### Conversions to and from std::span (lumex C++20 only; the ratio is to `copy` of the same variant)

| scenario | size | lumex C++20 |
| --- | --- | ---: |
| from_std_span | 1,024 | 0.43 (1.24) |
| to_std_span | 1,024 | 0.65 (1.88) |
| bytes_to_std_span | 1,024 | 0.43 (1.23) |

## clang

| scenario | size | lumex C++11 | lumex C++20 | std::span C++20 | boost::span C++11 | boost::span C++20 |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| ctor_ptr_size | 1,024 | 0.34† | 0.34 (1.01)† | 0.35 (1.04)† | 0.35 (1.01)† | 0.35 (1.01)† |
| ctor_c_array |  | 0.35† | 0.35 (1.00)† | 0.35 (1.02)† | 0.35 (1.00)† | 0.35 (1.00)† |
| ctor_std_array |  | 0.35† | 0.35 (1.00)† | 0.35 (1.01)† | 0.35 (1.01)† | 0.35 (0.99)† |
| ctor_vector |  | 0.46† | 0.46 (1.01)† | 0.54 (1.17)† | 0.47 (1.03)† | 0.49 (1.06)† |
| ctor_static_array |  | 0.34 | 0.35 (1.02) | 0.35 (1.01) | 0.35 (1.01) | 0.34 (1.00) |
| copy | 1,024 | 0.35 | 0.35 (1.00) | 0.34 (0.99) | 0.35 (0.99) | 0.34 (0.99) |
| sum_range_for | 8 | 3.11 | 3.15 (1.01) | 3.15 (1.01) | 3.14 (1.01) | 3.10 (1.00) |
| sum_range_for | 1,024 | 131.8 | 130.9 (0.99) | 158.6 (1.20) | 137.6 (1.04) | 160.3 (1.22) |
| sum_range_for | 1,000,000 | 131586 | 132312 (1.01) | 132291 (1.01) | 129890 (0.99) | 133170 (1.01) |
| sum_index | 8 | 3.17 | 3.21 (1.01) | 3.14 (0.99) | 3.14 (0.99) | 3.15 (0.99) |
| sum_index | 1,024 | 152.4 | 148.2 (0.97) | 156.7 (1.03) | 132.5 (0.87) | 153.2 (1.01) |
| sum_index | 1,000,000 | 133607 | 134280 (1.01) | 132451 (0.99) | 130426 (0.98) | 132406 (0.99) |
| sum_iterators | 8 | 3.16 | 3.12 (0.99) | 3.11 (0.98) | 3.13 (0.99) | 3.14 (0.99) |
| sum_iterators | 1,024 | 135.6 | 137.3 (1.01) | 136.4 (1.01) | 166.0 (1.22) | 137.1 (1.01) |
| sum_iterators | 1,000,000 | 135118 | 133782 (0.99) | 130784 (0.97) | 133321 (0.99) | 132770 (0.98) |
| sum_static_range_for |  | 2.21 | 2.20 (0.99) | 2.20 (1.00) | 2.18 (0.99) | 2.18 (0.99) |
| first | 1,024 | 0.69 | 0.69 (1.01) | 0.69 (1.00) | 0.69 (1.01) | 0.69 (1.01) |
| last | 1,024 | 0.76 | 0.77 (1.01) | 0.82 (1.09) | 0.76 (1.00) | 0.77 (1.01) |
| subspan | 1,024 | 0.69 | 0.70 (1.01) | 0.69 (1.00) | 0.70 (1.01) | 0.68 (0.99) |
| subspan_rest | 1,024 | 0.91 | 0.97 (1.08) | 0.83 (0.92) | 0.83 (0.92) | 0.82 (0.91) |
| static_first |  | 0.46 | 0.46 (1.00) | 0.46 (1.00) | 0.54 (1.17)† | 0.46 (1.00) |
| static_subspan |  | 0.57 | 0.56 (0.99) | 0.67 (1.18) | 0.53 (0.94) | 0.56 (0.99) |
| static_first_of_dynamic | 1,024 | 0.46 | 0.47 (1.01) | 0.47 (1.01) | 0.46 (1.01) | 0.46 (0.99) |
| static_to_dynamic |  | 0.66 | 0.64 (0.97) | 0.35 (0.53)† | 0.35 (0.53)† | 0.35 (0.53)† |
| dynamic_to_static |  | 0.35 | 0.34 (0.99) | 0.35 (1.00) | 0.35 (1.00) | 0.34 (0.98) |
| mutable_to_const | 1,024 | 0.35† | 0.35 (0.99)† | 0.57 (1.64) | 0.35 (1.00)† | 0.58 (1.68) |
| pass_noinline | 1,024 | 0.68 | 0.70 (1.02) | 0.68 (1.00) | 0.68 (1.00) | 0.70 (1.02) |
| pass_inline | 1,024 | 0.68 | 0.68 (1.00) | 0.69 (1.02) | 0.68 (1.00) | 0.68 (1.00) |
| pass_static_noinline |  | 0.48 | 0.48 (0.99) | 0.49 (1.01) | 0.49 (1.01) | 0.49 (1.01) |
| divide_and_conquer | 8 | 27.14 | 22.06 (0.81) | 27.91 (1.03) | 19.66 (0.72) | 27.92 (1.03) |
| divide_and_conquer | 1,024 | 3297 | 2536 (0.77) | 3427 (1.04) | 2589 (0.79) | 3435 (1.04) |
| divide_and_conquer | 1,000,000 | 3022540 | 2481330 (0.82) | 2835890 (0.94) | 2658500 (0.88) | 2931950 (0.97) |
| bytes_sum | 1,024 | 714.8 | 705.8 (0.99) | 703.6 (0.98) |  | 712.5 (1.00) |
| bytes_sum | 1,000,000 | 715265 | 711573 (0.99) | 704777 (0.99) |  | 701366 (0.98) |
| size_bytes | 1,024 | 0.70 | 0.69 (1.00) | 0.70 (1.00) | 0.70 (1.00) | 0.69 (1.00) |
| front_back | 1,024 | 0.51† | 0.52 (1.02)† | 0.47 (0.93) | 0.47 (0.92) | 0.47 (0.92) |

Summary against lumex C++11 (geometric mean of the ratio over all scenarios; scenarios slower or faster by more than 10 %):

| variant | scenarios | geometric mean | slower | faster |
| --- | ---: | ---: | ---: | ---: |
| lumex C++20 | 36 | 0.984 | 0 | 3 |
| std::span C++20 | 36 | 1.010 | 4 | 1 |
| boost::span C++11 | 34 | 0.963 | 2 | 5 |
| boost::span C++20 | 36 | 0.999 | 2 | 1 |

The columns of one C++ standard compared with each other (time of the first divided by the time of the second; the span of this library is faster below 1.00): geometric mean over all scenarios, the best and the worst scenario of the first:

| compared | scenarios | geometric mean | best | worst |
| --- | ---: | ---: | --- | --- |
| lumex C++20 / std::span C++20 | 36 | 0.975 | mutable_to_const (1,024) 0.61x | static_to_dynamic 1.83x |
| lumex C++20 / boost::span C++20 | 36 | 0.985 | mutable_to_const (1,024) 0.59x | static_to_dynamic 1.83x |
| lumex C++11 / boost::span C++11 | 34 | 1.038 | sum_iterators (1,024) 0.82x | static_to_dynamic 1.87x |

- std::span C++20 is slower by more than 10 % in: mutable_to_const (1,024) 1.64x, sum_range_for (1,024) 1.20x, static_subspan 1.18x, ctor_vector 1.17x
- boost::span C++11 is slower by more than 10 % in: sum_iterators (1,024) 1.22x, static_first 1.17x
- boost::span C++20 is slower by more than 10 % in: mutable_to_const (1,024) 1.68x, sum_range_for (1,024) 1.22x

### Conversions to and from std::span (lumex C++20 only; the ratio is to `copy` of the same variant)

| scenario | size | lumex C++20 |
| --- | --- | ---: |
| from_std_span | 1,024 | 0.35 (1.01) |
| to_std_span | 1,024 | 0.35 (1.00) |
| bytes_to_std_span | 1,024 | 0.34 (0.99) |

## Compile time

- compiler gcc: g++ (GCC) 13.2.0
- compiler clang: clang version 23.1.0

Wall time of `-fsyntax-only -O0` of `compile_time.cpp` (includes the span header, builds spans of an array, a `std::array` and a `std::vector`, the subviews and `as_bytes`), median of the repeats, in milliseconds. `no span` is the same unit with a stub instead of a span: the cost of the shared headers.

| toolchain | no span C++11 | no span C++20 | lumex C++11 | lumex C++20 | std::span C++20 | boost::span C++11 | boost::span C++20 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| gcc | 90 | 192 | 218 | 577 | 236 | 215 | 382 |
| clang | 210 | 304 | 259 | 553 | 404 | 252 | 392 |

