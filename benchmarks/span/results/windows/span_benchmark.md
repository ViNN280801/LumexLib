# span benchmark results

- machine="12th Gen Intel(R) Core(TM) i9-12900H" os="Windows 11" logical_cpus=20 python=3.12.10
- passes=5 cpu=4
- variant msvc lumex_cxx11: compiler="MSVC 1951" library="MSVC STL" cplusplus=201402
- variant msvc lumex_cxx20: compiler="MSVC 1951" library="MSVC STL" cplusplus=202002
- variant msvc lumex_checks_cxx11: compiler="MSVC 1951" library="MSVC STL" cplusplus=201402
- variant msvc lumex_checks_cxx20: compiler="MSVC 1951" library="MSVC STL" cplusplus=202002
- variant msvc std_cxx20: compiler="MSVC 1951" library="MSVC STL" cplusplus=202002
- variant msvc boost_cxx11: compiler="MSVC 1951" library="MSVC STL" cplusplus=201402
- variant msvc boost_cxx20: compiler="MSVC 1951" library="MSVC STL" cplusplus=202002
- variant clang-cl lumex_cxx11: compiler="Clang 21.1.5" library="MSVC STL" cplusplus=201402
- variant clang-cl lumex_cxx20: compiler="Clang 21.1.5" library="MSVC STL" cplusplus=202002
- variant clang-cl lumex_checks_cxx11: compiler="Clang 21.1.5" library="MSVC STL" cplusplus=201402
- variant clang-cl lumex_checks_cxx20: compiler="Clang 21.1.5" library="MSVC STL" cplusplus=202002
- variant clang-cl std_cxx20: compiler="Clang 21.1.5" library="MSVC STL" cplusplus=202002
- variant clang-cl boost_cxx11: compiler="Clang 21.1.5" library="MSVC STL" cplusplus=201402
- variant clang-cl boost_cxx20: compiler="Clang 21.1.5" library="MSVC STL" cplusplus=202002

Time per operation in nanoseconds: the lowest median of 15 repetitions over all passes (the best pass), and in parentheses the ratio to the span of this library at C++11 (below 1.00 is faster than that). The unit of an operation is the description of the scenario. A dagger marks a value whose slowest pass was more than 25 % slower than the best one: the processes of that executable do not agree (code and stack placement), so differences inside that band are noise.

## msvc

