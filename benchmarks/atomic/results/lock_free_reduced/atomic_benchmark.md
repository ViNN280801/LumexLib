# Atomic shared pointer benchmark results

- Run 1: 12th Gen Intel(R) Core(TM) i7-12700K, 20 logical CPUs, governor `powersave`, kernel `6.1.170-1-generic` (Linux-6.1.170-1-generic-x86_64-with-AstraLinux-1.7_x86-64-1.7_x86-64); GCC 13.2 with libstdc++ 13, release build; 7 repetitions, 3 s pauses, 100 ms windows; measured 2026-10-09 in 5.6 minutes, load average 8.49 8.95 7.77 at the start and 16.38 11.73 9.23 at the end.

| Series | Selected implementation | Language mode | is_lock_free |
| --- | --- | ---: | ---: |
| LumexLib lock-based, C++11 | `lock_based_table_wait` | 201103 | no |
| LumexLib lock-based, C++20 | `lock_based_std_wait` | 202002 | no |
| LumexLib lock-free, C++11 | `lock_free_table_wait` | 201103 | yes |
| LumexLib lock-free, C++20 | `lock_free_std_wait` | 202002 | yes |
| LumexLib lock-free, C++20, deferred | `lock_free_deferred_std_wait` | 202002 | yes |
| LumexLib common name, C++20 (lock-free) | `common_std_wait` | 202002 | yes |
| LumexLib std-backed, C++20 (wraps std) | `std_backed_std_wait` | 202002 | no |
| std::atomic, libstdc++ 13 | `libstdc++ 13` | 202002 | no |
| boost::atomic_shared_ptr (spinlock), Boost 1_92 | `boost 1_92` | 202002 | no |

## Baseline

`std::atomic<uint64_t>::compare_exchange_strong` with all threads on one word, timed right before each series' block (ns per operation and thread, median). It is the same code every time, so the rows should agree; the ratio of each row to the mean of the rows shows how far the machine drifted between the blocks.

| Series | 1 | 2 | 4 | 8 | 12 | 16 | 20 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| LumexLib lock-based, C++11 | 7.74 | 31.55 | 78.97 | 157.34 | 265.75 | 305.07 | 359.12 |
| LumexLib lock-based, C++20 | 7.69 | 31.34 | 70.63 | 160.03 | 271.15 | 303.79 | 356.71 |
| LumexLib lock-free, C++11 | 7.58 | 28.18 | 76.59 | 159.10 | 259.13 | 309.94 | 361.46 |
| LumexLib lock-free, C++20 | 7.71 | 34.52 | 75.25 | 160.33 | 260.11 | 300.25 | 347.59 |
| LumexLib lock-free, C++20, deferred | 7.59 | 31.30 | 75.59 | 164.46 | 259.74 | 300.69 | 346.79 |
| LumexLib common name, C++20 (lock-free) | 7.68 | 25.80 | 74.23 | 161.32 | 259.10 | 301.03 | 339.98 |
| LumexLib std-backed, C++20 (wraps std) | 7.68 | 30.52 | 75.45 | 164.74 | 263.56 | 300.31 | 341.90 |
| std::atomic, libstdc++ 13 | 7.63 | 27.14 | 71.46 | 163.18 | 262.60 | 307.45 | 339.71 |
| boost::atomic_shared_ptr (spinlock), Boost 1_92 | 7.56 | 29.31 | 71.51 | 156.43 | 245.95 | 300.26 | 339.35 |
| LumexLib lock-based, C++11 / mean | 1.01 | 1.05 | 1.06 | 0.98 | 1.02 | 1.01 | 1.03 |
| LumexLib lock-based, C++20 / mean | 1.01 | 1.05 | 0.95 | 1.00 | 1.04 | 1.00 | 1.02 |
| LumexLib lock-free, C++11 / mean | 0.99 | 0.94 | 1.03 | 0.99 | 0.99 | 1.02 | 1.04 |
| LumexLib lock-free, C++20 / mean | 1.01 | 1.15 | 1.01 | 1.00 | 1.00 | 0.99 | 1.00 |
| LumexLib lock-free, C++20, deferred / mean | 0.99 | 1.04 | 1.02 | 1.02 | 1.00 | 0.99 | 1.00 |
| LumexLib common name, C++20 (lock-free) / mean | 1.00 | 0.86 | 1.00 | 1.00 | 0.99 | 0.99 | 0.98 |
| LumexLib std-backed, C++20 (wraps std) / mean | 1.00 | 1.02 | 1.01 | 1.02 | 1.01 | 0.99 | 0.98 |
| std::atomic, libstdc++ 13 / mean | 1.00 | 0.91 | 0.96 | 1.01 | 1.01 | 1.01 | 0.98 |
| boost::atomic_shared_ptr (spinlock), Boost 1_92 / mean | 0.99 | 0.98 | 0.96 | 0.97 | 0.94 | 0.99 | 0.97 |

