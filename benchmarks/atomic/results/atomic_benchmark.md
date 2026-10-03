# Atomic shared pointer benchmark results

- Run 1: 12th Gen Intel(R) Core(TM) i7-12700K, 20 logical CPUs, governor `powersave`, kernel `6.1.170-1-generic` (Linux-6.1.170-1-generic-x86_64-with-glibc2.28); GCC 13.2 with libstdc++ 13, release build; 100 repetitions, 30 s pauses, 100 ms windows; measured 2026-10-03 in 91.3 minutes, load average 9.50 14.26 16.03 at the start and 12.04 10.48 10.05 at the end.

| Series | Selected implementation | Language mode | is_lock_free |
| --- | --- | ---: | ---: |
| LumexLib lock-based, C++11 | `lock_based_table_wait` | 201103 | no |
| LumexLib lock-based, C++20 | `lock_based_std_wait` | 202002 | no |
| LumexLib default, C++20 (wraps std) | `std_backed_std_wait` | 202002 | no |
| std::atomic, libstdc++ 13 | `libstdc++ 13` | 202002 | no |

## Baseline

`std::atomic<uint64_t>::compare_exchange_strong` with all threads on one word, timed right before each series' block (ns per operation and thread, median). It is the same code every time, so the rows should agree; the ratio of each row to the mean of the rows shows how far the machine drifted between the blocks.

| Series | 1 | 2 | 4 | 6 | 8 | 10 | 12 | 14 | 16 | 18 | 20 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| LumexLib lock-based, C++11 | 7.64 | 35.01 | 72.83 | 112.22 | 153.88 | 209.17 | 251.45 | 276.83 | 290.88 | 305.04 | 336.03 |
| LumexLib lock-based, C++20 | 7.63 | 34.69 | 72.16 | 112.56 | 158.12 | 216.08 | 262.50 | 288.36 | 308.27 | 319.19 | 349.43 |
| LumexLib default, C++20 (wraps std) | 7.64 | 34.71 | 72.35 | 112.94 | 157.01 | 213.49 | 256.74 | 285.20 | 306.47 | 321.42 | 350.85 |
| std::atomic, libstdc++ 13 | 7.64 | 34.34 | 74.10 | 111.64 | 155.55 | 209.11 | 249.40 | 279.50 | 291.75 | 306.30 | 336.65 |
| LumexLib lock-based, C++11 / mean | 1.00 | 1.01 | 1.00 | 1.00 | 0.99 | 0.99 | 0.99 | 0.98 | 0.97 | 0.97 | 0.98 |
| LumexLib lock-based, C++20 / mean | 1.00 | 1.00 | 0.99 | 1.00 | 1.01 | 1.02 | 1.03 | 1.02 | 1.03 | 1.02 | 1.02 |
| LumexLib default, C++20 (wraps std) / mean | 1.00 | 1.00 | 0.99 | 1.01 | 1.01 | 1.01 | 1.01 | 1.01 | 1.02 | 1.03 | 1.02 |
| std::atomic, libstdc++ 13 / mean | 1.00 | 0.99 | 1.02 | 0.99 | 1.00 | 0.99 | 0.98 | 0.99 | 0.97 | 0.98 | 0.98 |

## `load()`, contended

Ratio to the baseline of the same block (median, with the 25th and 75th percentiles of the repetitions; lower is better).

| Series | 1 | 2 | 4 | 6 | 8 | 10 | 12 | 14 | 16 | 18 | 20 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| LumexLib lock-based, C++11 | 2.5 (2.4-2.5) | 3.7 (3.4-4.0) | 3.7 (3.5-3.9) | 4.4 (4.2-4.6) | 4.9 (4.7-5.2) | 5.1 (4.8-5.4) | 5.5 (5.2-6.0) | 6.3 (5.8-6.7) | 7.0 (6.5-7.7) | 7.7 (7.0-8.2) | 8.0 (7.3-8.6) |
| LumexLib lock-based, C++20 | 2.2 (2.2-2.2) | 3.7 (3.5-4.0) | 4.2 (4.0-4.4) | 5.2 (4.9-5.4) | 6.0 (5.8-6.2) | 6.4 (6.2-6.8) | 7.3 (6.8-7.7) | 7.9 (7.5-8.7) | 8.9 (8.2-9.7) | 9.8 (8.9-10.7) | 10.1 (9.4-11.2) |
| LumexLib default, C++20 (wraps std) | 2.2 (2.2-2.3) | 4.0 (3.7-4.2) | 7.4 (6.8-7.8) | 10.8 (10.1-11.6) | 14.5 (13.9-15.2) | 16.4 (15.4-17.5) | 17.8 (16.2-18.7) | 19.7 (18.1-21.2) | 22.2 (20.5-23.9) | 24.6 (23.0-26.5) | 27.5 (25.0-29.6) |
| std::atomic, libstdc++ 13 | 2.2 (2.2-2.3) | 3.9 (3.7-4.2) | 7.2 (6.7-7.8) | 10.8 (10.4-11.7) | 14.9 (14.1-15.5) | 16.7 (15.5-18.3) | 18.2 (17.0-19.3) | 20.2 (19.0-21.6) | 22.8 (21.4-24.3) | 26.1 (24.3-27.9) | 28.1 (26.0-31.4) |