| scenario | size | lumex C++11 | lumex C++20 | lumex checks C++11 | lumex checks C++20 | std::span C++20 | boost::span C++11 | boost::span C++20 |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| ctor_ptr_size | 1,024 | 1.75 | 1.75 (1.00) | 1.74 (1.00) | 1.78 (1.02) | 1.77 (1.01) | 1.73 (0.99) | 1.76 (1.01) |
| ctor_c_array |  | 1.73 | 1.73 (1.00) | 1.74 (1.01) | 1.76 (1.02) | 1.76 (1.02) | 1.74 (1.00)† | 1.75 (1.01) |
| ctor_std_array |  | 1.74 | 1.73 (0.99) | 1.74 (1.00) | 1.76 (1.01) | 1.73 (1.00) | 1.72 (0.99)† | 1.76 (1.01) |
| ctor_vector |  | 1.74 | 1.74 (1.00) | 1.76 (1.01) | 1.77 (1.01) | 1.74 (1.00) | 1.75 (1.01)† | 1.76 (1.01) |
| ctor_static_array |  | 1.69 | 1.68 (0.99) | 1.74 (1.03) | 1.70 (1.01) | 1.68 (0.99) | 1.69 (1.00)† | 1.71 (1.01) |
| copy | 1,024 | 3.47 | 3.44 (0.99) | 3.51 (1.01) | 3.50 (1.01) | 3.46 (0.99) | 3.46 (1.00) | 3.50 (1.01) |
| sum_range_for | 8 | 2.49 | 2.49 (1.00) | 2.51 (1.01) | 2.50 (1.01) | 2.51 (1.01) | 2.52 (1.01) | 2.49 (1.00) |
| sum_range_for | 1,024 | 145.0 | 145.2 (1.00) | 146.5 (1.01) | 145.8 (1.01) | 146.1 (1.01) | 146.4 (1.01) | 146.3 (1.01) |
| sum_range_for | 1,000,000 | 131110 | 132891 (1.01) | 132854 (1.01) | 131507 (1.00) | 132170 (1.01) | 132493 (1.01) | 132338 (1.01) |
| sum_index | 8 | 3.02 | 2.25 (0.75) | 3.03 (1.00) | 2.37 (0.78) | 2.25 (0.75) | 3.16 (1.05) | 2.25 (0.74) |
| sum_index | 1,024 | 243.5 | 243.7 (1.00) | 242.7 (1.00) | 246.9 (1.01) | 245.1 (1.01) | 244.5 (1.00) | 245.1 (1.01) |
| sum_index | 1,000,000 | 238462 | 240008 (1.01) | 240650 (1.01) | 243870 (1.02) | 240023 (1.01) | 240827 (1.01)† | 244270 (1.02) |
| sum_iterators | 8 | 3.11 | 2.29 (0.74) | 2.99 (0.96) | 2.28 (0.73) | 2.27 (0.73) | 2.52 (0.81) | 2.52 (0.81) |
| sum_iterators | 1,024 | 246.0 | 246.0 (1.00) | 245.4 (1.00) | 246.6 (1.00) | 243.5 (0.99) | 146.0 (0.59) | 145.4 (0.59) |
| sum_iterators | 1,000,000 | 240352 | 240549 (1.00) | 241311 (1.00) | 244007 (1.02) | 240936 (1.00) | 132798 (0.55) | 132917 (0.55) |
| sum_static_range_for |  | 3.19 | 2.98 (0.93) | 3.24 (1.01) | 3.00 (0.94) | 2.96 (0.93) | 3.17 (0.99) | 2.96 (0.93) |
| first | 1,024 | 3.52 | 3.49 (0.99) | 3.50 (1.00) | 3.54 (1.01) | 3.51 (1.00) | 3.51 (1.00) | 3.52 (1.00) |
| last | 1,024 | 3.51 | 3.51 (1.00) | 3.54 (1.01) | 3.57 (1.02) | 3.52 (1.00) | 3.52 (1.00) | 3.55 (1.01) |
| subspan | 1,024 | 3.49 | 3.49 (1.00) | 3.50 (1.00) | 3.52 (1.01) | 3.47 (0.99) | 3.52 (1.01) | 3.51 (1.00) |
| subspan_rest | 1,024 | 3.53 | 3.51 (0.99) | 3.56 (1.01) | 3.55 (1.01) | 3.51 (0.99) | 3.54 (1.00) | 3.53 (1.00) |
| static_first |  | 3.36 | 3.34 (0.99) | 3.39 (1.01) | 3.41 (1.01) | 3.33 (0.99) | 3.36 (1.00) | 3.39 (1.01) |
| static_subspan |  | 3.40 | 3.40 (1.00) | 3.35 (0.98) | 3.47 (1.02) | 3.28 (0.96) | 3.43 (1.01) | 3.40 (1.00) |
| static_first_of_dynamic | 1,024 | 3.34 | 3.35 (1.00) | 3.11 (0.93) | 3.20 (0.96) | 3.34 (1.00) | 3.41 (1.02) | 3.40 (1.02) |
| static_to_dynamic |  | 3.45 | 3.46 (1.00) | 3.52 (1.02) | 3.54 (1.03) | 3.49 (1.01) | 3.53 (1.02) | 3.51 (1.02) |
| dynamic_to_static |  | 3.32 | 3.27 (0.99) | 3.47 (1.05) | 3.47 (1.04) | 3.29 (0.99) | 3.43 (1.03) | 3.29 (0.99) |
| mutable_to_const | 1,024 | 3.47 | 3.44 (0.99) | 3.50 (1.01) | 3.48 (1.00) | 3.48 (1.00) | 3.48 (1.00) | 3.50 (1.01) |
| pass_noinline | 1,024 | 1.50 | 1.50 (1.00) | 1.53 (1.02) | 1.56 (1.04) | 1.48 (0.99) | 1.53 (1.02) | 1.52 (1.02) |
| pass_inline | 1,024 | 1.73 | 1.73 (1.00) | 1.75 (1.01) | 1.76 (1.02) | 1.74 (1.00) | 1.74 (1.01) | 1.75 (1.01) |
| pass_static_noinline |  | 1.78 | 1.80 (1.01) | 1.80 (1.01) | 1.80 (1.01) | 1.80 (1.01) | 1.83 (1.02) | 1.81 (1.02) |
| divide_and_conquer | 8 | 22.85 | 23.15 (1.01) | 23.08 (1.01) | 23.34 (1.02) | 23.33 (1.02) | 23.16 (1.01) | 23.22 (1.02) |
| divide_and_conquer | 1,024 | 3102 | 3005 (0.97) | 3073 (0.99) | 3224 (1.04) | 2985 (0.96) | 3171 (1.02) | 3054 (0.98) |
| divide_and_conquer | 1,000,000 | 3109260 | 2970540 (0.96) | 2985560 (0.96) | 3149900 (1.01) | 2981720 (0.96) | 3126880 (1.01) | 2945240 (0.95) |
| bytes_sum | 1,024 | 974.3 | 974.4 (1.00) | 978.7 (1.00) | 990.1 (1.02) | 972.1 (1.00) |  | 983.2 (1.01) |
| bytes_sum | 1,000,000 | 955069 | 957903 (1.00) | 967312 (1.01) | 973331 (1.02) | 963528 (1.01) |  | 968019 (1.01) |
| size_bytes | 1,024 | 1.73 | 1.73 (1.00) | 1.75 (1.01) | 1.74 (1.01) | 1.72 (1.00) | 1.73 (1.00) | 1.74 (1.01) |
| front_back | 1,024 | 1.74 | 1.75 (1.00) | 1.77 (1.02) | 1.76 (1.01) | 1.75 (1.01) | 1.79 (1.03) | 1.76 (1.01) |