## `load()`, contended

Ratio to the baseline of the same block (median, with the 25th and 75th percentiles of the repetitions; lower is better).

| Series | 1 | 2 | 4 | 8 | 12 | 16 | 20 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| LumexLib lock-based, C++11 | 2.4 (2.4-2.5) | 3.7 (3.5-4.6) | 3.5 (3.3-3.6) | 4.6 (4.4-5.1) | 5.1 (5.0-5.3) | 6.7 (6.2-7.1) | 7.7 (7.3-8.3) |
| LumexLib lock-based, C++20 | 2.2 (2.2-2.2) | 4.0 (3.7-4.2) | 4.2 (4.0-4.3) | 5.9 (5.7-6.1) | 7.1 (6.7-7.4) | 8.6 (8.2-9.5) | 10.5 (9.5-11.4) |
| LumexLib lock-free, C++11 | 2.6 (2.5-2.6) | 2.1 (1.9-2.7) | 2.1 (1.9-2.2) | 2.0 (1.9-2.0) | 2.0 (1.9-2.0) | 2.0 (2.0-2.1) | 2.2 (2.1-2.3) |
| LumexLib lock-free, C++20 | 2.6 (2.5-2.6) | 2.1 (2.0-2.5) | 2.0 (2.0-2.1) | 2.0 (1.9-2.0) | 2.0 (1.9-2.0) | 2.1 (2.0-2.1) | 2.3 (2.2-2.3) |
| LumexLib lock-free, C++20, deferred | 2.6 (2.5-2.6) | 2.1 (1.9-2.5) | 2.1 (2.0-2.3) | 2.0 (1.9-2.1) | 2.0 (1.9-2.0) | 2.1 (2.0-2.2) | 2.2 (2.2-2.3) |
| LumexLib common name, C++20 (lock-free) | 2.5 (2.5-2.5) | 2.5 (2.1-2.7) | 2.0 (2.0-2.1) | 2.0 (1.9-2.0) | 2.0 (2.0-2.1) | 2.1 (2.1-2.2) | 2.2 (2.1-2.3) |
| LumexLib std-backed, C++20 (wraps std) | 2.2 (2.2-2.2) | 3.4 (3.1-4.1) | 7.3 (7.1-7.6) | 14.7 (13.8-15.1) | 18.2 (17.7-19.7) | 22.1 (20.5-24.6) | 31.3 (28.3-31.7) |
| std::atomic, libstdc++ 13 | 2.2 (2.2-2.2) | 3.4 (3.0-4.2) | 7.7 (7.2-8.0) | 14.4 (14.2-15.3) | 18.2 (17.5-19.9) | 21.7 (20.9-24.6) | 26.2 (26.1-27.7) |
| boost::atomic_shared_ptr (spinlock), Boost 1_92 | 2.0 (2.0-2.0) | 1.1 (1.0-1.2) | 0.9 (0.9-1.0) | 0.9 (0.9-1.0) | 0.9 (0.8-0.9) | 1.0 (1.0-1.1) | 1.1 (1.1-1.3) |

## `store()`, contended

Ratio to the baseline of the same block (median, with the 25th and 75th percentiles of the repetitions; lower is better).