## `store()`, contended

Ratio to the baseline of the same block (median, with the 25th and 75th percentiles of the repetitions; lower is better).

| Series | 1 | 2 | 4 | 6 | 8 | 10 | 12 | 14 | 16 | 18 | 20 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| LumexLib lock-based, C++11 | 2.9 (2.9-2.9) | 3.3 (3.0-3.7) | 5.5 (4.5-6.1) | 6.7 (6.2-7.3) | 7.4 (7.0-8.0) | 7.5 (7.1-7.9) | 7.9 (7.4-8.4) | 8.4 (7.9-9.1) | 9.3 (8.7-9.9) | 10.2 (9.4-10.8) | 10.4 (10.0-11.1) |
| LumexLib lock-based, C++20 | 2.4 (2.4-2.4) | 3.3 (3.0-3.7) | 5.8 (5.0-6.3) | 7.1 (6.5-7.9) | 7.8 (7.4-8.4) | 8.3 (7.8-8.7) | 8.9 (8.3-9.6) | 9.4 (9.0-10.4) | 10.4 (9.7-11.1) | 11.1 (10.4-12.1) | 11.6 (11.0-12.6) |
| LumexLib default, C++20 (wraps std) | 2.6 (2.5-2.6) | 2.5 (2.4-2.8) | 2.7 (2.5-4.1) | 5.8 (5.3-6.3) | 7.9 (7.4-8.5) | 10.2 (9.7-10.8) | 11.7 (10.9-12.5) | 13.2 (12.2-14.2) | 14.6 (13.3-15.8) | 15.8 (14.7-17.1) | 17.2 (15.8-18.6) |
| std::atomic, libstdc++ 13 | 2.6 (2.5-2.6) | 2.6 (2.4-3.0) | 2.8 (2.5-3.9) | 6.2 (5.6-6.9) | 9.1 (8.3-10.0) | 11.0 (10.4-11.8) | 12.5 (11.7-13.4) | 13.9 (12.8-15.4) | 15.4 (14.5-16.9) | 17.4 (16.0-18.5) | 18.8 (17.4-20.6) |

## `exchange()`, contended

Ratio to the baseline of the same block (median, with the 25th and 75th percentiles of the repetitions; lower is better).

| Series | 1 | 2 | 4 | 6 | 8 | 10 | 12 | 14 | 16 | 18 | 20 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| LumexLib lock-based, C++11 | 2.6 (2.6-2.7) | 3.3 (2.9-3.6) | 4.4 (3.8-5.3) | 6.9 (6.1-7.6) | 7.4 (7.0-8.1) | 7.5 (7.1-7.9) | 7.9 (7.5-8.4) | 8.4 (8.1-9.2) | 9.4 (8.7-10.1) | 10.2 (9.6-10.9) | 10.3 (9.8-11.0) |
| LumexLib lock-based, C++20 | 2.4 (2.4-2.5) | 3.2 (2.9-3.6) | 5.3 (4.2-5.9) | 6.9 (6.1-7.6) | 7.7 (7.1-8.3) | 8.2 (7.8-8.7) | 8.8 (8.3-9.6) | 9.6 (8.9-10.4) | 10.5 (9.9-11.3) | 11.3 (10.7-12.3) | 11.8 (10.9-12.8) |
| LumexLib default, C++20 (wraps std) | 2.6 (2.6-2.6) | 3.0 (2.8-3.2) | 5.2 (4.4-5.8) | 8.7 (7.6-9.6) | 11.1 (10.0-12.2) | 14.2 (13.5-15.1) | 15.9 (14.7-17.0) | 17.4 (16.4-18.8) | 18.9 (17.7-20.2) | 20.4 (18.7-21.6) | 22.5 (20.7-25.3) |
| std::atomic, libstdc++ 13 | 2.6 (2.6-2.6) | 3.1 (2.9-3.4) | 5.4 (4.9-6.0) | 8.3 (7.4-9.2) | 10.8 (9.8-11.8) | 14.0 (13.1-14.9) | 16.0 (14.9-17.0) | 17.5 (16.4-19.3) | 19.2 (17.9-20.8) | 20.6 (19.3-23.2) | 23.6 (21.2-26.1) |

## `compare_exchange_strong()`, contended

Ratio to the baseline of the same block (median, with the 25th and 75th percentiles of the repetitions; lower is better).