Summary against lumex C++11 (geometric mean of the ratio over all scenarios; scenarios slower or faster by more than 10 %):

| variant | scenarios | geometric mean | slower | faster |
| --- | ---: | ---: | ---: | ---: |
| lumex C++20 | 36 | 0.979 | 0 | 2 |
| lumex checks C++11 | 36 | 1.003 | 0 | 0 |
| lumex checks C++20 | 36 | 0.995 | 0 | 2 |
| std::span C++20 | 36 | 0.980 | 0 | 2 |
| boost::span C++11 | 34 | 0.970 | 0 | 3 |
| boost::span C++20 | 36 | 0.959 | 0 | 4 |

The columns of one C++ standard compared with each other (time of the first divided by the time of the second; the span of this library is faster below 1.00): geometric mean over all scenarios, the best and the worst scenario of the first:

| compared | scenarios | geometric mean | best | worst |
| --- | ---: | ---: | --- | --- |
| lumex C++20 / std::span C++20 | 36 | 0.999 | ctor_c_array 0.98x | static_subspan 1.04x |
| lumex C++20 / boost::span C++20 | 36 | 1.021 | sum_iterators (8) 0.91x | sum_iterators (1,000,000) 1.81x |
| lumex C++11 / boost::span C++11 | 34 | 1.031 | sum_index (8) 0.96x | sum_iterators (1,000,000) 1.81x |
| lumex checks C++11 / lumex C++11 | 36 | 1.003 | static_first_of_dynamic (1,024) 0.93x | dynamic_to_static 1.05x |
| lumex checks C++20 / lumex C++20 | 39 | 1.016 | static_first_of_dynamic (1,024) 0.95x | divide_and_conquer (1,024) 1.07x |