| Series | 1 | 2 | 4 | 8 | 12 | 16 | 20 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| LumexLib lock-based, C++11 | 2.9 (2.9-3.0) | 3.5 (2.9-4.1) | 5.0 (4.5-5.2) | 7.5 (7.1-7.9) | 7.3 (7.2-7.8) | 8.9 (8.7-9.5) | 9.8 (9.3-10.6) |
| LumexLib lock-based, C++20 | 2.4 (2.4-2.4) | 3.7 (3.3-3.8) | 5.8 (5.3-6.1) | 7.9 (7.7-8.3) | 8.5 (8.3-9.1) | 10.2 (9.9-10.8) | 11.9 (10.9-12.3) |
| LumexLib lock-free, C++11 | 5.9 (5.8-5.9) | 6.8 (6.5-7.9) | 5.0 (4.7-5.2) | 4.2 (4.1-4.5) | 4.0 (3.7-4.1) | 3.8 (3.6-4.1) | 4.0 (3.7-4.2) |
| LumexLib lock-free, C++20 | 5.8 (5.7-5.9) | 5.9 (5.8-6.8) | 4.9 (4.8-5.1) | 4.1 (4.1-4.3) | 3.9 (3.8-4.1) | 4.0 (3.7-4.1) | 3.9 (3.8-4.3) |
| LumexLib lock-free, C++20, deferred | 8.5 (8.5-8.6) | 9.4 (8.0-10.5) | 7.5 (7.2-8.2) | 4.7 (4.6-5.1) | 4.4 (3.8-4.5) | 4.4 (4.2-4.5) | 4.5 (4.2-4.8) |
| LumexLib common name, C++20 (lock-free) | 5.8 (5.7-5.9) | 6.7 (6.2-7.1) | 5.1 (4.9-5.5) | 4.4 (4.1-4.4) | 4.1 (3.8-4.1) | 3.9 (3.7-4.2) | 4.0 (3.7-4.2) |
| LumexLib std-backed, C++20 (wraps std) | 2.6 (2.6-2.6) | 2.3 (2.2-2.5) | 2.6 (2.3-3.3) | 8.0 (7.4-8.5) | 11.7 (11.2-12.1) | 14.4 (13.9-15.6) | 18.4 (17.7-19.2) |
| std::atomic, libstdc++ 13 | 2.6 (2.6-2.6) | 2.8 (2.6-2.9) | 2.6 (2.5-2.8) | 8.7 (8.7-9.5) | 11.7 (11.3-12.5) | 14.9 (13.9-16.0) | 17.7 (16.8-18.6) |
| boost::atomic_shared_ptr (spinlock), Boost 1_92 | 3.2 (3.1-3.2) | 2.5 (2.3-2.5) | 2.4 (2.2-2.4) | 2.8 (2.6-3.0) | 4.0 (3.5-4.3) | 3.5 (3.4-3.9) | 3.7 (3.3-3.9) |

## `exchange()`, contended

Ratio to the baseline of the same block (median, with the 25th and 75th percentiles of the repetitions; lower is better).