| Series | 1 | 2 | 4 | 6 | 8 | 10 | 12 | 14 | 16 | 18 | 20 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| LumexLib lock-based, C++11 | 6.1 (6.1-6.2) | 10.7 (10.2-11.4) | 14.8 (13.9-16.0) | 16.0 (15.5-17.5) | 17.1 (16.5-18.1) | 16.9 (16.2-17.8) | 17.5 (16.7-18.6) | 18.8 (18.1-20.5) | 20.8 (19.6-22.3) | 22.4 (21.2-24.1) | 22.7 (21.8-24.5) |
| LumexLib lock-based, C++20 | 5.8 (5.7-5.8) | 10.8 (10.2-11.6) | 14.9 (13.8-15.7) | 16.9 (16.1-17.8) | 18.9 (17.9-19.6) | 19.6 (18.5-20.5) | 20.7 (19.4-22.4) | 22.5 (21.0-24.2) | 24.5 (22.7-26.0) | 26.6 (24.9-28.7) | 27.4 (25.9-29.6) |
| LumexLib default, C++20 (wraps std) | 7.4 (7.4-7.5) | 11.4 (11.0-12.2) | 17.5 (16.6-18.7) | 23.1 (21.9-24.0) | 29.2 (27.9-30.4) | 35.4 (34.4-37.1) | 40.5 (38.7-42.4) | 45.0 (42.8-48.3) | 49.7 (47.1-54.1) | 55.3 (51.3-58.7) | 59.8 (55.0-65.2) |
| std::atomic, libstdc++ 13 | 7.5 (7.4-7.5) | 11.7 (11.0-12.3) | 17.4 (16.5-18.9) | 23.6 (22.4-24.8) | 30.0 (28.7-31.2) | 36.6 (35.3-37.7) | 42.0 (39.9-43.7) | 46.9 (44.1-50.0) | 52.4 (49.0-55.2) | 57.3 (53.9-63.6) | 65.5 (60.3-72.2) |

Share of successful compare-exchanges:

| Series | 1 | 2 | 4 | 6 | 8 | 10 | 12 | 14 | 16 | 18 | 20 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| LumexLib lock-based, C++11 | 1.00 | 0.64 | 0.64 | 0.62 | 0.61 | 0.61 | 0.61 | 0.61 | 0.61 | 0.61 | 0.61 |
| LumexLib lock-based, C++20 | 1.00 | 0.64 | 0.68 | 0.65 | 0.62 | 0.59 | 0.58 | 0.57 | 0.57 | 0.56 | 0.56 |
| LumexLib default, C++20 (wraps std) | 1.00 | 0.62 | 0.52 | 0.53 | 0.52 | 0.51 | 0.49 | 0.48 | 0.46 | 0.46 | 0.46 |
| std::atomic, libstdc++ 13 | 1.00 | 0.62 | 0.53 | 0.53 | 0.52 | 0.51 | 0.49 | 0.48 | 0.46 | 0.45 | 0.45 |

## Uncontended

One thread on a private object: median ns per operation, the 25th-75th percentile, and the ratio to the uncontended baseline of the same block.

| Operation | Series | Median, ns | p25-p75, ns | x uint64 CAS |
| --- | --- | ---: | ---: | ---: |
| `uint64_cas` | LumexLib lock-based, C++11 | 7.6 | 7.6-7.7 | 1.00 |
| `uint64_cas` | LumexLib lock-based, C++20 | 7.6 | 7.6-7.7 | 1.00 |
| `uint64_cas` | LumexLib default, C++20 (wraps std) | 7.6 | 7.6-7.7 | 1.00 |
| `uint64_cas` | std::atomic, libstdc++ 13 | 7.6 | 7.6-7.7 | 1.00 |
| `load` | LumexLib lock-based, C++11 | 18.7 | 18.5-18.8 | 2.46 |
| `load` | LumexLib lock-based, C++20 | 16.8 | 16.8-16.9 | 2.21 |
| `load` | LumexLib default, C++20 (wraps std) | 17.1 | 17.0-17.2 | 2.24 |
| `load` | std::atomic, libstdc++ 13 | 17.1 | 17.0-17.3 | 2.25 |
| `store` | LumexLib lock-based, C++11 | 22.1 | 22.0-22.2 | 2.90 |
| `store` | LumexLib lock-based, C++20 | 18.5 | 18.4-18.6 | 2.43 |
| `store` | LumexLib default, C++20 (wraps std) | 19.6 | 19.5-19.7 | 2.57 |
| `store` | std::atomic, libstdc++ 13 | 19.6 | 19.5-19.7 | 2.57 |
| `exchange` | LumexLib lock-based, C++11 | 20.1 | 20.0-20.3 | 2.64 |
| `exchange` | LumexLib lock-based, C++20 | 18.6 | 18.4-18.7 | 2.43 |
| `exchange` | LumexLib default, C++20 (wraps std) | 20.0 | 19.9-20.1 | 2.62 |
| `exchange` | std::atomic, libstdc++ 13 | 19.8 | 19.7-19.9 | 2.59 |
| `compare_exchange_strong` | LumexLib lock-based, C++11 | 46.7 | 46.4-47.0 | 6.12 |
| `compare_exchange_strong` | LumexLib lock-based, C++20 | 44.1 | 43.9-44.4 | 5.78 |
| `compare_exchange_strong` | LumexLib default, C++20 (wraps std) | 57.0 | 56.7-57.4 | 7.47 |
| `compare_exchange_strong` | std::atomic, libstdc++ 13 | 56.9 | 56.6-57.3 | 7.47 |