### Conversions to and from std::span (lumex C++20 only; the ratio is to `copy` of the same variant)

| scenario | size | lumex C++20 |
| --- | --- | ---: |
| from_std_span | 1,024 | 3.46 (1.01) |
| to_std_span | 1,024 | 3.46 (1.01) |
| bytes_to_std_span | 1,024 | 3.46 (1.01) |

## clang-cl

| scenario | size | lumex C++11 | lumex C++20 | lumex checks C++11 | lumex checks C++20 | std::span C++20 | boost::span C++11 | boost::span C++20 |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| ctor_ptr_size | 1,024 | 0.25 | 0.24 (0.94) | 0.26 (1.01) | 0.24 (0.95) | 0.24 (0.95)† | 0.24 (0.94) | 0.23 (0.92) |
| ctor_c_array |  | 0.25 | 0.25 (1.00) | 0.24 (0.98) | 0.24 (0.99) | 0.24 (0.99) | 0.24 (0.98) | 0.25 (1.02) |
| ctor_std_array |  | 0.25 | 0.25 (1.00) | 0.24 (0.98) | 0.24 (1.00) | 0.25 (1.03) | 0.24 (0.99) | 0.25 (1.01) |
| ctor_vector |  | 0.36† | 0.34 (0.94) | 0.37 (1.00) | 0.34 (0.93) | 0.53 (1.45) | 0.34 (0.94) | 0.52 (1.44) |
| ctor_static_array |  | 0.22 | 0.22 (1.02) | 0.22 (1.01) | 0.22 (1.01) | 0.22 (1.00) | 0.22 (1.00) | 0.22 (1.01) |
| copy | 1,024 | 0.25 | 0.25 (1.02) | 0.24 (0.98) | 0.25 (1.01) | 0.25 (1.00) | 0.30 (1.20)† | 0.25 (0.99) |
| sum_range_for | 8 | 2.21 | 2.30 (1.04) | 2.19 (0.99) | 2.33 (1.06) | 2.22 (1.00) | 2.23 (1.01) | 2.23 (1.01) |
| sum_range_for | 1,024 | 94.84 | 102.6 (1.08) | 94.16 (0.99) | 100.7 (1.06) | 93.78 (0.99) | 98.92 (1.04) | 116.7 (1.23) |
| sum_range_for | 1,000,000 | 92452 | 95048 (1.03) | 91093 (0.99) | 94917 (1.03) | 91638 (0.99) | 93286 (1.01) | 94303 (1.02) |
| sum_index | 8 | 2.25 | 2.19 (0.98) | 2.22 (0.99) | 2.20 (0.98) | 2.25 (1.00) | 2.28 (1.02) | 2.16 (0.96) |
| sum_index | 1,024 | 118.3 | 97.44 (0.82) | 118.8 (1.00) | 97.23 (0.82) | 117.9 (1.00) | 104.7 (0.88) | 97.87 (0.83) |
| sum_index | 1,000,000 | 93649 | 93820 (1.00) | 94715 (1.01) | 92906 (0.99) | 94720 (1.01) | 96149 (1.03) | 91793 (0.98) |
| sum_iterators | 8 | 2.22 | 2.27 (1.02) | 2.20 (0.99) | 2.33 (1.05) | 2.22 (1.00) | 2.22 (1.00) | 2.23 (1.00) |
| sum_iterators | 1,024 | 94.38 | 100.9 (1.07) | 94.03 (1.00) | 101.9 (1.08) | 93.81 (0.99) | 99.35 (1.05) | 118.6 (1.26) |
| sum_iterators | 1,000,000 | 91346 | 95088 (1.04) | 91769 (1.00) | 94681 (1.04) | 91826 (1.01) | 92512 (1.01) | 95073 (1.04) |
| sum_static_range_for |  | 1.58 | 1.57 (0.99) | 1.58 (1.00) | 1.57 (1.00) | 1.59 (1.01) | 1.59 (1.00) | 1.52 (0.96) |
| first | 1,024 | 0.55 | 0.51 (0.94) | 0.56 (1.03) | 0.57 (1.04) | 0.50 (0.92) | 0.54 (0.98) | 0.50 (0.91) |
| last | 1,024 | 0.65 | 0.61 (0.95) | 0.67 (1.04) | 0.66 (1.01) | 0.56 (0.86) | 0.61 (0.93) | 0.56 (0.87) |
| subspan | 1,024 | 0.51 | 0.55 (1.06) | 0.75 (1.47) | 0.76 (1.49) | 0.51 (0.99) | 0.54 (1.05) | 0.51 (0.98) |
| subspan_rest | 1,024 | 0.62 | 0.63 (1.02) | 0.65 (1.06) | 0.66 (1.07) | 0.64 (1.03) | 0.66 (1.07) | 0.65 (1.06) |
| static_first |  | 0.33 | 0.35 (1.04) | 0.33 (0.98) | 0.35 (1.05) | 0.35 (1.04) | 0.33 (0.99) | 0.33 (0.98) |
| static_subspan |  | 0.52 | 0.38 (0.73) | 0.51 (0.98) | 0.38 (0.73) | 0.38 (0.73) | 0.40 (0.76) | 0.38 (0.72) |
| static_first_of_dynamic | 1,024 | 0.35 | 0.33 (0.96) | 0.42 (1.20) | 0.42 (1.21) | 0.33 (0.96) | 0.33 (0.96) | 0.33 (0.96) |
| static_to_dynamic |  | 0.26† | 0.30 (1.12)† | 0.29 (1.10) | 0.27 (1.03)† | 0.27 (1.03)† | 0.27 (1.01) | 0.27 (1.01)† |
| dynamic_to_static |  | 0.24 | 0.24 (1.00) | 0.25 (1.06) | 0.25 (1.08) | 0.24 (1.00) | 0.23 (0.99) | 0.23 (0.99) |
| mutable_to_const | 1,024 | 0.25 | 0.25 (1.01) | 0.25 (1.00) | 0.29 (1.15) | 0.25 (1.01)† | 0.33 (1.32) | 0.24 (0.98) |
| pass_noinline | 1,024 | 0.52 | 0.51 (0.99) | 0.51 (1.00) | 0.70 (1.35) | 0.51 (1.00) | 0.72 (1.39) | 0.51 (0.98) |
| pass_inline | 1,024 | 0.48 | 0.48 (1.01) | 0.48 (1.01) | 0.50 (1.05) | 0.48 (1.01) | 0.49 (1.03) | 0.47 (1.00) |
| pass_static_noinline |  | 0.35 | 0.36 (1.01) | 0.70 (1.99) | 0.35 (1.00) | 0.49 (1.40) | 0.35 (0.99) | 0.62 (1.75) |
| divide_and_conquer | 8 | 26.43 | 29.96 (1.13) | 30.12 (1.14) | 33.39 (1.26) | 27.30 (1.03) | 29.22 (1.11) | 26.57 (1.01) |
| divide_and_conquer | 1,024 | 3760 | 3951 (1.05) | 4071 (1.08) | 4458 (1.19) | 3631 (0.97) | 3941 (1.05) | 3784 (1.01) |
| divide_and_conquer | 1,000,000 | 3729300 | 3912460 (1.05) | 3993210 (1.07) | 4295490 (1.15) | 3647340 (0.98) | 3931570 (1.05) | 3727880 (1.00) |
| bytes_sum | 1,024 | 520.1 | 512.1 (0.98) | 516.5 (0.99) | 512.6 (0.99) | 515.0 (0.99) |  | 513.5 (0.99) |
| bytes_sum | 1,000,000 | 505819 | 492977 (0.97) | 495795 (0.98) | 493922 (0.98) | 496225 (0.98) |  | 493038 (0.97) |
| size_bytes | 1,024 | 0.44 | 0.44 (1.01) | 0.43 (0.99) | 0.44 (1.00) | 0.44 (1.00) | 0.43 (0.98) | 0.43 (0.99) |
| front_back | 1,024 | 0.36 | 0.33 (0.89) | 0.36 (1.00) | 0.36 (0.99) | 0.33 (0.92) | 0.33 (0.91) | 0.33 (0.92) |