| Series | 1 | 2 | 4 | 8 | 12 | 16 | 20 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| LumexLib lock-based, C++11 | 2.7 (2.6-2.7) | 3.7 (2.8-4.3) | 3.7 (3.7-4.2) | 7.5 (7.2-8.0) | 7.5 (7.4-7.8) | 9.1 (8.8-9.6) | 10.0 (9.5-10.6) |
| LumexLib lock-based, C++20 | 2.4 (2.4-2.4) | 3.4 (3.3-3.9) | 5.3 (4.9-5.4) | 7.6 (7.1-8.0) | 8.6 (8.1-9.0) | 10.2 (9.7-10.9) | 11.9 (10.9-12.5) |
| LumexLib lock-free, C++11 | 7.0 (7.0-7.1) | 9.2 (7.7-10.0) | 6.4 (6.0-7.0) | 5.9 (5.8-6.1) | 6.0 (5.4-6.2) | 5.6 (5.5-6.1) | 5.9 (5.5-6.5) |
| LumexLib lock-free, C++20 | 7.0 (6.8-7.1) | 6.3 (4.9-7.1) | 6.3 (6.2-6.7) | 5.9 (5.8-5.9) | 6.0 (5.5-6.1) | 5.9 (5.7-6.0) | 5.8 (5.6-6.4) |
| LumexLib lock-free, C++20, deferred | 9.6 (9.3-9.6) | 10.3 (8.2-11.8) | 8.3 (8.3-9.4) | 6.9 (6.7-7.2) | 5.8 (5.6-6.3) | 6.1 (5.8-6.5) | 6.1 (5.7-6.7) |
| LumexLib common name, C++20 (lock-free) | 7.0 (6.8-7.0) | 8.4 (7.3-9.6) | 6.4 (6.3-6.8) | 6.0 (5.9-6.3) | 5.8 (5.5-6.0) | 6.1 (5.6-6.2) | 5.8 (5.7-6.1) |
| LumexLib std-backed, C++20 (wraps std) | 2.6 (2.5-2.6) | 2.8 (2.6-2.9) | 4.5 (4.4-5.0) | 11.4 (10.4-12.1) | 15.9 (15.1-16.9) | 18.5 (17.3-20.2) | 21.9 (21.3-23.4) |
| std::atomic, libstdc++ 13 | 2.6 (2.6-2.6) | 3.3 (2.8-3.7) | 5.6 (5.3-6.0) | 11.2 (10.9-12.1) | 16.4 (15.5-16.9) | 19.2 (17.7-20.1) | 24.3 (21.1-27.7) |
| boost::atomic_shared_ptr (spinlock), Boost 1_92 | 3.3 (3.2-3.3) | 2.3 (2.1-2.5) | 2.3 (2.1-2.5) | 2.5 (2.3-2.9) | 3.9 (3.1-4.9) | 3.6 (3.3-4.0) | 3.5 (3.2-3.8) |

## `compare_exchange_strong()`, contended

Ratio to the baseline of the same block (median, with the 25th and 75th percentiles of the repetitions; lower is better).

| Series | 1 | 2 | 4 | 8 | 12 | 16 | 20 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| LumexLib lock-based, C++11 | 6.2 (6.0-6.3) | 11.2 (10.0-12.8) | 13.9 (13.5-15.4) | 17.2 (16.2-17.8) | 16.8 (16.5-17.7) | 19.8 (19.4-20.9) | 21.6 (21.1-23.2) |
| LumexLib lock-based, C++20 | 5.7 (5.7-5.8) | 11.6 (11.4-13.6) | 15.0 (14.9-15.3) | 18.1 (17.9-19.4) | 20.7 (19.3-21.0) | 23.9 (23.3-25.8) | 27.3 (24.9-28.8) |
| LumexLib lock-free, C++11 | 9.8 (9.7-10.0) | 11.7 (9.7-13.9) | 11.6 (10.8-12.2) | 9.9 (9.8-10.4) | 8.5 (8.4-9.8) | 8.5 (8.1-8.8) | 8.7 (8.3-9.4) |
| LumexLib lock-free, C++20 | 9.7 (9.7-9.9) | 9.8 (8.7-12.3) | 11.4 (11.2-11.9) | 9.9 (9.7-10.1) | 8.8 (8.5-9.5) | 9.0 (8.3-9.3) | 8.5 (8.3-9.5) |
| LumexLib lock-free, C++20, deferred | 12.5 (12.2-12.6) | 10.4 (9.6-13.2) | 9.8 (9.6-10.8) | 8.4 (8.2-9.1) | 7.9 (7.5-8.3) | 8.0 (7.8-8.5) | 8.2 (7.8-9.0) |
| LumexLib common name, C++20 (lock-free) | 9.8 (9.6-9.9) | 12.7 (10.7-15.0) | 11.4 (11.1-12.2) | 9.8 (9.6-10.4) | 8.9 (8.5-9.4) | 8.6 (8.2-9.3) | 8.5 (8.4-9.1) |
| LumexLib std-backed, C++20 (wraps std) | 7.3 (7.3-7.4) | 8.5 (8.1-10.2) | 16.3 (16.2-17.4) | 27.6 (26.7-29.0) | 41.8 (40.2-42.5) | 51.0 (48.6-53.7) | 57.5 (55.4-66.7) |
| std::atomic, libstdc++ 13 | 7.5 (7.4-7.5) | 11.0 (10.4-12.3) | 17.7 (17.0-18.1) | 29.9 (27.7-30.7) | 41.6 (40.4-42.6) | 52.7 (51.5-54.3) | 62.1 (59.9-69.9) |
| boost::atomic_shared_ptr (spinlock), Boost 1_92 | 6.3 (6.1-6.4) | 3.4 (2.9-3.9) | 2.9 (2.8-3.0) | 2.9 (2.8-3.0) | 3.1 (2.6-3.7) | 3.5 (3.4-4.0) | 4.0 (3.7-4.4) |