Summary against lumex C++11 (geometric mean of the ratio over all scenarios; scenarios slower or faster by more than 10 %):

| variant | scenarios | geometric mean | slower | faster |
| --- | ---: | ---: | ---: | ---: |
| lumex C++20 | 36 | 0.995 | 2 | 3 |
| lumex checks C++11 | 36 | 1.047 | 4 | 0 |
| lumex checks C++20 | 36 | 1.042 | 7 | 2 |
| std::span C++20 | 36 | 1.002 | 2 | 2 |
| boost::span C++11 | 34 | 1.015 | 4 | 2 |
| boost::span C++20 | 36 | 1.010 | 4 | 3 |

The columns of one C++ standard compared with each other (time of the first divided by the time of the second; the span of this library is faster below 1.00): geometric mean over all scenarios, the best and the worst scenario of the first:

| compared | scenarios | geometric mean | best | worst |
| --- | ---: | ---: | --- | --- |
| lumex C++20 / std::span C++20 | 36 | 0.993 | ctor_vector 0.64x | divide_and_conquer (8) 1.10x |
| lumex C++20 / boost::span C++20 | 36 | 0.985 | pass_static_noinline 0.58x | divide_and_conquer (8) 1.13x |
| lumex C++11 / boost::span C++11 | 34 | 0.985 | pass_noinline (1,024) 0.72x | static_subspan 1.31x |
| lumex checks C++11 / lumex C++11 | 36 | 1.047 | ctor_c_array 0.98x | pass_static_noinline 1.99x |
| lumex checks C++20 / lumex C++20 | 39 | 1.044 | static_to_dynamic 0.91x | subspan (1,024) 1.40x |