Share of successful compare-exchanges:

| Series | 1 | 2 | 4 | 8 | 12 | 16 | 20 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| LumexLib lock-based, C++11 | 1.00 | 0.65 | 0.64 | 0.61 | 0.61 | 0.61 | 0.60 |
| LumexLib lock-based, C++20 | 1.00 | 0.64 | 0.67 | 0.60 | 0.57 | 0.56 | 0.56 |
| LumexLib lock-free, C++11 | 1.00 | 0.76 | 0.39 | 0.30 | 0.28 | 0.26 | 0.25 |
| LumexLib lock-free, C++20 | 1.00 | 0.79 | 0.39 | 0.30 | 0.28 | 0.26 | 0.24 |
| LumexLib lock-free, C++20, deferred | 1.00 | 0.77 | 0.40 | 0.30 | 0.28 | 0.26 | 0.25 |
| LumexLib common name, C++20 (lock-free) | 1.00 | 0.75 | 0.39 | 0.30 | 0.28 | 0.25 | 0.24 |
| LumexLib std-backed, C++20 (wraps std) | 1.00 | 0.81 | 0.53 | 0.51 | 0.50 | 0.46 | 0.46 |
| std::atomic, libstdc++ 13 | 1.00 | 0.76 | 0.53 | 0.52 | 0.50 | 0.47 | 0.45 |
| boost::atomic_shared_ptr (spinlock), Boost 1_92 | 1.00 | 1.00 | 1.00 | 0.99 | 0.98 | 0.98 | 0.98 |

## `load_one_writer()`, contended

Ratio to the baseline of the same block (median, with the 25th and 75th percentiles of the repetitions; lower is better).

| Series | 1 | 2 | 4 | 8 | 12 | 16 | 20 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| LumexLib lock-based, C++11 | 2.4 (2.4-2.5) | 2.7 (2.1-3.3) | 3.4 (3.3-3.9) | 5.4 (5.2-5.6) | 6.2 (5.9-6.2) | 7.5 (6.9-7.9) | 8.1 (7.8-8.6) |
| LumexLib lock-based, C++20 | 2.2 (2.2-2.2) | 2.7 (2.5-3.2) | 3.2 (3.2-3.3) | 5.4 (5.3-5.6) | 7.0 (6.9-7.4) | 8.7 (8.5-10.0) | 10.4 (9.3-11.3) |
| LumexLib lock-free, C++11 | 2.6 (2.6-2.7) | 5.1 (2.6-5.7) | 2.8 (2.7-3.0) | 2.1 (2.1-2.2) | 2.0 (1.9-2.1) | 1.9 (1.8-2.0) | 2.0 (1.9-2.2) |
| LumexLib lock-free, C++20 | 2.6 (2.5-2.6) | 4.9 (4.3-5.6) | 2.8 (2.7-2.8) | 2.1 (2.0-2.2) | 1.9 (1.8-2.0) | 2.0 (1.9-2.1) | 2.1 (2.0-2.2) |
| LumexLib lock-free, C++20, deferred | 2.6 (2.5-2.6) | 2.3 (1.8-2.9) | 1.9 (1.8-2.1) | 1.7 (1.6-1.8) | 1.8 (1.7-1.8) | 1.9 (1.9-2.0) | 2.1 (2.0-2.1) |
| LumexLib common name, C++20 (lock-free) | 2.6 (2.6-2.6) | 7.8 (4.7-8.6) | 3.0 (2.8-3.1) | 2.1 (2.1-2.2) | 2.0 (1.9-2.2) | 2.0 (1.9-2.1) | 2.0 (2.0-2.1) |
| LumexLib std-backed, C++20 (wraps std) | 2.2 (2.2-2.2) | 4.4 (2.9-6.0) | 7.0 (6.4-7.3) | 13.9 (12.8-14.5) | 18.0 (16.3-19.3) | 22.4 (21.3-24.2) | 25.7 (25.0-28.1) |
| std::atomic, libstdc++ 13 | 2.3 (2.2-2.3) | 7.9 (5.1-8.6) | 7.1 (6.9-7.5) | 13.8 (12.9-14.5) | 18.2 (16.6-19.2) | 22.4 (20.5-23.5) | 27.2 (26.4-29.5) |
| boost::atomic_shared_ptr (spinlock), Boost 1_92 | 2.0 (2.0-2.0) | 0.6 (0.5-0.6) | 0.7 (0.7-0.7) | 0.8 (0.8-0.8) | 0.8 (0.8-0.9) | 1.1 (1.1-1.2) | 1.2 (1.1-1.3) |