- lumex C++20 is slower by more than 10 % in: divide_and_conquer (8) 1.13x, static_to_dynamic 1.12x
- lumex checks C++11 is slower by more than 10 % in: pass_static_noinline 1.99x, subspan (1,024) 1.47x, static_first_of_dynamic (1,024) 1.20x, divide_and_conquer (8) 1.14x
- lumex checks C++20 is slower by more than 10 % in: subspan (1,024) 1.49x, pass_noinline (1,024) 1.35x, divide_and_conquer (8) 1.26x, static_first_of_dynamic (1,024) 1.21x, divide_and_conquer (1,024) 1.19x, divide_and_conquer (1,000,000) 1.15x, mutable_to_const (1,024) 1.15x
- std::span C++20 is slower by more than 10 % in: ctor_vector 1.45x, pass_static_noinline 1.40x
- boost::span C++11 is slower by more than 10 % in: pass_noinline (1,024) 1.39x, mutable_to_const (1,024) 1.32x, copy (1,024) 1.20x, divide_and_conquer (8) 1.11x
- boost::span C++20 is slower by more than 10 % in: pass_static_noinline 1.75x, ctor_vector 1.44x, sum_iterators (1,024) 1.26x, sum_range_for (1,024) 1.23x

### Conversions to and from std::span (lumex C++20 only; the ratio is to `copy` of the same variant)

| scenario | size | lumex C++20 |
| --- | --- | ---: |
| from_std_span | 1,024 | 0.26 (1.02) |
| to_std_span | 1,024 | 0.25 (1.00) |
| bytes_to_std_span | 1,024 | 0.26 (1.02) |