## Uncontended

One thread on a private object: median ns per operation, the 25th-75th percentile, and the ratio to the uncontended baseline of the same block.

| Operation | Series | Median, ns | p25-p75, ns | x uint64 CAS |
| --- | --- | ---: | ---: | ---: |
| `uint64_cas` | LumexLib lock-based, C++11 | 7.6 | 7.6-7.6 | 1.00 |
| `uint64_cas` | LumexLib lock-based, C++20 | 7.6 | 7.5-7.7 | 1.00 |
| `uint64_cas` | LumexLib lock-free, C++11 | 7.6 | 7.5-7.6 | 1.00 |
| `uint64_cas` | LumexLib lock-free, C++20 | 7.7 | 7.6-7.7 | 1.00 |
| `uint64_cas` | LumexLib lock-free, C++20, deferred | 7.7 | 7.6-7.7 | 1.00 |
| `uint64_cas` | LumexLib common name, C++20 (lock-free) | 7.7 | 7.6-7.9 | 1.00 |
| `uint64_cas` | LumexLib std-backed, C++20 (wraps std) | 7.6 | 7.6-7.7 | 1.00 |
| `uint64_cas` | std::atomic, libstdc++ 13 | 7.6 | 7.5-7.6 | 1.00 |
| `uint64_cas` | boost::atomic_shared_ptr (spinlock), Boost 1_92 | 7.6 | 7.6-7.7 | 1.00 |
| `load` | LumexLib lock-based, C++11 | 18.5 | 18.5-18.6 | 2.43 |
| `load` | LumexLib lock-based, C++20 | 16.7 | 16.5-16.8 | 2.20 |
| `load` | LumexLib lock-free, C++11 | 19.5 | 19.3-19.6 | 2.58 |
| `load` | LumexLib lock-free, C++20 | 19.6 | 19.4-19.6 | 2.54 |
| `load` | LumexLib lock-free, C++20, deferred | 19.8 | 19.6-20.0 | 2.60 |
| `load` | LumexLib common name, C++20 (lock-free) | 19.6 | 19.4-19.9 | 2.54 |
| `load` | LumexLib std-backed, C++20 (wraps std) | 17.1 | 17.0-17.1 | 2.25 |
| `load` | std::atomic, libstdc++ 13 | 17.0 | 16.9-17.3 | 2.25 |
| `load` | boost::atomic_shared_ptr (spinlock), Boost 1_92 | 15.1 | 15.1-15.2 | 1.99 |
| `store` | LumexLib lock-based, C++11 | 22.3 | 22.2-25.2 | 2.92 |
| `store` | LumexLib lock-based, C++20 | 18.6 | 18.4-18.7 | 2.46 |
| `store` | LumexLib lock-free, C++11 | 44.1 | 44.0-44.7 | 5.89 |
| `store` | LumexLib lock-free, C++20 | 44.5 | 44.2-44.6 | 5.77 |
| `store` | LumexLib lock-free, C++20, deferred | 65.2 | 65.0-66.0 | 8.53 |
| `store` | LumexLib common name, C++20 (lock-free) | 43.8 | 43.5-44.4 | 5.68 |
| `store` | LumexLib std-backed, C++20 (wraps std) | 19.6 | 19.4-19.8 | 2.59 |
| `store` | std::atomic, libstdc++ 13 | 19.6 | 19.5-19.6 | 2.57 |
| `store` | boost::atomic_shared_ptr (spinlock), Boost 1_92 | 24.4 | 24.3-24.5 | 3.19 |
| `exchange` | LumexLib lock-based, C++11 | 20.2 | 20.1-20.3 | 2.65 |
| `exchange` | LumexLib lock-based, C++20 | 18.3 | 18.2-18.5 | 2.43 |
| `exchange` | LumexLib lock-free, C++11 | 53.1 | 52.8-53.5 | 7.02 |
| `exchange` | LumexLib lock-free, C++20 | 53.4 | 52.8-53.6 | 6.92 |
| `exchange` | LumexLib lock-free, C++20, deferred | 72.8 | 71.4-73.2 | 9.42 |
| `exchange` | LumexLib common name, C++20 (lock-free) | 52.7 | 52.3-53.4 | 6.90 |
| `exchange` | LumexLib std-backed, C++20 (wraps std) | 19.8 | 19.8-20.0 | 2.61 |
| `exchange` | std::atomic, libstdc++ 13 | 19.8 | 19.7-20.0 | 2.62 |
| `exchange` | boost::atomic_shared_ptr (spinlock), Boost 1_92 | 24.8 | 24.5-25.2 | 3.24 |
| `compare_exchange_strong` | LumexLib lock-based, C++11 | 47.0 | 46.5-54.1 | 6.18 |
| `compare_exchange_strong` | LumexLib lock-based, C++20 | 43.9 | 43.3-44.5 | 5.78 |
| `compare_exchange_strong` | LumexLib lock-free, C++11 | 74.2 | 73.8-74.4 | 9.76 |
| `compare_exchange_strong` | LumexLib lock-free, C++20 | 74.4 | 73.5-74.7 | 9.70 |
| `compare_exchange_strong` | LumexLib lock-free, C++20, deferred | 95.4 | 94.0-95.8 | 12.33 |
| `compare_exchange_strong` | LumexLib common name, C++20 (lock-free) | 73.9 | 73.4-74.8 | 9.65 |
| `compare_exchange_strong` | LumexLib std-backed, C++20 (wraps std) | 56.6 | 56.3-57.5 | 7.44 |
| `compare_exchange_strong` | std::atomic, libstdc++ 13 | 57.9 | 57.4-58.7 | 7.62 |
| `compare_exchange_strong` | boost::atomic_shared_ptr (spinlock), Boost 1_92 | 46.9 | 46.8-47.1 | 6.15 |
| `load_one_writer` | LumexLib lock-based, C++11 | 18.6 | 18.6-18.9 | 2.46 |
| `load_one_writer` | LumexLib lock-based, C++20 | 16.8 | 16.7-16.8 | 2.21 |
| `load_one_writer` | LumexLib lock-free, C++11 | 19.7 | 19.6-19.8 | 2.63 |
| `load_one_writer` | LumexLib lock-free, C++20 | 19.8 | 19.6-20.0 | 2.59 |
| `load_one_writer` | LumexLib lock-free, C++20, deferred | 19.6 | 19.5-19.8 | 2.55 |
| `load_one_writer` | LumexLib common name, C++20 (lock-free) | 19.8 | 19.6-19.9 | 2.55 |
| `load_one_writer` | LumexLib std-backed, C++20 (wraps std) | 17.1 | 16.9-17.9 | 2.25 |
| `load_one_writer` | std::atomic, libstdc++ 13 | 17.3 | 17.0-17.7 | 2.27 |
| `load_one_writer` | boost::atomic_shared_ptr (spinlock), Boost 1_92 | 15.7 | 15.5-15.9 | 2.03 |
